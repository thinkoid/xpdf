// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#include <defs.hh>

#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cctype>

#include <xpdf/array.hh>
#include <xpdf/Error.hh>
#include <xpdf/Lexer.hh>

static const char specialChars[256] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, // 0x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
    1, 0, 0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 2, // 2x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, // 3x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, // 5x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, // 7x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // ax
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // bx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // cx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // dx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // ex
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // fx
};

static inline Array make_array(Object *pobj = 0)
{
    if (pobj) {
        if (pobj->is_stream()) {
            return Array{ *pobj };
        } else {
            return pobj->as_array();
        }
    } else {
        return Array{};
    }
}

Lexer::Lexer(Stream *pstr)
    : streams(make_array())
{
    // TODO: array of streams and nested parsing need some std-ing.
    streams.push_back(curStr = xpdf::make_stream_obj(pstr));
    strPtr = 0;
    curStr.streamReset();
}

Lexer::Lexer(Object *pobj)
    : streams(make_array(pobj))
{
    strPtr = 0;

    if (streams.size() > 0) {
        curStr = resolve(streams[strPtr]);
        curStr.streamReset();
    }
}

Lexer::~Lexer()
{
    if (!curStr.is_none()) {
        curStr.streamClose();
        curStr = {};
    }
}

int Lexer::get()
{
    int c = EOF;

    while (!curStr.is_none() && (c = curStr.streamGetChar()) == EOF) {
        curStr.streamClose();
        curStr = {};

        if (++strPtr < streams.size()) {
            curStr = resolve(streams[strPtr]);
            curStr.streamReset();
        }
    }

    return c;
}

int Lexer::peek()
{
    if (curStr.is_none()) {
        return EOF;
    }

    return curStr.streamLookChar();
}

Lexer::token_t Lexer::next()
{
    int c;

    //
    // Skip whitespace and comments:
    //
    bool comment = false;

    for (;;) {
        if ((c = get()) == EOF) {
            return { token_t::EOF_, {} };
        }

        if (comment) {
            if (c == '\r' || c == '\n') {
                comment = false;
            }
        } else if (c == '%') {
            comment = true;
        } else if (specialChars[c] != 1) {
            break;
        }
    }

    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
    case '.': {
        //
        // Number:
        //
        std::string s(1UL, c);

        if (s.back() == '.') {
            goto doReal;
        }

        for (;;) {
            c = peek();

            if (isdigit(c)) {
                get();
                s.append(1, c);
            } else if (c == '.') {
                get();
                s.append(1, c);
                goto doReal;
            } else {
                break;
            }
        }

        return { token_t::INT_, s };

    doReal:
        for (;;) {
            c = peek();

            if (c == '-') {
                // Ignore, just like Adobe(?):
                get();
                continue;
            }

            if (!isdigit(c)) {
                break;
            }

            get();
            s.append(1, c);
        }

        return { token_t::REAL_, s };
    } break;

    case '(': {
        //
        // String:
        //
        int         nesting = 1, c2;
        std::string s;
        bool        done = false;

        do {
            c2 = EOF;

            switch (c = get()) {
            case EOF:
#if 0
            case '\r': case '\n':
                // This breaks some PDF files, e.g., ones from Photoshop.
#endif
                error(errSyntaxError, tellg(), "Unterminated string");
                done = true;
                break;

            case '(':
                ++nesting;
                c2 = c;
                break;

            case ')':
                if (--nesting == 0) {
                    done = true;
                } else {
                    c2 = c;
                }

                break;

            case '\\':
                switch (c = get()) {
                case 'n':
                    c2 = '\n';
                    break;
                case 'r':
                    c2 = '\r';
                    break;
                case 't':
                    c2 = '\t';
                    break;
                case 'b':
                    c2 = '\b';
                    break;
                case 'f':
                    c2 = '\f';
                    break;
                case '\\':
                case '(':
                case ')':
                    c2 = c;
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                    c2 = c - '0';
                    c = peek();
                    if (c >= '0' && c <= '7') {
                        get();
                        c2 = (c2 << 3) + (c - '0');
                        c = peek();
                        if (c >= '0' && c <= '7') {
                            get();
                            c2 = (c2 << 3) + (c - '0');
                        }
                    }
                    break;

                case '\r':
                    if ((c = peek()) == '\n') {
                        get();
                    }
                    break;

                case '\n':
                    break;

                case EOF:
                    error(errSyntaxError, tellg(), "Unterminated string");
                    done = true;
                    break;

                default:
                    c2 = c;
                    break;
                }
                break;

            default:
                c2 = c;
                break;
            }

            if (c2 != EOF) {
                s.append(1, c2);
            }
        } while (!done);

        return { token_t::STRING_, s };
    }

    case '/': {
        //
        // Name:
        //
        int         c2;
        std::string s;

        while ((c = peek()) != EOF && !specialChars[c]) {
            get();

            if (c == '#') {
                c2 = peek();

                if (c2 >= '0' && c2 <= '9') {
                    c = c2 - '0';
                } else if (c2 >= 'A' && c2 <= 'F') {
                    c = c2 - 'A' + 10;
                } else if (c2 >= 'a' && c2 <= 'f') {
                    c = c2 - 'a' + 10;
                } else {
                    goto notEscChar;
                }

                get();

                c <<= 4;
                c2 = get();

                if (c2 >= '0' && c2 <= '9') {
                    c += c2 - '0';
                } else if (c2 >= 'A' && c2 <= 'F') {
                    c += c2 - 'A' + 10;
                } else if (c2 >= 'a' && c2 <= 'f') {
                    c += c2 - 'a' + 10;
                } else {
                    error(errSyntaxError, tellg(),
                          "Illegal digit in hex char in name");
                }
            }
        notEscChar:
            // the PDF spec claims that names are limited to 127 chars, but
            // Distiller 8 will produce longer names, and Acrobat 8 will
            // accept longer names
            s.append(1, c);
        }

        return { token_t::NAME_, s };
    }

    // array punctuation
    case '[':
    case ']':
        return { token_t::KEYWORD_, { char(c) } };

    // hex string or dict punctuation
    case '<': {
        c = peek();

        if (c == '<') {
            //
            // Dict punctuation:
            //
            get();
            return { token_t::KEYWORD_, "<<" };
        } else {
            //
            // Hex string:
            //
            int         c2, m = 0;
            std::string s;

            while (1) {
                c = get();

                if (c == '>') {
                    break;
                } else if (c == EOF) {
                    error(errSyntaxError, tellg(), "Unterminated hex string");
                    break;
                } else if (specialChars[c] != 1) {
                    c2 = c2 << 4;

                    if (c >= '0' && c <= '9')
                        c2 += c - '0';
                    else if (c >= 'A' && c <= 'F')
                        c2 += c - 'A' + 10;
                    else if (c >= 'a' && c <= 'f')
                        c2 += c - 'a' + 10;
                    else
                        error(errSyntaxError, tellg(),
                              "Illegal character <{0:02x}> in hex string", c);

                    if (++m == 2) {
                        s.append(1, c2);
                        c2 = m = 0;
                    }
                }
            }

            if (m == 1) {
                s.append(1, char(c2 << 4));
            }

            return { token_t::STRING_, s };
        }
    }

    case '>':
        //
        // Dict punctuation:
        //
        if ((c = peek()) == '>') {
            get();
            return { token_t::KEYWORD_, ">>" };
        } else {
            error(errSyntaxError, tellg(), "Illegal character '>'");
            return { token_t::ERROR_, {} };
        }

    case ')':
    case '{':
    case '}':
        //
        // Assorted errors:
        //
        error(errSyntaxError, tellg(), "Illegal character '{0:c}'", c);
        return { token_t::ERROR_, {} };

    default: {
        //
        // Other keywords:
        //
        std::string s(1UL, char(c));

        while ((c = peek()) != EOF && !specialChars[c]) {
            get();
            s.append(1, c);
        }

        if (s == "true" || s == "false") {
            return { token_t::BOOL_, s };
        } else if (s == "null") {
            return { token_t::NULL_, s };
        } else {
            return { token_t::KEYWORD_, s };
        }
    }
    }

    return {};
}

void Lexer::skipToNextLine()
{
    int c;

    while (1) {
        c = get();
        if (c == EOF || c == '\n') {
            return;
        }
        if (c == '\r') {
            if ((c = peek()) == '\n') {
                get();
            }
            return;
        }
    }
}

bool Lexer::isSpace(int c)
{
    return c >= 0 && c <= 0xff && specialChars[c] == 1;
}
