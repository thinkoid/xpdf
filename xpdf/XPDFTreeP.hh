// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_XPDFTREEP_HH
#define XPDF_XPDF_XPDFTREEP_HH

#include <defs.hh>
#include <Xm/ManagerP.h>
#include <xpdf/XPDFTree.hh>

extern "C" {

typedef void (*XPDFLayoutProc) (Widget widget, Widget instigator);
typedef void (*XPDFCalcSizeProc) (
    Widget widget, Widget instigator, Dimension* totalWidth,
    Dimension* totalHeight);
typedef Boolean (*XPDFNeedRelayoutProc) (Widget oldWidget, Widget newWidget);

#define XPDFInheritCreateGC ((XtWidgetProc)_XtInherit)
#define XPDFInheritDestroyGC ((XtWidgetProc)_XtInherit)
#define XPDFInheritLayout ((XPDFLayoutProc)_XtInherit)
#define XPDFInheritCalcSize ((XPDFCalcSizeProc)_XtInherit)
#define XPDFInheritNeedRelayout ((XPDFNeedRelayoutProc)_XtInherit)

typedef struct {
    XtWidgetProc createGC;
    XtWidgetProc destroyGC;
    XPDFLayoutProc layout;
    XPDFCalcSizeProc calcSize;
    XPDFNeedRelayoutProc needRelayout;
    XtPointer extension;
} XPDFTreeClassPart;

typedef struct _XPDFTreeClassRec {
    CoreClassPart coreClass;
    CompositeClassPart compositeClass;
    ConstraintClassPart constraintClass;
    XmManagerClassPart managerClass;
    XPDFTreeClassPart treeClass;
} XPDFTreeClassRec;

externalref XPDFTreeClassRec xpdfTreeClassRec;

typedef struct _XPDFTreeEntry XPDFTreeEntry;

typedef struct {
    Dimension marginWidth;
    Dimension marginHeight;
    XtCallbackList selectCallback;
    GC plainGC;
    GC dottedGC;
    XPDFTreeEntry* root;
    int redrawY;
} XPDFTreePart;

typedef struct _XPDFTreeRec {
    CorePart core;
    CompositePart composite;
    ConstraintPart constraint;
    XmManagerPart manager;
    XPDFTreePart tree;
} XPDFTreeRec;

#define XPDFTreeIndex (XmManagerIndex + 1)

typedef struct _XPDFTreeConstraintPart {
    Widget entryParent;
    Boolean entryExpanded;
    int entryPosition;
    XPDFTreeEntry* e;
} XPDFTreeConstraintPart, *XPDFTreeConstraint;

typedef struct _XPDFTreeConstraintRec {
    XmManagerConstraintPart manager;
    XPDFTreeConstraintPart tree;
} XPDFTreeConstraintRec, *XPDFTreeConstraintPtr;

#define XPDFTreeCPart(w) \
    (&((XPDFTreeConstraintPtr) (w)->core.constraints)->tree)

} // extern "C"

#endif // XPDF_XPDF_XPDFTREEP_HH
