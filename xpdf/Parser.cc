// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstddef>
#include <xpdf/object.hh>
#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Decrypt.hh>
#include <xpdf/Parser.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Error.hh>

// Max number of nested objects.  This is used to catch infinite loops
// in the object structure.
#define recursionLimit 500

static inline bool is_keyword (const xpdf::lexer_t::token_t& tok) {
    return tok.type == xpdf::lexer_t::token_t::KEYWORD_;
}

static inline bool is_keyword (const xpdf::lexer_t::token_t& tok, const char* s) {
    return tok.type == xpdf::lexer_t::token_t::KEYWORD_ && tok.s == s;
}

static inline bool is_eof (const xpdf::lexer_t::token_t& tok) {
    return tok.type == xpdf::lexer_t::token_t::EOF_;
}

static inline bool is_error (const xpdf::lexer_t::token_t& tok) {
    return tok.type == xpdf::lexer_t::token_t::ERROR_;
}

static inline bool is_int (const xpdf::lexer_t::token_t& tok) {
    return tok.type == xpdf::lexer_t::token_t::INT_;
}

static inline bool is_string (const xpdf::lexer_t::token_t& tok) {
    return tok.type == xpdf::lexer_t::token_t::STRING_;
}

static inline bool is_name (const xpdf::lexer_t::token_t& tok) {
    return tok.type == xpdf::lexer_t::token_t::NAME_;
}

static inline Object make_generic_object (const xpdf::lexer_t::token_t& tok) {
    Object obj;

    switch (tok.type) {
    case xpdf::lexer_t::token_t::NULL_    : obj.initNull ();                      break;
    case xpdf::lexer_t::token_t::EOF_     : obj.initEOF ();                       break;
    case xpdf::lexer_t::token_t::BOOL_    : obj.initBool (tok.s [0] == 't');      break;
    case xpdf::lexer_t::token_t::INT_     : obj.initInt (std::stoi (tok.s));      break;
    case xpdf::lexer_t::token_t::REAL_    : obj.initReal (std::stod (tok.s));     break;
    case xpdf::lexer_t::token_t::STRING_  : obj.initString (new GString (tok.s)); break;
    case xpdf::lexer_t::token_t::NAME_    : obj.initName (tok.s);                 break;
    case xpdf::lexer_t::token_t::KEYWORD_ : obj.initCmd (tok.s);                  break;

    case xpdf::lexer_t::token_t::ERROR_:
    default:
        obj.initError ();
        break;
    }

    return obj;
}

Parser::Parser (XRef* xrefA, xpdf::lexer_t* lexerA, bool allowStreamsA) {
    xref = xrefA;
    lexer = lexerA;

    inlineImg = 0;
    allowStreams = allowStreamsA;

    buf1 = lexer->next ();
    buf2 = lexer->next ();
}

Parser::~Parser () {
    delete lexer;
}

Object* Parser::getObj (
    Object* obj, bool simpleOnly, unsigned char* fileKey, CryptAlgorithm encAlgorithm,
    int keyLength, int objNum, int objGen, int recursion) {
    Stream* str;
    Object obj2;
    int num;
    DecryptStream* decrypt;
    int c;

    // refill buffer after inline image data
    if (inlineImg == 2) {
        buf1 = lexer->next ();
        buf2 = lexer->next ();

        inlineImg = 0;
    }

    if (!simpleOnly && recursion < recursionLimit && is_keyword (buf1, "[")) {
        //
        // Array:
        //
        shift ();

        obj->initArray (xref);

        while (!is_keyword (buf1, "]") && !is_eof (buf1)) {
            obj->arrayAdd (
                getObj (
                    &obj2, false, fileKey, encAlgorithm, keyLength,
                    objNum, objGen,
                    recursion + 1));
        }

        if (is_eof (buf1)) {
            error (errSyntaxError, getPos (), "End of file inside array");
        }

        shift ();
    }
    else if (!simpleOnly && recursion < recursionLimit && is_keyword (buf1, "<<")) {
        //
        // Dictionary or stream:
        //
        shift ();

        obj->initDict (xref);

        while (!is_keyword (buf1, ">>") && !is_eof (buf1)) {
            if (!is_name (buf1)) {
                error (
                    errSyntaxError, getPos (),
                    "Dictionary key must be a name object");

                shift ();
            }
            else {
                auto s = buf1.s;
                shift ();

                if (is_eof (buf1) || is_error (buf1)) {
                    break;
                }

                obj->dictAdd (
                    s.c_str (), getObj (
                        &obj2, false, fileKey, encAlgorithm, keyLength,
                        objNum, objGen, recursion + 1));
            }
        }

        if (is_eof (buf1)) {
            error (errSyntaxError, getPos (), "End of file inside dictionary");
        }

        // stream objects are not allowed inside content streams or
        // object streams
        if (allowStreams && is_keyword (buf2, "stream")) {
            if ((str = makeStream (
                     obj, fileKey, encAlgorithm, keyLength, objNum, objGen,
                     recursion + 1))) {
                obj->initStream (str);
            }
            else {
                *obj = xpdf::make_error_object ();
            }
        }
        else {
            shift ();
        }
    }
    else if (is_int (buf1)) {
        //
        // Indirect reference or integer:
        //
        num = std::stoi (buf1.s);

        shift ();

        if (is_int (buf1) && is_keyword (buf2, "R")) {
            int gen = std::stoi (buf1.s);
            obj->initRef (num, gen);
            shift ();
            shift ();
        }
        else {
            obj->initInt (num);
        }
    }
    else if (is_string (buf1) && fileKey) {
        //
        // String:
        //
        std::string s;

        obj2.initNull ();

        decrypt = new DecryptStream (
            new MemStream (buf1.s.c_str (), 0, buf1.s.size (), &obj2),
            fileKey, encAlgorithm, keyLength, objNum, objGen);

        decrypt->reset ();

        while ((c = decrypt->getChar ()) != EOF) {
            s.append (1UL, char (c));
        }

        delete decrypt;
        obj->initString (s);

        shift ();
    }
    else {
        // simple object
        *obj = make_generic_object (buf1);
        shift ();
    }

    return obj;
}

Stream* Parser::makeStream (
    Object* dict, unsigned char* fileKey, CryptAlgorithm encAlgorithm, int keyLength,
    int objNum, int objGen, int recursion) {
    Object obj;
    BaseStream* baseStr;
    Stream* str;
    GFileOffset pos, endPos, length;

    // get stream start position
    lexer->skipToNextLine ();
    if (!(str = lexer->getStream ())) { return NULL; }
    pos = str->getPos ();

    // check for length in damaged file
    if (xref && xref->getStreamEnd (pos, &endPos)) {
        length = endPos - pos;

        // get length from the stream object
    }
    else {
        dict->dictLookup ("Length", &obj, recursion);
        if (obj.isInt ()) {
            length = (GFileOffset) (unsigned)obj.getInt ();
            obj.free ();
        }
        else {
            error (
                errSyntaxError, getPos (), "Bad 'Length' attribute in stream");
            obj.free ();
            return NULL;
        }
    }

    // in badly damaged PDF files, we can run off the end of the input
    // stream immediately after the "stream" token
    if (!lexer->getStream ()) { return NULL; }
    baseStr = lexer->getStream ()->getBaseStream ();

    // skip over stream data
    lexer->setPos (pos + length);

    // refill token buffers and check for 'endstream'
    shift (); // kill '>>'
    shift (); // kill 'stream'
    if (is_keyword (buf1, "endstream")) { shift (); }
    else {
        error (errSyntaxError, getPos (), "Missing 'endstream'");
        // kludge for broken PDF files: just add 5k to the length, and
        // hope its enough
        length += 5000;
    }

    // make base stream
    str = baseStr->makeSubStream (pos, true, length, dict);

    // handle decryption
    if (fileKey) {
        str = new DecryptStream (
            str, fileKey, encAlgorithm, keyLength, objNum, objGen);
    }

    // get filters
    str = str->addFilters (dict, recursion);

    return str;
}

void Parser::shift () {
    if (inlineImg > 0) {
        if (inlineImg < 2) {
            ++inlineImg;
        }
        else {
            //
            // In a damaged content stream, if 'ID' shows up in the middle
            // of a dictionary, we need to reset:
            //
            inlineImg = 0;
        }
    }
    else if (is_keyword (buf2, "ID")) {
        // skip char after 'ID' command
        lexer->skipChar ();
        inlineImg = 1;
    }

    buf1 = buf2;

    if (inlineImg > 0){
        // don't buffer inline image data
        buf2 = { xpdf::lexer_t::token_t::NULL_, { } };
    }
    else {
        buf2 = lexer->next ();
    }
}
