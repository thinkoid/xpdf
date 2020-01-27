// -*- mode: c++; -*-
// Copyright 2008-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <goo/GString.hh>
#include <goo/GList.hh>

#include <xpdf/array.hh>
#include <xpdf/Error.hh>
#include <xpdf/obj.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/TextString.hh>
#include <xpdf/OptionalContent.hh>

//------------------------------------------------------------------------

#define ocPolicyAllOn 1
#define ocPolicyAnyOn 2
#define ocPolicyAnyOff 3
#define ocPolicyAllOff 4

//------------------------------------------------------------------------

// Max depth of nested visibility expressions.  This is used to catch
// infinite loops in the visibility expression object structure.
#define visibilityExprRecursionLimit 50

// Max depth of nested display nodes.  This is used to catch infinite
// loops in the "Order" object structure.
#define displayNodeRecursionLimit 50

//------------------------------------------------------------------------

OptionalContent::OptionalContent (PDFDoc* doc) {
    Object* ocProps;
    Object ocgList, defView, obj1, obj2;
    Ref ref1;
    OptionalContentGroup* ocg;
    int i;

    xref = doc->getXRef ();
    ocgs = new GList ();
    display = NULL;

    if ((ocProps = doc->getCatalog ()->getOCProperties ())->is_dict ()) {
        if (ocProps->dictLookup ("OCGs", &ocgList)->is_array ()) {
            //----- read the OCG list
            for (i = 0; i < ocgList.as_array ().size (); ++i) {
                obj1 = ocgList [i];

                if (obj1.is_ref ()) {
                    ref1 = obj1.as_ref ();
                    obj2 = resolve (obj1);
                    if ((ocg = OptionalContentGroup::parse (&ref1, &obj2))) {
                        ocgs->append (ocg);
                    }
                }
            }

            //----- read the default viewing OCCD
            if (ocProps->dictLookup ("D", &defView)->is_dict ()) {
                //----- initial state
                if (defView.dictLookup ("OFF", &obj1)->is_array ()) {
                    for (i = 0; i < obj1.as_array ().size (); ++i) {
                        obj2 = obj1 [i];
                        if (obj2.is_ref ()) {
                            ref1 = obj2.as_ref ();
                            if ((ocg = findOCG (&ref1))) {
                                ocg->setState (false);
                            }
                            else {
                                error (
                                    errSyntaxError, -1,
                                    "Invalid OCG reference in OFF array in "
                                    "default viewing OCCD");
                            }
                        }
                    }
                }

                //----- display order
                if (defView.dictLookup ("Order", &obj1)->is_array ()) {
                    display = OCDisplayNode::parse (&obj1, this, xref);
                }
            }
            else {
                error (
                    errSyntaxError, -1,
                    "Missing or invalid default viewing OCCD");
            }
        }
    }

    if (!display) { display = new OCDisplayNode (); }
}

OptionalContent::~OptionalContent () {
    deleteGList (ocgs, OptionalContentGroup);
    delete display;
}

int OptionalContent::getNumOCGs () { return ocgs->getLength (); }

OptionalContentGroup* OptionalContent::getOCG (int idx) {
    return (OptionalContentGroup*)ocgs->get (idx);
}

OptionalContentGroup* OptionalContent::findOCG (Ref* ref) {
    OptionalContentGroup* ocg;
    int i;

    for (i = 0; i < ocgs->getLength (); ++i) {
        ocg = (OptionalContentGroup*)ocgs->get (i);
        if (ocg->matches (ref)) { return ocg; }
    }
    return NULL;
}

bool OptionalContent::evalOCObject (Object* obj, bool* visible) {
    OptionalContentGroup* ocg;
    int policy;
    Ref ref;
    Object obj2, obj3, obj4, obj5;
    int i;

    if (obj->is_null ()) { return false; }
    if (obj->is_ref ()) {
        ref = obj->as_ref ();
        if ((ocg = findOCG (&ref))) {
            *visible = ocg->getState ();
            return true;
        }
    }

    obj2 = resolve (*obj);

    if (!obj2.is_dict ("OCMD")) {
        return false;
    }

    if (obj2.dictLookup ("VE", &obj3)->is_array ()) {
        *visible = evalOCVisibilityExpr (&obj3, 0);
    }
    else {
        policy = ocPolicyAnyOn;
        if (obj2.dictLookup ("P", &obj3)->is_name ()) {
            if (obj3.is_name ("AllOn")) { policy = ocPolicyAllOn; }
            else if (obj3.is_name ("AnyOn")) {
                policy = ocPolicyAnyOn;
            }
            else if (obj3.is_name ("AnyOff")) {
                policy = ocPolicyAnyOff;
            }
            else if (obj3.is_name ("AllOff")) {
                policy = ocPolicyAllOff;
            }
        }
        obj2.dictLookupNF ("OCGs", &obj3);
        ocg = NULL;
        if (obj3.is_ref ()) {
            ref = obj3.as_ref ();
            ocg = findOCG (&ref);
        }
        if (ocg) {
            *visible = (policy == ocPolicyAllOn || policy == ocPolicyAnyOn)
                           ? ocg->getState ()
                           : !ocg->getState ();
        }
        else {
            *visible = policy == ocPolicyAllOn || policy == ocPolicyAllOff;
            if (!(obj4 = resolve (obj3)).is_array ()) {
                return false;
            }
            for (i = 0; i < obj4.as_array ().size (); ++i) {
                obj5 = obj4 [i];
                if (obj5.is_ref ()) {
                    ref = obj5.as_ref ();
                    if (!(ocg = findOCG (&ref))) {
                        return false;
                    }
                    switch (policy) {
                    case ocPolicyAllOn:
                        *visible = *visible && ocg->getState ();
                        break;
                    case ocPolicyAnyOn:
                        *visible = *visible || ocg->getState ();
                        break;
                    case ocPolicyAnyOff:
                        *visible = *visible || !ocg->getState ();
                        break;
                    case ocPolicyAllOff:
                        *visible = *visible && !ocg->getState ();
                        break;
                    }
                }
            }
        }
    }
    return true;
}

bool OptionalContent::evalOCVisibilityExpr (Object* expr, int recursion) {
    OptionalContentGroup* ocg;
    Object expr2, op, obj;
    Ref ref;
    bool ret;
    int i;

    if (recursion > visibilityExprRecursionLimit) {
        error (
            errSyntaxError, -1,
            "Loop detected in optional content visibility expression");
        return true;
    }
    if (expr->is_ref ()) {
        ref = expr->as_ref ();
        if ((ocg = findOCG (&ref))) { return ocg->getState (); }
    }

    expr2 = resolve (*expr);

    if (!expr2.is_array () || expr2.as_array ().size () < 1) {
        error (
            errSyntaxError, -1,
            "Invalid optional content visibility expression");
        return true;
    }
    op = resolve (expr2 [0UL]);
    if (op.is_name ("Not")) {
        if (expr2.as_array ().size () == 2) {
            obj = expr2 [1];
            ret = !evalOCVisibilityExpr (&obj, recursion + 1);
        }
        else {
            error (
                errSyntaxError, -1,
                "Invalid optional content visibility expression");
            ret = true;
        }
    }
    else if (op.is_name ("And")) {
        ret = true;
        for (i = 1; i < expr2.as_array ().size () && ret; ++i) {
            obj = expr2 [i];
            ret = evalOCVisibilityExpr (&obj, recursion + 1);
        }
    }
    else if (op.is_name ("Or")) {
        ret = false;
        for (i = 1; i < expr2.as_array ().size () && !ret; ++i) {
            obj = expr2 [i];
            ret = evalOCVisibilityExpr (&obj, recursion + 1);
        }
    }
    else {
        error (
            errSyntaxError, -1,
            "Invalid optional content visibility expression");
        ret = true;
    }
    return ret;
}

//------------------------------------------------------------------------

OptionalContentGroup* OptionalContentGroup::parse (Ref* refA, Object* obj) {
    TextString* nameA;
    Object obj1, obj2, obj3;
    OCUsageState viewStateA, printStateA;

    if (!obj->is_dict ()) { return NULL; }
    if (!obj->dictLookup ("Name", &obj1)->is_string ()) {
        error (errSyntaxError, -1, "Missing or invalid Name in OCG");
        return NULL;
    }
    nameA = new TextString (obj1.as_string ());

    viewStateA = printStateA = ocUsageUnset;
    if (obj->dictLookup ("Usage", &obj1)->is_dict ()) {
        if (obj1.dictLookup ("View", &obj2)->is_dict ()) {
            if (obj2.dictLookup ("ViewState", &obj3)->is_name ()) {
                if (obj3.is_name ("ON")) { viewStateA = ocUsageOn; }
                else {
                    viewStateA = ocUsageOff;
                }
            }
        }
        if (obj1.dictLookup ("Print", &obj2)->is_dict ()) {
            if (obj2.dictLookup ("PrintState", &obj3)->is_name ()) {
                if (obj3.is_name ("ON")) { printStateA = ocUsageOn; }
                else {
                    printStateA = ocUsageOff;
                }
            }
        }
    }

    return new OptionalContentGroup (refA, nameA, viewStateA, printStateA);
}

OptionalContentGroup::OptionalContentGroup (
    Ref* refA, TextString* nameA, OCUsageState viewStateA,
    OCUsageState printStateA) {
    ref = *refA;
    name = nameA;
    viewState = viewStateA;
    printState = printStateA;
    state = true;
}

OptionalContentGroup::~OptionalContentGroup () { delete name; }

bool OptionalContentGroup::matches (Ref* refA) {
    return refA->num == ref.num && refA->gen == ref.gen;
}

Unicode* OptionalContentGroup::as_name () { return name->getUnicode (); }

int OptionalContentGroup::getNameLength () { return name->getLength (); }

//------------------------------------------------------------------------

OCDisplayNode* OCDisplayNode::parse (
    Object* obj, OptionalContent* oc, XRef* xref, int recursion) {
    Object obj2, obj3;
    Ref ref;
    OptionalContentGroup* ocgA;
    OCDisplayNode *node, *child;
    int i;

    if (recursion > displayNodeRecursionLimit) {
        error (errSyntaxError, -1, "Loop detected in optional content order");
        return NULL;
    }
    if (obj->is_ref ()) {
        ref = obj->as_ref ();
        if ((ocgA = oc->findOCG (&ref))) { return new OCDisplayNode (ocgA); }
    }
    obj2 = resolve (*obj);
    if (!obj2.is_array ()) {
        return NULL;
    }
    i = 0;
    if (obj2.as_array ().size () >= 1) {
        if ((obj3 = resolve (obj2 [0UL])).is_string ()) {
            node = new OCDisplayNode (obj3.as_string ());
            i = 1;
        }
        else {
            node = new OCDisplayNode ();
        }
    }
    else {
        node = new OCDisplayNode ();
    }
    for (; i < obj2.as_array ().size (); ++i) {
        obj3 = obj2.as_array () [i];
        if ((child = OCDisplayNode::parse (&obj3, oc, xref, recursion + 1))) {
            if (!child->ocg && !child->name && node->getNumChildren () > 0) {
                if (child->getNumChildren () > 0) {
                    node->getChild (node->getNumChildren () - 1)
                        ->addChildren (child->takeChildren ());
                }
                delete child;
            }
            else {
                node->addChild (child);
            }
        }
    }
    return node;
}

OCDisplayNode::OCDisplayNode () {
    name = new TextString ();
    ocg = NULL;
    children = NULL;
}

OCDisplayNode::OCDisplayNode (GString* nameA) {
    name = new TextString (nameA);
    ocg = NULL;
    children = NULL;
}

OCDisplayNode::OCDisplayNode (OptionalContentGroup* ocgA) {
    name = new TextString (ocgA->name);
    ocg = ocgA;
    children = NULL;
}

void OCDisplayNode::addChild (OCDisplayNode* child) {
    if (!children) { children = new GList (); }
    children->append (child);
}

void OCDisplayNode::addChildren (GList* childrenA) {
    if (!children) { children = new GList (); }
    children->append (childrenA);
    delete childrenA;
}

GList* OCDisplayNode::takeChildren () {
    GList* childrenA;

    childrenA = children;
    children = NULL;
    return childrenA;
}

OCDisplayNode::~OCDisplayNode () {
    delete name;
    if (children) { deleteGList (children, OCDisplayNode); }
}

Unicode* OCDisplayNode::as_name () { return name->getUnicode (); }

int OCDisplayNode::getNameLength () { return name->getLength (); }

int OCDisplayNode::getNumChildren () {
    if (!children) { return 0; }
    return children->getLength ();
}

OCDisplayNode* OCDisplayNode::getChild (int idx) {
    return (OCDisplayNode*)children->get (idx);
}
