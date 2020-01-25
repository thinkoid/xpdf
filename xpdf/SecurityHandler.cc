// -*- mode: c++; -*-
// Copyright 2004 Glyph & Cog, LLC

#include <defs.hh>

#include <goo/GString.hh>

#include <xpdf/array.hh>
#include <xpdf/Decrypt.hh>
#include <xpdf/Error.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/PDFCore.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/SecurityHandler.hh>

//------------------------------------------------------------------------
// SecurityHandler
//------------------------------------------------------------------------

SecurityHandler* SecurityHandler::make (PDFDoc* docA, Object* encryptDictA) {
    Object filterObj;
    SecurityHandler* secHdlr;

    encryptDictA->dictLookup ("Filter", &filterObj);
    if (filterObj.is_name ("Standard")) {
        secHdlr = new StandardSecurityHandler (docA, encryptDictA);
    }
    else if (filterObj.is_name ()) {
            error (
                errSyntaxError, -1,
                "Couldn't find the '{0:s}' security handler",
                filterObj.as_name ());
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
    if (versionObj.is_int () && revisionObj.is_int () && permObj.is_int () &&
        ownerKeyObj.is_string () && userKeyObj.is_string ()) {
        encVersion = versionObj.as_int ();
        encRevision = revisionObj.as_int ();
        if ((encRevision <= 4 && ownerKeyObj.as_string ()->getLength () == 32 &&
             userKeyObj.as_string ()->getLength () == 32) ||
            ((encRevision == 5 || encRevision == 6) &&
             // the spec says 48 bytes, but Acrobat pads them out longer
             ownerKeyObj.as_string ()->getLength () >= 48 &&
             userKeyObj.as_string ()->getLength () >= 48 &&
             ownerEncObj.is_string () &&
             ownerEncObj.as_string ()->getLength () == 32 &&
             userEncObj.is_string () &&
             userEncObj.as_string ()->getLength () == 32)) {
            encAlgorithm = cryptRC4;
            // revision 2 forces a 40-bit key - some buggy PDF generators
            // set the Length value incorrectly
            if (encRevision == 2 || !lengthObj.is_int ()) { fileKeyLength = 5; }
            else {
                fileKeyLength = lengthObj.as_int () / 8;
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
                if (cryptFiltersObj.is_dict () && streamFilterObj.is_name () &&
                    stringFilterObj.is_name () &&
                    !strcmp (
                        streamFilterObj.as_name (),
                        stringFilterObj.as_name ())) {
                    if (!strcmp (streamFilterObj.as_name (), "Identity")) {
                        // no encryption on streams or strings
                        encVersion = encRevision = -1;
                    }
                    else {
                        if (cryptFiltersObj
                                .dictLookup (
                                    streamFilterObj.as_name (), &cryptFilterObj)
                                ->is_dict ()) {
                            cryptFilterObj.dictLookup ("CFM", &cfmObj);
                            if (cfmObj.is_name ("V2")) {
                                encVersion = 2;
                                encRevision = 3;
                                if (cryptFilterObj
                                        .dictLookup ("Length", &cfLengthObj)
                                        ->is_int ()) {
                                    //~ according to the spec, this should be cfLengthObj / 8
                                    fileKeyLength = cfLengthObj.as_int ();
                                }
                            }
                            else if (cfmObj.is_name ("AESV2")) {
                                encVersion = 2;
                                encRevision = 3;
                                encAlgorithm = cryptAES;
                                if (cryptFilterObj
                                        .dictLookup ("Length", &cfLengthObj)
                                        ->is_int ()) {
                                    //~ according to the spec, this should be cfLengthObj / 8
                                    fileKeyLength = cfLengthObj.as_int ();
                                }
                            }
                            else if (cfmObj.is_name ("AESV3")) {
                                encVersion = 5;
                                if (encRevision != 5 && encRevision != 6) {
                                    encRevision = 6;
                                }
                                encAlgorithm = cryptAES256;
                                if (cryptFilterObj
                                        .dictLookup ("Length", &cfLengthObj)
                                        ->is_int ()) {
                                    //~ according to the spec, this should be cfLengthObj / 8
                                    fileKeyLength = cfLengthObj.as_int ();
                                }
                            }
                        }
                    }
                }
                if (encryptDictA
                        ->dictLookup ("EncryptMetadata", &encryptMetadataObj)
                        ->is_bool ()) {
                    encryptMetadata = encryptMetadataObj.as_bool ();
                }
            }
            permFlags = permObj.as_int ();
            ownerKey = ownerKeyObj.as_string ()->copy ();
            userKey = userKeyObj.as_string ()->copy ();
            if (encVersion >= 1 && encVersion <= 2 && encRevision >= 2 &&
                encRevision <= 3) {
                if (fileIDObj.is_array ()) {
                    fileIDObj1 = fileIDObj [0];
                    if (fileIDObj1.is_string ()) {
                        fileID = fileIDObj1.as_string ()->copy ();
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
                ownerEnc = ownerEncObj.as_string ()->copy ();
                userEnc = userEncObj.as_string ()->copy ();
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
