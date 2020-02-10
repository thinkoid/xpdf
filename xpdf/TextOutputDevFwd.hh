// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTOUTPUTDEV_FWD_HH
#define XPDF_XPDF_TEXTOUTPUTDEV_FWD_HH

#include <defs.hh>

#include <memory>
#include <vector>

#define XPDF_TYPEDEF(x)                                     \
    struct x;                                               \
    using XPDF_CAT(x, Ptr) = std::shared_ptr< x >;          \
    using XPDF_CAT(x, s) = std::vector< XPDF_CAT(x, Ptr) >

XPDF_TYPEDEF (TextFontInfo);
XPDF_TYPEDEF (TextChar);
XPDF_TYPEDEF (TextWord);
XPDF_TYPEDEF (TextLine);
XPDF_TYPEDEF (TextUnderline);
XPDF_TYPEDEF (TextLink);
XPDF_TYPEDEF (TextParagraph);
XPDF_TYPEDEF (TextColumn);
XPDF_TYPEDEF (TextBlock);
XPDF_TYPEDEF (TextPage);

#undef XPDF_TYPEDEF

#endif // XPDF_XPDF_TEXTOUTPUTDEV_FWD_HH
