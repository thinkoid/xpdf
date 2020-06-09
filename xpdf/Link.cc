// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstddef>
#include <cstring>

#include <utils/memory.hh>
#include <utils/string.hh>

#include <xpdf/Error.hh>
#include <xpdf/obj.hh>
#include <xpdf/array.hh>
#include <xpdf/dict.hh>
#include <xpdf/Link.hh>
#include <xpdf/Stream.hh>

//------------------------------------------------------------------------
// LinkAction
//------------------------------------------------------------------------

LinkAction* LinkAction::parseDest (Object* obj) {
    LinkAction* action;

    action = new LinkGoTo (obj);
    if (!action->isOk ()) {
        delete action;
        return NULL;
    }
    return action;
}

LinkAction* LinkAction::parseAction (Object* obj, GString* baseURI) {
    LinkAction* action;
    Object obj2, obj3, obj4, obj5;

    if (!obj->is_dict ()) {
        error (errSyntaxWarning, -1, "Bad annotation action");
        return NULL;
    }

    obj2 = resolve (obj->as_dict ()["S"]);

    // GoTo action
    if (obj2.is_name ("GoTo")) {
        obj3 = resolve (obj->as_dict ()["D"]);
        action = new LinkGoTo (&obj3);

        // GoToR action
    }
    else if (obj2.is_name ("GoToR")) {
        obj3 = resolve (obj->as_dict ()["F"]);
        obj4 = resolve (obj->as_dict ()["D"]);
        action = new LinkGoToR (&obj3, &obj4);

        // Launch action
    }
    else if (obj2.is_name ("Launch")) {
        action = new LinkLaunch (obj);

        // URI action
    }
    else if (obj2.is_name ("URI")) {
        obj3 = resolve (obj->as_dict ()["URI"]);
        action = new LinkURI (&obj3, baseURI);

        // Named action
    }
    else if (obj2.is_name ("Named")) {
        obj3 = resolve (obj->as_dict ()["N"]);
        action = new LinkNamed (&obj3);

        // Movie action
    }
    else if (obj2.is_name ("Movie")) {
        obj3 = (*obj).as_dict ()["Annot"];
        obj4 = resolve (obj->as_dict ()["T"]);
        action = new LinkMovie (&obj3, &obj4);

        // JavaScript action
    }
    else if (obj2.is_name ("JavaScript")) {
        obj3 = resolve (obj->as_dict ()["JS"]);
        action = new LinkJavaScript (&obj3);

        // SubmitForm action
    }
    else if (obj2.is_name ("SubmitForm")) {
        obj3 = resolve (obj->as_dict ()["F"]);
        obj4 = resolve (obj->as_dict ()["Fields"]);
        obj5 = resolve (obj->as_dict ()["Flags"]);
        action = new LinkSubmitForm (&obj3, &obj4, &obj5);

        // Hide action
    }
    else if (obj2.is_name ("Hide")) {
        obj3 = (*obj).as_dict ()["T"];
        obj4 = resolve (obj->as_dict ()["H"]);
        action = new LinkHide (&obj3, &obj4);

        // unknown action
    }
    else if (obj2.is_name ()) {
        action = new LinkUnknown (obj2.as_name ());

        // action is missing or wrong type
    }
    else {
        error (errSyntaxWarning, -1, "Bad annotation action");
        action = NULL;
    }


    if (action && !action->isOk ()) {
        delete action;
        return NULL;
    }
    return action;
}

GString* LinkAction::getFileSpecName (Object* fileSpecObj) {
    GString* name;
    Object obj1;

    name = NULL;

    // string
    if (fileSpecObj->is_string ()) {
        name = fileSpecObj->as_string ()->copy ();
        // dictionary
    }
    else if (fileSpecObj->is_dict ()) {
        if (!(obj1 = resolve (fileSpecObj->as_dict ()["Unix"])).is_string ()) {
            obj1 = resolve (fileSpecObj->as_dict ()["F"]);
        }

        if (obj1.is_string ()) {
            name = obj1.as_string ()->copy ();
        }
        else {
            error (errSyntaxWarning, -1, "Illegal file spec in link");
        }

        // error
    }
    else {
        error (errSyntaxWarning, -1, "Illegal file spec in link");
    }

    // system-dependent path manipulation
    if (name) {}

    return name;
}

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

LinkDest::LinkDest (Array& arr) {
    Object obj1, obj2;

    // initialize fields
    left = bottom = right = top = zoom = 0;
    ok = false;

    // get page
    if (arr.size () < 2) {
        error (
            errSyntaxWarning, -1, "Annotation destination array is too short");
        return;
    }
    obj1 = arr [0];
    if (obj1.is_int ()) {
        pageNum = obj1.as_int () + 1;
        pageIsRef = false;
    }
    else if (obj1.is_ref ()) {
        pageRef.num = obj1.getRefNum ();
        pageRef.gen = obj1.getRefGen ();
        pageIsRef = true;
    }
    else {
        error (errSyntaxWarning, -1, "Bad annotation destination");
        return;
    }

    // get destination type
    obj1 = resolve (arr [1]);

    // XYZ link
    if (obj1.is_name ("XYZ")) {
        kind = destXYZ;
        if (arr.size () < 3) { changeLeft = false; }
        else {
            obj2 = resolve (arr [2]);
            if (obj2.is_null ()) { changeLeft = false; }
            else if (obj2.is_num ()) {
                changeLeft = true;
                left = obj2.as_num ();
            }
            else {
                error (
                    errSyntaxWarning, -1,
                    "Bad annotation destination position");
                return;
            }
        }
        if (arr.size () < 4) { changeTop = false; }
        else {
            obj2 = resolve (arr [3]);
            if (obj2.is_null ()) { changeTop = false; }
            else if (obj2.is_num ()) {
                changeTop = true;
                top = obj2.as_num ();
            }
            else {
                error (
                    errSyntaxWarning, -1,
                    "Bad annotation destination position");
                return;
            }
        }
        if (arr.size () < 5) { changeZoom = false; }
        else {
            obj2 = resolve (arr [4]);
            if (obj2.is_null ()) { changeZoom = false; }
            else if (obj2.is_num ()) {
                changeZoom = true;
                zoom = obj2.as_num ();
            }
            else {
                error (
                    errSyntaxWarning, -1,
                    "Bad annotation destination position");
                return;
            }
        }

        // Fit link
    }
    else if (obj1.is_name ("Fit")) {
        if (arr.size () < 2) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFit;

        // FitH link
    }
    else if (obj1.is_name ("FitH")) {
        if (arr.size () < 3) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFitH;
        if ((obj2 = resolve (arr [2])).is_num ()) {
            top = obj2.as_num ();
            changeTop = true;
        }
        else if (obj2.is_null ()) {
            changeTop = false;
        }
        else {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }

        // FitV link
    }
    else if (obj1.is_name ("FitV")) {
        if (arr.size () < 3) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFitV;
        if ((obj2 = resolve (arr [2])).is_num ()) {
            left = obj2.as_num ();
            changeLeft = true;
        }
        else if (obj2.is_null ()) {
            changeLeft = false;
        }
        else {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }

        // FitR link
    }
    else if (obj1.is_name ("FitR")) {
        if (arr.size () < 6) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFitR;
        if ((obj2 = resolve (arr [2])).is_num ()) {
            left = obj2.as_num ();
        }
        else {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }
        if (!(obj2 = resolve (arr [3])).is_num ()) {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }
        bottom = obj2.as_num ();
        if (!(obj2 = resolve (arr [4])).is_num ()) {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }
        right = obj2.as_num ();
        if (!(obj2 = resolve (arr [5])).is_num ()) {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }
        top = obj2.as_num ();

        // FitB link
    }
    else if (obj1.is_name ("FitB")) {
        if (arr.size () < 2) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFitB;

        // FitBH link
    }
    else if (obj1.is_name ("FitBH")) {
        if (arr.size () < 3) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFitBH;
        if ((obj2 = resolve (arr [2])).is_num ()) {
            top = obj2.as_num ();
            changeTop = true;
        }
        else if (obj2.is_null ()) {
            changeTop = false;
        }
        else {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }

        // FitBV link
    }
    else if (obj1.is_name ("FitBV")) {
        if (arr.size () < 3) {
            error (
                errSyntaxWarning, -1,
                "Annotation destination array is too short");
            return;
        }
        kind = destFitBV;
        if ((obj2 = resolve (arr [2])).is_num ()) {
            left = obj2.as_num ();
            changeLeft = true;
        }
        else if (obj2.is_null ()) {
            changeLeft = false;
        }
        else {
            error (errSyntaxWarning, -1, "Bad annotation destination position");
            kind = destFit;
        }

        // unknown link kind
    }
    else {
        error (errSyntaxWarning, -1, "Unknown annotation destination type");
        return;
    }

    ok = true;
}

LinkDest::LinkDest (LinkDest* dest) {
    kind = dest->kind;
    pageIsRef = dest->pageIsRef;
    if (pageIsRef)
        pageRef = dest->pageRef;
    else
        pageNum = dest->pageNum;
    left = dest->left;
    bottom = dest->bottom;
    right = dest->right;
    top = dest->top;
    zoom = dest->zoom;
    changeLeft = dest->changeLeft;
    changeTop = dest->changeTop;
    changeZoom = dest->changeZoom;
    ok = true;
}

//------------------------------------------------------------------------
// LinkGoTo
//------------------------------------------------------------------------

LinkGoTo::LinkGoTo (Object* destObj) {
    dest = NULL;
    namedDest = NULL;

    // named destination
    if (destObj->is_name ()) { namedDest = new GString (destObj->as_name ()); }
    else if (destObj->is_string ()) {
        namedDest = destObj->as_string ()->copy ();

        // destination dictionary
    }
    else if (destObj->is_array ()) {
        dest = new LinkDest (destObj->as_array ());
        if (!dest->isOk ()) {
            delete dest;
            dest = NULL;
        }

        // error
    }
    else {
        error (errSyntaxWarning, -1, "Illegal annotation destination");
    }
}

LinkGoTo::~LinkGoTo () {
    if (dest) delete dest;
    if (namedDest) delete namedDest;
}

//------------------------------------------------------------------------
// LinkGoToR
//------------------------------------------------------------------------

LinkGoToR::LinkGoToR (Object* fileSpecObj, Object* destObj) {
    dest = NULL;
    namedDest = NULL;

    // get file name
    fileName = getFileSpecName (fileSpecObj);

    // named destination
    if (destObj->is_name ()) { namedDest = new GString (destObj->as_name ()); }
    else if (destObj->is_string ()) {
        namedDest = destObj->as_string ()->copy ();

        // destination dictionary
    }
    else if (destObj->is_array ()) {
        dest = new LinkDest (destObj->as_array ());
        if (!dest->isOk ()) {
            delete dest;
            dest = NULL;
        }

        // error
    }
    else {
        error (errSyntaxWarning, -1, "Illegal annotation destination");
    }
}

LinkGoToR::~LinkGoToR () {
    if (fileName) delete fileName;
    if (dest) delete dest;
    if (namedDest) delete namedDest;
}

//------------------------------------------------------------------------
// LinkLaunch
//------------------------------------------------------------------------

LinkLaunch::LinkLaunch (Object* actionObj) {
    Object obj1, obj2;

    fileName = NULL;
    params = NULL;

    if (actionObj->is_dict ()) {
        if (!(obj1 = resolve (actionObj->as_dict ()["F"])).is_null ()) {
            fileName = getFileSpecName (&obj1);
        }
        else {
            //~ This hasn't been defined by Adobe yet, so assume it looks
            //~ just like the Win dictionary until they say otherwise.
            if ((obj1 = resolve (actionObj->as_dict ()["Unix"])).is_dict ()) {
                obj2 = resolve (obj1.as_dict ()["F"]);
                fileName = getFileSpecName (&obj2);
                if ((obj2 = resolve (obj1.as_dict ()["P"])).is_string ()) {
                    params = obj2.as_string ()->copy ();
                }
            }
            else {
                error (errSyntaxWarning, -1, "Bad launch-type link action");
            }
        }
    }
}

LinkLaunch::~LinkLaunch () {
    if (fileName) delete fileName;
    if (params) delete params;
}

//------------------------------------------------------------------------
// LinkURI
//------------------------------------------------------------------------

LinkURI::LinkURI (Object* uriObj, GString* baseURI) {
    GString* uri2;
    int n;
    char c;

    uri = NULL;
    if (uriObj->is_string ()) {
        uri2 = uriObj->as_string ();
        n = (int)strcspn (uri2->c_str (), "/:");
        if (n < uri2->getLength () && (*uri2) [n] == ':') {
            // "http:..." etc.
            uri = uri2->copy ();
        }
        else if (!uri2->cmpN ("www.", 4)) {
            // "www.[...]" without the leading "http://"
            uri = new GString ("http://");
            uri->append (*uri2);
        }
        else {
            // relative URI
            if (baseURI) {
                uri = baseURI->copy ();
                c = uri->back ();
                if (c != '/' && c != '?') { uri->append (1UL, '/'); }
                if (uri2->front () == '/') {
                    uri->append (
                        uri2->c_str () + 1, uri2->getLength () - 1);
                }
                else {
                    uri->append (*uri2);
                }
            }
            else {
                uri = uri2->copy ();
            }
        }
    }
    else {
        error (errSyntaxWarning, -1, "Illegal URI-type link");
    }
}

LinkURI::~LinkURI () {
    if (uri) delete uri;
}

//------------------------------------------------------------------------
// LinkNamed
//------------------------------------------------------------------------

LinkNamed::LinkNamed (Object* nameObj) {
    name = NULL;
    if (nameObj->is_name ()) { name = new GString (nameObj->as_name ()); }
}

LinkNamed::~LinkNamed () {
    if (name) { delete name; }
}

//------------------------------------------------------------------------
// LinkMovie
//------------------------------------------------------------------------

LinkMovie::LinkMovie (Object* annotObj, Object* titleObj) {
    annotRef.num = -1;
    title = NULL;
    if (annotObj->is_ref ()) { annotRef = annotObj->as_ref (); }
    else if (titleObj->is_string ()) {
        title = titleObj->as_string ()->copy ();
    }
    else {
        error (
            errSyntaxError, -1,
            "Movie action is missing both the Annot and T keys");
    }
}

LinkMovie::~LinkMovie () {
    if (title) { delete title; }
}

//------------------------------------------------------------------------
// LinkJavaScript
//------------------------------------------------------------------------

LinkJavaScript::LinkJavaScript (Object* jsObj) {
    char buf[4096];
    int n;

    if (jsObj->is_string ()) { js = jsObj->as_string ()->copy (); }
    else if (jsObj->is_stream ()) {
        js = new GString ();
        jsObj->streamReset ();
        while ((n = jsObj->as_stream ()->getBlock (buf, sizeof (buf))) > 0) {
            js->append (buf, n);
        }
        jsObj->streamClose ();
    }
    else {
        error (errSyntaxError, -1, "JavaScript action JS key is wrong type");
        js = NULL;
    }
}

LinkJavaScript::~LinkJavaScript () {
    if (js) { delete js; }
}

//------------------------------------------------------------------------
// LinkSubmitForm
//------------------------------------------------------------------------

LinkSubmitForm::LinkSubmitForm (
    Object* urlObj, Object* fieldsObj, Object* flagsObj) {
    if (urlObj->is_string ()) { url = urlObj->as_string ()->copy (); }
    else {
        error (errSyntaxError, -1, "SubmitForm action URL is wrong type");
        url = NULL;
    }

    if (fieldsObj->is_array ()) {
        fields = *fieldsObj;
    }
    else {
        if (!fieldsObj->is_null ()) {
            error (
                errSyntaxError, -1,
                "SubmitForm action Fields value is wrong type");
        }
        fields = { };
    }

    if (flagsObj->is_int ()) {
        flags = flagsObj->as_int ();
    }
    else {
        if (!flagsObj->is_null ()) {
            error (
                errSyntaxError, -1,
                "SubmitForm action Flags value is wrong type");
        }
        flags = 0;
    }
}

LinkSubmitForm::~LinkSubmitForm () {
    if (url) { delete url; }
}

//------------------------------------------------------------------------
// LinkHide
//------------------------------------------------------------------------

LinkHide::LinkHide (Object* fieldsObj, Object* hideFlagObj) {
    if (fieldsObj->is_ref () || fieldsObj->is_string () || fieldsObj->is_array ()) {
        fields = *fieldsObj;
    }
    else {
        error (errSyntaxError, -1, "Hide action T value is wrong type");
        fields = { };
    }

    if (hideFlagObj->is_bool ()) { hideFlag = hideFlagObj->as_bool (); }
    else {
        error (errSyntaxError, -1, "Hide action H value is wrong type");
        hideFlag = false;
    }
}

LinkHide::~LinkHide () { }

//------------------------------------------------------------------------
// LinkUnknown
//------------------------------------------------------------------------

LinkUnknown::LinkUnknown (const char* actionA) {
    action = new GString (actionA);
}

LinkUnknown::~LinkUnknown () { delete action; }

//------------------------------------------------------------------------
// Link
//------------------------------------------------------------------------

Link::Link (const xpdf::obj_t& dictObj, GString* baseURI) {
    ASSERT (dictObj.is_dict ());
    auto& dict = dictObj.as_dict ();

    Object obj2;
    double t;

    action = NULL;
    ok = false;

    auto rect = resolve (dict ["Rect"]);

    // get rectangle
    if (!rect.is_array ()) {
        error (errSyntaxError, -1, "Annotation rectangle is wrong type");
        return;
    }

    if (!(obj2 = resolve (rect [0UL])).is_num ()) {
        error (errSyntaxError, -1, "Bad annotation rectangle");
        return;
    }

    x1 = obj2.as_num ();

    if (!(obj2 = resolve (rect [1])).is_num ()) {
        error (errSyntaxError, -1, "Bad annotation rectangle");
        return;
    }

    y1 = obj2.as_num ();

    if (!(obj2 = resolve (rect [2])).is_num ()) {
        error (errSyntaxError, -1, "Bad annotation rectangle");
        return;
    }

    x2 = obj2.as_num ();

    if (!(obj2 = resolve (rect [3])).is_num ()) {
        error (errSyntaxError, -1, "Bad annotation rectangle");
        return;
    }

    y2 = obj2.as_num ();

    if (x1 > x2) {
        t = x1;
        x1 = x2;
        x2 = t;
    }

    if (y1 > y2) {
        t = y1;
        y1 = y2;
        y2 = t;
    }

    // look for destination
    auto dest = resolve (dict ["Dest"]);

    if (!dest.is_null ()) {
        action = LinkAction::parseDest (&dest);
    }
    else {
        auto A = resolve (dict ["A"]);

        if (A.is_dict ()) {
            action = LinkAction::parseAction (&A, baseURI);
        }
    }

    // check for bad action
    if (action) { ok = true; }
}

Link::~Link () {
    if (action) { delete action; }
}

//------------------------------------------------------------------------
// Links
//------------------------------------------------------------------------

Links::Links (const Object& annots, GString* baseURI) {
    Link* link;
    Object obj1, obj2, obj3;
    int size;
    int i;

    links = NULL;
    size = 0;
    numLinks = 0;

    if (annots.is_array ()) {
        for (i = 0; i < annots.as_array ().size (); ++i) {
            if ((obj1 = resolve (annots [i])).is_dict ()) {
                obj2 = resolve (obj1.as_dict ()["Subtype"]);
                obj3 = resolve (obj1.as_dict ()["FT"]);
                if (obj2.is_name ("Link") ||
                    (obj2.is_name ("Widget") &&
                     (obj3.is_name ("Btn") || obj3.is_null ()))) {
                    link = new Link (obj1, baseURI);
                    if (link->isOk ()) {
                        if (numLinks >= size) {
                            size += 16;
                            links =
                                (Link**)reallocarray (links, size, sizeof (Link*));
                        }
                        links[numLinks++] = link;
                    }
                    else {
                        delete link;
                    }
                }
            }
        }
    }
}

Links::~Links () {
    int i;

    for (i = 0; i < numLinks; ++i) delete links[i];
    free (links);
}

LinkAction* Links::find (double x, double y) {
    int i;

    for (i = numLinks - 1; i >= 0; --i) {
        if (links[i]->inRect (x, y)) { return links[i]->getAction (); }
    }
    return NULL;
}

bool Links::onLink (double x, double y) {
    int i;

    for (i = 0; i < numLinks; ++i) {
        if (links[i]->inRect (x, y)) return true;
    }
    return false;
}
