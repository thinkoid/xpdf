// -*- mode: c++; -*-
// Copyright 2008-2013 Glyph & Cog, LLC

#ifndef XPDF_XPDF_OPTIONALCONTENT_HH
#define XPDF_XPDF_OPTIONALCONTENT_HH

#include <defs.hh>

#include <xpdf/obj.hh>
#include <xpdf/CharTypes.hh>

class GList;
class PDFDoc;
class TextString;
class XRef;
class OptionalContentGroup;
class OCDisplayNode;

//------------------------------------------------------------------------

class OptionalContent {
public:
    OptionalContent (PDFDoc* doc);
    ~OptionalContent ();

    // Walk the list of optional content groups.
    int getNumOCGs ();
    OptionalContentGroup* getOCG (int idx);

    // Find an OCG by indirect reference.
    OptionalContentGroup* findOCG (Ref* ref);

    // Get the root node of the optional content group display tree
    // (which does not necessarily include all of the OCGs).
    OCDisplayNode* getDisplayRoot () { return display; }

    // Evaluate an optional content object -- either an OCG or an OCMD.
    // If <obj> is a valid OCG or OCMD, sets *<visible> and returns
    // true; otherwise returns false.
    bool evalOCObject (Object* obj, bool* visible);

private:
    bool evalOCVisibilityExpr (Object* expr, int recursion);

    XRef* xref;
    GList* ocgs;            // all OCGs [OptionalContentGroup]
    OCDisplayNode* display; // root node of display tree
};

//------------------------------------------------------------------------

// Values from the optional content usage dictionary.
enum OCUsageState { ocUsageOn, ocUsageOff, ocUsageUnset };

//------------------------------------------------------------------------

class OptionalContentGroup {
public:
    static OptionalContentGroup* parse (Ref* refA, Object* obj);
    ~OptionalContentGroup ();

    bool matches (Ref* refA);

    Unicode* as_name ();
    int getNameLength ();
    OCUsageState getViewState () { return viewState; }
    OCUsageState getPrintState () { return printState; }
    bool getState () { return state; }
    void setState (bool stateA) { state = stateA; }

private:
    OptionalContentGroup (
        Ref* refA, TextString* nameA, OCUsageState viewStateA,
        OCUsageState printStateA);

    Ref ref;
    TextString* name;
    OCUsageState viewState, // suggested state when viewing
        printState;         // suggested state when printing
    bool state;            // current state (on/off)

    friend class OCDisplayNode;
};

//------------------------------------------------------------------------

class OCDisplayNode {
public:
    static OCDisplayNode*
    parse (Object* obj, OptionalContent* oc, XRef* xref, int recursion = 0);
    OCDisplayNode ();
    ~OCDisplayNode ();

    Unicode* as_name ();
    int getNameLength ();
    OptionalContentGroup* getOCG () { return ocg; }
    int getNumChildren ();
    OCDisplayNode* getChild (int idx);

private:
    OCDisplayNode (GString* nameA);
    OCDisplayNode (OptionalContentGroup* ocgA);
    void addChild (OCDisplayNode* child);
    void addChildren (GList* childrenA);
    GList* takeChildren ();

    TextString* name;          // display name
    OptionalContentGroup* ocg; // NULL for display labels
    GList* children;           // NULL if there are no children
        //   [OCDisplayNode]
};

#endif // XPDF_XPDF_OPTIONALCONTENT_HH
