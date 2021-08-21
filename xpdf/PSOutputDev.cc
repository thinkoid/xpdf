// -*- mode: c++; -*-
// Copyright 1996-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstddef>
#include <cstdarg>
#include <signal.h>
#include <cmath>

#include <utils/memory.hh>
#include <utils/string.hh>
#include <utils/GList.hh>
#include <utils/GHash.hh>

#include <fofi/FoFiType1C.hh>
#include <fofi/FoFiTrueType.hh>

#include <splash/Splash.hh>
#include <splash/SplashBitmap.hh>

#include <xpdf/Annot.hh>
#include <xpdf/Catalog.hh>
#include <xpdf/CharCodeToUnicode.hh>
#include <xpdf/Error.hh>
#include <xpdf/Form.hh>
#include <xpdf/Gfx.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/PSOutputDev.hh>
#include <xpdf/Page.hh>
#include <xpdf/SplashOutputDev.hh>
#include <xpdf/Stream.hh>
#include <xpdf/TextString.hh>
#include <xpdf/XRef.hh>
#include <xpdf/array.hh>
#include <xpdf/dict.hh>
#include <xpdf/function.hh>
#include <xpdf/obj.hh>
#include <xpdf/unicode_map.hh>

#include <range/v3/algorithm/find_if.hpp>
using namespace ranges;

//------------------------------------------------------------------------
// PostScript prolog and setup
//------------------------------------------------------------------------

// The '~' escapes mark prolog code that is emitted only in certain
// levels:
//
//   ~[123][sn]
//      ^   ^----- s=psLevel*Sep, n=psLevel*
//      +----- 1=psLevel1*, 2=psLevel2*, 3=psLevel3*

static const char *prolog[] = {
    "/xpdf 75 dict def xpdf begin",
    "% PDF special state",
    "/pdfDictSize 15 def",
    "~1sn",
    "/pdfStates 64 array def",
    "  0 1 63 {",
    "    pdfStates exch pdfDictSize dict",
    "    dup /pdfStateIdx 3 index put",
    "    put",
    "  } for",
    "~123sn",
    "/pdfSetup {",
    "  /pdfDuplex exch def",
    "  /setpagedevice where {",
    "    pop 2 dict begin",
    "      /Policies 1 dict dup begin /PageSize 6 def end def",
    "      pdfDuplex { /Duplex true def } if",
    "    currentdict end setpagedevice",
    "  } if",
    "  /pdfPageW 0 def",
    "  /pdfPageH 0 def",
    "} def",
    "/pdfSetupPaper {",
    "  2 copy pdfPageH ne exch pdfPageW ne or {",
    "    /pdfPageH exch def",
    "    /pdfPageW exch def",
    "    /setpagedevice where {",
    "      pop 3 dict begin",
    "        /PageSize [pdfPageW pdfPageH] def",
    "        pdfDuplex { /Duplex true def } if",
    "        /ImagingBBox null def",
    "      currentdict end setpagedevice",
    "    } if",
    "  } {",
    "    pop pop",
    "  } ifelse",
    "} def",
    "~1sn",
    "/pdfOpNames [",
    "  /pdfFill /pdfStroke /pdfLastFill /pdfLastStroke",
    "  /pdfTextMat /pdfFontSize /pdfCharSpacing /pdfTextRender",
    "  /pdfTextRise /pdfWordSpacing /pdfHorizScaling /pdfTextClipPath",
    "] def",
    "~123sn",
    "/pdfStartPage {",
    "~1sn",
    "  pdfStates 0 get begin",
    "~23sn",
    "  pdfDictSize dict begin",
    "~23n",
    "  /pdfFillCS [] def",
    "  /pdfFillXform {} def",
    "  /pdfStrokeCS [] def",
    "  /pdfStrokeXform {} def",
    "~1n",
    "  /pdfFill 0 def",
    "  /pdfStroke 0 def",
    "~1s",
    "  /pdfFill [0 0 0 1] def",
    "  /pdfStroke [0 0 0 1] def",
    "~23sn",
    "  /pdfFill [0] def",
    "  /pdfStroke [0] def",
    "  /pdfFillOP false def",
    "  /pdfStrokeOP false def",
    "~123sn",
    "  /pdfLastFill false def",
    "  /pdfLastStroke false def",
    "  /pdfTextMat [1 0 0 1 0 0] def",
    "  /pdfFontSize 0 def",
    "  /pdfCharSpacing 0 def",
    "  /pdfTextRender 0 def",
    "  /pdfTextRise 0 def",
    "  /pdfWordSpacing 0 def",
    "  /pdfHorizScaling 1 def",
    "  /pdfTextClipPath [] def",
    "} def",
    "/pdfEndPage { end } def",
    "~23s",
    "% separation convention operators",
    "/findcmykcustomcolor where {",
    "  pop",
    "}{",
    "  /findcmykcustomcolor { 5 array astore } def",
    "} ifelse",
    "/setcustomcolor where {",
    "  pop",
    "}{",
    "  /setcustomcolor {",
    "    exch",
    "    [ exch /Separation exch dup 4 get exch /DeviceCMYK exch",
    "      0 4 getinterval cvx",
    "      [ exch /dup load exch { mul exch dup } /forall load",
    "        /pop load dup ] cvx",
    "    ] setcolorspace setcolor",
    "  } def",
    "} ifelse",
    "/customcolorimage where {",
    "  pop",
    "}{",
    "  /customcolorimage {",
    "    gsave",
    "    [ exch /Separation exch dup 4 get exch /DeviceCMYK exch",
    "      0 4 getinterval",
    "      [ exch /dup load exch { mul exch dup } /forall load",
    "        /pop load dup ] cvx",
    "    ] setcolorspace",
    "    10 dict begin",
    "      /ImageType 1 def",
    "      /DataSource exch def",
    "      /ImageMatrix exch def",
    "      /BitsPerComponent exch def",
    "      /Height exch def",
    "      /Width exch def",
    "      /Decode [1 0] def",
    "    currentdict end",
    "    image",
    "    grestore",
    "  } def",
    "} ifelse",
    "~123sn",
    "% PDF color state",
    "~1n",
    "/g { dup /pdfFill exch def setgray",
    "     /pdfLastFill true def /pdfLastStroke false def } def",
    "/G { dup /pdfStroke exch def setgray",
    "     /pdfLastStroke true def /pdfLastFill false def } def",
    "/fCol {",
    "  pdfLastFill not {",
    "    pdfFill setgray",
    "    /pdfLastFill true def /pdfLastStroke false def",
    "  } if",
    "} def",
    "/sCol {",
    "  pdfLastStroke not {",
    "    pdfStroke setgray",
    "    /pdfLastStroke true def /pdfLastFill false def",
    "  } if",
    "} def",
    "~1s",
    "/k { 4 copy 4 array astore /pdfFill exch def setcmykcolor",
    "     /pdfLastFill true def /pdfLastStroke false def } def",
    "/K { 4 copy 4 array astore /pdfStroke exch def setcmykcolor",
    "     /pdfLastStroke true def /pdfLastFill false def } def",
    "/fCol {",
    "  pdfLastFill not {",
    "    pdfFill aload pop setcmykcolor",
    "    /pdfLastFill true def /pdfLastStroke false def",
    "  } if",
    "} def",
    "/sCol {",
    "  pdfLastStroke not {",
    "    pdfStroke aload pop setcmykcolor",
    "    /pdfLastStroke true def /pdfLastFill false def",
    "  } if",
    "} def",
    "~23n",
    "/cs { /pdfFillXform exch def dup /pdfFillCS exch def",
    "      setcolorspace } def",
    "/CS { /pdfStrokeXform exch def dup /pdfStrokeCS exch def",
    "      setcolorspace } def",
    "/sc { pdfLastFill not { pdfFillCS setcolorspace } if",
    "      dup /pdfFill exch def aload pop pdfFillXform setcolor",
    "     /pdfLastFill true def /pdfLastStroke false def } def",
    "/SC { pdfLastStroke not { pdfStrokeCS setcolorspace } if",
    "      dup /pdfStroke exch def aload pop pdfStrokeXform setcolor",
    "     /pdfLastStroke true def /pdfLastFill false def } def",
    "/op { /pdfFillOP exch def",
    "      pdfLastFill { pdfFillOP setoverprint } if } def",
    "/OP { /pdfStrokeOP exch def",
    "      pdfLastStroke { pdfStrokeOP setoverprint } if } def",
    "/fCol {",
    "  pdfLastFill not {",
    "    pdfFillCS setcolorspace",
    "    pdfFill aload pop pdfFillXform setcolor",
    "    pdfFillOP setoverprint",
    "    /pdfLastFill true def /pdfLastStroke false def",
    "  } if",
    "} def",
    "/sCol {",
    "  pdfLastStroke not {",
    "    pdfStrokeCS setcolorspace",
    "    pdfStroke aload pop pdfStrokeXform setcolor",
    "    pdfStrokeOP setoverprint",
    "    /pdfLastStroke true def /pdfLastFill false def",
    "  } if",
    "} def",
    "~23s",
    "/k { 4 copy 4 array astore /pdfFill exch def setcmykcolor",
    "     /pdfLastFill true def /pdfLastStroke false def } def",
    "/K { 4 copy 4 array astore /pdfStroke exch def setcmykcolor",
    "     /pdfLastStroke true def /pdfLastFill false def } def",
    "/ck { 6 copy 6 array astore /pdfFill exch def",
    "      findcmykcustomcolor exch setcustomcolor",
    "      /pdfLastFill true def /pdfLastStroke false def } def",
    "/CK { 6 copy 6 array astore /pdfStroke exch def",
    "      findcmykcustomcolor exch setcustomcolor",
    "      /pdfLastStroke true def /pdfLastFill false def } def",
    "/op { /pdfFillOP exch def",
    "      pdfLastFill { pdfFillOP setoverprint } if } def",
    "/OP { /pdfStrokeOP exch def",
    "      pdfLastStroke { pdfStrokeOP setoverprint } if } def",
    "/fCol {",
    "  pdfLastFill not {",
    "    pdfFill aload length 4 eq {",
    "      setcmykcolor",
    "    }{",
    "      findcmykcustomcolor exch setcustomcolor",
    "    } ifelse",
    "    pdfFillOP setoverprint",
    "    /pdfLastFill true def /pdfLastStroke false def",
    "  } if",
    "} def",
    "/sCol {",
    "  pdfLastStroke not {",
    "    pdfStroke aload length 4 eq {",
    "      setcmykcolor",
    "    }{",
    "      findcmykcustomcolor exch setcustomcolor",
    "    } ifelse",
    "    pdfStrokeOP setoverprint",
    "    /pdfLastStroke true def /pdfLastFill false def",
    "  } if",
    "} def",
    "~123sn",
    "% build a font",
    "/pdfMakeFont {",
    "  4 3 roll findfont",
    "  4 2 roll matrix scale makefont",
    "  dup length dict begin",
    "    { 1 index /FID ne { def } { pop pop } ifelse } forall",
    "    /Encoding exch def",
    "    currentdict",
    "  end",
    "  definefont pop",
    "} def",
    "/pdfMakeFont16 {",
    "  exch findfont",
    "  dup length dict begin",
    "    { 1 index /FID ne { def } { pop pop } ifelse } forall",
    "    /WMode exch def",
    "    currentdict",
    "  end",
    "  definefont pop",
    "} def",
    "~3sn",
    "/pdfMakeFont16L3 {",
    "  1 index /CIDFont resourcestatus {",
    "    pop pop 1 index /CIDFont findresource /CIDFontType known",
    "  } {",
    "    false",
    "  } ifelse",
    "  {",
    "    0 eq { /Identity-H } { /Identity-V } ifelse",
    "    exch 1 array astore composefont pop",
    "  } {",
    "    pdfMakeFont16",
    "  } ifelse",
    "} def",
    "~123sn",
    "% graphics state operators",
    "~1sn",
    "/q {",
    "  gsave",
    "  pdfOpNames length 1 sub -1 0 { pdfOpNames exch get load } for",
    "  pdfStates pdfStateIdx 1 add get begin",
    "  pdfOpNames { exch def } forall",
    "} def",
    "/Q { end grestore } def",
    "~23sn",
    "/q { gsave pdfDictSize dict begin } def",
    "/Q {",
    "  end grestore",
    "  /pdfLastFill where {",
    "    pop",
    "    pdfLastFill {",
    "      pdfFillOP setoverprint",
    "    } {",
    "      pdfStrokeOP setoverprint",
    "    } ifelse",
    "  } if",
    "} def",
    "~123sn",
    "/cm { concat } def",
    "/d { setdash } def",
    "/i { setflat } def",
    "/j { setlinejoin } def",
    "/J { setlinecap } def",
    "/M { setmiterlimit } def",
    "/w { setlinewidth } def",
    "% path segment operators",
    "/m { moveto } def",
    "/l { lineto } def",
    "/c { curveto } def",
    "/re { 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto",
    "      neg 0 rlineto closepath } def",
    "/h { closepath } def",
    "% path painting operators",
    "/S { sCol stroke } def",
    "/Sf { fCol stroke } def",
    "/f { fCol fill } def",
    "/f* { fCol eofill } def",
    "% clipping operators",
    "/W { clip newpath } def",
    "/W* { eoclip newpath } def",
    "/Ws { strokepath clip newpath } def",
    "% text state operators",
    "/Tc { /pdfCharSpacing exch def } def",
    "/Tf { dup /pdfFontSize exch def",
    "      dup pdfHorizScaling mul exch matrix scale",
    "      pdfTextMat matrix concatmatrix dup 4 0 put dup 5 0 put",
    "      exch findfont exch makefont setfont } def",
    "/Tr { /pdfTextRender exch def } def",
    "/Ts { /pdfTextRise exch def } def",
    "/Tw { /pdfWordSpacing exch def } def",
    "/Tz { /pdfHorizScaling exch def } def",
    "% text positioning operators",
    "/Td { pdfTextMat transform moveto } def",
    "/Tm { /pdfTextMat exch def } def",
    "% text string operators",
    "/xyshow where {",
    "  pop",
    "  /xyshow2 {",
    "    dup length array",
    "    0 2 2 index length 1 sub {",
    "      2 index 1 index 2 copy get 3 1 roll 1 add get",
    "      pdfTextMat dtransform",
    "      4 2 roll 2 copy 6 5 roll put 1 add 3 1 roll dup 4 2 roll put",
    "    } for",
    "    exch pop",
    "    xyshow",
    "  } def",
    "}{",
    "  /xyshow2 {",
    "    currentfont /FontType get 0 eq {",
    "      0 2 3 index length 1 sub {",
    "        currentpoint 4 index 3 index 2 getinterval show moveto",
    "        2 copy get 2 index 3 2 roll 1 add get",
    "        pdfTextMat dtransform rmoveto",
    "      } for",
    "    } {",
    "      0 1 3 index length 1 sub {",
    "        currentpoint 4 index 3 index 1 getinterval show moveto",
    "        2 copy 2 mul get 2 index 3 2 roll 2 mul 1 add get",
    "        pdfTextMat dtransform rmoveto",
    "      } for",
    "    } ifelse",
    "    pop pop",
    "  } def",
    "} ifelse",
    "/cshow where {",
    "  pop",
    "  /xycp {", // xycharpath
    "    0 3 2 roll",
    "    {",
    "      pop pop currentpoint 3 2 roll",
    "      1 string dup 0 4 3 roll put false charpath moveto",
    "      2 copy get 2 index 2 index 1 add get",
    "      pdfTextMat dtransform rmoveto",
    "      2 add",
    "    } exch cshow",
    "    pop pop",
    "  } def",
    "}{",
    "  /xycp {", // xycharpath
    "    currentfont /FontType get 0 eq {",
    "      0 2 3 index length 1 sub {",
    "        currentpoint 4 index 3 index 2 getinterval false charpath moveto",
    "        2 copy get 2 index 3 2 roll 1 add get",
    "        pdfTextMat dtransform rmoveto",
    "      } for",
    "    } {",
    "      0 1 3 index length 1 sub {",
    "        currentpoint 4 index 3 index 1 getinterval false charpath moveto",
    "        2 copy 2 mul get 2 index 3 2 roll 2 mul 1 add get",
    "        pdfTextMat dtransform rmoveto",
    "      } for",
    "    } ifelse",
    "    pop pop",
    "  } def",
    "} ifelse",
    "/Tj {",
    "  fCol", // because stringwidth has to draw Type 3 chars
    "  0 pdfTextRise pdfTextMat dtransform rmoveto",
    "  currentpoint 4 2 roll",
    "  pdfTextRender 1 and 0 eq {",
    "    2 copy xyshow2",
    "  } if",
    "  pdfTextRender 3 and dup 1 eq exch 2 eq or {",
    "    3 index 3 index moveto",
    "    2 copy",
    "    currentfont /FontType get 3 eq { fCol } { sCol } ifelse",
    "    xycp currentpoint stroke moveto",
    "  } if",
    "  pdfTextRender 4 and 0 ne {",
    "    4 2 roll moveto xycp",
    "    /pdfTextClipPath [ pdfTextClipPath aload pop",
    "      {/moveto cvx}",
    "      {/lineto cvx}",
    "      {/curveto cvx}",
    "      {/closepath cvx}",
    "    pathforall ] def",
    "    currentpoint newpath moveto",
    "  } {",
    "    pop pop pop pop",
    "  } ifelse",
    "  0 pdfTextRise neg pdfTextMat dtransform rmoveto",
    "} def",
    "/TJm { 0.001 mul pdfFontSize mul pdfHorizScaling mul neg 0",
    "       pdfTextMat dtransform rmoveto } def",
    "/TJmV { 0.001 mul pdfFontSize mul neg 0 exch",
    "        pdfTextMat dtransform rmoveto } def",
    "/Tclip { pdfTextClipPath cvx exec clip newpath",
    "         /pdfTextClipPath [] def } def",
    "~1ns",
    "% Level 1 image operators",
    "~1n",
    "/pdfIm1 {",
    "  /pdfImBuf1 4 index string def",
    "  { currentfile pdfImBuf1 readhexstring pop } image",
    "} def",
    "~1s",
    "/pdfIm1Sep {",
    "  /pdfImBuf1 4 index string def",
    "  /pdfImBuf2 4 index string def",
    "  /pdfImBuf3 4 index string def",
    "  /pdfImBuf4 4 index string def",
    "  { currentfile pdfImBuf1 readhexstring pop }",
    "  { currentfile pdfImBuf2 readhexstring pop }",
    "  { currentfile pdfImBuf3 readhexstring pop }",
    "  { currentfile pdfImBuf4 readhexstring pop }",
    "  true 4 colorimage",
    "} def",
    "~1ns",
    "/pdfImM1 {",
    "  fCol /pdfImBuf1 4 index 7 add 8 idiv string def",
    "  { currentfile pdfImBuf1 readhexstring pop } imagemask",
    "} def",
    "/pdfImStr {",
    "  2 copy exch length lt {",
    "    2 copy get exch 1 add exch",
    "  } {",
    "    ()",
    "  } ifelse",
    "} def",
    "/pdfImM1a {",
    "  { pdfImStr } imagemask",
    "  pop pop",
    "} def",
    "~23sn",
    "% Level 2/3 image operators",
    "/pdfImBuf 100 string def",
    "/pdfImStr {",
    "  2 copy exch length lt {",
    "    2 copy get exch 1 add exch",
    "  } {",
    "    ()",
    "  } ifelse",
    "} def",
    "/skipEOD {",
    "  { currentfile pdfImBuf readline",
    "    not { pop exit } if",
    "    (%-EOD-) eq { exit } if } loop",
    "} def",
    "/pdfIm { image skipEOD } def",
    "~3sn",
    "/pdfMask {",
    "  /ReusableStreamDecode filter",
    "  skipEOD",
    "  /maskStream exch def",
    "} def",
    "/pdfMaskEnd { maskStream closefile } def",
    "/pdfMaskInit {",
    "  /maskArray exch def",
    "  /maskIdx 0 def",
    "} def",
    "/pdfMaskSrc {",
    "  maskIdx maskArray length lt {",
    "    maskArray maskIdx get",
    "    /maskIdx maskIdx 1 add def",
    "  } {",
    "    ()",
    "  } ifelse",
    "} def",
    "~23s",
    "/pdfImSep {",
    "  findcmykcustomcolor exch",
    "  dup /Width get /pdfImBuf1 exch string def",
    "  dup /Decode get aload pop 1 index sub /pdfImDecodeRange exch def",
    "  /pdfImDecodeLow exch def",
    "  begin Width Height BitsPerComponent ImageMatrix DataSource end",
    "  /pdfImData exch def",
    "  { pdfImData pdfImBuf1 readstring pop",
    "    0 1 2 index length 1 sub {",
    "      1 index exch 2 copy get",
    "      pdfImDecodeRange mul 255 div pdfImDecodeLow add round cvi",
    "      255 exch sub put",
    "    } for }",
    "  6 5 roll customcolorimage",
    "  skipEOD",
    "} def",
    "~23sn",
    "/pdfImM { fCol imagemask skipEOD } def",
    "/pr {",
    "  4 2 roll exch 5 index div exch 4 index div moveto",
    "  exch 3 index div dup 0 rlineto",
    "  exch 2 index div 0 exch rlineto",
    "  neg 0 rlineto",
    "  closepath",
    "} def",
    "/pdfImClip { gsave clip } def",
    "/pdfImClipEnd { grestore } def",
    "~23sn",
    "% shading operators",
    "/colordelta {",
    "  false 0 1 3 index length 1 sub {",
    "    dup 4 index exch get 3 index 3 2 roll get sub abs 0.004 gt {",
    "      pop true",
    "    } if",
    "  } for",
    "  exch pop exch pop",
    "} def",
    "/funcCol { func n array astore } def",
    "/funcSH {",
    "  dup 0 eq {",
    "    true",
    "  } {",
    "    dup 6 eq {",
    "      false",
    "    } {",
    "      4 index 4 index funcCol dup",
    "      6 index 4 index funcCol dup",
    "      3 1 roll colordelta 3 1 roll",
    "      5 index 5 index funcCol dup",
    "      3 1 roll colordelta 3 1 roll",
    "      6 index 8 index funcCol dup",
    "      3 1 roll colordelta 3 1 roll",
    "      colordelta or or or",
    "    } ifelse",
    "  } ifelse",
    "  {",
    "    1 add",
    "    4 index 3 index add 0.5 mul exch 4 index 3 index add 0.5 mul exch",
    "    6 index 6 index 4 index 4 index 4 index funcSH",
    "    2 index 6 index 6 index 4 index 4 index funcSH",
    "    6 index 2 index 4 index 6 index 4 index funcSH",
    "    5 3 roll 3 2 roll funcSH pop pop",
    "  } {",
    "    pop 3 index 2 index add 0.5 mul 3 index  2 index add 0.5 mul",
    "~23n",
    "    funcCol sc",
    "~23s",
    "    funcCol aload pop k",
    "~23sn",
    "    dup 4 index exch mat transform m",
    "    3 index 3 index mat transform l",
    "    1 index 3 index mat transform l",
    "    mat transform l pop pop h f*",
    "  } ifelse",
    "} def",
    "/axialCol {",
    "  dup 0 lt {",
    "    pop t0",
    "  } {",
    "    dup 1 gt {",
    "      pop t1",
    "    } {",
    "      dt mul t0 add",
    "    } ifelse",
    "  } ifelse",
    "  func n array astore",
    "} def",
    "/axialSH {",
    "  dup 2 lt {",
    "    true",
    "  } {",
    "    dup 8 eq {",
    "      false",
    "    } {",
    "      2 index axialCol 2 index axialCol colordelta",
    "    } ifelse",
    "  } ifelse",
    "  {",
    "    1 add 3 1 roll 2 copy add 0.5 mul",
    "    dup 4 3 roll exch 4 index axialSH",
    "    exch 3 2 roll axialSH",
    "  } {",
    "    pop 2 copy add 0.5 mul",
    "~23n",
    "    axialCol sc",
    "~23s",
    "    axialCol aload pop k",
    "~23sn",
    "    exch dup dx mul x0 add exch dy mul y0 add",
    "    3 2 roll dup dx mul x0 add exch dy mul y0 add",
    "    dx abs dy abs ge {",
    "      2 copy yMin sub dy mul dx div add yMin m",
    "      yMax sub dy mul dx div add yMax l",
    "      2 copy yMax sub dy mul dx div add yMax l",
    "      yMin sub dy mul dx div add yMin l",
    "      h f*",
    "    } {",
    "      exch 2 copy xMin sub dx mul dy div add xMin exch m",
    "      xMax sub dx mul dy div add xMax exch l",
    "      exch 2 copy xMax sub dx mul dy div add xMax exch l",
    "      xMin sub dx mul dy div add xMin exch l",
    "      h f*",
    "    } ifelse",
    "  } ifelse",
    "} def",
    "/radialCol {",
    "  dup t0 lt {",
    "    pop t0",
    "  } {",
    "    dup t1 gt {",
    "      pop t1",
    "    } if",
    "  } ifelse",
    "  func n array astore",
    "} def",
    "/radialSH {",
    "  dup 0 eq {",
    "    true",
    "  } {",
    "    dup 8 eq {",
    "      false",
    "    } {",
    "      2 index dt mul t0 add radialCol",
    "      2 index dt mul t0 add radialCol colordelta",
    "    } ifelse",
    "  } ifelse",
    "  {",
    "    1 add 3 1 roll 2 copy add 0.5 mul",
    "    dup 4 3 roll exch 4 index radialSH",
    "    exch 3 2 roll radialSH",
    "  } {",
    "    pop 2 copy add 0.5 mul dt mul t0 add",
    "~23n",
    "    radialCol sc",
    "~23s",
    "    radialCol aload pop k",
    "~23sn",
    "    encl {",
    "      exch dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
    "      0 360 arc h",
    "      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
    "      360 0 arcn h f",
    "    } {",
    "      2 copy",
    "      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
    "      a1 a2 arcn",
    "      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
    "      a2 a1 arcn h",
    "      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
    "      a1 a2 arc",
    "      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
    "      a2 a1 arc h f",
    "    } ifelse",
    "  } ifelse",
    "} def",
    "~123sn",
    "end",
    NULL
};

static const char *minLineWidthProlog[] = {
    "/pdfDist { dup dtransform dup mul exch dup mul add 0.5 mul sqrt } def",
    "/pdfIDist { dup idtransform dup mul exch dup mul add 0.5 mul sqrt } def",
    "/pdfMinLineDist pdfMinLineWidth pdfDist def",
    "/setlinewidth {",
    "  dup pdfDist pdfMinLineDist lt {",
    "    pop pdfMinLineDist pdfIDist",
    "  } if",
    "  setlinewidth",
    "} bind def",
    NULL
};

static const char *cmapProlog[] = {
    "/CIDInit /ProcSet findresource begin",
    "10 dict begin",
    "  begincmap",
    "  /CMapType 1 def",
    "  /CMapName /Identity-H def",
    "  /CIDSystemInfo 3 dict dup begin",
    "    /Registry (Adobe) def",
    "    /Ordering (Identity) def",
    "    /Supplement 0 def",
    "  end def",
    "  1 begincodespacerange",
    "    <0000> <ffff>",
    "  endcodespacerange",
    "  0 usefont",
    "  1 begincidrange",
    "    <0000> <ffff> 0",
    "  endcidrange",
    "  endcmap",
    "  currentdict CMapName exch /CMap defineresource pop",
    "end",
    "10 dict begin",
    "  begincmap",
    "  /CMapType 1 def",
    "  /CMapName /Identity-V def",
    "  /CIDSystemInfo 3 dict dup begin",
    "    /Registry (Adobe) def",
    "    /Ordering (Identity) def",
    "    /Supplement 0 def",
    "  end def",
    "  /WMode 1 def",
    "  1 begincodespacerange",
    "    <0000> <ffff>",
    "  endcodespacerange",
    "  0 usefont",
    "  1 begincidrange",
    "    <0000> <ffff> 0",
    "  endcidrange",
    "  endcmap",
    "  currentdict CMapName exch /CMap defineresource pop",
    "end",
    "end",
    NULL
};

//------------------------------------------------------------------------
// Fonts
//------------------------------------------------------------------------

struct PSSubstFont
{
    const char *psName; // PostScript name
    double      mWidth; // width of 'm' character
};

// NB: must be in same order as base14SubstFonts in GfxFont.cc
static PSSubstFont psBase14SubstFonts[14] = {
    { "Courier", 0.600 },
    { "Courier-Oblique", 0.600 },
    { "Courier-Bold", 0.600 },
    { "Courier-BoldOblique", 0.600 },
    { "Helvetica", 0.833 },
    { "Helvetica-Oblique", 0.833 },
    { "Helvetica-Bold", 0.889 },
    { "Helvetica-BoldOblique", 0.889 },
    { "Times-Roman", 0.788 },
    { "Times-Italic", 0.722 },
    { "Times-Bold", 0.833 },
    { "Times-BoldItalic", 0.778 },
    // the last two are never used for substitution
    { "Symbol", 0 },
    { "ZapfDingbats", 0 }
};

class PSFontInfo
{
public:
    PSFontInfo(Ref fontIDA)
    {
        fontID = fontIDA;
        ff = NULL;
    }

    Ref             fontID;
    PSFontFileInfo *ff; // pointer to font file info; NULL indicates
        //   font mapping failed
};

enum PSFontFileLocation {
    psFontFileResident,
    psFontFileEmbedded,
    psFontFileExternal
};

class PSFontFileInfo
{
public:
    PSFontFileInfo(GString *psNameA, GfxFontType typeA, PSFontFileLocation locA);
    ~PSFontFileInfo();

    GString *          psName; // name under which font is defined
    GfxFontType        type; // font type
    PSFontFileLocation loc; // font location
    Ref                embFontID; // object ID for the embedded font file
        //   (for all embedded fonts)
    GString *extFileName; // external font file path
        //   (for all external fonts)
    GString *encoding; // encoding name (for resident CID fonts)
    int *    codeToGID; // mapping from code/CID to GID
        //   (for TrueType, OpenType-TrueType, and
        //   CID OpenType-CFF fonts)
    int codeToGIDLen; // length of codeToGID array
};

PSFontFileInfo::PSFontFileInfo(GString *psNameA, GfxFontType typeA,
                               PSFontFileLocation locA)
{
    psName = psNameA;
    type = typeA;
    loc = locA;
    embFontID.num = embFontID.gen = -1;
    extFileName = NULL;
    encoding = NULL;
    codeToGID = NULL;
}

PSFontFileInfo::~PSFontFileInfo()
{
    delete psName;
    if (extFileName) {
        delete extFileName;
    }
    if (encoding) {
        delete encoding;
    }
    if (codeToGID) {
        free(codeToGID);
    }
}

//------------------------------------------------------------------------
// process colors
//------------------------------------------------------------------------

#define psProcessCyan 1
#define psProcessMagenta 2
#define psProcessYellow 4
#define psProcessBlack 8
#define psProcessCMYK 15

//------------------------------------------------------------------------
// PSOutCustomColor
//------------------------------------------------------------------------

class PSOutCustomColor
{
public:
    PSOutCustomColor(double cA, double mA, double yA, double kA, GString *nameA);
    ~PSOutCustomColor();

    double            c, m, y, k;
    GString *         name;
    PSOutCustomColor *next;
};

PSOutCustomColor::PSOutCustomColor(double cA, double mA, double yA, double kA,
                                   GString *nameA)
{
    c = cA;
    m = mA;
    y = yA;
    k = kA;
    name = nameA;
    next = NULL;
}

PSOutCustomColor::~PSOutCustomColor()
{
    delete name;
}

//------------------------------------------------------------------------

struct PSOutImgClipRect
{
    int x0, x1, y0, y1;
};

//------------------------------------------------------------------------

struct PSOutPaperSize
{
    PSOutPaperSize(int wA, int hA)
    {
        w = wA;
        h = hA;
    }
    int w, h;
};

//------------------------------------------------------------------------
// DeviceNRecoder
//------------------------------------------------------------------------

class DeviceNRecoder : public FilterStream
{
public:
    DeviceNRecoder(Stream *strA, int widthA, int heightA,
                   GfxImageColorMap *colorMapA);
    virtual ~DeviceNRecoder();

    const std::type_info &type() const override { return typeid(*this); }

    virtual void       reset();
    virtual int        get()
    {
        return (bufIdx >= bufSize && !fillBuf()) ? EOF : buf[bufIdx++];
    }
    virtual int peek()
    {
        return (bufIdx >= bufSize && !fillBuf()) ? EOF : buf[bufIdx];
    }
    virtual GString *getPSFilter(int psLevel, const char *indent) { return NULL; }
    virtual bool     isBinary(bool last = true) { return true; }
    virtual bool     isEncoder() { return true; }

private:
    bool fillBuf();

    int               width, height;
    GfxImageColorMap *colorMap;
    Function          func;
    ImageStream *     imgStr;
    int               buf[gfxColorMaxComps];
    int               pixelIdx;
    int               bufIdx;
    int               bufSize;
};

DeviceNRecoder::DeviceNRecoder(Stream *strA, int widthA, int heightA,
                               GfxImageColorMap *colorMapA)
    : FilterStream(strA)
{
    width = widthA;
    height = heightA;
    colorMap = colorMapA;
    imgStr = NULL;
    pixelIdx = 0;
    bufIdx = gfxColorMaxComps;
    bufSize =
        ((GfxDeviceNColorSpace *)colorMap->getColorSpace())->getAlt()->getNComps();
    func = ((GfxDeviceNColorSpace *)colorMap->getColorSpace())
               ->getTintTransformFunc();
}

DeviceNRecoder::~DeviceNRecoder()
{
    if (imgStr) {
        delete imgStr;
    }
    if (str->isEncoder()) {
        delete str;
    }
}

void DeviceNRecoder::reset()
{
    imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                             colorMap->getBits());
    imgStr->reset();
}

bool DeviceNRecoder::fillBuf()
{
    unsigned char pixBuf[gfxColorMaxComps];
    GfxColor      color;
    double        x[gfxColorMaxComps], y[gfxColorMaxComps];

    if (pixelIdx >= width * height) {
        return false;
    }
    imgStr->getPixel(pixBuf);
    colorMap->getColor(pixBuf, &color);

    const size_t n =
        ((GfxDeviceNColorSpace *)colorMap->getColorSpace())->getNComps();

    for (size_t i = 0; i < n; ++i) {
        x[i] = xpdf::to_double(color.c[i]);
    }

    func(x, x + n, y);

    for (size_t i = 0; i < bufSize; ++i) {
        buf[i] = (int)(y[i] * 255 + 0.5);
    }

    bufIdx = 0;
    ++pixelIdx;

    return true;
}

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

extern "C" {
typedef void (*SignalFunc)(int);
}

static void outputToFile(void *stream, const char *data, int len)
{
    fwrite(data, 1, len, (FILE *)stream);
}

PSOutputDev::PSOutputDev(const char *fileName, PDFDoc *docA, int firstPage,
                         int lastPage, PSOutMode modeA, int imgLLXA, int imgLLYA,
                         int imgURXA, int imgURYA, bool manualCtrlA,
                         PSOutCustomCodeCbk customCodeCbkA,
                         void *             customCodeCbkDataA)
{
    FILE *     f;
    PSFileType fileTypeA;

    underlayCbk = NULL;
    underlayCbkData = NULL;
    overlayCbk = NULL;
    overlayCbkData = NULL;
    customCodeCbk = customCodeCbkA;
    customCodeCbkData = customCodeCbkDataA;

    fontFileInfo = new GHash();
    imgIDs = NULL;
    formIDs = NULL;
    embFontList = NULL;
    customColors = NULL;
    haveTextClip = false;
    t3String = NULL;

    // open file or pipe
    if (!strcmp(fileName, "-")) {
        fileTypeA = psStdout;
        f = stdout;
    } else if (fileName[0] == '|') {
        fileTypeA = psPipe;
        signal(SIGPIPE, (SignalFunc)SIG_IGN);

        if (!(f = popen(fileName + 1, "w"))) {
            error(errIO, -1, "Couldn't run print command '{0:s}'", fileName);
            ok = false;
            return;
        }
    } else {
        fileTypeA = psFile;
        if (!(f = fopen(fileName, "w"))) {
            error(errIO, -1, "Couldn't open PostScript file '{0:s}'", fileName);
            ok = false;
            return;
        }
    }

    init(outputToFile, f, fileTypeA, docA, firstPage, lastPage, modeA, imgLLXA,
         imgLLYA, imgURXA, imgURYA, manualCtrlA);
}

PSOutputDev::PSOutputDev(PSOutputFunc outputFuncA, void *outputStreamA,
                         PDFDoc *docA, int firstPage, int lastPage,
                         PSOutMode modeA, int imgLLXA, int imgLLYA, int imgURXA,
                         int imgURYA, bool manualCtrlA,
                         PSOutCustomCodeCbk customCodeCbkA,
                         void *             customCodeCbkDataA)
{
    underlayCbk = NULL;
    underlayCbkData = NULL;
    overlayCbk = NULL;
    overlayCbkData = NULL;
    customCodeCbk = customCodeCbkA;
    customCodeCbkData = customCodeCbkDataA;

    fontFileInfo = new GHash();
    imgIDs = NULL;
    formIDs = NULL;
    embFontList = NULL;
    customColors = NULL;
    haveTextClip = false;
    t3String = NULL;

    init(outputFuncA, outputStreamA, psGeneric, docA, firstPage, lastPage, modeA,
         imgLLXA, imgLLYA, imgURXA, imgURYA, manualCtrlA);
}

void PSOutputDev::init(PSOutputFunc outputFuncA, void *outputStreamA,
                       PSFileType fileTypeA, PDFDoc *docA, int firstPage,
                       int lastPage, PSOutMode modeA, int imgLLXA, int imgLLYA,
                       int imgURXA, int imgURYA, bool manualCtrlA)
{
    Catalog *       catalog;
    Page *          page;
    PDFRectangle *  box;
    PSFontFileInfo *ff;
    GList *         names;
    int             pg, w, h, i;

    // initialize
    ok = true;
    outputFunc = outputFuncA;
    outputStream = outputStreamA;
    fileType = fileTypeA;
    doc = docA;
    xref = doc->getXRef();
    catalog = doc->getCatalog();
    level = globalParams->getPSLevel();
    mode = modeA;
    paperWidth = globalParams->getPSPaperWidth();
    paperHeight = globalParams->getPSPaperHeight();
    imgLLX = imgLLXA;
    imgLLY = imgLLYA;
    imgURX = imgURXA;
    imgURY = imgURYA;
    if (imgLLX == 0 && imgURX == 0 && imgLLY == 0 && imgURY == 0) {
        globalParams->getPSImageableArea(&imgLLX, &imgLLY, &imgURX, &imgURY);
    }
    if (paperWidth < 0 || paperHeight < 0) {
        paperMatch = true;
        paperWidth = paperHeight = 1; // in case the document has zero pages
        for (pg = (firstPage >= 1) ? firstPage : 1;
             pg <= lastPage && pg <= catalog->getNumPages(); ++pg) {
            page = catalog->getPage(pg);
            if (globalParams->getPSUseCropBoxAsPage()) {
                w = (int)ceil(page->getCropWidth());
                h = (int)ceil(page->getCropHeight());
            } else {
                w = (int)ceil(page->getMediaWidth());
                h = (int)ceil(page->getMediaHeight());
            }
            for (i = 0; i < paperSizes.size(); ++i) {
                auto &size = paperSizes[i];
                if (size.w == w && size.h == h) {
                    break;
                }
            }
            if (i == paperSizes.size()) {
                paperSizes.push_back(PSOutPaperSize(w, h));
            }
            if (w > paperWidth) {
                paperWidth = w;
            }
            if (h > paperHeight) {
                paperHeight = h;
            }
        }
        // NB: img{LLX,LLY,URX,URY} will be set by startPage()
    } else {
        paperMatch = false;
    }
    preload = globalParams->getPSPreload();
    manualCtrl = manualCtrlA;
    if (mode == psModeForm) {
        lastPage = firstPage;
    }
    processColors = 0;
    inType3Char = false;

#if OPI_SUPPORT
    // initialize OPI nesting levels
    opi13Nest = 0;
    opi20Nest = 0;
#endif

    tx0 = ty0 = -1;
    xScale0 = yScale0 = 0;
    rotate0 = -1;
    clipLLX0 = clipLLY0 = 0;
    clipURX0 = clipURY0 = -1;

    // initialize font lists, etc.
    for (i = 0; i < 14; ++i) {
        ff = new PSFontFileInfo(new GString(psBase14SubstFonts[i].psName),
                                fontType1, psFontFileResident);
        fontFileInfo->add(ff->psName, ff);
    }
    names = globalParams->getPSResidentFonts();
    for (i = 0; i < names->getLength(); ++i) {
        if (!fontFileInfo->lookup((GString *)names->get(i))) {
            ff = new PSFontFileInfo((GString *)names->get(i), fontType1,
                                    psFontFileResident);
            fontFileInfo->add(ff->psName, ff);
        }
    }
    delete names;
    imgIDLen = 0;
    imgIDSize = 0;
    formIDLen = 0;
    formIDSize = 0;

    numSaves = 0;
    numTilingPatterns = 0;
    nextFunc = 0;

    // initialize embedded font resource comment list
    embFontList = new GString();

    if (!manualCtrl) {
        // this check is needed in case the document has zero pages
        if (firstPage > 0 && firstPage <= catalog->getNumPages()) {
            writeHeader(firstPage, lastPage,
                        catalog->getPage(firstPage)->getMediaBox(),
                        catalog->getPage(firstPage)->getCropBox(),
                        catalog->getPage(firstPage)->getRotate());
        } else {
            box = new PDFRectangle(0, 0, 1, 1);
            writeHeader(firstPage, lastPage, box, box, 0);
            delete box;
        }
        if (mode != psModeForm) {
            writePS("%%BeginProlog\n");
        }
        writeXpdfProcset();
        if (mode != psModeForm) {
            writePS("%%EndProlog\n");
            writePS("%%BeginSetup\n");
        }
        writeDocSetup(catalog, firstPage, lastPage);
        if (mode != psModeForm) {
            writePS("%%EndSetup\n");
        }
    }

    // initialize sequential page number
    seqPage = 1;
}

PSOutputDev::~PSOutputDev()
{
    PSOutCustomColor *cc;

    if (ok) {
        if (!manualCtrl) {
            writePS("%%Trailer\n");
            writeTrailer();
            if (mode != psModeForm) {
                writePS("%%EOF\n");
            }
        }
        if (fileType == psFile) {
            fclose((FILE *)outputStream);
        } else if (fileType == psPipe) {
            pclose((FILE *)outputStream);
            signal(SIGPIPE, (SignalFunc)SIG_DFL);
        }
    }
    if (embFontList) {
        delete embFontList;
    }
    deleteGHash(fontFileInfo, PSFontFileInfo);
    free(imgIDs);
    free(formIDs);
    while (customColors) {
        cc = customColors;
        customColors = cc->next;
        delete cc;
    }
}

bool PSOutputDev::checkIO()
{
    if (fileType == psFile || fileType == psPipe || fileType == psStdout) {
        if (ferror((FILE *)outputStream)) {
            error(errIO, -1, "Error writing to PostScript file");
            return false;
        }
    }
    return true;
}

void PSOutputDev::writeHeader(int firstPage, int lastPage, PDFRectangle *mediaBox,
                              PDFRectangle *cropBox, int pageRotate)
{
    Object info, obj1;
    double x1, y1, x2, y2;
    int    i;

    switch (mode) {
    case psModePS:
        writePS("%!PS-Adobe-3.0\n");
        break;
    case psModeEPS:
        writePS("%!PS-Adobe-3.0 EPSF-3.0\n");
        break;
    case psModeForm:
        writePS("%!PS-Adobe-3.0 Resource-Form\n");
        break;
    }

    writePSFmt("%XpdfVersion: {0:s}\n", PACKAGE_VERSION);
    xref->getDocInfo(&info);

    if (info.is_dict() &&
        (obj1 = resolve(info.as_dict()["Creator"])).is_string()) {
        writePS("%%Creator: ");
        writePSTextLine(obj1.as_string());
    }

    if (info.is_dict() && (obj1 = resolve(info.as_dict()["Title"])).is_string()) {
        writePS("%%Title: ");
        writePSTextLine(obj1.as_string());
    }

    writePSFmt("%%LanguageLevel: {0:d}\n",
               (level == psLevel1 || level == psLevel1Sep) ?
                   1 :
                   (level == psLevel2 || level == psLevel2Sep) ? 2 : 3);
    if (level == psLevel1Sep || level == psLevel2Sep || level == psLevel3Sep) {
        writePS("%%DocumentProcessColors: (atend)\n");
        writePS("%%DocumentCustomColors: (atend)\n");
    }
    writePS("%%DocumentSuppliedResources: (atend)\n");

    switch (mode) {
    case psModePS:
        if (paperMatch) {
            for (i = 0; i < paperSizes.size(); ++i) {
                auto &size = paperSizes[i];

                writePSFmt("%%{0:s} {1:d}x{2:d} {1:d} {2:d} 0 () ()\n",
                           i == 0 ? "DocumentMedia:" : "+", size.w, size.h);
            }
        } else {
            writePSFmt("%%DocumentMedia: plain {0:d} {1:d} 0 () ()\n", paperWidth,
                       paperHeight);
        }
        writePSFmt("%%BoundingBox: 0 0 {0:d} {1:d}\n", paperWidth, paperHeight);
        writePSFmt("%%Pages: {0:d}\n", lastPage - firstPage + 1);
        writePS("%%EndComments\n");
        if (!paperMatch) {
            writePS("%%BeginDefaults\n");
            writePS("%%PageMedia: plain\n");
            writePS("%%EndDefaults\n");
        }
        break;
    case psModeEPS:
        epsX1 = cropBox->x1;
        epsY1 = cropBox->y1;
        epsX2 = cropBox->x2;
        epsY2 = cropBox->y2;
        if (pageRotate == 0 || pageRotate == 180) {
            x1 = epsX1;
            y1 = epsY1;
            x2 = epsX2;
            y2 = epsY2;
        } else { // pageRotate == 90 || pageRotate == 270
            x1 = 0;
            y1 = 0;
            x2 = epsY2 - epsY1;
            y2 = epsX2 - epsX1;
        }
        writePSFmt("%%BoundingBox: {0:d} {1:d} {2:d} {3:d}\n", (int)floor(x1),
                   (int)floor(y1), (int)ceil(x2), (int)ceil(y2));
        if (floor(x1) != ceil(x1) || floor(y1) != ceil(y1) ||
            floor(x2) != ceil(x2) || floor(y2) != ceil(y2)) {
            writePSFmt("%%HiResBoundingBox: {0:.6g} {1:.6g} {2:.6g} {3:.6g}\n",
                       x1, y1, x2, y2);
        }
        writePS("%%EndComments\n");
        break;
    case psModeForm:
        writePS("%%EndComments\n");
        writePS("32 dict dup begin\n");
        writePSFmt("/BBox [{0:d} {1:d} {2:d} {3:d}] def\n",
                   (int)floor(mediaBox->x1), (int)floor(mediaBox->y1),
                   (int)ceil(mediaBox->x2), (int)ceil(mediaBox->y2));
        writePS("/FormType 1 def\n");
        writePS("/Matrix [1 0 0 1 0 0] def\n");
        break;
    }
}

void PSOutputDev::writeXpdfProcset()
{
    bool         lev1, lev2, lev3, sep, nonSep;
    const char **p;
    const char * q;
    double       w;

    writePSFmt("%%BeginResource: procset xpdf {0:s} 0\n", PACKAGE_VERSION);
    writePSFmt("%%Copyright: {0:s}\n", XPDF_COPYRIGHT);
    lev1 = lev2 = lev3 = sep = nonSep = true;
    for (p = prolog; *p; ++p) {
        if ((*p)[0] == '~') {
            lev1 = lev2 = lev3 = sep = nonSep = false;
            for (q = *p + 1; *q; ++q) {
                switch (*q) {
                case '1':
                    lev1 = true;
                    break;
                case '2':
                    lev2 = true;
                    break;
                case '3':
                    lev3 = true;
                    break;
                case 's':
                    sep = true;
                    break;
                case 'n':
                    nonSep = true;
                    break;
                }
            }
        } else if ((level == psLevel1 && lev1 && nonSep) ||
                   (level == psLevel1Sep && lev1 && sep) ||
                   (level == psLevel2 && lev2 && nonSep) ||
                   (level == psLevel2Sep && lev2 && sep) ||
                   (level == psLevel3 && lev3 && nonSep) ||
                   (level == psLevel3Sep && lev3 && sep)) {
            writePSFmt("{0:s}\n", *p);
        }
    }
    if ((w = globalParams->getPSMinLineWidth()) > 0) {
        writePSFmt("/pdfMinLineWidth {0:.4g} def\n", w);
        for (p = minLineWidthProlog; *p; ++p) {
            writePSFmt("{0:s}\n", *p);
        }
    }
    writePS("%%EndResource\n");

    if (level >= psLevel3) {
        for (p = cmapProlog; *p; ++p) {
            writePSFmt("{0:s}\n", *p);
        }
    }
}

void PSOutputDev::writeDocSetup(Catalog *catalog, int firstPage, int lastPage)
{
    Page *   page;
    Dict *   resDict;
    Annots * annots;
    Form *   form;
    Object   obj1, obj2, obj3;
    GString *s;
    int      pg, i, j;

    if (mode == psModeForm) {
        // swap the form and xpdf dicts
        writePS("xpdf end begin dup begin\n");
    } else {
        writePS("xpdf begin\n");
    }
    for (pg = firstPage; pg <= lastPage; ++pg) {
        page = catalog->getPage(pg);
        if ((resDict = page->getResourceDict())) {
            setupResources(resDict);
        }
        annots = new Annots(doc, page->getAnnots());
        for (i = 0; i < annots->getNumAnnots(); ++i) {
            if ((obj1 = annots->getAnnot(i)->getAppearance()).is_stream()) {
                obj2 = resolve((*obj1.streamGetDict())["Resources"]);
                if (obj2.is_dict()) {
                    setupResources(&obj2.as_dict());
                }
            }
        }
        delete annots;
    }
    if ((form = catalog->getForm())) {
        for (i = 0; i < form->getNumFields(); ++i) {
            form->getField(i)->getResources(&obj1);
            if (obj1.is_array()) {
                for (j = 0; j < obj1.as_array().size(); ++j) {
                    obj2 = resolve(obj1[j]);
                    if (obj2.is_dict()) {
                        setupResources(&obj2.as_dict());
                    }
                }
            } else if (obj1.is_dict()) {
                setupResources(&obj1.as_dict());
            }
        }
    }
    if (mode != psModeForm) {
        if (mode != psModeEPS && !manualCtrl) {
            writePSFmt("{0:s} pdfSetup\n",
                       globalParams->getPSDuplex() ? "true" : "false");
            if (!paperMatch) {
                writePSFmt("{0:d} {1:d} pdfSetupPaper\n", paperWidth,
                           paperHeight);
            }
        }
#if OPI_SUPPORT
        if (globalParams->getPSOPI()) {
            writePS("/opiMatrix matrix currentmatrix def\n");
        }
#endif
    }
    if (customCodeCbk) {
        if ((s = (*customCodeCbk)(this, psOutCustomDocSetup, 0,
                                  customCodeCbkData))) {
            writePS(s->c_str());
            delete s;
        }
    }
    if (mode != psModeForm) {
        writePS("end\n");
    }
}

void PSOutputDev::writePageTrailer()
{
    if (mode != psModeForm) {
        writePS("pdfEndPage\n");
    }
}

void PSOutputDev::writeTrailer()
{
    PSOutCustomColor *cc;

    if (mode == psModeForm) {
        writePS("/Foo exch /Form defineresource pop\n");
    } else {
        writePS("%%DocumentSuppliedResources:\n");
        writePS(embFontList->c_str());
        if (level == psLevel1Sep || level == psLevel2Sep ||
            level == psLevel3Sep) {
            writePS("%%DocumentProcessColors:");
            if (processColors & psProcessCyan) {
                writePS(" Cyan");
            }
            if (processColors & psProcessMagenta) {
                writePS(" Magenta");
            }
            if (processColors & psProcessYellow) {
                writePS(" Yellow");
            }
            if (processColors & psProcessBlack) {
                writePS(" Black");
            }
            writePS("\n");
            writePS("%%DocumentCustomColors:");
            for (cc = customColors; cc; cc = cc->next) {
                writePS(" ");
                writePSString(cc->name);
            }
            writePS("\n");
            writePS("%%CMYKCustomColor:\n");
            for (cc = customColors; cc; cc = cc->next) {
                writePSFmt("%%+ {0:.4g} {1:.4g} {2:.4g} {3:.4g} ", cc->c, cc->m,
                           cc->y, cc->k);
                writePSString(cc->name);
                writePS("\n");
            }
        }
    }
}

void PSOutputDev::setupResources(Dict *resDict)
{
    Object xObjDict, xObjRef, patDict, patRef;
    Object gsDict, gsRef, gs, smask, smaskGroup, resObj;
    Ref    ref0;
    bool   skip;
    int    i, j;

    setupFonts(resDict);
    setupImages(resDict);

    //----- recursively scan XObjects
    xObjDict = resolve((*resDict)["XObject"]);
    if (xObjDict.is_dict()) {
        for (i = 0; i < xObjDict.as_dict().size(); ++i) {
            // avoid infinite recursion on XObjects
            skip = false;

            auto &xObjRef = xObjDict.val_at(i);

            if (xObjRef.is_ref()) {
                ref0 = xObjRef.as_ref();

                for (j = 0; j < xobjStack.size(); ++j) {
                    auto &ref1 = xobjStack[j];
                    if (ref1 == ref0) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    xobjStack.push_back(ref0);
                }
            }

            if (!skip) {
                // process the XObject's resource dictionary
                auto &xObj = xObjDict.val_at(i);

                if (xObj.is_stream()) {
                    resObj = resolve((*xObj.streamGetDict())["Resources"]);
                    if (resObj.is_dict()) {
                        setupResources(&resObj.as_dict());
                    }
                }
            }

            if (xObjRef.is_ref() && !skip) {
                xobjStack.pop_back();
            }
        }
    }

    //----- recursively scan Patterns
    patDict = resolve((*resDict)["Pattern"]);
    if (patDict.is_dict()) {
        inType3Char = true;
        for (i = 0; i < patDict.as_dict().size(); ++i) {
            // avoid infinite recursion on Patterns
            skip = false;

            auto &patRef = patDict.val_at(i);

            if (patRef.is_ref()) {
                ref0 = patRef.as_ref();

                for (j = 0; j < xobjStack.size(); ++j) {
                    auto &ref1 = xobjStack[j];
                    if (ref1 == ref0) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    xobjStack.push_back(ref0);
                }
            }
            if (!skip) {
                // process the Pattern's resource dictionary
                auto &pat = patDict.val_at(i);

                if (pat.is_stream()) {
                    resObj = resolve((*pat.streamGetDict())["Resources"]);
                    if (resObj.is_dict()) {
                        setupResources(&resObj.as_dict());
                    }
                }
            }

            if (patRef.is_ref() && !skip) {
                xobjStack.pop_back();
            }
        }
        inType3Char = false;
    }

    //----- recursively scan SMask transparency groups in ExtGState dicts
    gsDict = resolve((*resDict)["ExtGState"]);
    if (gsDict.is_dict()) {
        for (i = 0; i < gsDict.as_dict().size(); ++i) {
            // avoid infinite recursion on ExtGStates
            skip = false;

            auto &gsRef = gsDict.val_at(i);

            if (gsRef.is_ref()) {
                ref0 = gsRef.as_ref();
                for (j = 0; j < xobjStack.size(); ++j) {
                    auto &ref1 = xobjStack[j];
                    if (ref1 == ref0) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    xobjStack.push_back(ref0);
                }
            }

            if (!skip) {
                // process the ExtGState's SMask's transparency group's resource dict
                auto &gs = gsDict.val_at(i);

                if (gs.is_dict()) {
                    if ((smask = resolve(gs.as_dict()["SMask"])).is_dict()) {
                        if ((smaskGroup = resolve(smask.as_dict()["G"]))
                                .is_stream()) {
                            resObj = resolve(
                                (*smaskGroup.streamGetDict())["Resources"]);
                            if (resObj.is_dict()) {
                                setupResources(&resObj.as_dict());
                            }
                        }
                    }
                }
            }

            if (gsRef.is_ref() && !skip) {
                xobjStack.pop_back();
            }
        }
    }

    setupForms(resDict);
}

void PSOutputDev::setupFonts(Dict *resDict)
{
    Object       obj1, obj2;
    Ref          r;
    GfxFontDict *gfxFontDict;
    GfxFont *    font;
    int          i;

    gfxFontDict = NULL;
    obj1 = (*resDict)["Font"];
    if (obj1.is_ref()) {
        obj2 = resolve(obj1);
        if (obj2.is_dict()) {
            r = obj1.as_ref();
            gfxFontDict = new GfxFontDict(xref, &r, &obj2.as_dict());
        }
    } else if (obj1.is_dict()) {
        gfxFontDict = new GfxFontDict(xref, NULL, &obj1.as_dict());
    }
    if (gfxFontDict) {
        for (i = 0; i < gfxFontDict->getNumFonts(); ++i) {
            if ((font = gfxFontDict->getFont(i))) {
                setupFont(font, resDict);
            }
        }
        delete gfxFontDict;
    }
}

void PSOutputDev::setupFont(GfxFont *font, Dict *parentResDict)
{
    GfxFontLoc *fontLoc;
    bool        subst;
    char        buf[16];
    char *      charName;
    double      xs, ys;
    int         code;
    double      w1, w2;
    int         i, j;

    // check if font is already set up
    for (auto &fi : fontInfo) {
        if (fi.fontID == *font->getID()) {
            return;
        }
    }

    // add fontInfo entry
    fontInfo.push_back(*font->getID());
    auto &fi = fontInfo.back();

    xs = ys = 1;
    subst = false;

    if (font->getType() == fontType3) {
        fi.ff = setupType3Font(font, parentResDict);
    } else {
        if ((fontLoc = font->locateFont(xref, true))) {
            switch (fontLoc->locType) {
            case gfxFontLocEmbedded:
                switch (fontLoc->fontType) {
                case fontType1:
                    fi.ff = setupEmbeddedType1Font(font, &fontLoc->embFontID);
                    break;
                case fontType1C:
                    fi.ff = setupEmbeddedType1CFont(font, &fontLoc->embFontID);
                    break;
                case fontType1COT:
                    fi.ff =
                        setupEmbeddedOpenTypeT1CFont(font, &fontLoc->embFontID);
                    break;
                case fontTrueType:
                case fontTrueTypeOT:
                    fi.ff = setupEmbeddedTrueTypeFont(font, &fontLoc->embFontID);
                    break;
                case fontCIDType0C:
                    fi.ff = setupEmbeddedCIDType0Font(font, &fontLoc->embFontID);
                    break;
                case fontCIDType2:
                case fontCIDType2OT:
                    //~ should check to see if font actually uses vertical mode
                    fi.ff = setupEmbeddedCIDTrueTypeFont(
                        font, &fontLoc->embFontID, true);
                    break;
                case fontCIDType0COT:
                    fi.ff =
                        setupEmbeddedOpenTypeCFFFont(font, &fontLoc->embFontID);
                    break;
                default:
                    break;
                }
                break;
            case gfxFontLocExternal:
                //~ add cases for other external 16-bit fonts
                switch (fontLoc->fontType) {
                case fontType1:
                    fi.ff = setupExternalType1Font(font, fontLoc->path);
                    break;
                case fontTrueType:
                case fontTrueTypeOT:
                    fi.ff = setupExternalTrueTypeFont(font, fontLoc->path,
                                                      fontLoc->fontNum);
                    break;
                case fontCIDType2:
                case fontCIDType2OT:
                    //~ should check to see if font actually uses vertical mode
                    fi.ff = setupExternalCIDTrueTypeFont(font, fontLoc->path,
                                                         fontLoc->fontNum, true);
                    break;
                default:
                    break;
                }
                break;
            case gfxFontLocResident:
                if (!(fi.ff = (PSFontFileInfo *)fontFileInfo->lookup(
                          fontLoc->path))) {
                    // handle psFontPassthrough
                    fi.ff = new PSFontFileInfo(fontLoc->path->copy(),
                                               fontLoc->fontType,
                                               psFontFileResident);
                    fontFileInfo->add(fi.ff->psName, fi.ff);
                }
                break;
            }
        }

        if (!fi.ff) {
            if (font->isCIDFont()) {
                error(errSyntaxError, -1,
                      "Couldn't find a font for '{0:s}' ('{1:s}' character "
                      "collection)",
                      font->as_name() ? font->as_name()->c_str() : "(unnamed)",
                      ((GfxCIDFont *)font)->getCollection() ?
                          ((GfxCIDFont *)font)->getCollection()->c_str() :
                          "(unknown)");
            } else {
                error(errSyntaxError, -1, "Couldn't find a font for '{0:s}'",
                      font->as_name() ? font->as_name()->c_str() : "(unnamed)");
            }
            delete fontLoc;
            return;
        }

        // scale substituted 8-bit fonts
        if (fontLoc->locType == gfxFontLocResident && fontLoc->substIdx >= 0) {
            subst = true;
            for (code = 0; code < 256; ++code) {
                if ((charName = ((Gfx8BitFont *)font)->getCharName(code)) &&
                    charName[0] == 'm' && charName[1] == '\0') {
                    break;
                }
            }
            if (code < 256) {
                w1 = ((Gfx8BitFont *)font)->getWidth(code);
            } else {
                w1 = 0;
            }
            w2 = psBase14SubstFonts[fontLoc->substIdx].mWidth;
            xs = w1 / w2;
            if (xs < 0.1) {
                xs = 1;
            }
        }

        // handle encodings for substituted CID fonts
        if (fontLoc->locType == gfxFontLocResident &&
            fontLoc->fontType >= fontCIDType0) {
            subst = true;

            if (globalParams->hasUnicodeMap(fontLoc->encoding->c_str()))
                fi.ff->encoding = fontLoc->encoding->copy();
        }

        delete fontLoc;
    }

    // generate PostScript code to set up the font
    if (font->isCIDFont()) {
        if (level == psLevel3 || level == psLevel3Sep) {
            writePSFmt("/F{0:d}_{1:d} /{2:t} {3:d} pdfMakeFont16L3\n",
                       font->getID()->num, font->getID()->gen, fi.ff->psName,
                       font->getWMode());
        } else {
            writePSFmt("/F{0:d}_{1:d} /{2:t} {3:d} pdfMakeFont16\n",
                       font->getID()->num, font->getID()->gen, fi.ff->psName,
                       font->getWMode());
        }
    } else {
        writePSFmt("/F{0:d}_{1:d} /{2:t} {3:.6g} {4:.6g}\n", font->getID()->num,
                   font->getID()->gen, fi.ff->psName, xs, ys);
        for (i = 0; i < 256; i += 8) {
            writePS((char *)((i == 0) ? "[ " : "  "));
            for (j = 0; j < 8; ++j) {
                if (font->getType() == fontTrueType && !subst &&
                    !((Gfx8BitFont *)font)->getHasEncoding()) {
                    sprintf(buf, "c%02x", i + j);
                    charName = buf;
                } else {
                    charName = ((Gfx8BitFont *)font)->getCharName(i + j);
                }
                writePS("/");
                writePSName(charName ? charName : (char *)".notdef");
                // the empty name is legal in PDF and PostScript, but PostScript
                // uses a double-slash (//...) for "immediately evaluated names",
                // so we need to add a space character here
                if (charName && !charName[0]) {
                    writePS(" ");
                }
            }
            writePS((i == 256 - 8) ? (char *)"]\n" : (char *)"\n");
        }
        writePS("pdfMakeFont\n");
    }
}

PSFontFileInfo *PSOutputDev::setupEmbeddedType1Font(GfxFont *font, Ref *id)
{
    static char     hexChar[17] = "0123456789abcdef";
    GString *       psName;
    PSFontFileInfo *ff;
    Object          refObj, strObj, obj1, obj2;
    Dict *          dict;
    int             length1, length2;
    int             c;
    int             start[6];
    bool            binMode;
    int             n, i;

    // check if font is already embedded
    if ((ff = (PSFontFileInfo *)fontFileInfo->lookup(
             font->getEmbeddedFontName()))) {
        return ff;
    }

    // generate name
    // (this assumes that the PS font name matches the PDF font name)
    psName = font->getEmbeddedFontName()->copy();

    // get the font stream and info
    refObj = xpdf::make_ref_obj(id->num, id->gen, xref);
    strObj = resolve(refObj);
    if (!strObj.is_stream()) {
        error(errSyntaxError, -1, "Embedded font file object is not a stream");
        goto err1;
    }
    if (!(dict = strObj.streamGetDict())) {
        error(errSyntaxError, -1,
              "Embedded font stream is missing its dictionary");
        goto err1;
    }

    obj1 = resolve((*dict)["Length1"]);
    obj2 = resolve((*dict)["Length2"]);

    if (!obj1.is_int() || !obj2.is_int()) {
        error(errSyntaxError, -1,
              "Missing length fields in embedded font stream dictionary");
        goto err1;
    }
    length1 = obj1.as_int();
    length2 = obj2.as_int();

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // check for PFB format
    strObj.streamReset();
    start[0] = strObj.streamGetChar();
    start[1] = strObj.streamGetChar();
    if (start[0] == 0x80 && start[1] == 0x01) {
        error(errSyntaxWarning, -1, "Embedded Type 1 font is in PFB format");
        while (1) {
            for (i = 2; i < 6; ++i) {
                start[i] = strObj.streamGetChar();
            }
            if (start[2] == EOF || start[3] == EOF || start[4] == EOF ||
                start[5] == EOF) {
                break;
            }
            n = start[2] + (start[3] << 8) + (start[4] << 16) + (start[5] << 24);
            if (start[1] == 0x01) {
                for (i = 0; i < n; ++i) {
                    if ((c = strObj.streamGetChar()) == EOF) {
                        break;
                    }
                    writePSChar(c);
                }
            } else {
                for (i = 0; i < n; ++i) {
                    if ((c = strObj.streamGetChar()) == EOF) {
                        break;
                    }
                    writePSChar(hexChar[(c >> 4) & 0x0f]);
                    writePSChar(hexChar[c & 0x0f]);
                    if (i % 32 == 31) {
                        writePSChar('\n');
                    }
                }
            }
            start[0] = strObj.streamGetChar();
            start[1] = strObj.streamGetChar();
            if (start[0] == EOF || start[1] == EOF ||
                (start[0] == 0x80 && start[1] == 0x03)) {
                break;
            } else if (!(start[0] == 0x80 &&
                         (start[1] == 0x01 || start[1] == 0x02))) {
                error(errSyntaxError, -1,
                      "Invalid PFB header in embedded font stream");
                break;
            }
        }
        writePSChar('\n');

        // plain text (PFA) format
    } else {
        // copy ASCII portion of font
        writePSChar(start[0]);
        writePSChar(start[1]);
        for (i = 2; i < length1 && (c = strObj.streamGetChar()) != EOF; ++i) {
            writePSChar(c);
        }

        // figure out if encrypted portion is binary or ASCII
        binMode = false;
        for (i = 0; i < 4; ++i) {
            start[i] = strObj.streamGetChar();
            if (start[i] == EOF) {
                error(errSyntaxError, -1,
                      "Unexpected end of file in embedded font stream");
                goto err1;
            }
            if (!((start[i] >= '0' && start[i] <= '9') ||
                  (start[i] >= 'A' && start[i] <= 'F') ||
                  (start[i] >= 'a' && start[i] <= 'f')))
                binMode = true;
        }

        // convert binary data to ASCII
        if (binMode) {
            for (i = 0; i < 4; ++i) {
                writePSChar(hexChar[(start[i] >> 4) & 0x0f]);
                writePSChar(hexChar[start[i] & 0x0f]);
            }
#if 0
      // this causes trouble for various PostScript printers
      // if Length2 is incorrect (too small), font data gets chopped, so
      // we take a few extra characters from the trailer just in case
      length2 += length3 >= 8 ? 8 : length3;
#endif
            while (i < length2) {
                if ((c = strObj.streamGetChar()) == EOF) {
                    break;
                }
                writePSChar(hexChar[(c >> 4) & 0x0f]);
                writePSChar(hexChar[c & 0x0f]);
                if (++i % 32 == 0) {
                    writePSChar('\n');
                }
            }
            if (i % 32 > 0) {
                writePSChar('\n');
            }

            // already in ASCII format -- just copy it
        } else {
            for (i = 0; i < 4; ++i) {
                writePSChar(start[i]);
            }
            for (i = 4; i < length2; ++i) {
                if ((c = strObj.streamGetChar()) == EOF) {
                    break;
                }
                writePSChar(c);
            }
        }

        // write padding and "cleartomark"
        for (i = 0; i < 8; ++i) {
            writePS("00000000000000000000000000000000"
                    "00000000000000000000000000000000\n");
        }
        writePS("cleartomark\n");
    }

    // ending comment
    writePS("%%EndResource\n");

    strObj.streamClose();

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    fontFileInfo->add(ff->psName, ff);
    return ff;

err1:
    strObj.streamClose();
    delete psName;
    return NULL;
}

PSFontFileInfo *PSOutputDev::setupExternalType1Font(GfxFont *font,
                                                    GString *fileName)
{
    static char     hexChar[17] = "0123456789abcdef";
    GString *       psName;
    PSFontFileInfo *ff;
    FILE *          fontFile;
    int             buf[6];
    int             c, n, i;

    if (font->as_name()) {
        // check if font is already embedded
        if ((ff = (PSFontFileInfo *)fontFileInfo->lookup(font->as_name()))) {
            return ff;
        }
        // this assumes that the PS font name matches the PDF font name
        psName = font->as_name()->copy();
    } else {
        // generate name
        //~ this won't work -- the PS font name won't match
        psName = makePSFontName(font, font->getID());
    }

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // open the font file
    if (!(fontFile = fopen(fileName->c_str(), "rb"))) {
        error(errIO, -1, "Couldn't open external font file");
        return NULL;
    }

    // check for PFB format
    buf[0] = fgetc(fontFile);
    buf[1] = fgetc(fontFile);
    if (buf[0] == 0x80 && buf[1] == 0x01) {
        while (1) {
            for (i = 2; i < 6; ++i) {
                buf[i] = fgetc(fontFile);
            }
            if (buf[2] == EOF || buf[3] == EOF || buf[4] == EOF ||
                buf[5] == EOF) {
                break;
            }
            n = buf[2] + (buf[3] << 8) + (buf[4] << 16) + (buf[5] << 24);
            if (buf[1] == 0x01) {
                for (i = 0; i < n; ++i) {
                    if ((c = fgetc(fontFile)) == EOF) {
                        break;
                    }
                    writePSChar(c);
                }
            } else {
                for (i = 0; i < n; ++i) {
                    if ((c = fgetc(fontFile)) == EOF) {
                        break;
                    }
                    writePSChar(hexChar[(c >> 4) & 0x0f]);
                    writePSChar(hexChar[c & 0x0f]);
                    if (i % 32 == 31) {
                        writePSChar('\n');
                    }
                }
            }
            buf[0] = fgetc(fontFile);
            buf[1] = fgetc(fontFile);
            if (buf[0] == EOF || buf[1] == EOF ||
                (buf[0] == 0x80 && buf[1] == 0x03)) {
                break;
            } else if (!(buf[0] == 0x80 && (buf[1] == 0x01 || buf[1] == 0x02))) {
                error(errSyntaxError, -1,
                      "Invalid PFB header in external font file");
                break;
            }
        }
        writePSChar('\n');

        // plain text (PFA) format
    } else {
        writePSChar(buf[0]);
        writePSChar(buf[1]);
        while ((c = fgetc(fontFile)) != EOF) {
            writePSChar(c);
        }
    }

    fclose(fontFile);

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileExternal);
    ff->extFileName = fileName->copy();
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupEmbeddedType1CFont(GfxFont *font, Ref *id)
{
    GString *       psName;
    PSFontFileInfo *ff;
    char *          fontBuf;
    int             fontLen;
    FoFiType1C *    ffT1C;
    GHashIter *     iter;

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num &&
            ff->embFontID.gen == id->gen) {
            fontFileInfo->killIter(&iter);
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, id);

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 1 font
    if ((fontBuf = font->readEmbFontFile(xref, &fontLen))) {
        if ((ffT1C = FoFiType1C::make(fontBuf, fontLen))) {
            ffT1C->convertToType1(psName->c_str(), NULL, true, outputFunc,
                                  outputStream);
            delete ffT1C;
        }
        free(fontBuf);
    }

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupEmbeddedOpenTypeT1CFont(GfxFont *font, Ref *id)
{
    GString *       psName;
    PSFontFileInfo *ff;
    char *          fontBuf;
    int             fontLen;
    FoFiTrueType *  ffTT;
    GHashIter *     iter;

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num &&
            ff->embFontID.gen == id->gen) {
            fontFileInfo->killIter(&iter);
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, id);

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 1 font
    if ((fontBuf = font->readEmbFontFile(xref, &fontLen))) {
        if ((ffTT = FoFiTrueType::make(fontBuf, fontLen, 0))) {
            if (ffTT->isOpenTypeCFF()) {
                ffTT->convertToType1(psName->c_str(), NULL, true, outputFunc,
                                     outputStream);
            }
            delete ffTT;
        }
        free(fontBuf);
    }

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupEmbeddedTrueTypeFont(GfxFont *font, Ref *id)
{
    GString *       psName;
    PSFontFileInfo *ff;
    char *          fontBuf;
    int             fontLen;
    FoFiTrueType *  ffTT;
    int *           codeToGID;
    GHashIter *     iter;

    // get the code-to-GID mapping
    if (!(fontBuf = font->readEmbFontFile(xref, &fontLen))) {
        return NULL;
    }
    if (!(ffTT = FoFiTrueType::make(fontBuf, fontLen, 0))) {
        free(fontBuf);
        return NULL;
    }
    codeToGID = ((Gfx8BitFont *)font)->getCodeToGIDMap(ffTT);

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num &&
            ff->embFontID.gen == id->gen && ff->codeToGIDLen == 256 &&
            !memcmp(ff->codeToGID, codeToGID, 256 * sizeof(int))) {
            fontFileInfo->killIter(&iter);
            free(codeToGID);
            delete ffTT;
            free(fontBuf);
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, id);

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 42 font
    ffTT->convertToType42(psName->c_str(),
                          ((Gfx8BitFont *)font)->getHasEncoding() ?
                              ((Gfx8BitFont *)font)->getEncoding() :
                              (char **)NULL,
                          codeToGID, outputFunc, outputStream);
    delete ffTT;
    free(fontBuf);

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    ff->codeToGID = codeToGID;
    ff->codeToGIDLen = 256;
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupExternalTrueTypeFont(GfxFont *font,
                                                       GString *fileName,
                                                       int      fontNum)
{
    GString *       psName;
    PSFontFileInfo *ff;
    FoFiTrueType *  ffTT;
    int *           codeToGID;
    GHashIter *     iter;

    // get the code-to-GID mapping
    if (!(ffTT = FoFiTrueType::load(fileName->c_str(), fontNum))) {
        return NULL;
    }
    codeToGID = ((Gfx8BitFont *)font)->getCodeToGIDMap(ffTT);

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileExternal && ff->type == font->getType() &&
            !ff->extFileName->cmp(fileName) && ff->codeToGIDLen == 256 &&
            !memcmp(ff->codeToGID, codeToGID, 256 * sizeof(int))) {
            fontFileInfo->killIter(&iter);
            free(codeToGID);
            delete ffTT;
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, font->getID());

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 42 font
    ffTT->convertToType42(psName->c_str(),
                          ((Gfx8BitFont *)font)->getHasEncoding() ?
                              ((Gfx8BitFont *)font)->getEncoding() :
                              (char **)NULL,
                          codeToGID, outputFunc, outputStream);
    delete ffTT;

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileExternal);
    ff->extFileName = fileName->copy();
    ff->codeToGID = codeToGID;
    ff->codeToGIDLen = 256;
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupEmbeddedCIDType0Font(GfxFont *font, Ref *id)
{
    GString *       psName;
    PSFontFileInfo *ff;
    char *          fontBuf;
    int             fontLen;
    FoFiType1C *    ffT1C;
    GHashIter *     iter;

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num &&
            ff->embFontID.gen == id->gen) {
            fontFileInfo->killIter(&iter);
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, id);

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 0 font
    if ((fontBuf = font->readEmbFontFile(xref, &fontLen))) {
        if ((ffT1C = FoFiType1C::make(fontBuf, fontLen))) {
            if (globalParams->getPSLevel() >= psLevel3) {
                // Level 3: use a CID font
                ffT1C->convertToCIDType0(psName->c_str(), NULL, 0, outputFunc,
                                         outputStream);
            } else {
                // otherwise: use a non-CID composite font
                ffT1C->convertToType0(psName->c_str(), NULL, 0, outputFunc,
                                      outputStream);
            }
            delete ffT1C;
        }
        free(fontBuf);
    }

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *
PSOutputDev::setupEmbeddedCIDTrueTypeFont(GfxFont *font, Ref *id,
                                          bool needVerticalMetrics)
{
    GString *       psName;
    PSFontFileInfo *ff;
    char *          fontBuf;
    int             fontLen;
    FoFiTrueType *  ffTT;
    int *           codeToGID;
    int             codeToGIDLen;
    GHashIter *     iter;

    // get the code-to-GID mapping
    codeToGID = ((GfxCIDFont *)font)->getCIDToGID();
    codeToGIDLen = ((GfxCIDFont *)font)->getCIDToGIDLen();

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num &&
            ff->embFontID.gen == id->gen && ff->codeToGIDLen == codeToGIDLen &&
            ((!ff->codeToGID && !codeToGID) ||
             (ff->codeToGID && codeToGID &&
              !memcmp(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int))))) {
            fontFileInfo->killIter(&iter);
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, id);

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 0 font
    if ((fontBuf = font->readEmbFontFile(xref, &fontLen))) {
        if ((ffTT = FoFiTrueType::make(fontBuf, fontLen, 0))) {
            if (globalParams->getPSLevel() >= psLevel3) {
                // Level 3: use a CID font
                ffTT->convertToCIDType2(psName->c_str(), codeToGID, codeToGIDLen,
                                        needVerticalMetrics, outputFunc,
                                        outputStream);
            } else {
                // otherwise: use a non-CID composite font
                ffTT->convertToType0(psName->c_str(), codeToGID, codeToGIDLen,
                                     needVerticalMetrics, outputFunc,
                                     outputStream);
            }
            delete ffTT;
        }
        free(fontBuf);
    }

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    if (codeToGIDLen) {
        ff->codeToGID = (int *)calloc(codeToGIDLen, sizeof(int));
        memcpy(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int));
        ff->codeToGIDLen = codeToGIDLen;
    }
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *
PSOutputDev::setupExternalCIDTrueTypeFont(GfxFont *font, GString *fileName,
                                          int fontNum, bool needVerticalMetrics)
{
    GString *          psName;
    PSFontFileInfo *   ff;
    FoFiTrueType *     ffTT;
    int *              codeToGID;
    int                codeToGIDLen;
    CharCodeToUnicode *ctu;
    Unicode            uBuf[8];
    int                cmap, code;
    GHashIter *        iter;

    // create a code-to-GID mapping, via Unicode
    if (!(ffTT = FoFiTrueType::load(fileName->c_str(), fontNum))) {
        return NULL;
    }
    if (!(ctu = ((GfxCIDFont *)font)->getToUnicode())) {
        error(errSyntaxError, -1,
              "Couldn't find a mapping to Unicode for font '{0:s}'",
              font->as_name() ? font->as_name()->c_str() : "(unnamed)");
        delete ffTT;
        return NULL;
    }
    // look for a Unicode cmap
    for (cmap = 0; cmap < ffTT->getNumCmaps(); ++cmap) {
        if ((ffTT->getCmapPlatform(cmap) == 3 &&
             ffTT->getCmapEncoding(cmap) == 1) ||
            ffTT->getCmapPlatform(cmap) == 0) {
            break;
        }
    }
    if (cmap >= ffTT->getNumCmaps()) {
        error(errSyntaxError, -1, "Couldn't find a Unicode cmap in font '{0:s}'",
              font->as_name() ? font->as_name()->c_str() : "(unnamed)");
        ctu->decRefCnt();
        delete ffTT;
        return NULL;
    }
    // map CID -> Unicode -> GID
    if (ctu->isIdentity()) {
        codeToGIDLen = 65536;
    } else {
        codeToGIDLen = ctu->getLength();
    }
    codeToGID = (int *)calloc(codeToGIDLen, sizeof(int));
    for (code = 0; code < codeToGIDLen; ++code) {
        if (ctu->mapToUnicode(code, uBuf, 8) > 0) {
            codeToGID[code] = ffTT->mapCodeToGID(cmap, uBuf[0]);
        } else {
            codeToGID[code] = 0;
        }
    }
    ctu->decRefCnt();

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileExternal && ff->type == font->getType() &&
            !ff->extFileName->cmp(fileName) && ff->codeToGIDLen == codeToGIDLen &&
            ff->codeToGID &&
            !memcmp(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int))) {
            fontFileInfo->killIter(&iter);
            free(codeToGID);
            delete ffTT;
            return ff;
        }
    }

    // check for embedding permission
    if (ffTT->getEmbeddingRights() < 1) {
        error(errSyntaxError, -1,
              "TrueType font '{0:s}' does not allow embedding",
              font->as_name() ? font->as_name()->c_str() : "(unnamed)");
        free(codeToGID);
        delete ffTT;
        return NULL;
    }

    // generate name
    psName = makePSFontName(font, font->getID());

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 0 font
    //~ this should use fontNum to load the correct font
    if (globalParams->getPSLevel() >= psLevel3) {
        // Level 3: use a CID font
        ffTT->convertToCIDType2(psName->c_str(), codeToGID, codeToGIDLen,
                                needVerticalMetrics, outputFunc, outputStream);
    } else {
        // otherwise: use a non-CID composite font
        ffTT->convertToType0(psName->c_str(), codeToGID, codeToGIDLen,
                             needVerticalMetrics, outputFunc, outputStream);
    }
    delete ffTT;

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileExternal);
    ff->extFileName = fileName->copy();
    ff->codeToGID = codeToGID;
    ff->codeToGIDLen = codeToGIDLen;
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupEmbeddedOpenTypeCFFFont(GfxFont *font, Ref *id)
{
    GString *       psName;
    PSFontFileInfo *ff;
    char *          fontBuf;
    int             fontLen;
    FoFiTrueType *  ffTT;
    GHashIter *     iter;
    int             n;

    // check if font is already embedded
    fontFileInfo->startIter(&iter);
    while (fontFileInfo->getNext(&iter, &psName, (void **)&ff)) {
        if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num &&
            ff->embFontID.gen == id->gen) {
            fontFileInfo->killIter(&iter);
            return ff;
        }
    }

    // generate name
    psName = makePSFontName(font, id);

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // convert it to a Type 0 font
    if ((fontBuf = font->readEmbFontFile(xref, &fontLen))) {
        if ((ffTT = FoFiTrueType::make(fontBuf, fontLen, 0))) {
            if (ffTT->isOpenTypeCFF()) {
                if (globalParams->getPSLevel() >= psLevel3) {
                    // Level 3: use a CID font
                    ffTT->convertToCIDType0(
                        psName->c_str(), ((GfxCIDFont *)font)->getCIDToGID(),
                        ((GfxCIDFont *)font)->getCIDToGIDLen(), outputFunc,
                        outputStream);
                } else {
                    // otherwise: use a non-CID composite font
                    ffTT->convertToType0(psName->c_str(),
                                         ((GfxCIDFont *)font)->getCIDToGID(),
                                         ((GfxCIDFont *)font)->getCIDToGIDLen(),
                                         outputFunc, outputStream);
                }
            }
            delete ffTT;
        }
        free(fontBuf);
    }

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    ff->embFontID = *id;
    if ((n = ((GfxCIDFont *)font)->getCIDToGIDLen())) {
        ff->codeToGID = (int *)calloc(n, sizeof(int));
        memcpy(ff->codeToGID, ((GfxCIDFont *)font)->getCIDToGID(),
               n * sizeof(int));
        ff->codeToGIDLen = n;
    }
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

PSFontFileInfo *PSOutputDev::setupType3Font(GfxFont *font, Dict *parentResDict)
{
    PSFontFileInfo *ff;
    GString *       psName;
    Dict *          resDict;
    Dict *          charProcs;
    PDFRectangle    box;
    double *        m;
    GString *       buf;
    int             i;

    // generate name
    psName =
        GString::format("T3_{0:d}_{1:d}", font->getID()->num, font->getID()->gen);

    // set up resources used by font
    if ((resDict = ((Gfx8BitFont *)font)->getResources())) {
        inType3Char = true;
        setupResources(resDict);
        inType3Char = false;
    } else {
        resDict = parentResDict;
    }

    // beginning comment
    writePSFmt("%%BeginResource: font {0:t}\n", psName);
    embFontList->append("%%+ font ");
    embFontList->append(psName->c_str());
    embFontList->append("\n");

    // font dictionary
    writePS("8 dict begin\n");
    writePS("/FontType 3 def\n");
    m = font->getFontMatrix();
    writePSFmt(
        "/FontMatrix [{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] def\n",
        m[0], m[1], m[2], m[3], m[4], m[5]);
    m = font->getFontBBox();
    writePSFmt("/FontBBox [{0:.6g} {1:.6g} {2:.6g} {3:.6g}] def\n", m[0], m[1],
               m[2], m[3]);
    writePS("/Encoding 256 array def\n");
    writePS("  0 1 255 { Encoding exch /.notdef put } for\n");
    writePS("/BuildGlyph {\n");
    writePS("  exch /CharProcs get exch\n");
    writePS("  2 copy known not { pop /.notdef } if\n");
    writePS("  get exec\n");
    writePS("} bind def\n");
    writePS("/BuildChar {\n");
    writePS("  1 index /Encoding get exch get\n");
    writePS("  1 index /BuildGlyph get exec\n");
    writePS("} bind def\n");
    if ((charProcs = ((Gfx8BitFont *)font)->getCharProcs())) {
        writePSFmt("/CharProcs {0:d} dict def\n", charProcs->size());
        writePS("CharProcs begin\n");
        box.x1 = m[0];
        box.y1 = m[1];
        box.x2 = m[2];
        box.y2 = m[3];

        auto gfx = std::make_unique< Gfx >(doc, this, resDict, &box, nullptr);
        inType3Char = true;
        for (i = 0; i < charProcs->size(); ++i) {
            t3FillColorOnly = false;
            t3Cacheable = false;
            t3NeedsRestore = false;
            writePS("/");
            writePSName(charProcs->key_at(i));
            writePS(" {\n");

            // TODO: anitize interface
            gfx->display(&charProcs->val_at(i));
            if (t3String) {
                if (t3Cacheable) {
                    buf = GString::format(
                        "{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g} "
                        "setcachedevice\n",
                        t3WX, t3WY, t3LLX, t3LLY, t3URX, t3URY);
                } else {
                    buf = GString::format("{0:.6g} {1:.6g} setcharwidth\n", t3WX,
                                          t3WY);
                }
                (*outputFunc)(outputStream, buf->c_str(), buf->getLength());
                delete buf;
                (*outputFunc)(outputStream, t3String->c_str(),
                              t3String->getLength());
                delete t3String;
                t3String = NULL;
            }
            if (t3NeedsRestore) {
                (*outputFunc)(outputStream, "Q\n", 2);
            }
            writePS("} def\n");
        }
        inType3Char = false;
        writePS("end\n");
    }
    writePS("currentdict end\n");
    writePSFmt("/{0:t} exch definefont pop\n", psName);

    // ending comment
    writePS("%%EndResource\n");

    ff = new PSFontFileInfo(psName, font->getType(), psFontFileEmbedded);
    fontFileInfo->add(ff->psName, ff);
    return ff;
}

// Make a unique PS font name, based on the names given in the PDF
// font object, and an object ID (font file object for
GString *PSOutputDev::makePSFontName(GfxFont *font, Ref *id)
{
    GString *psName, *s;

    if ((s = font->getEmbeddedFontName())) {
        psName = filterPSName(s);
        if (!fontFileInfo->lookup(psName)) {
            return psName;
        }
        delete psName;
    }
    if ((s = font->as_name())) {
        psName = filterPSName(s);
        if (!fontFileInfo->lookup(psName)) {
            return psName;
        }
        delete psName;
    }
    psName = GString::format("FF{0:d}_{1:d}", id->num, id->gen);
    if ((s = font->getEmbeddedFontName())) {
        s = filterPSName(s);
        psName->append(1UL, '_');
        psName->append(*s);
        delete s;
    } else if ((s = font->as_name())) {
        s = filterPSName(s);
        psName->append(1UL, '_');
        psName->append(*s);
        delete s;
    }
    return psName;
}

void PSOutputDev::setupImages(Dict *resDict)
{
    Object xObjDict, subtypeObj, maskObj, maskRef;
    Ref    imgID;
    int    i, j;

    if (!(mode == psModeForm || inType3Char || preload)) {
        return;
    }

    xObjDict = resolve((*resDict)["XObject"]);
    if (xObjDict.is_dict()) {
        for (i = 0; i < xObjDict.as_dict().size(); ++i) {
            auto &xObjRef = xObjDict.val_at(i);
            auto &xObj = xObjDict.val_at(i);
            if (xObj.is_stream()) {
                subtypeObj = resolve((*xObj.streamGetDict())["Subtype"]);
                if (subtypeObj.is_name("Image")) {
                    if (xObjRef.is_ref()) {
                        imgID = xObjRef.as_ref();
                        for (j = 0; j < imgIDLen; ++j) {
                            if (imgIDs[j].num == imgID.num &&
                                imgIDs[j].gen == imgID.gen) {
                                break;
                            }
                        }
                        if (j == imgIDLen) {
                            if (imgIDLen >= imgIDSize) {
                                if (imgIDSize == 0) {
                                    imgIDSize = 64;
                                } else {
                                    imgIDSize *= 2;
                                }
                                imgIDs = (Ref *)reallocarray(imgIDs, imgIDSize,
                                                             sizeof(Ref));
                            }
                            imgIDs[imgIDLen++] = imgID;
                            setupImage(imgID, xObj.as_stream(), false);
                            if (level >= psLevel3 &&
                                (maskObj =
                                     resolve((*xObj.streamGetDict())["Mask"]))
                                    .is_stream()) {
                                setupImage(imgID, maskObj.as_stream(), true);
                            }
                        }
                    } else {
                        error(errSyntaxError, -1,
                              "Image in resource dict is not an indirect "
                              "reference");
                    }
                }
            }
        }
    }
}

void PSOutputDev::setupImage(Ref id, Stream *str, bool mask)
{
    bool     useLZW, useRLE, useCompressed, useASCIIHex;
    GString *s;
    int      c;
    int      size, line, col, i;

    // filters
    //~ this does not correctly handle the DeviceN color space
    //~   -- need to use DeviceNRecoder
    if (level < psLevel2) {
        useLZW = useRLE = false;
        useCompressed = false;
        useASCIIHex = true;
    } else {
        if (globalParams->getPSUncompressPreloadedImages()) {
            useLZW = useRLE = false;
            useCompressed = false;
        } else {
            s = str->getPSFilter(level < psLevel3 ? 2 : 3, "");
            if (s) {
                useLZW = useRLE = false;
                useCompressed = true;
                delete s;
            } else {
                if (globalParams->getPSLZW()) {
                    useLZW = true;
                    useRLE = false;
                } else {
                    useRLE = true;
                    useLZW = false;
                }
                useCompressed = false;
            }
        }
        useASCIIHex = globalParams->getPSASCIIHex();
    }
    if (useCompressed) {
        str = str->getUndecodedStream();
    }
    if (useLZW) {
        str = new LZWEncoder(str);
    } else if (useRLE) {
        str = new RunLengthEncoder(str);
    }
    if (useASCIIHex) {
        str = new ASCIIHexEncoder(str);
    } else {
        str = new ASCII85Encoder(str);
    }

    // compute image data size
    str->reset();
    col = size = 0;
    do {
        do {
            c = str->get();
        } while (c == '\n' || c == '\r');
        if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
            break;
        }
        if (c == 'z') {
            ++col;
        } else {
            ++col;
            for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i) {
                do {
                    c = str->get();
                } while (c == '\n' || c == '\r');
                if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
                    break;
                }
                ++col;
            }
        }
        if (col > 225) {
            ++size;
            col = 0;
        }
    } while (c != (useASCIIHex ? '>' : '~') && c != EOF);
    // add one entry for the final line of data; add another entry
    // because the LZWDecode/RunLengthDecode filter may read past the end
    ++size;
    if (useLZW || useRLE) {
        ++size;
    }
    writePSFmt("{0:d} array dup /{1:s}Data_{2:d}_{3:d} exch def\n", size,
               mask ? "Mask" : "Im", id.num, id.gen);
    str->close();

    // write the data into the array
    str->reset();
    line = col = 0;
    writePS((char *)(useASCIIHex ? "dup 0 <" : "dup 0 <~"));
    do {
        do {
            c = str->get();
        } while (c == '\n' || c == '\r');
        if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
            break;
        }
        if (c == 'z') {
            writePSChar(c);
            ++col;
        } else {
            writePSChar(c);
            ++col;
            for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i) {
                do {
                    c = str->get();
                } while (c == '\n' || c == '\r');
                if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
                    break;
                }
                writePSChar(c);
                ++col;
            }
        }
        // each line is: "dup nnnnn <~...data...~> put<eol>"
        // so max data length = 255 - 20 = 235
        // chunks are 1 or 4 bytes each, so we have to stop at 232
        // but make it 225 just to be safe
        if (col > 225) {
            writePS((char *)(useASCIIHex ? "> put\n" : "~> put\n"));
            ++line;
            writePSFmt((char *)(useASCIIHex ? "dup {0:d} <" : "dup {0:d} <~"),
                       line);
            col = 0;
        }
    } while (c != (useASCIIHex ? '>' : '~') && c != EOF);
    writePS((char *)(useASCIIHex ? "> put\n" : "~> put\n"));
    if (useLZW || useRLE) {
        ++line;
        writePSFmt("{0:d} <> put\n", line);
    } else {
        writePS("pop\n");
    }
    str->close();

    delete str;
}

void PSOutputDev::setupForms(Dict *resDict)
{
    Object xObjDict, subtypeObj;
    int    i;

    if (!preload) {
        return;
    }

    xObjDict = resolve((*resDict)["XObject"]);
    if (xObjDict.is_dict()) {
        for (i = 0; i < xObjDict.as_dict().size(); ++i) {
            auto &xObjRef = xObjDict.val_at(i);
            auto &xObj = xObjDict.val_at(i);
            if (xObj.is_stream()) {
                subtypeObj = resolve((*xObj.streamGetDict())["Subtype"]);
                if (subtypeObj.is_name("Form")) {
                    if (xObjRef.is_ref()) {
                        setupForm(&xObjRef, &xObj);
                    } else {
                        error(errSyntaxError, -1,
                              "Form in resource dict is not an indirect "
                              "reference");
                    }
                }
            }
        }
    }
}

void PSOutputDev::setupForm(Object *strRef, Object *strObj)
{
    Dict *       dict, *resDict;
    Object       matrixObj, bboxObj, resObj, obj1;
    double       m[6], bbox[4];
    PDFRectangle box;
    int          i;

    // check if form is already defined
    for (i = 0; i < formIDLen; ++i) {
        if (formIDs[i].num == strRef->getRefNum() &&
            formIDs[i].gen == strRef->getRefGen()) {
            return;
        }
    }

    // add entry to formIDs list
    if (formIDLen >= formIDSize) {
        if (formIDSize == 0) {
            formIDSize = 64;
        } else {
            formIDSize *= 2;
        }
        formIDs = (Ref *)reallocarray(formIDs, formIDSize, sizeof(Ref));
    }
    formIDs[formIDLen++] = strRef->as_ref();

    dict = strObj->streamGetDict();

    // get bounding box
    bboxObj = resolve((*dict)["BBox"]);
    if (!bboxObj.is_array()) {
        error(errSyntaxError, -1, "Bad form bounding box");
        return;
    }
    for (i = 0; i < 4; ++i) {
        obj1 = resolve(bboxObj[i]);
        bbox[i] = obj1.as_num();
    }

    // get matrix
    matrixObj = resolve((*dict)["Matrix"]);
    if (matrixObj.is_array()) {
        for (i = 0; i < 6; ++i) {
            obj1 = resolve(matrixObj[i]);
            m[i] = obj1.as_num();
        }
    } else {
        m[0] = 1;
        m[1] = 0;
        m[2] = 0;
        m[3] = 1;
        m[4] = 0;
        m[5] = 0;
    }

    // get resources
    resObj = resolve((*dict)["Resources"]);
    resDict = resObj.is_dict() ? &resObj.as_dict() : (Dict *)NULL;

    writePSFmt("/f_{0:d}_{1:d} {{\n", strRef->getRefNum(), strRef->getRefGen());
    writePS("q\n");
    writePSFmt("[{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] cm\n", m[0],
               m[1], m[2], m[3], m[4], m[5]);

    box.x1 = bbox[0];
    box.y1 = bbox[1];
    box.x2 = bbox[2];
    box.y2 = bbox[3];

    auto gfx = std::make_unique< Gfx >(doc, this, resDict, &box, &box);
    gfx->display(strRef);

    writePS("Q\n");
    writePS("} def\n");
}

bool PSOutputDev::checkPageSlice(Page *page, double hDPI, double vDPI,
                                 int rotateA, bool useMediaBox, bool crop,
                                 int sliceX, int sliceY, int sliceW, int sliceH,
                                 bool printing, bool (*abortCheckCbk)(void *data),
                                 void *abortCheckCbkData)
{
    bool             mono;
    bool             useLZW;
    double           dpi;
    SplashOutputDev *splashOut;
    SplashColor      paperColor;
    PDFRectangle     box;
    GfxState *       state;
    SplashBitmap *   bitmap;
    Stream *         str0, *str;
    Object           obj;
    unsigned char *  p;
    unsigned char    col[4];
    char             buf[4096];
    double           hDPI2, vDPI2;
    double           m0, m1, m2, m3, m4, m5;
    int              nStripes, stripeH, stripeY;
    int              w, h, x, y, comp, i, n;

    // get the rasterization parameters
    dpi = globalParams->getPSRasterResolution();
    mono = globalParams->getPSRasterMono();
    useLZW = globalParams->getPSLZW();

    // start the PS page
    page->makeBox(dpi, dpi, rotateA, useMediaBox, false, sliceX, sliceY, sliceW,
                  sliceH, &box, &crop);
    rotateA += page->getRotate();
    if (rotateA >= 360) {
        rotateA -= 360;
    } else if (rotateA < 0) {
        rotateA += 360;
    }
    state = new GfxState(dpi, dpi, &box, rotateA, false);
    startPage(page->as_num(), state);
    delete state;

    // set up the SplashOutputDev
    if (mono || level == psLevel1) {
        paperColor[0] = 0xff;
        splashOut = new SplashOutputDev(splashModeMono8, 1, false, paperColor,
                                        false,
                                        globalParams->getAntialiasPrinting());
#if SPLASH_CMYK
    } else if (level == psLevel1Sep) {
        paperColor[0] = paperColor[1] = paperColor[2] = paperColor[3] = 0;
        splashOut = new SplashOutputDev(splashModeCMYK8, 1, false, paperColor,
                                        false,
                                        globalParams->getAntialiasPrinting());
#endif
    } else {
        paperColor[0] = paperColor[1] = paperColor[2] = 0xff;
        splashOut = new SplashOutputDev(splashModeRGB8, 1, false, paperColor,
                                        false,
                                        globalParams->getAntialiasPrinting());
    }
    splashOut->startDoc(xref);

    // break the page into stripes
    hDPI2 = xScale * dpi;
    vDPI2 = yScale * dpi;
    if (sliceW < 0 || sliceH < 0) {
        if (useMediaBox) {
            box = *page->getMediaBox();
        } else {
            box = *page->getCropBox();
        }
        sliceX = sliceY = 0;
        sliceW = (int)((box.x2 - box.x1) * hDPI2 / 72.0);
        sliceH = (int)((box.y2 - box.y1) * vDPI2 / 72.0);
    }
    nStripes = (int)ceil(((double)sliceW * (double)sliceH) /
                         (double)globalParams->getPSRasterSliceSize());
    stripeH = (sliceH + nStripes - 1) / nStripes;

    // render the stripes
    for (stripeY = sliceY; stripeY < sliceH; stripeY += stripeH) {
        // rasterize a stripe
        page->makeBox(hDPI2, vDPI2, 0, useMediaBox, false, sliceX, stripeY,
                      sliceW, stripeH, &box, &crop);
        m0 = box.x2 - box.x1;
        m1 = 0;
        m2 = 0;
        m3 = box.y2 - box.y1;
        m4 = box.x1;
        m5 = box.y1;
        page->displaySlice(splashOut, hDPI2, vDPI2,
                           (360 - page->getRotate()) % 360, useMediaBox, crop,
                           sliceX, stripeY, sliceW, stripeH, printing,
                           abortCheckCbk, abortCheckCbkData);

        // draw the rasterized image
        bitmap = splashOut->getBitmap();
        w = bitmap->getWidth();
        h = bitmap->getHeight();
        writePS("gsave\n");
        writePSFmt("[{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] concat\n",
                   m0, m1, m2, m3, m4, m5);
        switch (level) {
        case psLevel1:
            writePSFmt("{0:d} {1:d} 8 [{2:d} 0 0 {3:d} 0 {4:d}] pdfIm1\n", w, h,
                       w, -h, h);
            p = bitmap->getDataPtr() + (h - 1) * bitmap->getRowSize();
            i = 0;
            for (y = 0; y < h; ++y) {
                for (x = 0; x < w; ++x) {
                    writePSFmt("{0:02x}", *p++);
                    if (++i == 32) {
                        writePSChar('\n');
                        i = 0;
                    }
                }
            }
            if (i != 0) {
                writePSChar('\n');
            }
            break;
        case psLevel1Sep:
            writePSFmt("{0:d} {1:d} 8 [{2:d} 0 0 {3:d} 0 {4:d}] pdfIm1Sep\n", w,
                       h, w, -h, h);
            p = bitmap->getDataPtr() + (h - 1) * bitmap->getRowSize();
            i = 0;
            col[0] = col[1] = col[2] = col[3] = 0;
            for (y = 0; y < h; ++y) {
                for (comp = 0; comp < 4; ++comp) {
                    for (x = 0; x < w; ++x) {
                        writePSFmt("{0:02x}", p[4 * x + comp]);
                        col[comp] |= p[4 * x + comp];
                        if (++i == 32) {
                            writePSChar('\n');
                            i = 0;
                        }
                    }
                }
                p -= bitmap->getRowSize();
            }
            if (i != 0) {
                writePSChar('\n');
            }
            if (col[0]) {
                processColors |= psProcessCyan;
            }
            if (col[1]) {
                processColors |= psProcessMagenta;
            }
            if (col[2]) {
                processColors |= psProcessYellow;
            }
            if (col[3]) {
                processColors |= psProcessBlack;
            }
            break;
        case psLevel2:
        case psLevel2Sep:
        case psLevel3:
        case psLevel3Sep:
            if (mono) {
                writePS("/DeviceGray setcolorspace\n");
            } else {
                writePS("/DeviceRGB setcolorspace\n");
            }
            writePS("<<\n  /ImageType 1\n");
            writePSFmt("  /Width {0:d}\n", bitmap->getWidth());
            writePSFmt("  /Height {0:d}\n", bitmap->getHeight());
            writePSFmt("  /ImageMatrix [{0:d} 0 0 {1:d} 0 {2:d}]\n", w, -h, h);
            writePS("  /BitsPerComponent 8\n");
            if (mono) {
                writePS("  /Decode [0 1]\n");
            } else {
                writePS("  /Decode [0 1 0 1 0 1]\n");
            }
            writePS("  /DataSource currentfile\n");
            if (globalParams->getPSASCIIHex()) {
                writePS("    /ASCIIHexDecode filter\n");
            } else {
                writePS("    /ASCII85Decode filter\n");
            }
            if (useLZW) {
                writePS("    /LZWDecode filter\n");
            } else {
                writePS("    /RunLengthDecode filter\n");
            }
            writePS(">>\n");
            writePS("image\n");
            obj = {};
            p = bitmap->getDataPtr() + (h - 1) * bitmap->getRowSize();
            str0 = new MemStream((char *)p, 0, w * h * (mono ? 1 : 3), &obj);
            if (useLZW) {
                str = new LZWEncoder(str0);
            } else {
                str = new RunLengthEncoder(str0);
            }
            if (globalParams->getPSASCIIHex()) {
                str = new ASCIIHexEncoder(str);
            } else {
                str = new ASCII85Encoder(str);
            }
            str->reset();
            while ((n = str->readblock(buf, sizeof(buf))) > 0) {
                writePSBlock(buf, n);
            }
            str->close();
            delete str;
            delete str0;
            writePSChar('\n');
            processColors |= mono ? psProcessBlack : psProcessCMYK;
            break;
        }
        writePS("grestore\n");
    }

    delete splashOut;

    // finish the PS page
    endPage();

    return false;
}

void PSOutputDev::startPage(int pageNum, GfxState *state)
{
    Page *   page;
    int      x1, y1, x2, y2, width, height, t;
    int      imgWidth, imgHeight, imgWidth2, imgHeight2;
    bool     landscape;
    GString *s;

    if (mode == psModePS) {
        writePSFmt("%%Page: {0:d} {1:d}\n", pageNum, seqPage);
        if (paperMatch) {
            page = doc->getCatalog()->getPage(pageNum);
            imgLLX = imgLLY = 0;
            if (globalParams->getPSUseCropBoxAsPage()) {
                imgURX = (int)ceil(page->getCropWidth());
                imgURY = (int)ceil(page->getCropHeight());
            } else {
                imgURX = (int)ceil(page->getMediaWidth());
                imgURY = (int)ceil(page->getMediaHeight());
            }
            if (state->getRotate() == 90 || state->getRotate() == 270) {
                t = imgURX;
                imgURX = imgURY;
                imgURY = t;
            }
            writePSFmt("%%PageMedia: {0:d}x{1:d}\n", imgURX, imgURY);
            writePSFmt("%%PageBoundingBox: 0 0 {0:d} {1:d}\n", imgURX, imgURY);
        }
        writePS("%%BeginPageSetup\n");
    }
    if (mode != psModeForm) {
        writePS("xpdf begin\n");
    }

    // underlays
    if (underlayCbk) {
        (*underlayCbk)(this, underlayCbkData);
    }
    if (overlayCbk) {
        saveState(NULL);
    }

    switch (mode) {
    case psModePS:
        // rotate, translate, and scale page
        imgWidth = imgURX - imgLLX;
        imgHeight = imgURY - imgLLY;
        x1 = (int)floor(state->getX1());
        y1 = (int)floor(state->getY1());
        x2 = (int)ceil(state->getX2());
        y2 = (int)ceil(state->getY2());
        width = x2 - x1;
        height = y2 - y1;
        tx = ty = 0;
        // rotation and portrait/landscape mode
        if (paperMatch) {
            rotate = (360 - state->getRotate()) % 360;
            landscape = false;
        } else if (rotate0 >= 0) {
            rotate = (360 - rotate0) % 360;
            landscape = false;
        } else {
            rotate = (360 - state->getRotate()) % 360;
            if (rotate == 0 || rotate == 180) {
                if ((width < height && imgWidth > imgHeight &&
                     height > imgHeight) ||
                    (width > height && imgWidth < imgHeight &&
                     width > imgWidth)) {
                    rotate += 90;
                    landscape = true;
                } else {
                    landscape = false;
                }
            } else { // rotate == 90 || rotate == 270
                if ((height < width && imgWidth > imgHeight &&
                     width > imgHeight) ||
                    (height > width && imgWidth < imgHeight &&
                     height > imgWidth)) {
                    rotate = 270 - rotate;
                    landscape = true;
                } else {
                    landscape = false;
                }
            }
        }
        writePSFmt("%%PageOrientation: {0:s}\n",
                   landscape ? "Landscape" : "Portrait");
        if (paperMatch) {
            writePSFmt("{0:d} {1:d} pdfSetupPaper\n", imgURX, imgURY);
        }
        writePS("pdfStartPage\n");
        if (rotate == 0) {
            imgWidth2 = imgWidth;
            imgHeight2 = imgHeight;
        } else if (rotate == 90) {
            writePS("90 rotate\n");
            ty = -imgWidth;
            imgWidth2 = imgHeight;
            imgHeight2 = imgWidth;
        } else if (rotate == 180) {
            writePS("180 rotate\n");
            imgWidth2 = imgWidth;
            imgHeight2 = imgHeight;
            tx = -imgWidth;
            ty = -imgHeight;
        } else { // rotate == 270
            writePS("270 rotate\n");
            tx = -imgHeight;
            imgWidth2 = imgHeight;
            imgHeight2 = imgWidth;
        }
        // shrink or expand
        if (xScale0 > 0 && yScale0 > 0) {
            xScale = xScale0;
            yScale = yScale0;
        } else if ((globalParams->getPSShrinkLarger() &&
                    (width > imgWidth2 || height > imgHeight2)) ||
                   (globalParams->getPSExpandSmaller() &&
                    (width < imgWidth2 && height < imgHeight2))) {
            xScale = (double)imgWidth2 / (double)width;
            yScale = (double)imgHeight2 / (double)height;
            if (yScale < xScale) {
                xScale = yScale;
            } else {
                yScale = xScale;
            }
        } else {
            xScale = yScale = 1;
        }
        // deal with odd bounding boxes or clipping
        if (clipLLX0 < clipURX0 && clipLLY0 < clipURY0) {
            tx -= xScale * clipLLX0;
            ty -= yScale * clipLLY0;
        } else {
            tx -= xScale * x1;
            ty -= yScale * y1;
        }
        // center
        if (tx0 >= 0 && ty0 >= 0) {
            tx += (rotate == 0 || rotate == 180) ? tx0 : ty0;
            ty += (rotate == 0 || rotate == 180) ? ty0 : -tx0;
        } else if (globalParams->getPSCenter()) {
            if (clipLLX0 < clipURX0 && clipLLY0 < clipURY0) {
                tx += (imgWidth2 - xScale * (clipURX0 - clipLLX0)) / 2;
                ty += (imgHeight2 - yScale * (clipURY0 - clipLLY0)) / 2;
            } else {
                tx += (imgWidth2 - xScale * width) / 2;
                ty += (imgHeight2 - yScale * height) / 2;
            }
        }
        tx += (rotate == 0 || rotate == 180) ? imgLLX : imgLLY;
        ty += (rotate == 0 || rotate == 180) ? imgLLY : -imgLLX;
        if (tx != 0 || ty != 0) {
            writePSFmt("{0:.6g} {1:.6g} translate\n", tx, ty);
        }
        if (xScale != 1 || yScale != 1) {
            writePSFmt("{0:.4f} {1:.4f} scale\n", xScale, yScale);
        }
        if (clipLLX0 < clipURX0 && clipLLY0 < clipURY0) {
            writePSFmt("{0:.6g} {1:.6g} {2:.6g} {3:.6g} re W\n", clipLLX0,
                       clipLLY0, clipURX0 - clipLLX0, clipURY0 - clipLLY0);
        } else {
            writePSFmt("{0:d} {1:d} {2:d} {3:d} re W\n", x1, y1, x2 - x1,
                       y2 - y1);
        }

        ++seqPage;
        break;

    case psModeEPS:
        writePS("pdfStartPage\n");
        tx = ty = 0;
        rotate = (360 - state->getRotate()) % 360;
        if (rotate == 0) {
        } else if (rotate == 90) {
            writePS("90 rotate\n");
            tx = -epsX1;
            ty = -epsY2;
        } else if (rotate == 180) {
            writePS("180 rotate\n");
            tx = -(epsX1 + epsX2);
            ty = -(epsY1 + epsY2);
        } else { // rotate == 270
            writePS("270 rotate\n");
            tx = -epsX2;
            ty = -epsY1;
        }
        if (tx != 0 || ty != 0) {
            writePSFmt("{0:.6g} {1:.6g} translate\n", tx, ty);
        }
        xScale = yScale = 1;
        break;

    case psModeForm:
        writePS("/PaintProc {\n");
        writePS("begin xpdf begin\n");
        writePS("pdfStartPage\n");
        tx = ty = 0;
        xScale = yScale = 1;
        rotate = 0;
        break;
    }

    if (customCodeCbk) {
        if ((s = (*customCodeCbk)(this, psOutCustomPageSetup, pageNum,
                                  customCodeCbkData))) {
            writePS(s->c_str());
            delete s;
        }
    }

    if (mode == psModePS) {
        writePS("%%EndPageSetup\n");
    }
}

void PSOutputDev::endPage()
{
    if (overlayCbk) {
        restoreState(NULL);
        (*overlayCbk)(this, overlayCbkData);
    }

    if (mode == psModeForm) {
        writePS("pdfEndPage\n");
        writePS("end end\n");
        writePS("} def\n");
        writePS("end end\n");
    } else {
        if (!manualCtrl) {
            writePS("showpage\n");
        }
        writePS("%%PageTrailer\n");
        writePageTrailer();
        writePS("end\n");
    }
}

void PSOutputDev::saveState(GfxState *state)
{
    writePS("q\n");
    ++numSaves;
}

void PSOutputDev::restoreState(GfxState *state)
{
    writePS("Q\n");
    --numSaves;
}

void PSOutputDev::updateCTM(GfxState *state, double m11, double m12, double m21,
                            double m22, double m31, double m32)
{
    if (fabs(m11 * m22 - m12 * m21) < 0.00001) {
        // avoid a singular (or close-to-singular) matrix
        writePSFmt("[0.00001 0 0 0.00001 {0:.6g} {1:.6g}] Tm\n", m31, m32);
    } else {
        writePSFmt("[{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] cm\n", m11,
                   m12, m21, m22, m31, m32);
    }
}

void PSOutputDev::updateLineDash(GfxState *state)
{
    double *dash;
    double  start;
    int     length, i;

    state->getLineDash(&dash, &length, &start);
    writePS("[");
    for (i = 0; i < length; ++i) {
        writePSFmt("{0:.6g}{1:w}", dash[i] < 0 ? 0 : dash[i],
                   (i == length - 1) ? 0 : 1);
    }
    writePSFmt("] {0:.6g} d\n", start);
}

void PSOutputDev::updateFlatness(GfxState *state)
{
    writePSFmt("{0:d} i\n", state->getFlatness());
}

void PSOutputDev::updateLineJoin(GfxState *state)
{
    writePSFmt("{0:d} j\n", state->getLineJoin());
}

void PSOutputDev::updateLineCap(GfxState *state)
{
    writePSFmt("{0:d} J\n", state->getLineCap());
}

void PSOutputDev::updateMiterLimit(GfxState *state)
{
    writePSFmt("{0:.4g} M\n", state->getMiterLimit());
}

void PSOutputDev::updateLineWidth(GfxState *state)
{
    writePSFmt("{0:.6g} w\n", state->getLineWidth());
}

void PSOutputDev::updateFillColorSpace(GfxState *state)
{
    switch (level) {
    case psLevel1:
    case psLevel1Sep:
        break;
    case psLevel2:
    case psLevel3:
        if (state->getFillColorSpace()->getMode() != csPattern) {
            dumpColorSpaceL2(state->getFillColorSpace(), true, false, false);
            writePS(" cs\n");
        }
        break;
    case psLevel2Sep:
    case psLevel3Sep:
        break;
    }
}

void PSOutputDev::updateStrokeColorSpace(GfxState *state)
{
    switch (level) {
    case psLevel1:
    case psLevel1Sep:
        break;
    case psLevel2:
    case psLevel3:
        if (state->getStrokeColorSpace()->getMode() != csPattern) {
            dumpColorSpaceL2(state->getStrokeColorSpace(), true, false, false);
            writePS(" CS\n");
        }
        break;
    case psLevel2Sep:
    case psLevel3Sep:
        break;
    }
}

void PSOutputDev::updateFillColor(GfxState *state)
{
    GfxColor                 color;
    GfxColor *               colorPtr;
    GfxGray                  gray;
    GfxCMYK                  cmyk;
    GfxSeparationColorSpace *sepCS;
    double                   c, m, y, k;
    int                      i;

    switch (level) {
    case psLevel1:
        state->getFillGray(&gray);
        writePSFmt("{0:.4g} g\n", xpdf::to_double(gray.x));
        break;
    case psLevel1Sep:
        state->getFillCMYK(&cmyk);
        c = xpdf::to_double(cmyk.c);
        m = xpdf::to_double(cmyk.m);
        y = xpdf::to_double(cmyk.y);
        k = xpdf::to_double(cmyk.k);
        writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} k\n", c, m, y, k);
        addProcessColor(c, m, y, k);
        break;
    case psLevel2:
    case psLevel3:
        if (state->getFillColorSpace()->getMode() != csPattern) {
            colorPtr = state->getFillColor();
            writePS("[");
            for (i = 0; i < state->getFillColorSpace()->getNComps(); ++i) {
                if (i > 0) {
                    writePS(" ");
                }
                writePSFmt("{0:.4g}", xpdf::to_double(colorPtr->c[i]));
            }
            writePS("] sc\n");
        }
        break;
    case psLevel2Sep:
    case psLevel3Sep:
        if (state->getFillColorSpace()->getMode() == csSeparation) {
            sepCS = (GfxSeparationColorSpace *)state->getFillColorSpace();
            color.c[0] = XPDF_FIXED_POINT_ONE;
            sepCS->getCMYK(&color, &cmyk);
            writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} {4:.4g} ({5:t}) ck\n",
                       xpdf::to_double(state->getFillColor()->c[0]),
                       xpdf::to_double(cmyk.c), xpdf::to_double(cmyk.m),
                       xpdf::to_double(cmyk.y), xpdf::to_double(cmyk.k),
                       sepCS->as_name());
            addCustomColor(sepCS);
        } else {
            state->getFillCMYK(&cmyk);
            c = xpdf::to_double(cmyk.c);
            m = xpdf::to_double(cmyk.m);
            y = xpdf::to_double(cmyk.y);
            k = xpdf::to_double(cmyk.k);
            writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} k\n", c, m, y, k);
            addProcessColor(c, m, y, k);
        }
        break;
    }
    t3Cacheable = false;
}

void PSOutputDev::updateStrokeColor(GfxState *state)
{
    GfxColor                 color;
    GfxColor *               colorPtr;
    GfxGray                  gray;
    GfxCMYK                  cmyk;
    GfxSeparationColorSpace *sepCS;
    double                   c, m, y, k;
    int                      i;

    switch (level) {
    case psLevel1:
        state->getStrokeGray(&gray);
        writePSFmt("{0:.4g} G\n", xpdf::to_double(gray.x));
        break;
    case psLevel1Sep:
        state->getStrokeCMYK(&cmyk);
        c = xpdf::to_double(cmyk.c);
        m = xpdf::to_double(cmyk.m);
        y = xpdf::to_double(cmyk.y);
        k = xpdf::to_double(cmyk.k);
        writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} K\n", c, m, y, k);
        addProcessColor(c, m, y, k);
        break;
    case psLevel2:
    case psLevel3:
        if (state->getStrokeColorSpace()->getMode() != csPattern) {
            colorPtr = state->getStrokeColor();
            writePS("[");
            for (i = 0; i < state->getStrokeColorSpace()->getNComps(); ++i) {
                if (i > 0) {
                    writePS(" ");
                }
                writePSFmt("{0:.4g}", xpdf::to_double(colorPtr->c[i]));
            }
            writePS("] SC\n");
        }
        break;
    case psLevel2Sep:
    case psLevel3Sep:
        if (state->getStrokeColorSpace()->getMode() == csSeparation) {
            sepCS = (GfxSeparationColorSpace *)state->getStrokeColorSpace();
            color.c[0] = XPDF_FIXED_POINT_ONE;
            sepCS->getCMYK(&color, &cmyk);
            writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} {4:.4g} ({5:t}) CK\n",
                       xpdf::to_double(state->getStrokeColor()->c[0]),
                       xpdf::to_double(cmyk.c), xpdf::to_double(cmyk.m),
                       xpdf::to_double(cmyk.y), xpdf::to_double(cmyk.k),
                       sepCS->as_name());
            addCustomColor(sepCS);
        } else {
            state->getStrokeCMYK(&cmyk);
            c = xpdf::to_double(cmyk.c);
            m = xpdf::to_double(cmyk.m);
            y = xpdf::to_double(cmyk.y);
            k = xpdf::to_double(cmyk.k);
            writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} K\n", c, m, y, k);
            addProcessColor(c, m, y, k);
        }
        break;
    }
    t3Cacheable = false;
}

void PSOutputDev::addProcessColor(double c, double m, double y, double k)
{
    if (c > 0) {
        processColors |= psProcessCyan;
    }
    if (m > 0) {
        processColors |= psProcessMagenta;
    }
    if (y > 0) {
        processColors |= psProcessYellow;
    }
    if (k > 0) {
        processColors |= psProcessBlack;
    }
}

void PSOutputDev::addCustomColor(GfxSeparationColorSpace *sepCS)
{
    PSOutCustomColor *cc;
    GfxColor          color;
    GfxCMYK           cmyk;

    for (cc = customColors; cc; cc = cc->next) {
        if (!cc->name->cmp(sepCS->as_name())) {
            return;
        }
    }
    color.c[0] = XPDF_FIXED_POINT_ONE;
    sepCS->getCMYK(&color, &cmyk);
    cc = new PSOutCustomColor(xpdf::to_double(cmyk.c), xpdf::to_double(cmyk.m),
                              xpdf::to_double(cmyk.y), xpdf::to_double(cmyk.k),
                              sepCS->as_name()->copy());
    cc->next = customColors;
    customColors = cc;
}

void PSOutputDev::updateFillOverprint(GfxState *state)
{
    if (level >= psLevel2) {
        writePSFmt("{0:s} op\n", state->getFillOverprint() ? "true" : "false");
    }
}

void PSOutputDev::updateStrokeOverprint(GfxState *state)
{
    if (level >= psLevel2) {
        writePSFmt("{0:s} OP\n", state->getStrokeOverprint() ? "true" : "false");
    }
}

void PSOutputDev::updateTransfer(GfxState *state)
{
    Function *funcs;
    int       i;

    funcs = state->getTransfer();

    if (funcs[0] && funcs[1] && funcs[2] && funcs[3]) {
        if (level >= psLevel2) {
            for (i = 0; i < 4; ++i) {
                cvtFunction(funcs[i]);
            }
            writePS("setcolortransfer\n");
        } else {
            cvtFunction(funcs[3]);
            writePS("settransfer\n");
        }
    } else if (funcs[0]) {
        cvtFunction(funcs[0]);
        writePS("settransfer\n");
    } else {
        writePS("{} settransfer\n");
    }
}

void PSOutputDev::updateFont(GfxState *state)
{
    if (state->getFont()) {
        writePSFmt("/F{0:d}_{1:d} {2:.6g} Tf\n", state->getFont()->getID()->num,
                   state->getFont()->getID()->gen,
                   fabs(state->getFontSize()) < 0.0001 ? 0.0001 :
                                                         state->getFontSize());
    }
}

void PSOutputDev::updateTextMat(GfxState *state)
{
    double *mat;

    mat = state->getTextMat();
    if (fabs(mat[0] * mat[3] - mat[1] * mat[2]) < 0.00001) {
        // avoid a singular (or close-to-singular) matrix
        writePSFmt("[0.00001 0 0 0.00001 {0:.6g} {1:.6g}] Tm\n", mat[4], mat[5]);
    } else {
        writePSFmt("[{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] Tm\n",
                   mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
    }
}

void PSOutputDev::updateCharSpace(GfxState *state)
{
    writePSFmt("{0:.6g} Tc\n", state->getCharSpace());
}

void PSOutputDev::updateRender(GfxState *state)
{
    int rm;

    rm = state->getRender();
    writePSFmt("{0:d} Tr\n", rm);
    rm &= 3;
    if (rm != 0 && rm != 3) {
        t3Cacheable = false;
    }
}

void PSOutputDev::updateRise(GfxState *state)
{
    writePSFmt("{0:.6g} Ts\n", state->getRise());
}

void PSOutputDev::updateWordSpace(GfxState *state)
{
    writePSFmt("{0:.6g} Tw\n", state->getWordSpace());
}

void PSOutputDev::updateHorizScaling(GfxState *state)
{
    double h;

    h = state->getHorizScaling();
    if (fabs(h) < 0.01) {
        h = 0.01;
    }
    writePSFmt("{0:.6g} Tz\n", h);
}

void PSOutputDev::updateTextPos(GfxState *state)
{
    writePSFmt("{0:.6g} {1:.6g} Td\n", state->getLineX(), state->getLineY());
}

void PSOutputDev::updateTextShift(GfxState *state, double shift)
{
    if (state->getFont()->getWMode()) {
        writePSFmt("{0:.6g} TJmV\n", shift);
    } else {
        writePSFmt("{0:.6g} TJm\n", shift);
    }
}

void PSOutputDev::saveTextPos(GfxState *state)
{
    writePS("currentpoint\n");
}

void PSOutputDev::restoreTextPos(GfxState *state)
{
    writePS("m\n");
}

void PSOutputDev::stroke(GfxState *state)
{
    doPath(state->getPath());
    if (inType3Char && t3FillColorOnly) {
        // if we're constructing a cacheable Type 3 glyph, we need to do
        // everything in the fill color
        writePS("Sf\n");
    } else {
        writePS("S\n");
    }
}

void PSOutputDev::fill(GfxState *state)
{
    doPath(state->getPath());
    writePS("f\n");
}

void PSOutputDev::eoFill(GfxState *state)
{
    doPath(state->getPath());
    writePS("f*\n");
}

void PSOutputDev::tilingPatternFill(GfxState *state, Gfx *gfx, Object *strRef,
                                    int paintType, Dict *resDict, double *mat,
                                    double *bbox, int x0, int y0, int x1, int y1,
                                    double xStep, double yStep)
{
    PDFRectangle box;

    // define a Type 3 font
    writePS("8 dict begin\n");
    writePS("/FontType 3 def\n");
    writePS("/FontMatrix [1 0 0 1 0 0] def\n");
    writePSFmt("/FontBBox [{0:.6g} {1:.6g} {2:.6g} {3:.6g}] def\n", bbox[0],
               bbox[1], bbox[2], bbox[3]);
    writePS("/Encoding 256 array def\n");
    writePS("  0 1 255 { Encoding exch /.notdef put } for\n");
    writePS("  Encoding 120 /x put\n");
    writePS("/BuildGlyph {\n");
    writePS("  exch /CharProcs get exch\n");
    writePS("  2 copy known not { pop /.notdef } if\n");
    writePS("  get exec\n");
    writePS("} bind def\n");
    writePS("/BuildChar {\n");
    writePS("  1 index /Encoding get exch get\n");
    writePS("  1 index /BuildGlyph get exec\n");
    writePS("} bind def\n");
    writePS("/CharProcs 1 dict def\n");
    writePS("CharProcs begin\n");
    box.x1 = bbox[0];
    box.y1 = bbox[1];
    box.x2 = bbox[2];
    box.y2 = bbox[3];
    auto gfx2 = std::make_unique< Gfx >(doc, this, resDict, &box, nullptr);
    gfx2->takeContentStreamStack(gfx);
    writePS("/x {\n");
    if (paintType == 2) {
        writePSFmt("{0:.6g} 0 {1:.6g} {2:.6g} {3:.6g} {4:.6g} setcachedevice\n",
                   xStep, bbox[0], bbox[1], bbox[2], bbox[3]);
        t3FillColorOnly = true;
    } else {
        if (x1 - 1 <= x0) {
            writePS("1 0 setcharwidth\n");
        } else {
            writePSFmt("{0:.6g} 0 setcharwidth\n", xStep);
        }
        t3FillColorOnly = false;
    }
    inType3Char = true;
    ++numTilingPatterns;
    gfx2->display(strRef);
    --numTilingPatterns;
    inType3Char = false;
    writePS("} def\n");

    writePS("end\n");
    writePS("currentdict end\n");
    writePSFmt("/xpdfTile{0:d} exch definefont pop\n", numTilingPatterns);

    // draw the tiles
    writePSFmt("/xpdfTile{0:d} findfont setfont\n", numTilingPatterns);
    writePS("fCol\n");
    writePSFmt("gsave [{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] concat\n",
               mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
    writePSFmt(
        "{0:d} 1 {1:d} {{ {2:.6g} exch {3:.6g} mul m {4:d} 1 {5:d} {{ pop (x) "
        "show }} for }} for\n",
        y0, y1 - 1, x0 * xStep, yStep, x0, x1 - 1);
    writePS("grestore\n");
}

bool PSOutputDev::functionShadedFill(GfxState *state, GfxFunctionShading *shading)
{
    double  x0, y0, x1, y1;
    double *mat;
    int     i;

    if (level == psLevel2Sep || level == psLevel3Sep) {
        if (shading->getColorSpace()->getMode() != csDeviceCMYK) {
            return false;
        }
        processColors |= psProcessCMYK;
    }

    shading->getDomain(&x0, &y0, &x1, &y1);
    mat = shading->getMatrix();
    writePSFmt("/mat [{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g}] def\n",
               mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
    writePSFmt("/n {0:d} def\n", shading->getColorSpace()->getNComps());
    if (shading->getNFuncs() == 1) {
        writePS("/func ");
        cvtFunction(shading->getFunc(0));
        writePS("def\n");
    } else {
        writePS("/func {\n");
        for (i = 0; i < shading->getNFuncs(); ++i) {
            if (i < shading->getNFuncs() - 1) {
                writePS("2 copy\n");
            }
            cvtFunction(shading->getFunc(i));
            writePS("exec\n");
            if (i < shading->getNFuncs() - 1) {
                writePS("3 1 roll\n");
            }
        }
        writePS("} def\n");
    }
    writePSFmt("{0:.6g} {1:.6g} {2:.6g} {3:.6g} 0 funcSH\n", x0, y0, x1, y1);

    return true;
}

bool PSOutputDev::axialShadedFill(GfxState *state, GfxAxialShading *shading)
{
    double xMin, yMin, xMax, yMax;
    double x0, y0, x1, y1, dx, dy, mul;
    double tMin, tMax, t, t0, t1;
    int    i;

    if (level == psLevel2Sep || level == psLevel3Sep) {
        if (shading->getColorSpace()->getMode() != csDeviceCMYK) {
            return false;
        }
        processColors |= psProcessCMYK;
    }

    // get the clip region bbox
    state->getUserClipBBox(&xMin, &yMin, &xMax, &yMax);

    // compute min and max t values, based on the four corners of the
    // clip region bbox
    shading->getCoords(&x0, &y0, &x1, &y1);
    dx = x1 - x0;
    dy = y1 - y0;
    if (fabs(dx) < 0.01 && fabs(dy) < 0.01) {
        return true;
    } else {
        mul = 1 / (dx * dx + dy * dy);
        tMin = tMax = ((xMin - x0) * dx + (yMin - y0) * dy) * mul;
        t = ((xMin - x0) * dx + (yMax - y0) * dy) * mul;
        if (t < tMin) {
            tMin = t;
        } else if (t > tMax) {
            tMax = t;
        }
        t = ((xMax - x0) * dx + (yMin - y0) * dy) * mul;
        if (t < tMin) {
            tMin = t;
        } else if (t > tMax) {
            tMax = t;
        }
        t = ((xMax - x0) * dx + (yMax - y0) * dy) * mul;
        if (t < tMin) {
            tMin = t;
        } else if (t > tMax) {
            tMax = t;
        }
        if (tMin < 0 && !shading->getExtend0()) {
            tMin = 0;
        }
        if (tMax > 1 && !shading->getExtend1()) {
            tMax = 1;
        }
    }

    // get the function domain
    t0 = shading->getDomain0();
    t1 = shading->getDomain1();

    // generate the PS code
    writePSFmt("/t0 {0:.6g} def\n", t0);
    writePSFmt("/t1 {0:.6g} def\n", t1);
    writePSFmt("/dt {0:.6g} def\n", t1 - t0);
    writePSFmt("/x0 {0:.6g} def\n", x0);
    writePSFmt("/y0 {0:.6g} def\n", y0);
    writePSFmt("/dx {0:.6g} def\n", x1 - x0);
    writePSFmt("/x1 {0:.6g} def\n", x1);
    writePSFmt("/y1 {0:.6g} def\n", y1);
    writePSFmt("/dy {0:.6g} def\n", y1 - y0);
    writePSFmt("/xMin {0:.6g} def\n", xMin);
    writePSFmt("/yMin {0:.6g} def\n", yMin);
    writePSFmt("/xMax {0:.6g} def\n", xMax);
    writePSFmt("/yMax {0:.6g} def\n", yMax);
    writePSFmt("/n {0:d} def\n", shading->getColorSpace()->getNComps());
    if (shading->getNFuncs() == 1) {
        writePS("/func ");
        cvtFunction(shading->getFunc(0));
        writePS("def\n");
    } else {
        writePS("/func {\n");
        for (i = 0; i < shading->getNFuncs(); ++i) {
            if (i < shading->getNFuncs() - 1) {
                writePS("dup\n");
            }
            cvtFunction(shading->getFunc(i));
            writePS("exec\n");
            if (i < shading->getNFuncs() - 1) {
                writePS("exch\n");
            }
        }
        writePS("} def\n");
    }
    writePSFmt("{0:.6g} {1:.6g} 0 axialSH\n", tMin, tMax);

    return true;
}

bool PSOutputDev::radialShadedFill(GfxState *state, GfxRadialShading *shading)
{
    double xMin, yMin, xMax, yMax;
    double x0, y0, r0, x1, y1, r1, t0, t1;
    double xa, ya, ra;
    double sMin, sMax, h, ta;
    double sLeft, sRight, sTop, sBottom, sZero, sDiag;
    bool   haveSLeft, haveSRight, haveSTop, haveSBottom, haveSZero;
    bool   haveSMin, haveSMax;
    double theta, alpha, a1, a2;
    bool   enclosed;
    int    i;

    if (level == psLevel2Sep || level == psLevel3Sep) {
        if (shading->getColorSpace()->getMode() != csDeviceCMYK) {
            return false;
        }
        processColors |= psProcessCMYK;
    }

    // get the shading info
    shading->getCoords(&x0, &y0, &r0, &x1, &y1, &r1);
    t0 = shading->getDomain0();
    t1 = shading->getDomain1();

    // Compute the point at which r(s) = 0; check for the enclosed
    // circles case; and compute the angles for the tangent lines.
    h = sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    if (h == 0) {
        enclosed = true;
        theta = 0; // make gcc happy
    } else if (r1 - r0 == 0) {
        enclosed = false;
        theta = 0;
    } else if (fabs(r1 - r0) >= h) {
        enclosed = true;
        theta = 0; // make gcc happy
    } else {
        enclosed = false;
        theta = asin((r1 - r0) / h);
    }
    if (enclosed) {
        a1 = 0;
        a2 = 360;
    } else {
        alpha = atan2(y1 - y0, x1 - x0);
        a1 = (180 / M_PI) * (alpha + theta) + 90;
        a2 = (180 / M_PI) * (alpha - theta) - 90;
        while (a2 < a1) {
            a2 += 360;
        }
    }

    // compute the (possibly extended) s range
    state->getUserClipBBox(&xMin, &yMin, &xMax, &yMax);
    if (enclosed) {
        sMin = 0;
        sMax = 1;
    } else {
        // solve x(sLeft) + r(sLeft) = xMin
        if ((haveSLeft = fabs((x1 + r1) - (x0 + r0)) > 0.000001)) {
            sLeft = (xMin - (x0 + r0)) / ((x1 + r1) - (x0 + r0));
        } else {
            sLeft = 0; // make gcc happy
        }
        // solve x(sRight) - r(sRight) = xMax
        if ((haveSRight = fabs((x1 - r1) - (x0 - r0)) > 0.000001)) {
            sRight = (xMax - (x0 - r0)) / ((x1 - r1) - (x0 - r0));
        } else {
            sRight = 0; // make gcc happy
        }
        // solve y(sBottom) + r(sBottom) = yMin
        if ((haveSBottom = fabs((y1 + r1) - (y0 + r0)) > 0.000001)) {
            sBottom = (yMin - (y0 + r0)) / ((y1 + r1) - (y0 + r0));
        } else {
            sBottom = 0; // make gcc happy
        }
        // solve y(sTop) - r(sTop) = yMax
        if ((haveSTop = fabs((y1 - r1) - (y0 - r0)) > 0.000001)) {
            sTop = (yMax - (y0 - r0)) / ((y1 - r1) - (y0 - r0));
        } else {
            sTop = 0; // make gcc happy
        }
        // solve r(sZero) = 0
        if ((haveSZero = fabs(r1 - r0) > 0.000001)) {
            sZero = -r0 / (r1 - r0);
        } else {
            sZero = 0; // make gcc happy
        }
        // solve r(sDiag) = sqrt((xMax-xMin)^2 + (yMax-yMin)^2)
        if (haveSZero) {
            sDiag = (sqrt((xMax - xMin) * (xMax - xMin) +
                          (yMax - yMin) * (yMax - yMin)) -
                     r0) /
                    (r1 - r0);
        } else {
            sDiag = 0; // make gcc happy
        }
        // compute sMin
        if (shading->getExtend0()) {
            sMin = 0;
            haveSMin = false;
            if (x0 < x1 && haveSLeft && sLeft < 0) {
                sMin = sLeft;
                haveSMin = true;
            } else if (x0 > x1 && haveSRight && sRight < 0) {
                sMin = sRight;
                haveSMin = true;
            }
            if (y0 < y1 && haveSBottom && sBottom < 0) {
                if (!haveSMin || sBottom > sMin) {
                    sMin = sBottom;
                    haveSMin = true;
                }
            } else if (y0 > y1 && haveSTop && sTop < 0) {
                if (!haveSMin || sTop > sMin) {
                    sMin = sTop;
                    haveSMin = true;
                }
            }
            if (haveSZero && sZero < 0) {
                if (!haveSMin || sZero > sMin) {
                    sMin = sZero;
                }
            }
        } else {
            sMin = 0;
        }
        // compute sMax
        if (shading->getExtend1()) {
            sMax = 1;
            haveSMax = false;
            if (x1 < x0 && haveSLeft && sLeft > 1) {
                sMax = sLeft;
                haveSMax = true;
            } else if (x1 > x0 && haveSRight && sRight > 1) {
                sMax = sRight;
                haveSMax = true;
            }
            if (y1 < y0 && haveSBottom && sBottom > 1) {
                if (!haveSMax || sBottom < sMax) {
                    sMax = sBottom;
                    haveSMax = true;
                }
            } else if (y1 > y0 && haveSTop && sTop > 1) {
                if (!haveSMax || sTop < sMax) {
                    sMax = sTop;
                    haveSMax = true;
                }
            }
            if (haveSZero && sDiag > 1) {
                if (!haveSMax || sDiag < sMax) {
                    sMax = sDiag;
                }
            }
        } else {
            sMax = 1;
        }
    }

    // generate the PS code
    writePSFmt("/x0 {0:.6g} def\n", x0);
    writePSFmt("/x1 {0:.6g} def\n", x1);
    writePSFmt("/dx {0:.6g} def\n", x1 - x0);
    writePSFmt("/y0 {0:.6g} def\n", y0);
    writePSFmt("/y1 {0:.6g} def\n", y1);
    writePSFmt("/dy {0:.6g} def\n", y1 - y0);
    writePSFmt("/r0 {0:.6g} def\n", r0);
    writePSFmt("/r1 {0:.6g} def\n", r1);
    writePSFmt("/dr {0:.6g} def\n", r1 - r0);
    writePSFmt("/t0 {0:.6g} def\n", t0);
    writePSFmt("/t1 {0:.6g} def\n", t1);
    writePSFmt("/dt {0:.6g} def\n", t1 - t0);
    writePSFmt("/n {0:d} def\n", shading->getColorSpace()->getNComps());
    writePSFmt("/encl {0:s} def\n", enclosed ? "true" : "false");
    writePSFmt("/a1 {0:.6g} def\n", a1);
    writePSFmt("/a2 {0:.6g} def\n", a2);
    if (shading->getNFuncs() == 1) {
        writePS("/func ");
        cvtFunction(shading->getFunc(0));
        writePS("def\n");
    } else {
        writePS("/func {\n");
        for (i = 0; i < shading->getNFuncs(); ++i) {
            if (i < shading->getNFuncs() - 1) {
                writePS("dup\n");
            }
            cvtFunction(shading->getFunc(i));
            writePS("exec\n");
            if (i < shading->getNFuncs() - 1) {
                writePS("exch\n");
            }
        }
        writePS("} def\n");
    }
    writePSFmt("{0:.6g} {1:.6g} 0 radialSH\n", sMin, sMax);

    // extend the 'enclosed' case
    if (enclosed) {
        // extend the smaller circle
        if ((shading->getExtend0() && r0 <= r1) ||
            (shading->getExtend1() && r1 < r0)) {
            if (r0 <= r1) {
                ta = t0;
                ra = r0;
                xa = x0;
                ya = y0;
            } else {
                ta = t1;
                ra = r1;
                xa = x1;
                ya = y1;
            }
            if (level == psLevel2Sep || level == psLevel3Sep) {
                writePSFmt("{0:.6g} radialCol aload pop k\n", ta);
            } else {
                writePSFmt("{0:.6g} radialCol sc\n", ta);
            }
            writePSFmt("{0:.6g} {1:.6g} {2:.6g} 0 360 arc h f*\n", xa, ya, ra);
        }

        // extend the larger circle
        if ((shading->getExtend0() && r0 > r1) ||
            (shading->getExtend1() && r1 >= r0)) {
            if (r0 > r1) {
                ta = t0;
                ra = r0;
                xa = x0;
                ya = y0;
            } else {
                ta = t1;
                ra = r1;
                xa = x1;
                ya = y1;
            }
            if (level == psLevel2Sep || level == psLevel3Sep) {
                writePSFmt("{0:.6g} radialCol aload pop k\n", ta);
            } else {
                writePSFmt("{0:.6g} radialCol sc\n", ta);
            }
            writePSFmt("{0:.6g} {1:.6g} {2:.6g} 0 360 arc h\n", xa, ya, ra);
            writePSFmt(
                "{0:.6g} {1:.6g} m {2:.6g} {3:.6g} l {4:.6g} {5:.6g} l {6:.6g} "
                "{7:.6g} l h f*\n",
                xMin, yMin, xMin, yMax, xMax, yMax, xMax, yMin);
        }
    }

    return true;
}

void PSOutputDev::clip(GfxState *state)
{
    doPath(state->getPath());
    writePS("W\n");
}

void PSOutputDev::eoClip(GfxState *state)
{
    doPath(state->getPath());
    writePS("W*\n");
}

void PSOutputDev::clipToStrokePath(GfxState *state)
{
    doPath(state->getPath());
    writePS("Ws\n");
}

void PSOutputDev::doPath(GfxPath *path)
{
    GfxSubpath *subpath;
    double      x0, y0, x1, y1, x2, y2, x3, y3, x4, y4;
    int         n, m, i, j;

    n = path->getNumSubpaths();

    if (n == 1 && path->getSubpath(0)->getNumPoints() == 5) {
        subpath = path->getSubpath(0);
        x0 = subpath->getX(0);
        y0 = subpath->getY(0);
        x4 = subpath->getX(4);
        y4 = subpath->getY(4);
        if (x4 == x0 && y4 == y0) {
            x1 = subpath->getX(1);
            y1 = subpath->getY(1);
            x2 = subpath->getX(2);
            y2 = subpath->getY(2);
            x3 = subpath->getX(3);
            y3 = subpath->getY(3);
            if (x0 == x1 && x2 == x3 && y0 == y3 && y1 == y2) {
                writePSFmt("{0:.6g} {1:.6g} {2:.6g} {3:.6g} re\n",
                           x0 < x2 ? x0 : x2, y0 < y1 ? y0 : y1, fabs(x2 - x0),
                           fabs(y1 - y0));
                return;
            } else if (x0 == x3 && x1 == x2 && y0 == y1 && y2 == y3) {
                writePSFmt("{0:.6g} {1:.6g} {2:.6g} {3:.6g} re\n",
                           x0 < x1 ? x0 : x1, y0 < y2 ? y0 : y2, fabs(x1 - x0),
                           fabs(y2 - y0));
                return;
            }
        }
    }

    for (i = 0; i < n; ++i) {
        subpath = path->getSubpath(i);
        m = subpath->getNumPoints();
        writePSFmt("{0:.6g} {1:.6g} m\n", subpath->getX(0), subpath->getY(0));
        j = 1;
        while (j < m) {
            if (subpath->getCurve(j)) {
                writePSFmt("{0:.6g} {1:.6g} {2:.6g} {3:.6g} {4:.6g} {5:.6g} c\n",
                           subpath->getX(j), subpath->getY(j),
                           subpath->getX(j + 1), subpath->getY(j + 1),
                           subpath->getX(j + 2), subpath->getY(j + 2));
                j += 3;
            } else {
                writePSFmt("{0:.6g} {1:.6g} l\n", subpath->getX(j),
                           subpath->getY(j));
                ++j;
            }
        }
        if (subpath->isClosed()) {
            writePS("h\n");
        }
    }
}

void PSOutputDev::drawString(GfxState *state, GString *s)
{
    GfxFont *font;
    int      wMode;
    GString *s2;
    double   dx, dy, originX, originY, originX0, originY0, tOriginX0, tOriginY0;
    const char *p;
    CharCode    code;
    Unicode     u[8];
    char        buf[8];
    double *    dxdy;
    int         dxdySize, len, nChars, uLen, n, m, i, j;

    // check for invisible text -- this is used by Acrobat Capture
    if (3 == state->getRender())
        return;

    if (0 == s || s->empty())
        return;

    // get the font
    if (0 == (font = state->getFont())) {
        return;
    }

    wMode = font->getWMode();

    //
    // Check for a subtitute 16-bit font:
    //
    xpdf::unicode_map_t uMap;

    int *codeToGID = 0;

    auto iter = find_if(fontInfo, [&](auto &fi) {
        return fi.fontID == *font->getID(); });

    if (iter == fontInfo.end()) {
        if (font->isCIDFont())
            return;
    } else {
        auto &fi = *iter;

        if (font->isCIDFont()) {
            if (0 == fi.ff)
                return;

            if (fi.ff->encoding)
                uMap = globalParams->getUnicodeMap(fi.ff->encoding->c_str());

            // check for an 8-bit code-to-GID map
        } else {
            if (fi.ff)
                codeToGID = fi.ff->codeToGID;
        }
    }

    // compute the positioning (dx, dy) for each char in the string
    nChars = 0;
    p = s->c_str();
    len = s->getLength();
    s2 = new GString();
    dxdySize = font->isCIDFont() ? 8 : s->getLength();
    dxdy = (double *)calloc(2 * dxdySize, sizeof(double));
    originX0 = originY0 = 0; // make gcc happy
    while (len > 0) {
        n = font->getNextChar(p, len, &code, u,
                              (int)(sizeof(u) / sizeof(Unicode)), &uLen, &dx, &dy,
                              &originX, &originY);
        //~ this doesn't handle the case where the origin offset changes
        //~   within a string of characters -- which could be fixed by
        //~   modifying dx,dy as needed for each character
        if (p == s->c_str()) {
            originX0 = originX;
            originY0 = originY;
        }
        dx *= state->getFontSize();
        dy *= state->getFontSize();
        if (wMode) {
            dy += state->getCharSpace();
            if (n == 1 && *p == ' ') {
                dy += state->getWordSpace();
            }
        } else {
            dx += state->getCharSpace();
            if (n == 1 && *p == ' ') {
                dx += state->getWordSpace();
            }
        }
        dx *= state->getHorizScaling();
        if (font->isCIDFont()) {
            if (uMap) {
                if (nChars + uLen > dxdySize) {
                    do {
                        dxdySize *= 2;
                    } while (nChars + uLen > dxdySize);
                    dxdy = (double *)reallocarray(dxdy, 2 * dxdySize,
                                                  sizeof(double));
                }
                for (i = 0; i < uLen; ++i) {
                    m = uMap(u[i], buf, (int)sizeof(buf));
                    for (j = 0; j < m; ++j) {
                        s2->append(1UL, buf[j]);
                    }
                    //~ this really needs to get the number of chars in the target
                    //~ encoding - which may be more than the number of Unicode
                    //~ chars
                    dxdy[2 * nChars] = dx;
                    dxdy[2 * nChars + 1] = dy;
                    ++nChars;
                }
            } else {
                if (nChars + 1 > dxdySize) {
                    dxdySize *= 2;
                    dxdy = (double *)reallocarray(dxdy, 2 * dxdySize,
                                                  sizeof(double));
                }
                s2->append(1UL, (char)((code >> 8) & 0xff));
                s2->append(1UL, (char)(code & 0xff));
                dxdy[2 * nChars] = dx;
                dxdy[2 * nChars + 1] = dy;
                ++nChars;
            }
        } else {
            if (!codeToGID || codeToGID[code] >= 0) {
                s2->append(1UL, (char)code);
                dxdy[2 * nChars] = dx;
                dxdy[2 * nChars + 1] = dy;
                ++nChars;
            }
        }
        p += n;
        len -= n;
    }

    originX0 *= state->getFontSize();
    originY0 *= state->getFontSize();

    state->textTransformDelta(originX0, originY0, &tOriginX0, &tOriginY0);

    if (nChars > 0) {
        if (wMode) {
            writePSFmt("{0:.6g} {1:.6g} rmoveto\n", -tOriginX0, -tOriginY0);
        }
        writePSString(s2);
        writePS("\n[");
        for (i = 0; i < 2 * nChars; ++i) {
            if (i > 0) {
                writePS("\n");
            }
            writePSFmt("{0:.6g}", dxdy[i]);
        }
        writePS("] Tj\n");
        if (wMode) {
            writePSFmt("{0:.6g} {1:.6g} rmoveto\n", tOriginX0, tOriginY0);
        }
    }
    free(dxdy);
    delete s2;

    if (state->getRender() & 4) {
        haveTextClip = true;
    }
}

void PSOutputDev::endTextObject(GfxState *state)
{
    if (haveTextClip) {
        writePS("Tclip\n");
        haveTextClip = false;
    }
}

void PSOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str,
                                int width, int height, bool invert,
                                bool inlineImg, bool interpolate)
{
    int len;

    len = height * ((width + 7) / 8);
    switch (level) {
    case psLevel1:
    case psLevel1Sep:
        doImageL1(ref, NULL, invert, inlineImg, str, width, height, len);
        break;
    case psLevel2:
    case psLevel2Sep:
        doImageL2(ref, NULL, invert, inlineImg, str, width, height, len, NULL,
                  NULL, 0, 0, false);
        break;
    case psLevel3:
    case psLevel3Sep:
        doImageL3(ref, NULL, invert, inlineImg, str, width, height, len, NULL,
                  NULL, 0, 0, false);
        break;
    }
}

void PSOutputDev::drawImage(GfxState *state, Object *ref, Stream *str, int width,
                            int height, GfxImageColorMap *colorMap,
                            int *maskColors, bool inlineImg, bool interpolate)
{
    int len;

    len = height *
          ((width * colorMap->getNumPixelComps() * colorMap->getBits() + 7) / 8);
    switch (level) {
    case psLevel1:
        doImageL1(ref, colorMap, false, inlineImg, str, width, height, len);
        break;
    case psLevel1Sep:
        //~ handle indexed, separation, ... color spaces
        doImageL1Sep(colorMap, false, inlineImg, str, width, height, len);
        break;
    case psLevel2:
    case psLevel2Sep:
        doImageL2(ref, colorMap, false, inlineImg, str, width, height, len,
                  maskColors, NULL, 0, 0, false);
        break;
    case psLevel3:
    case psLevel3Sep:
        doImageL3(ref, colorMap, false, inlineImg, str, width, height, len,
                  maskColors, NULL, 0, 0, false);
        break;
    }
    t3Cacheable = false;
}

void PSOutputDev::drawMaskedImage(GfxState *state, Object *ref, Stream *str,
                                  int width, int height,
                                  GfxImageColorMap *colorMap, Stream *maskStr,
                                  int maskWidth, int maskHeight, bool maskInvert,
                                  bool interpolate)
{
    int len;

    len = height *
          ((width * colorMap->getNumPixelComps() * colorMap->getBits() + 7) / 8);
    switch (level) {
    case psLevel1:
        doImageL1(ref, colorMap, false, false, str, width, height, len);
        break;
    case psLevel1Sep:
        //~ handle indexed, separation, ... color spaces
        doImageL1Sep(colorMap, false, false, str, width, height, len);
        break;
    case psLevel2:
    case psLevel2Sep:
        doImageL2(ref, colorMap, false, false, str, width, height, len, NULL,
                  maskStr, maskWidth, maskHeight, maskInvert);
        break;
    case psLevel3:
    case psLevel3Sep:
        doImageL3(ref, colorMap, false, false, str, width, height, len, NULL,
                  maskStr, maskWidth, maskHeight, maskInvert);
        break;
    }
    t3Cacheable = false;
}

void PSOutputDev::doImageL1(Object *ref, GfxImageColorMap *colorMap, bool invert,
                            bool inlineImg, Stream *str, int width, int height,
                            int len)
{
    ImageStream * imgStr;
    unsigned char pixBuf[gfxColorMaxComps];
    GfxGray       gray;
    int           col, x, y, c, i;

    if ((inType3Char || preload) && !colorMap) {
        if (inlineImg) {
            // create an array
            str = new FixedLengthEncoder(str, len);
            str = new ASCIIHexEncoder(str);
            str->reset();
            col = 0;
            writePS("[<");
            do {
                do {
                    c = str->get();
                } while (c == '\n' || c == '\r');
                if (c == '>' || c == EOF) {
                    break;
                }
                writePSChar(c);
                ++col;
                // each line is: "<...data...><eol>"
                // so max data length = 255 - 4 = 251
                // but make it 240 just to be safe
                // chunks are 2 bytes each, so we need to stop on an even col number
                if (col == 240) {
                    writePS(">\n<");
                    col = 0;
                }
            } while (c != '>' && c != EOF);
            writePS(">]\n");
            writePS("0\n");
            str->close();
            delete str;
        } else {
            // set up to use the array already created by setupImages()
            writePSFmt("ImData_{0:d}_{1:d} 0\n", ref->getRefNum(),
                       ref->getRefGen());
        }
    }

    // image/imagemask command
    if ((inType3Char || preload) && !colorMap) {
        writePSFmt("{0:d} {1:d} {2:s} [{3:d} 0 0 {4:d} 0 {5:d}] pdfImM1a\n",
                   width, height, invert ? "true" : "false", width, -height,
                   height);
    } else if (colorMap) {
        writePSFmt("{0:d} {1:d} 8 [{2:d} 0 0 {3:d} 0 {4:d}] pdfIm1\n", width,
                   height, width, -height, height);
    } else {
        writePSFmt("{0:d} {1:d} {2:s} [{3:d} 0 0 {4:d} 0 {5:d}] pdfImM1\n", width,
                   height, invert ? "true" : "false", width, -height, height);
    }

    // image data
    if (!((inType3Char || preload) && !colorMap)) {
        if (colorMap) {
            // set up to process the data stream
            imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                                     colorMap->getBits());
            imgStr->reset();

            // process the data stream
            i = 0;
            for (y = 0; y < height; ++y) {
                // write the line
                for (x = 0; x < width; ++x) {
                    imgStr->getPixel(pixBuf);
                    colorMap->getGray(pixBuf, &gray);
                    writePSFmt("{0:02x}", xpdf::to_small_color(gray.x));
                    if (++i == 32) {
                        writePSChar('\n');
                        i = 0;
                    }
                }
            }
            if (i != 0) {
                writePSChar('\n');
            }
            str->close();
            delete imgStr;

            // imagemask
        } else {
            str->reset();
            i = 0;
            for (y = 0; y < height; ++y) {
                for (x = 0; x < width; x += 8) {
                    writePSFmt("{0:02x}", str->get() & 0xff);
                    if (++i == 32) {
                        writePSChar('\n');
                        i = 0;
                    }
                }
            }
            if (i != 0) {
                writePSChar('\n');
            }
            str->close();
        }
    }
}

void PSOutputDev::doImageL1Sep(GfxImageColorMap *colorMap, bool invert,
                               bool inlineImg, Stream *str, int width, int height,
                               int len)
{
    ImageStream *  imgStr;
    unsigned char *lineBuf;
    unsigned char  pixBuf[gfxColorMaxComps];
    GfxCMYK        cmyk;
    int            x, y, i, comp;

    // width, height, matrix, bits per component
    writePSFmt("{0:d} {1:d} 8 [{2:d} 0 0 {3:d} 0 {4:d}] pdfIm1Sep\n", width,
               height, width, -height, height);

    // allocate a line buffer
    lineBuf = (unsigned char *)calloc(width, 4);

    // set up to process the data stream
    imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                             colorMap->getBits());
    imgStr->reset();

    // process the data stream
    i = 0;
    for (y = 0; y < height; ++y) {
        // read the line
        for (x = 0; x < width; ++x) {
            imgStr->getPixel(pixBuf);
            colorMap->getCMYK(pixBuf, &cmyk);
            lineBuf[4 * x + 0] = xpdf::to_small_color(cmyk.c);
            lineBuf[4 * x + 1] = xpdf::to_small_color(cmyk.m);
            lineBuf[4 * x + 2] = xpdf::to_small_color(cmyk.y);
            lineBuf[4 * x + 3] = xpdf::to_small_color(cmyk.k);
            addProcessColor(xpdf::to_double(cmyk.c), xpdf::to_double(cmyk.m),
                            xpdf::to_double(cmyk.y), xpdf::to_double(cmyk.k));
        }

        // write one line of each color component
        for (comp = 0; comp < 4; ++comp) {
            for (x = 0; x < width; ++x) {
                writePSFmt("{0:02x}", lineBuf[4 * x + comp]);
                if (++i == 32) {
                    writePSChar('\n');
                    i = 0;
                }
            }
        }
    }

    if (i != 0) {
        writePSChar('\n');
    }

    str->close();
    delete imgStr;
    free(lineBuf);
}

void PSOutputDev::doImageL2(Object *ref, GfxImageColorMap *colorMap, bool invert,
                            bool inlineImg, Stream *str, int width, int height,
                            int len, int *maskColors, Stream *maskStr,
                            int maskWidth, int maskHeight, bool maskInvert)
{
    Stream *          str2;
    ImageStream *     imgStr;
    unsigned char *   line;
    PSOutImgClipRect *rects0, *rects1, *rectsTmp, *rectsOut;
    int               rects0Len, rects1Len, rectsSize, rectsOutLen, rectsOutSize;
    bool              emitRect, addRect, extendRect;
    GString *         s;
    int               n, numComps;
    bool              useLZW, useRLE, useASCII, useASCIIHex, useCompressed;
    GfxSeparationColorSpace *sepCS;
    GfxColor                 color;
    GfxCMYK                  cmyk;
    char                     buf[4096];
    int                      c;
    int                      col, i, j, x0, x1, y, maskXor;

    // color key masking
    if (maskColors && colorMap && !inlineImg) {
        // can't read the stream twice for inline images -- but masking
        // isn't allowed with inline images anyway
        numComps = colorMap->getNumPixelComps();
        imgStr = new ImageStream(str, width, numComps, colorMap->getBits());
        imgStr->reset();
        rects0Len = rects1Len = rectsOutLen = 0;
        rectsSize = rectsOutSize = 64;
        rects0 = (PSOutImgClipRect *)calloc(rectsSize, sizeof(PSOutImgClipRect));
        rects1 = (PSOutImgClipRect *)calloc(rectsSize, sizeof(PSOutImgClipRect));
        rectsOut =
            (PSOutImgClipRect *)calloc(rectsOutSize, sizeof(PSOutImgClipRect));
        for (y = 0; y < height; ++y) {
            if (!(line = imgStr->readline())) {
                break;
            }
            i = 0;
            rects1Len = 0;
            for (x0 = 0; x0 < width; ++x0) {
                for (j = 0; j < numComps; ++j) {
                    if (line[x0 * numComps + j] < maskColors[2 * j] ||
                        line[x0 * numComps + j] > maskColors[2 * j + 1]) {
                        break;
                    }
                }
                if (j < numComps) {
                    break;
                }
            }
            for (x1 = x0; x1 < width; ++x1) {
                for (j = 0; j < numComps; ++j) {
                    if (line[x1 * numComps + j] < maskColors[2 * j] ||
                        line[x1 * numComps + j] > maskColors[2 * j + 1]) {
                        break;
                    }
                }
                if (j == numComps) {
                    break;
                }
            }
            while (x0 < width || i < rects0Len) {
                emitRect = addRect = extendRect = false;
                if (x0 >= width) {
                    emitRect = true;
                } else if (i >= rects0Len) {
                    addRect = true;
                } else if (rects0[i].x0 < x0) {
                    emitRect = true;
                } else if (x0 < rects0[i].x0) {
                    addRect = true;
                } else if (rects0[i].x1 == x1) {
                    extendRect = true;
                } else {
                    emitRect = addRect = true;
                }
                if (emitRect) {
                    if (rectsOutLen == rectsOutSize) {
                        rectsOutSize *= 2;
                        rectsOut = (PSOutImgClipRect *)reallocarray(
                            rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
                    }
                    rectsOut[rectsOutLen].x0 = rects0[i].x0;
                    rectsOut[rectsOutLen].x1 = rects0[i].x1;
                    rectsOut[rectsOutLen].y0 = height - y - 1;
                    rectsOut[rectsOutLen].y1 = height - rects0[i].y0 - 1;
                    ++rectsOutLen;
                    ++i;
                }
                if (addRect || extendRect) {
                    if (rects1Len == rectsSize) {
                        rectsSize *= 2;
                        rects0 = (PSOutImgClipRect *)reallocarray(
                            rects0, rectsSize, sizeof(PSOutImgClipRect));
                        rects1 = (PSOutImgClipRect *)reallocarray(
                            rects1, rectsSize, sizeof(PSOutImgClipRect));
                    }
                    rects1[rects1Len].x0 = x0;
                    rects1[rects1Len].x1 = x1;
                    if (addRect) {
                        rects1[rects1Len].y0 = y;
                    }
                    if (extendRect) {
                        rects1[rects1Len].y0 = rects0[i].y0;
                        ++i;
                    }
                    ++rects1Len;
                    for (x0 = x1; x0 < width; ++x0) {
                        for (j = 0; j < numComps; ++j) {
                            if (line[x0 * numComps + j] < maskColors[2 * j] ||
                                line[x0 * numComps + j] > maskColors[2 * j + 1]) {
                                break;
                            }
                        }
                        if (j < numComps) {
                            break;
                        }
                    }
                    for (x1 = x0; x1 < width; ++x1) {
                        for (j = 0; j < numComps; ++j) {
                            if (line[x1 * numComps + j] < maskColors[2 * j] ||
                                line[x1 * numComps + j] > maskColors[2 * j + 1]) {
                                break;
                            }
                        }
                        if (j == numComps) {
                            break;
                        }
                    }
                }
            }
            rectsTmp = rects0;
            rects0 = rects1;
            rects1 = rectsTmp;
            i = rects0Len;
            rects0Len = rects1Len;
            rects1Len = i;
        }
        for (i = 0; i < rects0Len; ++i) {
            if (rectsOutLen == rectsOutSize) {
                rectsOutSize *= 2;
                rectsOut = (PSOutImgClipRect *)reallocarray(
                    rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
            }
            rectsOut[rectsOutLen].x0 = rects0[i].x0;
            rectsOut[rectsOutLen].x1 = rects0[i].x1;
            rectsOut[rectsOutLen].y0 = height - y - 1;
            rectsOut[rectsOutLen].y1 = height - rects0[i].y0 - 1;
            ++rectsOutLen;
        }
        writePSFmt("{0:d} {1:d}\n", maskWidth, maskHeight);
        for (i = 0; i < rectsOutLen; ++i) {
            writePSFmt("{0:d} {1:d} {2:d} {3:d} pr\n", rectsOut[i].x0,
                       rectsOut[i].y0, rectsOut[i].x1 - rectsOut[i].x0,
                       rectsOut[i].y1 - rectsOut[i].y0);
        }
        writePS("pop pop pdfImClip\n");
        free(rectsOut);
        free(rects0);
        free(rects1);
        delete imgStr;
        str->close();

        // explicit masking
    } else if (maskStr) {
        imgStr = new ImageStream(maskStr, maskWidth, 1, 1);
        imgStr->reset();
        rects0Len = rects1Len = rectsOutLen = 0;
        rectsSize = rectsOutSize = 64;
        rects0 = (PSOutImgClipRect *)calloc(rectsSize, sizeof(PSOutImgClipRect));
        rects1 = (PSOutImgClipRect *)calloc(rectsSize, sizeof(PSOutImgClipRect));
        rectsOut =
            (PSOutImgClipRect *)calloc(rectsOutSize, sizeof(PSOutImgClipRect));
        maskXor = maskInvert ? 1 : 0;
        for (y = 0; y < maskHeight; ++y) {
            if (!(line = imgStr->readline())) {
                break;
            }
            i = 0;
            rects1Len = 0;
            for (x0 = 0; x0 < maskWidth && (line[x0] ^ maskXor); ++x0)
                ;
            for (x1 = x0; x1 < maskWidth && !(line[x1] ^ maskXor); ++x1)
                ;
            while (x0 < maskWidth || i < rects0Len) {
                emitRect = addRect = extendRect = false;
                if (x0 >= maskWidth) {
                    emitRect = true;
                } else if (i >= rects0Len) {
                    addRect = true;
                } else if (rects0[i].x0 < x0) {
                    emitRect = true;
                } else if (x0 < rects0[i].x0) {
                    addRect = true;
                } else if (rects0[i].x1 == x1) {
                    extendRect = true;
                } else {
                    emitRect = addRect = true;
                }
                if (emitRect) {
                    if (rectsOutLen == rectsOutSize) {
                        rectsOutSize *= 2;
                        rectsOut = (PSOutImgClipRect *)reallocarray(
                            rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
                    }
                    rectsOut[rectsOutLen].x0 = rects0[i].x0;
                    rectsOut[rectsOutLen].x1 = rects0[i].x1;
                    rectsOut[rectsOutLen].y0 = maskHeight - y - 1;
                    rectsOut[rectsOutLen].y1 = maskHeight - rects0[i].y0 - 1;
                    ++rectsOutLen;
                    ++i;
                }
                if (addRect || extendRect) {
                    if (rects1Len == rectsSize) {
                        rectsSize *= 2;
                        rects0 = (PSOutImgClipRect *)reallocarray(
                            rects0, rectsSize, sizeof(PSOutImgClipRect));
                        rects1 = (PSOutImgClipRect *)reallocarray(
                            rects1, rectsSize, sizeof(PSOutImgClipRect));
                    }
                    rects1[rects1Len].x0 = x0;
                    rects1[rects1Len].x1 = x1;
                    if (addRect) {
                        rects1[rects1Len].y0 = y;
                    }
                    if (extendRect) {
                        rects1[rects1Len].y0 = rects0[i].y0;
                        ++i;
                    }
                    ++rects1Len;
                    for (x0 = x1; x0 < maskWidth && (line[x0] ^ maskXor); ++x0)
                        ;
                    for (x1 = x0; x1 < maskWidth && !(line[x1] ^ maskXor); ++x1)
                        ;
                }
            }
            rectsTmp = rects0;
            rects0 = rects1;
            rects1 = rectsTmp;
            i = rects0Len;
            rects0Len = rects1Len;
            rects1Len = i;
        }
        for (i = 0; i < rects0Len; ++i) {
            if (rectsOutLen == rectsOutSize) {
                rectsOutSize *= 2;
                rectsOut = (PSOutImgClipRect *)reallocarray(
                    rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
            }
            rectsOut[rectsOutLen].x0 = rects0[i].x0;
            rectsOut[rectsOutLen].x1 = rects0[i].x1;
            rectsOut[rectsOutLen].y0 = maskHeight - y - 1;
            rectsOut[rectsOutLen].y1 = maskHeight - rects0[i].y0 - 1;
            ++rectsOutLen;
        }
        writePSFmt("{0:d} {1:d}\n", maskWidth, maskHeight);
        for (i = 0; i < rectsOutLen; ++i) {
            writePSFmt("{0:d} {1:d} {2:d} {3:d} pr\n", rectsOut[i].x0,
                       rectsOut[i].y0, rectsOut[i].x1 - rectsOut[i].x0,
                       rectsOut[i].y1 - rectsOut[i].y0);
        }
        writePS("pop pop pdfImClip\n");
        free(rectsOut);
        free(rects0);
        free(rects1);
        delete imgStr;
        maskStr->close();
    }

    // color space
    if (colorMap) {
        dumpColorSpaceL2(colorMap->getColorSpace(), false, true, false);
        writePS(" setcolorspace\n");
    }

    useASCIIHex = globalParams->getPSASCIIHex();

    // set up the image data
    if (mode == psModeForm || inType3Char || preload) {
        if (inlineImg) {
            // create an array
            str2 = new FixedLengthEncoder(str, len);
            if (globalParams->getPSLZW()) {
                str2 = new LZWEncoder(str2);
            } else {
                str2 = new RunLengthEncoder(str2);
            }
            if (useASCIIHex) {
                str2 = new ASCIIHexEncoder(str2);
            } else {
                str2 = new ASCII85Encoder(str2);
            }
            str2->reset();
            col = 0;
            writePS((char *)(useASCIIHex ? "[<" : "[<~"));
            do {
                do {
                    c = str2->get();
                } while (c == '\n' || c == '\r');
                if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
                    break;
                }
                if (c == 'z') {
                    writePSChar(c);
                    ++col;
                } else {
                    writePSChar(c);
                    ++col;
                    for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i) {
                        do {
                            c = str2->get();
                        } while (c == '\n' || c == '\r');
                        if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
                            break;
                        }
                        writePSChar(c);
                        ++col;
                    }
                }
                // each line is: "<~...data...~><eol>"
                // so max data length = 255 - 6 = 249
                // chunks are 1 or 5 bytes each, so we have to stop at 245
                // but make it 240 just to be safe
                if (col > 240) {
                    writePS((char *)(useASCIIHex ? ">\n<" : "~>\n<~"));
                    col = 0;
                }
            } while (c != (useASCIIHex ? '>' : '~') && c != EOF);
            writePS((char *)(useASCIIHex ? ">\n" : "~>\n"));
            // add an extra entry because the LZWDecode/RunLengthDecode
            // filter may read past the end
            writePS("<>]\n");
            writePS("0\n");
            str2->close();
            delete str2;
        } else {
            // set up to use the array already created by setupImages()
            writePSFmt("ImData_{0:d}_{1:d} 0\n", ref->getRefNum(),
                       ref->getRefGen());
        }
    }

    // image dictionary
    writePS("<<\n  /ImageType 1\n");

    // width, height, matrix, bits per component
    writePSFmt("  /Width {0:d}\n", width);
    writePSFmt("  /Height {0:d}\n", height);
    writePSFmt("  /ImageMatrix [{0:d} 0 0 {1:d} 0 {2:d}]\n", width, -height,
               height);
    if (colorMap && colorMap->getColorSpace()->getMode() == csDeviceN) {
        writePS("  /BitsPerComponent 8\n");
    } else {
        writePSFmt("  /BitsPerComponent {0:d}\n",
                   colorMap ? colorMap->getBits() : 1);
    }

    // decode
    if (colorMap) {
        writePS("  /Decode [");
        if ((level == psLevel2Sep || level == psLevel3Sep) &&
            colorMap->getColorSpace()->getMode() == csSeparation) {
            // this matches up with the code in the pdfImSep operator
            n = (1 << colorMap->getBits()) - 1;
            writePSFmt("{0:.4g} {1:.4g}", colorMap->getDecodeLow(0) * n,
                       colorMap->getDecodeHigh(0) * n);
        } else if (colorMap->getColorSpace()->getMode() == csDeviceN) {
            numComps = ((GfxDeviceNColorSpace *)colorMap->getColorSpace())
                           ->getAlt()
                           ->getNComps();
            for (i = 0; i < numComps; ++i) {
                if (i > 0) {
                    writePS(" ");
                }
                writePS("0 1");
            }
        } else {
            numComps = colorMap->getNumPixelComps();
            for (i = 0; i < numComps; ++i) {
                if (i > 0) {
                    writePS(" ");
                }
                writePSFmt("{0:.4g} {1:.4g}", colorMap->getDecodeLow(i),
                           colorMap->getDecodeHigh(i));
            }
        }
        writePS("]\n");
    } else {
        writePSFmt("  /Decode [{0:d} {1:d}]\n", invert ? 1 : 0, invert ? 0 : 1);
    }

    // data source
    if (mode == psModeForm || inType3Char || preload) {
        writePS("  /DataSource { pdfImStr }\n");
    } else {
        writePS("  /DataSource currentfile\n");
    }

    // filters
    if ((mode == psModeForm || inType3Char || preload) &&
        globalParams->getPSUncompressPreloadedImages()) {
        s = NULL;
        useLZW = useRLE = false;
        useCompressed = false;
        useASCII = false;
    } else {
        s = str->getPSFilter(level < psLevel2 ? 1 : level < psLevel3 ? 2 : 3,
                             "    ");
        if ((colorMap && colorMap->getColorSpace()->getMode() == csDeviceN) ||
            inlineImg || !s) {
            if (globalParams->getPSLZW()) {
                useLZW = true;
                useRLE = false;
            } else {
                useRLE = true;
                useLZW = false;
            }
            useASCII = !(mode == psModeForm || inType3Char || preload);
            useCompressed = false;
        } else {
            useLZW = useRLE = false;
            useASCII = str->isBinary() &&
                       !(mode == psModeForm || inType3Char || preload);
            useCompressed = true;
        }
    }
    if (useASCII) {
        writePSFmt("    /ASCII{0:s}Decode filter\n", useASCIIHex ? "Hex" : "85");
    }
    if (useLZW) {
        writePS("    /LZWDecode filter\n");
    } else if (useRLE) {
        writePS("    /RunLengthDecode filter\n");
    }
    if (useCompressed) {
        writePS(s->c_str());
    }
    if (s) {
        delete s;
    }

    if (mode == psModeForm || inType3Char || preload) {
        // end of image dictionary
        writePSFmt(">>\n{0:s}\n", colorMap ? "image" : "imagemask");

        // get rid of the array and index
        writePS("pop pop\n");
    } else {
        // cut off inline image streams at appropriate length
        if (inlineImg) {
            str = new FixedLengthEncoder(str, len);
        } else if (useCompressed) {
            str = str->getUndecodedStream();
        }

        // recode DeviceN data
        if (colorMap && colorMap->getColorSpace()->getMode() == csDeviceN) {
            str = new DeviceNRecoder(str, width, height, colorMap);
        }

        // add LZWEncode/RunLengthEncode and ASCIIHex/85 encode filters
        if (useLZW) {
            str = new LZWEncoder(str);
        } else if (useRLE) {
            str = new RunLengthEncoder(str);
        }
        if (useASCII) {
            if (useASCIIHex) {
                str = new ASCIIHexEncoder(str);
            } else {
                str = new ASCII85Encoder(str);
            }
        }

        // end of image dictionary
        writePS(">>\n");
#if OPI_SUPPORT
        if (opi13Nest) {
            if (inlineImg) {
                // this can't happen -- OPI dictionaries are in XObjects
                error(errSyntaxError, -1, "OPI in inline image");
                n = 0;
            } else {
                // need to read the stream to count characters -- the length
                // is data-dependent (because of ASCII and LZW/RunLength
                // filters)
                str->reset();
                n = 0;
                do {
                    i = str->skip(4096);
                    n += i;
                } while (i == 4096);
                str->close();
            }
            // +6/7 for "pdfIm\n" / "pdfImM\n"
            // +8 for newline + trailer
            n += colorMap ? 14 : 15;
            writePSFmt("%%BeginData: {0:d} Hex Bytes\n", n);
        }
#endif
        if ((level == psLevel2Sep || level == psLevel3Sep) && colorMap &&
            colorMap->getColorSpace()->getMode() == csSeparation) {
            color.c[0] = XPDF_FIXED_POINT_ONE;
            sepCS = (GfxSeparationColorSpace *)colorMap->getColorSpace();
            sepCS->getCMYK(&color, &cmyk);
            writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} ({4:t}) pdfImSep\n",
                       xpdf::to_double(cmyk.c), xpdf::to_double(cmyk.m),
                       xpdf::to_double(cmyk.y), xpdf::to_double(cmyk.k),
                       sepCS->as_name());
        } else {
            writePSFmt("{0:s}\n", colorMap ? "pdfIm" : "pdfImM");
        }

        // copy the stream data
        str->reset();
        while ((n = str->readblock(buf, sizeof(buf))) > 0) {
            writePSBlock(buf, n);
        }
        str->close();

        // add newline and trailer to the end
        writePSChar('\n');
        writePS("%-EOD-\n");
#if OPI_SUPPORT
        if (opi13Nest) {
            writePS("%%EndData\n");
        }
#endif

        // delete encoders
        if (useLZW || useRLE || useASCII || inlineImg) {
            delete str;
        }
    }

    if ((maskColors && colorMap && !inlineImg) || maskStr) {
        writePS("pdfImClipEnd\n");
    }
}

//~ this doesn't currently support OPI
void PSOutputDev::doImageL3(Object *ref, GfxImageColorMap *colorMap, bool invert,
                            bool inlineImg, Stream *str, int width, int height,
                            int len, int *maskColors, Stream *maskStr,
                            int maskWidth, int maskHeight, bool maskInvert)
{
    Stream * str2;
    GString *s;
    int      n, numComps;
    bool     useLZW, useRLE, useASCII, useASCIIHex, useCompressed;
    bool     maskUseLZW, maskUseRLE, maskUseASCII, maskUseCompressed;
    GString *maskFilters;
    GfxSeparationColorSpace *sepCS;
    GfxColor                 color;
    GfxCMYK                  cmyk;
    char                     buf[4096];
    int                      c;
    int                      col, i;

    useASCIIHex = globalParams->getPSASCIIHex();
    useLZW = useRLE = useASCII = useCompressed = false; // make gcc happy
    maskUseLZW = maskUseRLE = maskUseASCII = false; // make gcc happy
    maskUseCompressed = false; // make gcc happy
    maskFilters = NULL; // make gcc happy

    // explicit masking
    if (maskStr) {
        // mask data source
        if ((mode == psModeForm || inType3Char || preload) &&
            globalParams->getPSUncompressPreloadedImages()) {
            s = NULL;
            maskUseLZW = maskUseRLE = false;
            maskUseCompressed = false;
            maskUseASCII = false;
        } else {
            s = maskStr->getPSFilter(3, "  ");
            if (!s) {
                if (globalParams->getPSLZW()) {
                    maskUseLZW = true;
                    maskUseRLE = false;
                } else {
                    maskUseRLE = true;
                    maskUseLZW = false;
                }
                maskUseASCII = !(mode == psModeForm || inType3Char || preload);
                maskUseCompressed = false;
            } else {
                maskUseLZW = maskUseRLE = false;
                maskUseASCII = maskStr->isBinary() &&
                               !(mode == psModeForm || inType3Char || preload);
                maskUseCompressed = true;
            }
        }
        maskFilters = new GString();
        if (maskUseASCII) {
            maskFilters->appendf("  /ASCII{0:s}Decode filter\n",
                                 useASCIIHex ? "Hex" : "85");
        }
        if (maskUseLZW) {
            maskFilters->append("  /LZWDecode filter\n");
        } else if (maskUseRLE) {
            maskFilters->append("  /RunLengthDecode filter\n");
        }
        if (maskUseCompressed) {
            maskFilters->append(*s);
        }
        if (s) {
            delete s;
        }
        if (mode == psModeForm || inType3Char || preload) {
            writePSFmt("MaskData_{0:d}_{1:d} pdfMaskInit\n", ref->getRefNum(),
                       ref->getRefGen());
        } else {
            writePS("currentfile\n");
            writePS(maskFilters->c_str());
            writePS("pdfMask\n");

            // add LZWEncode/RunLengthEncode and ASCIIHex/85 encode filters
            if (maskUseCompressed) {
                maskStr = maskStr->getUndecodedStream();
            }
            if (maskUseLZW) {
                maskStr = new LZWEncoder(maskStr);
            } else if (maskUseRLE) {
                maskStr = new RunLengthEncoder(maskStr);
            }
            if (maskUseASCII) {
                if (useASCIIHex) {
                    maskStr = new ASCIIHexEncoder(maskStr);
                } else {
                    maskStr = new ASCII85Encoder(maskStr);
                }
            }

            // copy the stream data
            maskStr->reset();
            while ((n = maskStr->readblock(buf, sizeof(buf))) > 0) {
                writePSBlock(buf, n);
            }
            maskStr->close();
            writePSChar('\n');
            writePS("%-EOD-\n");

            // delete encoders
            if (maskUseLZW || maskUseRLE || maskUseASCII) {
                delete maskStr;
            }
        }
    }

    // color space
    if (colorMap) {
        dumpColorSpaceL2(colorMap->getColorSpace(), false, true, false);
        writePS(" setcolorspace\n");
    }

    // set up the image data
    if (mode == psModeForm || inType3Char || preload) {
        if (inlineImg) {
            // create an array
            str2 = new FixedLengthEncoder(str, len);
            if (globalParams->getPSLZW()) {
                str2 = new LZWEncoder(str2);
            } else {
                str2 = new RunLengthEncoder(str2);
            }
            if (useASCIIHex) {
                str2 = new ASCIIHexEncoder(str2);
            } else {
                str2 = new ASCII85Encoder(str2);
            }
            str2->reset();
            col = 0;
            writePS((char *)(useASCIIHex ? "[<" : "[<~"));
            do {
                do {
                    c = str2->get();
                } while (c == '\n' || c == '\r');
                if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
                    break;
                }
                if (c == 'z') {
                    writePSChar(c);
                    ++col;
                } else {
                    writePSChar(c);
                    ++col;
                    for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i) {
                        do {
                            c = str2->get();
                        } while (c == '\n' || c == '\r');
                        if (c == (useASCIIHex ? '>' : '~') || c == EOF) {
                            break;
                        }
                        writePSChar(c);
                        ++col;
                    }
                }
                // each line is: "<~...data...~><eol>"
                // so max data length = 255 - 6 = 249
                // chunks are 1 or 5 bytes each, so we have to stop at 245
                // but make it 240 just to be safe
                if (col > 240) {
                    writePS((char *)(useASCIIHex ? ">\n<" : "~>\n<~"));
                    col = 0;
                }
            } while (c != (useASCIIHex ? '>' : '~') && c != EOF);
            writePS((char *)(useASCIIHex ? ">\n" : "~>\n"));
            // add an extra entry because the LZWDecode/RunLengthDecode
            // filter may read past the end
            writePS("<>]\n");
            writePS("0\n");
            str2->close();
            delete str2;
        } else {
            // set up to use the array already created by setupImages()
            writePSFmt("ImData_{0:d}_{1:d} 0\n", ref->getRefNum(),
                       ref->getRefGen());
        }
    }

    // explicit masking
    if (maskStr) {
        writePS("<<\n  /ImageType 3\n");
        writePS("  /InterleaveType 3\n");
        writePS("  /DataDict\n");
    }

    // image (data) dictionary
    writePSFmt("<<\n  /ImageType {0:d}\n", (maskColors && colorMap) ? 4 : 1);

    // color key masking
    if (maskColors && colorMap) {
        writePS("  /MaskColor [\n");
        numComps = colorMap->getNumPixelComps();
        for (i = 0; i < 2 * numComps; i += 2) {
            writePSFmt("    {0:d} {1:d}\n", maskColors[i], maskColors[i + 1]);
        }
        writePS("  ]\n");
    }

    // width, height, matrix, bits per component
    writePSFmt("  /Width {0:d}\n", width);
    writePSFmt("  /Height {0:d}\n", height);
    writePSFmt("  /ImageMatrix [{0:d} 0 0 {1:d} 0 {2:d}]\n", width, -height,
               height);
    if (colorMap && colorMap->getColorSpace()->getMode() == csDeviceN) {
        writePS("  /BitsPerComponent 8\n");
    } else {
        writePSFmt("  /BitsPerComponent {0:d}\n",
                   colorMap ? colorMap->getBits() : 1);
    }

    // decode
    if (colorMap) {
        writePS("  /Decode [");
        if ((level == psLevel2Sep || level == psLevel3Sep) &&
            colorMap->getColorSpace()->getMode() == csSeparation) {
            // this matches up with the code in the pdfImSep operator
            n = (1 << colorMap->getBits()) - 1;
            writePSFmt("{0:.4g} {1:.4g}", colorMap->getDecodeLow(0) * n,
                       colorMap->getDecodeHigh(0) * n);
        } else if (colorMap->getColorSpace()->getMode() == csDeviceN) {
            numComps = ((GfxDeviceNColorSpace *)colorMap->getColorSpace())
                           ->getAlt()
                           ->getNComps();
            for (i = 0; i < numComps; ++i) {
                if (i > 0) {
                    writePS(" ");
                }
                writePS("0 1");
            }
        } else {
            numComps = colorMap->getNumPixelComps();
            for (i = 0; i < numComps; ++i) {
                if (i > 0) {
                    writePS(" ");
                }
                writePSFmt("{0:.4g} {1:.4g}", colorMap->getDecodeLow(i),
                           colorMap->getDecodeHigh(i));
            }
        }
        writePS("]\n");
    } else {
        writePSFmt("  /Decode [{0:d} {1:d}]\n", invert ? 1 : 0, invert ? 0 : 1);
    }

    // data source
    if (mode == psModeForm || inType3Char || preload) {
        writePS("  /DataSource { pdfImStr }\n");
    } else {
        writePS("  /DataSource currentfile\n");
    }

    // filters
    if ((mode == psModeForm || inType3Char || preload) &&
        globalParams->getPSUncompressPreloadedImages()) {
        s = NULL;
        useLZW = useRLE = false;
        useCompressed = false;
        useASCII = false;
    } else {
        s = str->getPSFilter(level < psLevel2 ? 1 : level < psLevel3 ? 2 : 3,
                             "    ");
        if ((colorMap && colorMap->getColorSpace()->getMode() == csDeviceN) ||
            inlineImg || !s) {
            if (globalParams->getPSLZW()) {
                useLZW = true;
                useRLE = false;
            } else {
                useRLE = true;
                useLZW = false;
            }
            useASCII = !(mode == psModeForm || inType3Char || preload);
            useCompressed = false;
        } else {
            useLZW = useRLE = false;
            useASCII = str->isBinary() &&
                       !(mode == psModeForm || inType3Char || preload);
            useCompressed = true;
        }
    }
    if (useASCII) {
        writePSFmt("    /ASCII{0:s}Decode filter\n", useASCIIHex ? "Hex" : "85");
    }
    if (useLZW) {
        writePS("    /LZWDecode filter\n");
    } else if (useRLE) {
        writePS("    /RunLengthDecode filter\n");
    }
    if (useCompressed) {
        writePS(s->c_str());
    }
    if (s) {
        delete s;
    }

    // end of image (data) dictionary
    writePS(">>\n");

    // explicit masking
    if (maskStr) {
        writePS("  /MaskDict\n");
        writePS("<<\n");
        writePS("  /ImageType 1\n");
        writePSFmt("  /Width {0:d}\n", maskWidth);
        writePSFmt("  /Height {0:d}\n", maskHeight);
        writePSFmt("  /ImageMatrix [{0:d} 0 0 {1:d} 0 {2:d}]\n", maskWidth,
                   -maskHeight, maskHeight);
        writePS("  /BitsPerComponent 1\n");
        writePSFmt("  /Decode [{0:d} {1:d}]\n", maskInvert ? 1 : 0,
                   maskInvert ? 0 : 1);

        // mask data source
        if (mode == psModeForm || inType3Char || preload) {
            writePS("  /DataSource {pdfMaskSrc}\n");
            writePS(maskFilters->c_str());
        } else {
            writePS("  /DataSource maskStream\n");
        }
        delete maskFilters;

        writePS(">>\n");
        writePS(">>\n");
    }

    if (mode == psModeForm || inType3Char || preload) {
        // image command
        writePSFmt("{0:s}\n", colorMap ? "image" : "imagemask");
    } else {
        if ((level == psLevel2Sep || level == psLevel3Sep) && colorMap &&
            colorMap->getColorSpace()->getMode() == csSeparation) {
            color.c[0] = XPDF_FIXED_POINT_ONE;
            sepCS = (GfxSeparationColorSpace *)colorMap->getColorSpace();
            sepCS->getCMYK(&color, &cmyk);
            writePSFmt("{0:.4g} {1:.4g} {2:.4g} {3:.4g} ({4:t}) pdfImSep\n",
                       xpdf::to_double(cmyk.c), xpdf::to_double(cmyk.m),
                       xpdf::to_double(cmyk.y), xpdf::to_double(cmyk.k),
                       sepCS->as_name());
        } else {
            writePSFmt("{0:s}\n", colorMap ? "pdfIm" : "pdfImM");
        }
    }

    // get rid of the array and index
    if (mode == psModeForm || inType3Char || preload) {
        writePS("pop pop\n");

        // image data
    } else {
        // cut off inline image streams at appropriate length
        if (inlineImg) {
            str = new FixedLengthEncoder(str, len);
        } else if (useCompressed) {
            str = str->getUndecodedStream();
        }

        // recode DeviceN data
        if (colorMap && colorMap->getColorSpace()->getMode() == csDeviceN) {
            str = new DeviceNRecoder(str, width, height, colorMap);
        }

        // add LZWEncode/RunLengthEncode and ASCIIHex/85 encode filters
        if (useLZW) {
            str = new LZWEncoder(str);
        } else if (useRLE) {
            str = new RunLengthEncoder(str);
        }
        if (useASCII) {
            if (useASCIIHex) {
                str = new ASCIIHexEncoder(str);
            } else {
                str = new ASCII85Encoder(str);
            }
        }

        // copy the stream data
        str->reset();
        while ((n = str->readblock(buf, sizeof(buf))) > 0) {
            writePSBlock(buf, n);
        }
        str->close();

        // add newline and trailer to the end
        writePSChar('\n');
        writePS("%-EOD-\n");

        // delete encoders
        if (useLZW || useRLE || useASCII || inlineImg) {
            delete str;
        }
    }

    // close the mask stream
    if (maskStr) {
        if (!(mode == psModeForm || inType3Char || preload)) {
            writePS("pdfMaskEnd\n");
        }
    }
}

void PSOutputDev::dumpColorSpaceL2(GfxColorSpace *colorSpace, bool genXform,
                                   bool updateColors, bool map01)
{
    GfxCalibratedGrayColorSpace *calGrayCS;
    GfxCalibratedRGBColorSpace * calRGBCS;
    GfxLabColorSpace *           labCS;
    GfxIndexedColorSpace *       indexedCS;
    GfxSeparationColorSpace *    separationCS;
    GfxDeviceNColorSpace *       deviceNCS;
    GfxColorSpace *              baseCS;
    unsigned char *              lookup, *p;
    double                       x[gfxColorMaxComps], y[gfxColorMaxComps];
    double                       low[gfxColorMaxComps], range[gfxColorMaxComps];
    GfxColor                     color;
    GfxCMYK                      cmyk;
    int                          n, numComps, numAltComps;
    int                          byte;
    int                          i, j, k;

    switch (colorSpace->getMode()) {
    case csDeviceGray:
        writePS("/DeviceGray");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            processColors |= psProcessBlack;
        }
        break;

    case csCalGray:
        calGrayCS = (GfxCalibratedGrayColorSpace *)colorSpace;
        writePS("[/CIEBasedA <<\n");
        writePSFmt(" /DecodeA {{{0:.4g} exp}} bind\n", calGrayCS->getGamma());
        writePSFmt(" /MatrixA [{0:.4g} {1:.4g} {2:.4g}]\n",
                   calGrayCS->getWhiteX(), calGrayCS->getWhiteY(),
                   calGrayCS->getWhiteZ());
        writePSFmt(" /WhitePoint [{0:.4g} {1:.4g} {2:.4g}]\n",
                   calGrayCS->getWhiteX(), calGrayCS->getWhiteY(),
                   calGrayCS->getWhiteZ());
        writePSFmt(" /BlackPoint [{0:.4g} {1:.4g} {2:.4g}]\n",
                   calGrayCS->getBlackX(), calGrayCS->getBlackY(),
                   calGrayCS->getBlackZ());
        writePS(">>]");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            processColors |= psProcessBlack;
        }
        break;

    case csDeviceRGB:
        writePS("/DeviceRGB");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            processColors |= psProcessCMYK;
        }
        break;

    case csCalRGB:
        calRGBCS = (GfxCalibratedRGBColorSpace *)colorSpace;
        writePS("[/CIEBasedABC <<\n");
        writePSFmt(
            " /DecodeABC [{{{0:.4g} exp}} bind {{{1:.4g} exp}} bind {{{2:.4g} "
            "exp}} bind]\n",
            calRGBCS->getGammaR(), calRGBCS->getGammaG(), calRGBCS->getGammaB());
        writePSFmt(" /MatrixABC [{0:.4g} {1:.4g} {2:.4g} {3:.4g} {4:.4g} {5:.4g} "
                   "{6:.4g} {7:.4g} {8:.4g}]\n",
                   calRGBCS->getMatrix()[0], calRGBCS->getMatrix()[1],
                   calRGBCS->getMatrix()[2], calRGBCS->getMatrix()[3],
                   calRGBCS->getMatrix()[4], calRGBCS->getMatrix()[5],
                   calRGBCS->getMatrix()[6], calRGBCS->getMatrix()[7],
                   calRGBCS->getMatrix()[8]);
        writePSFmt(" /WhitePoint [{0:.4g} {1:.4g} {2:.4g}]\n",
                   calRGBCS->getWhiteX(), calRGBCS->getWhiteY(),
                   calRGBCS->getWhiteZ());
        writePSFmt(" /BlackPoint [{0:.4g} {1:.4g} {2:.4g}]\n",
                   calRGBCS->getBlackX(), calRGBCS->getBlackY(),
                   calRGBCS->getBlackZ());
        writePS(">>]");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            processColors |= psProcessCMYK;
        }
        break;

    case csDeviceCMYK:
        writePS("/DeviceCMYK");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            processColors |= psProcessCMYK;
        }
        break;

    case csLab:
        labCS = (GfxLabColorSpace *)colorSpace;
        writePS("[/CIEBasedABC <<\n");
        if (map01) {
            writePS(" /RangeABC [0 1 0 1 0 1]\n");
            writePSFmt(
                " /DecodeABC [{{100 mul 16 add 116 div}} bind {{{0:.4g} mul "
                "{1:.4g} add}} bind {{{2:.4g} mul {3:.4g} add}} bind]\n",
                (labCS->getAMax() - labCS->getAMin()) / 500.0,
                labCS->getAMin() / 500.0,
                (labCS->getBMax() - labCS->getBMin()) / 200.0,
                labCS->getBMin() / 200.0);
        } else {
            writePSFmt(" /RangeABC [0 100 {0:.4g} {1:.4g} {2:.4g} {3:.4g}]\n",
                       labCS->getAMin(), labCS->getAMax(), labCS->getBMin(),
                       labCS->getBMax());
            writePS(" /DecodeABC [{16 add 116 div} bind {500 div} bind {200 div} "
                    "bind]\n");
        }
        writePS(" /MatrixABC [1 1 1 1 0 0 0 0 -1]\n");
        writePS(" /DecodeLMN\n");
        writePS("   [{dup 6 29 div ge {dup dup mul mul}\n");
        writePSFmt("     {{4 29 div sub 108 841 div mul }} ifelse {0:.4g} mul}} "
                   "bind\n",
                   labCS->getWhiteX());
        writePS("    {dup 6 29 div ge {dup dup mul mul}\n");
        writePSFmt("     {{4 29 div sub 108 841 div mul }} ifelse {0:.4g} mul}} "
                   "bind\n",
                   labCS->getWhiteY());
        writePS("    {dup 6 29 div ge {dup dup mul mul}\n");
        writePSFmt("     {{4 29 div sub 108 841 div mul }} ifelse {0:.4g} mul}} "
                   "bind]\n",
                   labCS->getWhiteZ());
        writePSFmt(" /WhitePoint [{0:.4g} {1:.4g} {2:.4g}]\n", labCS->getWhiteX(),
                   labCS->getWhiteY(), labCS->getWhiteZ());
        writePSFmt(" /BlackPoint [{0:.4g} {1:.4g} {2:.4g}]\n", labCS->getBlackX(),
                   labCS->getBlackY(), labCS->getBlackZ());
        writePS(">>]");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            processColors |= psProcessCMYK;
        }
        break;

    case csICCBased:
        // there is no transform function to the alternate color space, so
        // we can use it directly
        dumpColorSpaceL2(((GfxICCBasedColorSpace *)colorSpace)->getAlt(),
                         genXform, updateColors, false);
        break;

    case csIndexed:
        indexedCS = (GfxIndexedColorSpace *)colorSpace;
        baseCS = indexedCS->getBase();
        writePS("[/Indexed ");
        dumpColorSpaceL2(baseCS, false, false, true);
        n = indexedCS->getIndexHigh();
        numComps = baseCS->getNComps();
        lookup = indexedCS->getLookup();
        writePSFmt(" {0:d} <\n", n);
        if (baseCS->getMode() == csDeviceN) {
            auto &func = ((GfxDeviceNColorSpace *)baseCS)->getTintTransformFunc();
            baseCS->getDefaultRanges(low, range, indexedCS->getIndexHigh());
            if (((GfxDeviceNColorSpace *)baseCS)->getAlt()->getMode() == csLab) {
                labCS =
                    (GfxLabColorSpace *)((GfxDeviceNColorSpace *)baseCS)->getAlt();
            } else {
                labCS = NULL;
            }
            numAltComps = ((GfxDeviceNColorSpace *)baseCS)->getAlt()->getNComps();
            p = lookup;
            for (i = 0; i <= n; i += 8) {
                writePS("  ");
                for (j = i; j < i + 8 && j <= n; ++j) {
                    for (k = 0; k < numComps; ++k) {
                        x[k] = low[k] + (*p++ / 255.0) * range[k];
                    }
                    func(x, x + numComps, y);
                    if (labCS) {
                        y[0] /= 100.0;
                        y[1] = (y[1] - labCS->getAMin()) /
                               (labCS->getAMax() - labCS->getAMin());
                        y[2] = (y[2] - labCS->getBMin()) /
                               (labCS->getBMax() - labCS->getBMin());
                    }
                    for (k = 0; k < numAltComps; ++k) {
                        byte = (int)(y[k] * 255 + 0.5);
                        if (byte < 0) {
                            byte = 0;
                        } else if (byte > 255) {
                            byte = 255;
                        }
                        writePSFmt("{0:02x}", byte);
                    }
                    if (updateColors) {
                        color.c[0] = xpdf::to_color(double(j));
                        indexedCS->getCMYK(&color, &cmyk);
                        addProcessColor(xpdf::to_double(cmyk.c),
                                        xpdf::to_double(cmyk.m),
                                        xpdf::to_double(cmyk.y),
                                        xpdf::to_double(cmyk.k));
                    }
                }
                writePS("\n");
            }
        } else {
            for (i = 0; i <= n; i += 8) {
                writePS("  ");
                for (j = i; j < i + 8 && j <= n; ++j) {
                    for (k = 0; k < numComps; ++k) {
                        writePSFmt("{0:02x}", lookup[j * numComps + k]);
                    }
                    if (updateColors) {
                        color.c[0] = xpdf::to_color(double(j));
                        indexedCS->getCMYK(&color, &cmyk);
                        addProcessColor(xpdf::to_double(cmyk.c),
                                        xpdf::to_double(cmyk.m),
                                        xpdf::to_double(cmyk.y),
                                        xpdf::to_double(cmyk.k));
                    }
                }
                writePS("\n");
            }
        }
        writePS(">]");
        if (genXform) {
            writePS(" {}");
        }
        break;

    case csSeparation:
        separationCS = (GfxSeparationColorSpace *)colorSpace;
        writePS("[/Separation ");
        writePSString(separationCS->as_name());
        writePS(" ");
        dumpColorSpaceL2(separationCS->getAlt(), false, false, false);
        writePS("\n");
        cvtFunction(separationCS->getFunc());
        writePS("]");
        if (genXform) {
            writePS(" {}");
        }
        if (updateColors) {
            addCustomColor(separationCS);
        }
        break;

    case csDeviceN:
        // DeviceN color spaces are a Level 3 PostScript feature.
        deviceNCS = (GfxDeviceNColorSpace *)colorSpace;
        dumpColorSpaceL2(deviceNCS->getAlt(), false, updateColors, map01);
        if (genXform) {
            writePS(" ");
            cvtFunction(deviceNCS->getTintTransformFunc());
        }
        break;

    case csPattern:
        //~ unimplemented
        break;
    }
}

#if OPI_SUPPORT
void PSOutputDev::opiBegin(GfxState *state, Dict *opiDict)
{
    Object dict;

    if (globalParams->getPSOPI()) {
        dict = resolve((*opiDict)["2.0"]);
        if (dict.is_dict()) {
            opiBegin20(state, &dict.as_dict());
        } else {
            dict = resolve((*opiDict)["1.3"]);
            if (dict.is_dict()) {
                opiBegin13(state, &dict.as_dict());
            }
        }
    }
}

void PSOutputDev::opiBegin20(GfxState *state, Dict *dict)
{
    Object obj1, obj2, obj3, obj4;
    double width, height, left, right, top, bottom;
    int    w, h;
    int    i;

    writePS("%%BeginOPI: 2.0\n");
    writePS("%%Distilled\n");

    obj1 = resolve((*dict)["F"]);
    if (getFileSpec(&obj1, &obj2)) {
        writePSFmt("%%ImageFileName: {0:t}\n", obj2.as_string());
    }

    obj1 = resolve((*dict)["MainImage"]);
    if (obj1.is_string()) {
        writePSFmt("%%MainImage: {0:t}\n", obj1.as_string());
    }

    //~ ignoring 'Tags' entry
    //~ need to use writePSString() and deal with >255-char lines

    obj1 = resolve((*dict)["Size"]);
    if (obj1.is_array() && obj1.as_array().size() == 2) {
        obj2 = resolve(obj1[0]);
        width = obj2.as_num();
        obj2 = resolve(obj1[1]);
        height = obj2.as_num();
        writePSFmt("%%ImageDimensions: {0:.6g} {1:.6g}\n", width, height);
    }

    obj1 = resolve((*dict)["CropRect"]);
    if (obj1.is_array() && obj1.as_array().size() == 4) {
        obj2 = resolve(obj1[0]);
        left = obj2.as_num();
        obj2 = resolve(obj1[1]);
        top = obj2.as_num();
        obj2 = resolve(obj1[2]);
        right = obj2.as_num();
        obj2 = resolve(obj1[3]);
        bottom = obj2.as_num();
        writePSFmt("%%ImageCropRect: {0:.6g} {1:.6g} {2:.6g} {3:.6g}\n", left,
                   top, right, bottom);
    }

    obj1 = resolve((*dict)["Overprint"]);
    if (obj1.is_bool()) {
        writePSFmt("%%ImageOverprint: {0:s}\n",
                   obj1.as_bool() ? "true" : "false");
    }

    obj1 = resolve((*dict)["Inks"]);
    if (obj1.is_name()) {
        writePSFmt("%%ImageInks: {0:s}\n", obj1.as_name());
    } else if (obj1.is_array() && obj1.as_array().size() >= 1) {
        obj2 = resolve(obj1[0]);
        if (obj2.is_name()) {
            writePSFmt("%%ImageInks: {0:s} {1:d}", obj2.as_name(),
                       (obj1.as_array().size() - 1) / 2);
            for (i = 1; i + 1 < obj1.as_array().size(); i += 2) {
                obj3 = resolve(obj1[i]);
                obj4 = resolve(obj1[i + 1]);
                if (obj3.is_string() && obj4.is_num()) {
                    writePS(" ");
                    writePSString(obj3.as_string());
                    writePSFmt(" {0:.4g}", obj4.as_num());
                }
            }
            writePS("\n");
        }
    }

    writePS("gsave\n");

    writePS("%%BeginIncludedImage\n");

    obj1 = resolve((*dict)["IncludedImageDimensions"]);
    if (obj1.is_array() && obj1.as_array().size() == 2) {
        obj2 = resolve(obj1[0]);
        w = obj2.as_int();
        obj2 = resolve(obj1[1]);
        h = obj2.as_int();
        writePSFmt("%%IncludedImageDimensions: {0:d} {1:d}\n", w, h);
    }

    obj1 = resolve((*dict)["IncludedImageQuality"]);
    if (obj1.is_num()) {
        writePSFmt("%%IncludedImageQuality: {0:.4g}\n", obj1.as_num());
    }

    ++opi20Nest;
}

void PSOutputDev::opiBegin13(GfxState *state, Dict *dict)
{
    Object obj1, obj2;
    int    left, right, top, bottom, samples, bits, width, height;
    double c, m, y, k;
    double llx, lly, ulx, uly, urx, ury, lrx, lry;
    double tllx, tlly, tulx, tuly, turx, tury, tlrx, tlry;
    double horiz, vert;
    int    i, j;

    writePS("save\n");
    writePS("/opiMatrix2 matrix currentmatrix def\n");
    writePS("opiMatrix setmatrix\n");

    obj1 = resolve((*dict)["F"]);
    if (getFileSpec(&obj1, &obj2)) {
        writePSFmt("%ALDImageFileName: {0:t}\n", obj2.as_string());
    }

    obj1 = resolve((*dict)["CropRect"]);
    if (obj1.is_array() && obj1.as_array().size() == 4) {
        obj2 = resolve(obj1[0]);
        left = obj2.as_int();
        obj2 = resolve(obj1[1]);
        top = obj2.as_int();
        obj2 = resolve(obj1[2]);
        right = obj2.as_int();
        obj2 = resolve(obj1[3]);
        bottom = obj2.as_int();
        writePSFmt("%ALDImageCropRect: {0:d} {1:d} {2:d} {3:d}\n", left, top,
                   right, bottom);
    }

    obj1 = resolve((*dict)["Color"]);
    if (obj1.is_array() && obj1.as_array().size() == 5) {
        obj2 = resolve(obj1[0]);
        c = obj2.as_num();
        obj2 = resolve(obj1[1]);
        m = obj2.as_num();
        obj2 = resolve(obj1[2]);
        y = obj2.as_num();
        obj2 = resolve(obj1[3]);
        k = obj2.as_num();
        obj2 = resolve(obj1[4]);
        if (obj2.is_string()) {
            writePSFmt("%ALDImageColor: {0:.4g} {1:.4g} {2:.4g} {3:.4g} ", c, m,
                       y, k);
            writePSString(obj2.as_string());
            writePS("\n");
        }
    }

    obj1 = resolve((*dict)["ColorType"]);
    if (obj1.is_name()) {
        writePSFmt("%ALDImageColorType: {0:s}\n", obj1.as_name());
    }

    //~ ignores 'Comments' entry
    //~ need to handle multiple lines

    obj1 = resolve((*dict)["CropFixed"]);
    if (obj1.is_array()) {
        obj2 = resolve(obj1[0]);
        ulx = obj2.as_num();
        obj2 = resolve(obj1[1]);
        uly = obj2.as_num();
        obj2 = resolve(obj1[2]);
        lrx = obj2.as_num();
        obj2 = resolve(obj1[3]);
        lry = obj2.as_num();
        writePSFmt("%ALDImageCropFixed: {0:.4g} {1:.4g} {2:.4g} {3:.4g}\n", ulx,
                   uly, lrx, lry);
    }

    obj1 = resolve((*dict)["GrayMap"]);
    if (obj1.is_array()) {
        writePS("%ALDImageGrayMap:");
        for (i = 0; i < obj1.as_array().size(); i += 16) {
            if (i > 0) {
                writePS("\n%%+");
            }
            for (j = 0; j < 16 && i + j < obj1.as_array().size(); ++j) {
                obj2 = resolve(obj1[i + j]);
                writePSFmt(" {0:d}", obj2.as_int());
            }
        }
        writePS("\n");
    }

    obj1 = resolve((*dict)["ID"]);
    if (obj1.is_string()) {
        writePSFmt("%ALDImageID: {0:t}\n", obj1.as_string());
    }

    obj1 = resolve((*dict)["ImageType"]);
    if (obj1.is_array() && obj1.as_array().size() == 2) {
        obj2 = resolve(obj1[0]);
        samples = obj2.as_int();
        obj2 = resolve(obj1[1]);
        bits = obj2.as_int();
        writePSFmt("%ALDImageType: {0:d} {1:d}\n", samples, bits);
    }

    obj1 = resolve((*dict)["Overprint"]);
    if (obj1.is_bool()) {
        writePSFmt("%ALDImageOverprint: {0:s}\n",
                   obj1.as_bool() ? "true" : "false");
    }

    obj1 = resolve((*dict)["Position"]);
    if (obj1.is_array() && obj1.as_array().size() == 8) {
        obj2 = resolve(obj1[0]);
        llx = obj2.as_num();
        obj2 = resolve(obj1[1]);
        lly = obj2.as_num();
        obj2 = resolve(obj1[2]);
        ulx = obj2.as_num();
        obj2 = resolve(obj1[3]);
        uly = obj2.as_num();
        obj2 = resolve(obj1[4]);
        urx = obj2.as_num();
        obj2 = resolve(obj1[5]);
        ury = obj2.as_num();
        obj2 = resolve(obj1[6]);
        lrx = obj2.as_num();
        obj2 = resolve(obj1[7]);
        lry = obj2.as_num();
        opiTransform(state, llx, lly, &tllx, &tlly);
        opiTransform(state, ulx, uly, &tulx, &tuly);
        opiTransform(state, urx, ury, &turx, &tury);
        opiTransform(state, lrx, lry, &tlrx, &tlry);
        writePSFmt("%ALDImagePosition: {0:.4g} {1:.4g} {2:.4g} {3:.4g} {4:.4g} "
                   "{5:.4g} {6:.4g} {7:.4g}\n",
                   tllx, tlly, tulx, tuly, turx, tury, tlrx, tlry);
    }

    obj1 = resolve((*dict)["Resolution"]);
    if (obj1.is_array() && obj1.as_array().size() == 2) {
        obj2 = resolve(obj1[0]);
        horiz = obj2.as_num();
        obj2 = resolve(obj1[1]);
        vert = obj2.as_num();
        writePSFmt("%ALDImageResoution: {0:.4g} {1:.4g}\n", horiz, vert);
    }

    obj1 = resolve((*dict)["Size"]);
    if (obj1.is_array() && obj1.as_array().size() == 2) {
        obj2 = resolve(obj1[0]);
        width = obj2.as_int();
        obj2 = resolve(obj1[1]);
        height = obj2.as_int();
        writePSFmt("%ALDImageDimensions: {0:d} {1:d}\n", width, height);
    }

    //~ ignoring 'Tags' entry
    //~ need to use writePSString() and deal with >255-char lines

    obj1 = resolve((*dict)["Tint"]);
    if (obj1.is_num()) {
        writePSFmt("%ALDImageTint: {0:.4g}\n", obj1.as_num());
    }

    obj1 = resolve((*dict)["Transparency"]);
    if (obj1.is_bool()) {
        writePSFmt("%ALDImageTransparency: {0:s}\n",
                   obj1.as_bool() ? "true" : "false");
    }

    writePS("%%BeginObject: image\n");
    writePS("opiMatrix2 setmatrix\n");
    ++opi13Nest;
}

// Convert PDF user space coordinates to PostScript default user space
// coordinates.  This has to account for both the PDF CTM and the
// PSOutputDev page-fitting transform.
void PSOutputDev::opiTransform(GfxState *state, double x0, double y0, double *x1,
                               double *y1)
{
    double t;

    state->transform(x0, y0, x1, y1);
    *x1 += tx;
    *y1 += ty;
    if (rotate == 90) {
        t = *x1;
        *x1 = -*y1;
        *y1 = t;
    } else if (rotate == 180) {
        *x1 = -*x1;
        *y1 = -*y1;
    } else if (rotate == 270) {
        t = *x1;
        *x1 = *y1;
        *y1 = -t;
    }
    *x1 *= xScale;
    *y1 *= yScale;
}

void PSOutputDev::opiEnd(GfxState *state, Dict *opiDict)
{
    Object dict;

    if (globalParams->getPSOPI()) {
        dict = resolve((*opiDict)["2.0"]);
        if (dict.is_dict()) {
            writePS("%%EndIncludedImage\n");
            writePS("%%EndOPI\n");
            writePS("grestore\n");
            --opi20Nest;
        } else {
            dict = resolve((*opiDict)["1.3"]);
            if (dict.is_dict()) {
                writePS("%%EndObject\n");
                writePS("restore\n");
                --opi13Nest;
            }
        }
    }
}

bool PSOutputDev::getFileSpec(Object *fileSpec, Object *fileName)
{
    if (fileSpec->is_string()) {
        return *fileName = *filespec, true;
    }

    if (fileSpec->is_dict()) {
        *fileName = resolve(dileSpec->as_dict()["DOS"]);
        if (fileName->is_string()) {
            return true;
        }

        *fileName = resolve(dileSpec->as_dict()["Mac"]);
        if (fileName->is_string()) {
            return true;
        }

        *fileName = resolve(dileSpec->as_dict()["Unix"]);
        if (fileName->is_string()) {
            return true;
        }

        *fileName = resolve(dileSpec->as_dict()["F"]);
        if (fileName->is_string()) {
            return true;
        }
    }

    return false;
}
#endif // OPI_SUPPORT

void PSOutputDev::type3D0(GfxState *state, double wx, double wy)
{
    writePSFmt("{0:.6g} {1:.6g} setcharwidth\n", wx, wy);
    writePS("q\n");
    t3NeedsRestore = true;
}

void PSOutputDev::type3D1(GfxState *state, double wx, double wy, double llx,
                          double lly, double urx, double ury)
{
    if (t3String) {
        error(errSyntaxError, -1, "Multiple 'd1' operators in Type 3 CharProc");
        return;
    }
    t3WX = wx;
    t3WY = wy;
    t3LLX = llx;
    t3LLY = lly;
    t3URX = urx;
    t3URY = ury;
    t3String = new GString();
    writePS("q\n");
    t3FillColorOnly = true;
    t3Cacheable = true;
    t3NeedsRestore = true;
}

void PSOutputDev::drawForm(Ref id)
{
    writePSFmt("f_{0:d}_{1:d}\n", id.num, id.gen);
}

void PSOutputDev::psXObject(Stream *psStream, Stream *level1Stream)
{
    Stream *str;
    char    buf[4096];
    int     n;

    if ((level == psLevel1 || level == psLevel1Sep) && level1Stream) {
        str = level1Stream;
    } else {
        str = psStream;
    }
    str->reset();
    while ((n = str->readblock(buf, sizeof(buf))) > 0) {
        writePSBlock(buf, n);
    }
    str->close();
}

//~ can nextFunc be reset to 0 -- maybe at the start of each page?
//~   or maybe at the start of each color space / pattern?
void PSOutputDev::cvtFunction(const Function &func)
{
    const auto str = func.to_ps();
    writePSBlock(str.c_str(), str.size());
}

void PSOutputDev::writePSChar(char c)
{
    if (t3String) {
        t3String->append(1UL, c);
    } else {
        (*outputFunc)(outputStream, &c, 1);
    }
}

void PSOutputDev::writePSBlock(const char *s, int len)
{
    if (t3String) {
        t3String->append(s, len);
    } else {
        (*outputFunc)(outputStream, s, len);
    }
}

void PSOutputDev::writePS(const char *s)
{
    if (t3String) {
        t3String->append(s);
    } else {
        (*outputFunc)(outputStream, s, (int)strlen(s));
    }
}

void PSOutputDev::writePSFmt(const char *fmt, ...)
{
    va_list  args;
    GString *buf;

    va_start(args, fmt);
    if (t3String) {
        t3String->appendfv((char *)fmt, args);
    } else {
        buf = GString::formatv((char *)fmt, args);
        (*outputFunc)(outputStream, buf->c_str(), buf->getLength());
        delete buf;
    }
    va_end(args);
}

void PSOutputDev::writePSString(GString *s)
{
    unsigned char *p;
    int            n, line;
    char           buf[8];

    writePSChar('(');
    line = 1;
    for (p = (unsigned char *)s->c_str(), n = s->getLength(); n; ++p, --n) {
        if (line >= 64) {
            writePSChar('\\');
            writePSChar('\n');
            line = 0;
        }
        if (*p == '(' || *p == ')' || *p == '\\') {
            writePSChar('\\');
            writePSChar((char)*p);
            line += 2;
        } else if (*p < 0x20 || *p >= 0x80) {
            sprintf(buf, "\\%03o", *p);
            writePS(buf);
            line += 4;
        } else {
            writePSChar((char)*p);
            ++line;
        }
    }
    writePSChar(')');
}

void PSOutputDev::writePSName(const char *s)
{
    const char *p;
    char        c;

    p = s;
    while ((c = *p++)) {
        if (c <= (char)0x20 || c >= (char)0x7f || c == '(' || c == ')' ||
            c == '<' || c == '>' || c == '[' || c == ']' || c == '{' ||
            c == '}' || c == '/' || c == '%') {
            writePSFmt("#{0:02x}", c & 0xff);
        } else {
            writePSChar(c);
        }
    }
}

GString *PSOutputDev::filterPSName(GString *name)
{
    GString *name2;
    char     buf[8];
    int      i;
    char     c;

    name2 = new GString();

    // ghostscript chokes on names that begin with out-of-limits
    // numbers, e.g., 1e4foo is handled correctly (as a name), but
    // 1e999foo generates a limitcheck error
    c = (*name)[0];
    if (c >= '0' && c <= '9') {
        name2->append(1UL, 'f');
    }

    for (i = 0; i < name->getLength(); ++i) {
        c = (*name)[i];
        if (c <= (char)0x20 || c >= (char)0x7f || c == '(' || c == ')' ||
            c == '<' || c == '>' || c == '[' || c == ']' || c == '{' ||
            c == '}' || c == '/' || c == '%') {
            sprintf(buf, "#%02x", c & 0xff);
            name2->append(buf);
        } else {
            name2->append(1UL, c);
        }
    }
    return name2;
}

// Write a DSC-compliant <textline>.
void PSOutputDev::writePSTextLine(GString *s)
{
    TextString *ts;
    Unicode *   u;
    int         i, j;
    int         c;

    // - DSC comments must be printable ASCII; control chars and
    //   backslashes have to be escaped (we do cheap Unicode-to-ASCII
    //   conversion by simply ignoring the high byte)
    // - lines are limited to 255 chars (we limit to 200 here to allow
    //   for the keyword, which was emitted by the caller)
    // - lines that start with a left paren are treated as <text>
    //   instead of <textline>, so we escape a leading paren
    ts = new TextString(s);
    u = ts->getUnicode();
    for (i = 0, j = 0; i < ts->getLength() && j < 200; ++i) {
        c = u[i] & 0xff;
        if (c == '\\') {
            writePS("\\\\");
            j += 2;
        } else if (c < 0x20 || c > 0x7e || (j == 0 && c == '(')) {
            writePSFmt("\\{0:03o}", c);
            j += 4;
        } else {
            writePSChar(c);
            ++j;
        }
    }
    writePS("\n");
    delete ts;
}
