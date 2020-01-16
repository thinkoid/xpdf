//========================================================================
//
// SecurityHandler.h
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#ifndef SECURITYHANDLER_H
#define SECURITYHANDLER_H

#include <defs.hh>

#include <xpdf/object.hh>

class GString;
class PDFDoc;
struct XpdfSecurityHandler;

//------------------------------------------------------------------------
// SecurityHandler
//------------------------------------------------------------------------

class SecurityHandler {
public:
    static SecurityHandler* make (PDFDoc* docA, Object* encryptDictA);

    SecurityHandler (PDFDoc* docA);
    virtual ~SecurityHandler ();

    // Returns true if the file is actually unencrypted.
    virtual bool isUnencrypted () { return false; }

    // Check the document's encryption.  If the document is encrypted,
    // this will first try <ownerPassword> and <userPassword> (in
    // "batch" mode), and if those fail, it will attempt to request a
    // password from the user.  This is the high-level function that
    // calls the lower level functions for the specific security handler
    // (requesting a password three times, etc.).  Returns true if the
    // document can be opened (if it's unencrypted, or if a correct
    // password is obtained); false otherwise (encrypted and no correct
    // password).
    bool checkEncryption (GString* ownerPassword, GString* userPassword);

    // Create authorization data for the specified owner and user
    // passwords.  If the security handler doesn't support "batch" mode,
    // this function should return NULL.
    virtual void*
    makeAuthData (GString* ownerPassword, GString* userPassword) = 0;

    // Construct authorization data, typically by prompting the user for
    // a password.  Returns an authorization data object, or NULL to
    // cancel.
    virtual void* getAuthData () = 0;

    // Free the authorization data returned by makeAuthData or
    // getAuthData.
    virtual void freeAuthData (void* authData) = 0;

    // Attempt to authorize the document, using the supplied
    // authorization data (which may be NULL).  Returns true if
    // successful (i.e., if at least the right to open the document was
    // granted).
    virtual bool authorize (void* authData) = 0;

    // Return the various authorization parameters.  These are only
    // valid after authorize has returned true.
    virtual int getPermissionFlags () = 0;
    virtual bool getOwnerPasswordOk () = 0;
    virtual unsigned char* getFileKey () = 0;
    virtual int getFileKeyLength () = 0;
    virtual int getEncVersion () = 0;
    virtual CryptAlgorithm getEncAlgorithm () = 0;

protected:
    PDFDoc* doc;
};

//------------------------------------------------------------------------
// StandardSecurityHandler
//------------------------------------------------------------------------

class StandardSecurityHandler : public SecurityHandler {
public:
    StandardSecurityHandler (PDFDoc* docA, Object* encryptDictA);
    virtual ~StandardSecurityHandler ();

    virtual bool isUnencrypted ();
    virtual void* makeAuthData (GString* ownerPassword, GString* userPassword);
    virtual void* getAuthData ();
    virtual void freeAuthData (void* authData);
    virtual bool authorize (void* authData);
    virtual int getPermissionFlags () { return permFlags; }
    virtual bool getOwnerPasswordOk () { return ownerPasswordOk; }
    virtual unsigned char* getFileKey () { return fileKey; }
    virtual int getFileKeyLength () { return fileKeyLength; }
    virtual int getEncVersion () { return encVersion; }
    virtual CryptAlgorithm getEncAlgorithm () { return encAlgorithm; }

private:
    int permFlags;
    bool ownerPasswordOk;
    unsigned char fileKey[32];
    int fileKeyLength;
    int encVersion;
    int encRevision;
    CryptAlgorithm encAlgorithm;
    bool encryptMetadata;

    GString *ownerKey, *userKey;
    GString *ownerEnc, *userEnc;
    GString* fileID;
    bool ok;
};

#ifdef ENABLE_PLUGINS
//------------------------------------------------------------------------
// ExternalSecurityHandler
//------------------------------------------------------------------------

class ExternalSecurityHandler : public SecurityHandler {
public:
    ExternalSecurityHandler (
        PDFDoc* docA, Object* encryptDictA, XpdfSecurityHandler* xshA);
    virtual ~ExternalSecurityHandler ();

    virtual void* makeAuthData (GString* ownerPassword, GString* userPassword);
    virtual void* getAuthData ();
    virtual void freeAuthData (void* authData);
    virtual bool authorize (void* authData);
    virtual int getPermissionFlags () { return permFlags; }
    virtual bool getOwnerPasswordOk () { return false; }
    virtual unsigned char* getFileKey () { return fileKey; }
    virtual int getFileKeyLength () { return fileKeyLength; }
    virtual int getEncVersion () { return encVersion; }
    virtual CryptAlgorithm getEncAlgorithm () { return encAlgorithm; }

private:
    Object encryptDict;
    XpdfSecurityHandler* xsh;
    void* docData;
    int permFlags;
    unsigned char fileKey[16];
    int fileKeyLength;
    int encVersion;
    CryptAlgorithm encAlgorithm;
    bool ok;
};
#endif // ENABLE_PLUGINS

#endif
