//========================================================================
//
// Zoox.h
//
//========================================================================

#ifndef ZOOX_H
#define ZOOX_H

#include <defs.hh>


class GString;
class GList;
class GHash;

class ZxAttr;
class ZxDocTypeDecl;
class ZxElement;
class ZxXMLDecl;

//------------------------------------------------------------------------

class ZxNode {
public:
    ZxNode ();
    virtual ~ZxNode ();

    virtual bool isDoc () { return false; }
    virtual bool isXMLDecl () { return false; }
    virtual bool isDocTypeDecl () { return false; }
    virtual bool isComment () { return false; }
    virtual bool isPI () { return false; }
    virtual bool isElement () { return false; }
    virtual bool isElement (const char* type) { return false; }
    virtual bool isCharData () { return false; }
    virtual ZxNode* getFirstChild () { return firstChild; }
    virtual ZxNode* getNextChild () { return next; }
    ZxElement* findFirstElement (const char* type);
    ZxElement* findFirstChildElement (const char* type);
    GList* findAllElements (const char* type);
    GList* findAllChildElements (const char* type);
    virtual void addChild (ZxNode* child);

protected:
    void findAllElements (const char* type, GList* results);

    ZxNode* next;
    ZxNode* parent;
    ZxNode *firstChild, *lastChild;
};

//------------------------------------------------------------------------

class ZxDoc : public ZxNode {
public:
    ZxDoc ();

    // Parse from memory.  Returns NULL on error.
    static ZxDoc* loadMem (const char* data, unsigned dataLen);

    // Parse from disk.  Returns NULL on error.
    static ZxDoc* loadFile (const char* fileName);

    virtual ~ZxDoc ();

    virtual bool isDoc () { return true; }
    ZxXMLDecl* getXMLDecl () { return xmlDecl; }
    ZxDocTypeDecl* getDocTypeDecl () { return docTypeDecl; }
    ZxElement* getRoot () { return root; }
    virtual void addChild (ZxNode* node);

private:
    bool parse (const char* data, unsigned dataLen);
    void parseXMLDecl (ZxNode* par);
    void parseDocTypeDecl (ZxNode* par);
    void parseElement (ZxNode* par);
    ZxAttr* parseAttr ();
    void parseContent (ZxElement* par);
    void parseCharData (ZxElement* par);
    void appendUTF8 (GString* s, int c);
    void parseCDSect (ZxNode* par);
    void parseMisc (ZxNode* par);
    void parseComment (ZxNode* par);
    void parsePI (ZxNode* par);
    GString* parseName ();
    GString* parseQuotedString ();
    void parseSpace ();
    bool match (const char* s);

    ZxXMLDecl* xmlDecl;         // may be NULL
    ZxDocTypeDecl* docTypeDecl; // may be NULL
    ZxElement* root;            // may be NULL

    const char* parsePtr;
    const char* parseEnd;
};

//------------------------------------------------------------------------

class ZxXMLDecl : public ZxNode {
public:
    ZxXMLDecl (GString* versionA, GString* encodingA, bool standaloneA);
    virtual ~ZxXMLDecl ();

    virtual bool isXMLDecl () { return true; }
    GString* getVersion () { return version; }
    GString* getEncoding () { return encoding; }
    bool getStandalone () { return standalone; }

private:
    GString* version;
    GString* encoding; // may be NULL
    bool standalone;
};

//------------------------------------------------------------------------

class ZxDocTypeDecl : public ZxNode {
public:
    ZxDocTypeDecl (GString* nameA);
    virtual ~ZxDocTypeDecl ();

    virtual bool isDocTypeDecl () { return true; }
    GString* getName () { return name; }

private:
    GString* name;
};

//------------------------------------------------------------------------

class ZxComment : public ZxNode {
public:
    ZxComment (GString* textA);
    virtual ~ZxComment ();

    virtual bool isComment () { return true; }
    GString* getText () { return text; }

private:
    GString* text;
};

//------------------------------------------------------------------------

class ZxPI : public ZxNode {
public:
    ZxPI (GString* targetA, GString* textA);
    virtual ~ZxPI ();

    virtual bool isPI () { return true; }
    GString* getTarget () { return target; }
    GString* getText () { return text; }

private:
    GString* target;
    GString* text;
};

//------------------------------------------------------------------------

class ZxElement : public ZxNode {
public:
    ZxElement (GString* typeA);
    virtual ~ZxElement ();

    virtual bool isElement () { return true; }
    virtual bool isElement (const char* typeA);
    GString* getType () { return type; }
    ZxAttr* findAttr (const char* attrName);
    ZxAttr* getFirstAttr () { return firstAttr; }
    void addAttr (ZxAttr* attr);

private:
    GString* type;
    GHash* attrs; // [ZxAttr]
    ZxAttr *firstAttr, *lastAttr;
};

//------------------------------------------------------------------------

class ZxAttr {
public:
    ZxAttr (GString* nameA, GString* valueA);
    ~ZxAttr ();

    GString* getName () { return name; }
    GString* getValue () { return value; }
    ZxAttr* getNextAttr () { return next; }

private:
    GString* name;
    GString* value;
    ZxElement* parent;
    ZxAttr* next;

    friend class ZxElement;
};

//------------------------------------------------------------------------

class ZxCharData : public ZxNode {
public:
    ZxCharData (GString* dataA, bool parsedA);
    virtual ~ZxCharData ();

    virtual bool isCharData () { return true; }
    GString* getData () { return data; }
    bool isParsed () { return parsed; }

private:
    GString* data; // in UTF-8 format
    bool parsed;
};

#endif
