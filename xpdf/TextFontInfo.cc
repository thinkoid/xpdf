// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/TextFontInfo.hh>

TextFontInfo::TextFontInfo(GfxState *state)
    : id{ 0, -1, -1 }
    , name()
    , width()
    , ascent(0.75)
    , descent(-0.25)
    , flags()
{
    GfxFont *gfxFont = state->getFont();

    if (gfxFont) {
        id = *gfxFont->getID();

        ascent = gfxFont->getAscent();
        descent = gfxFont->getDescent();

        // "odd" ascent/descent values cause trouble more often than not
        // (in theory these could be legitimate values for oddly designed
        // fonts -- but they are more often due to buggy PDF generators)
        // (values that are too small are a different issue -- those seem
        // to be more commonly legitimate)
        if (ascent > 1) {
            ascent = 0.75;
        }
        if (descent < -0.5) {
            descent = -0.25;
        }

        flags = gfxFont->getFlags();

        if (gfxFont->as_name()) {
            name = gfxFont->as_name();
        }
    } else {
        ascent = 0.75;
        descent = -0.25;
    }

    if (gfxFont && !gfxFont->isCIDFont()) {
        Gfx8BitFont *cidFont = reinterpret_cast< Gfx8BitFont * >(gfxFont);

        for (int code = 0; code < 256; ++code) {
            const char *name = cidFont->getCharName(code);

            if (name && name[0] == 'm' && name[1] == '\0') {
                width = cidFont->getWidth(code);
                break;
            }
        }
    }
}

bool TextFontInfo::matches(GfxState *state) const
{
    return state->getFont() && *state->getFont()->getID() == id;
}
