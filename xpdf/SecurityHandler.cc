// -*- mode: c++; -*-
// Copyright 2004 Glyph & Cog, LLC

#include <defs.hh>

#include <goo/GString.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/Decrypt.hh>
#include <xpdf/Error.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/PDFCore.hh>
#include <xpdf/SecurityHandler.hh>

//------------------------------------------------------------------------
// SecurityHandler
//------------------------------------------------------------------------

SecurityHandler* SecurityHandler::make (PDFDoc* docA, Object* encryptDictA) {
    Object filterObj;
    SecurityHandler* secHdlr;

    encryptDictA->dictLookup ("Filter", &filterObj);
    if (filterObj.isName ("Standard")) {
        secHdlr = new StandardSecurityHandler (docA, encryptDictA);
    }
    else if (filterObj.isName ()) {
            error (
                errSyntaxError, -1,
                "Couldn't find the '{0:s}' security handler",
                filterObj.getName ());
            secHdlr = NULL;
    }
    else {
        error (
            errSyntaxError, -1,
            "Missing or invalid 'Filter' entry in encryption dictionary");
        secHdlr = NULL;
    }
    return secHdlr;
}

SecurityHandler::SecurityHandler (PDFDoc* docA) { doc = docA; }

SecurityHandler::~SecurityHandler () {}

bool SecurityHandler::checkEncryption (
    GString* ownerPassword, GString* userPassword) {
    void* authData;
    bool ok;
    int i;

    if (ownerPassword || userPassword) {
        authData = makeAuthData (ownerPassword, userPassword);
    }
    else {
        authData = NULL;
    }
    ok = authorize (authData);
    if (authData) { freeAuthData (authData); }
    for (i = 0; !ok && i < 3; ++i) {
        if (!(authData = getAuthData ())) { break; }
        ok = authorize (authData);
        if (authData) { freeAuthData (authData); }
    }
    if (!ok) { error (errCommandLine, -1, "Incorrect password"); }
    return ok;
}

//------------------------------------------------------------------------
// StandardSecurityHandler
//------------------------------------------------------------------------

class StandardAuthData {
public:
    StandardAuthData (GString* ownerPasswordA, GString* userPasswordA) {
        ownerPassword = ownerPasswordA;
        userPassword = userPasswordA;
    }

    ~StandardAuthData () {
        if (ownerPassword) { delete ownerPassword; }
        if (userPassword) { delete userPassword; }
    }

    GString* ownerPassword;
    GString* userPassword;
};

StandardSecurityHandler::StandardSecurityHandler (
    PDFDoc* docA, Object* encryptDictA)
    : SecurityHandler (docA) {
    Object versionObj, revisionObj, lengthObj;
    Object ownerKeyObj, userKeyObj, ownerEncObj, userEncObj;
    Object permObj, fileIDObj, fileIDObj1;
    Object cryptFiltersObj, streamFilterObj, stringFilterObj;
    Object cryptFilterObj, cfmObj, cfLengthObj;
    Object encryptMetadataObj;

    ok = false;
    fileID = NULL;
    ownerKey = NULL;
    userKey = NULL;
    ownerEnc = NULL;
    userEnc = NULL;
    fileKeyLength = 0;

    encryptDictA->dictLookup ("V", &versionObj);
    encryptDictA->dictLookup ("R", &revisionObj);
    encryptDictA->dictLookup ("Length", &lengthObj);
    encryptDictA->dictLookup ("O", &ownerKeyObj);
    encryptDictA->dictLookup ("U", &userKeyObj);
    encryptDictA->dictLookup ("OE", &ownerEncObj);
    encryptDictA->dictLookup ("UE", &userEncObj);
    encryptDictA->dictLookup ("P", &permObj);
    doc->getXRef ()->getTrailerDict ()->dictLookup ("ID", &fileIDObj);
    if (versionObj.isInt () && revisionObj.isInt () && permObj.isInt () &&
        ownerKeyObj.isString () && userKeyObj.isString ()) {
        encVersion = versionObj.getInt ();
        encRevision = revisionObj.getInt ();
        if ((encRevision <= 4 && ownerKeyObj.getString ()->getLength () == 32 &&
             userKeyObj.getString ()->getLength () == 32) ||
            ((encRevision == 5 || encRevision == 6) &&
             // the spec says 48 bytes, but Acrobat pads them out longer
             ownerKeyObj.getString ()->getLength () >= 48 &&
             userKeyObj.getString ()->getLength () >= 48 &&
             ownerEncObj.isString () &&
             ownerEncObj.getString ()->getLength () == 32 &&
             userEncObj.isString () &&
             userEncObj.getString ()->getLength () == 32)) {
            encAlgorithm = cryptRC4;
            // revision 2 forces a 40-bit key - some buggy PDF generators
            // set the Length value incorrectly
            if (encRevision == 2 || !lengthObj.isInt ()) { fileKeyLength = 5; }
            else {
                fileKeyLength = lengthObj.getInt () / 8;
            }
            encryptMetadata = true;
            //~ this currently only handles a subset of crypt filter functionality
            //~ (in particular, it ignores the EFF entry in encryptDictA, and
            //~ doesn't handle the case where StmF, StrF, and EFF are not all the
            //~ same)
            if ((encVersion == 4 || encVersion == 5) &&
                (encRevision == 4 || encRevision == 5 || encRevision == 6)) {
                encryptDictA->dictLookup ("CF", &cryptFiltersObj);
                encryptDictA->dictLookup ("StmF", &streamFilterObj);
                encryptDictA->dictLookup ("StrF", &stringFilterObj);
                if (cryptFiltersObj.isDict () && streamFilterObj.isName () &&
                    stringFilterObj.isName () &&
                    !strcmp (
                        streamFilterObj.getName (),
                        stringFilterObj.getName ())) {
                    if (!strcmp (streamFilterObj.getName (), "Identity")) {
                        // no encryption on streams or strings
                        encVersion = encRevision = -1;
                    }
                    else {
                        if (cryptFiltersObj
                                .dictLookup (
                                    streamFilterObj.getName (), &cryptFilterObj)
                                ->isDict ()) {
                            cryptFilterObj.dictLookup ("CFM", &cfmObj);
                            if (cfmObj.isName ("V2")) {
                                encVersion = 2;
                                encRevision = 3;
                                if (cryptFilterObj
                                        .dictLookup ("Length", &cfLengthObj)
                                        ->isInt ()) {
                                    //~ according to the spec, this should be cfLengthObj / 8
                                    fileKeyLength = cfLengthObj.getInt ();
                                }
                            }
                            else if (cfmObj.isName ("AESV2")) {
                                encVersion = 2;
                                encRevision = 3;
                                encAlgorithm = cryptAES;
                                if (cryptFilterObj
                                        .dictLookup ("Length", &cfLengthObj)
                                        ->isInt ()) {
                                    //~ according to the spec, this should be cfLengthObj / 8
                                    fileKeyLength = cfLengthObj.getInt ();
                                }
                            }
                            else if (cfmObj.isName ("AESV3")) {
                                encVersion = 5;
                                if (encRevision != 5 && encRevision != 6) {
                                    encRevision = 6;
                                }
                                encAlgorithm = cryptAES256;
                                if (cryptFilterObj
                                        .dictLookup ("Length", &cfLengthObj)
                                        ->isInt ()) {
                                    //~ according to the spec, this should be cfLengthObj / 8
                                    fileKeyLength = cfLengthObj.getInt ();
                                }
                            }
                        }
                    }
                }
                if (encryptDictA
                        ->dictLookup ("EncryptMetadata", &encryptMetadataObj)
                        ->isBool ()) {
                    encryptMetadata = encryptMetadataObj.getBool ();
                }
            }
            permFlags = permObj.getInt ();
            ownerKey = ownerKeyObj.getString ()->copy ();
            userKey = userKeyObj.getString ()->copy ();
            if (encVersion >= 1 && encVersion <= 2 && encRevision >= 2 &&
                encRevision <= 3) {
                if (fileIDObj.isArray ()) {
                    if (fileIDObj.arrayGet (0, &fileIDObj1)->isString ()) {
                        fileID = fileIDObj1.getString ()->copy ();
                    }
                    else {
                        fileID = new GString ();
                    }
                }
                else {
                    fileID = new GString ();
                }
                if (fileKeyLength > 16 || fileKeyLength <= 0) {
                    fileKeyLength = 16;
                }
                ok = true;
            }
            else if (
                encVersion == 5 && (encRevision == 5 || encRevision == 6)) {
                fileID = new GString (); // unused for V=R=5
                ownerEnc = ownerEncObj.getString ()->copy ();
                userEnc = userEncObj.getString ()->copy ();
                if (fileKeyLength > 32 || fileKeyLength <= 0) {
                    fileKeyLength = 32;
                }
                ok = true;
            }
            else if (!(encVersion == -1 && encRevision == -1)) {
                error (
                    errUnimplemented, -1,
                    "Unsupported version/revision ({0:d}/{1:d}) of Standard "
                    "security handler",
                    encVersion, encRevision);
            }
        }
        else {
            error (errSyntaxError, -1, "Invalid encryption key length");
        }
    }
    else {
        error (errSyntaxError, -1, "Weird encryption info");
    }
}

StandardSecurityHandler::~StandardSecurityHandler () {
    if (fileID) { delete fileID; }
    if (ownerKey) { delete ownerKey; }
    if (userKey) { delete userKey; }
    if (ownerEnc) { delete ownerEnc; }
    if (userEnc) { delete userEnc; }
}

bool StandardSecurityHandler::isUnencrypted () {
    return encVersion == -1 && encRevision == -1;
}

void* StandardSecurityHandler::makeAuthData (
    GString* ownerPassword, GString* userPassword) {
    return new StandardAuthData (
        ownerPassword ? ownerPassword->copy () : (GString*)NULL,
        userPassword ? userPassword->copy () : (GString*)NULL);
}

void* StandardSecurityHandler::getAuthData () {
    PDFCore* core;
    GString* password;

    if (!(core = doc->getCore ()) || !(password = core->getPassword ())) {
        return NULL;
    }
    return new StandardAuthData (password, password->copy ());
}

void StandardSecurityHandler::freeAuthData (void* authData) {
    delete (StandardAuthData*)authData;
}

bool StandardSecurityHandler::authorize (void* authData) {
    GString *ownerPassword, *userPassword;

    if (!ok) { return false; }
    if (authData) {
        ownerPassword = ((StandardAuthData*)authData)->ownerPassword;
        userPassword = ((StandardAuthData*)authData)->userPassword;
    }
    else {
        ownerPassword = NULL;
        userPassword = NULL;
    }
    if (!Decrypt::makeFileKey (
            encVersion, encRevision, fileKeyLength, ownerKey, userKey, ownerEnc,
            userEnc, permFlags, fileID, ownerPassword, userPassword, fileKey,
            encryptMetadata, &ownerPasswordOk)) {
        return false;
    }
    return true;
}
