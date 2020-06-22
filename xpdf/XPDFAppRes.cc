//========================================================================
//
// XPDFAppRes.c
//
//========================================================================

#include <xpdf/XPDFAppRes.hh>

static int defRGBCubeSize = XPDF_RGBCUBE_MAX;

static Bool defInstallCmap = False;
static Bool defReverseVideo = False;

static char *fallbackResourcesData[] = {
    "*.zoomComboBox*fontList: "
    "-*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1",
    "*XmTextField.fontList: -*-courier-medium-r-normal--12-*-*-*-*-*-iso8859-1",
    "*.fontList: -*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1",
    "*XmTextField.translations: #override\\n"
    "  Ctrl<Key>a:beginning-of-line()\\n"
    "  Ctrl<Key>b:backward-character()\\n"
    "  Ctrl<Key>d:delete-next-character()\\n"
    "  Ctrl<Key>e:end-of-line()\\n"
    "  Ctrl<Key>f:forward-character()\\n"
    "  Ctrl<Key>u:beginning-of-line()delete-to-end-of-line()\\n"
    "  Ctrl<Key>k:delete-to-end-of-line()\\n",
    "*.toolTipEnable: True",
    "*.toolTipPostDelay: 1500",
    "*.toolTipPostDuration: 0",
    "*.TipLabel.foreground: black",
    "*.TipLabel.background: LightYellow",
    "*.TipShell.borderWidth: 1",
    "*.TipShell.borderColor: black",
    0
};

char **fallbackResources()
{
    return fallbackResourcesData;
}

size_t fallbackResourcesSize()
{
    return sizeof(fallbackResourcesData) / sizeof(char *);
}

static XrmOptionDescRec xOptsData[] = {
    { "-display", ".display", XrmoptionSepArg, 0 },
    { "-foreground", "*Foreground", XrmoptionSepArg, 0 },
    { "-fg", "*Foreground", XrmoptionSepArg, 0 },
    { "-background", "*Background", XrmoptionSepArg, 0 },
    { "-bg", "*Background", XrmoptionSepArg, 0 },
    { "-geometry", ".geometry", XrmoptionSepArg, 0 },
    { "-g", ".geometry", XrmoptionSepArg, 0 },
    { "-font", "*.fontList", XrmoptionSepArg, 0 },
    { "-fn", "*.fontList", XrmoptionSepArg, 0 },
    { "-title", ".title", XrmoptionSepArg, 0 },
    { "-cmap", ".installCmap", XrmoptionNoArg, (XPointer) "on" },
    { "-rgb", ".rgbCubeSize", XrmoptionSepArg, 0 },
    { "-rv", ".reverseVideo", XrmoptionNoArg, (XPointer) "true" },
    { "-papercolor", ".paperColor", XrmoptionSepArg, 0 },
    { "-mattecolor", ".matteColor", XrmoptionSepArg, 0 },
    { "-z", ".initialZoom", XrmoptionSepArg, 0 }
};

XrmOptionDescRec *xOpts()
{
    return xOptsData;
}

size_t xOptsSize()
{
    return sizeof(xOptsData) / sizeof(XrmOptionDescRec);
}

static XtResource xResourcesData[] = {
#define T(a, b, type, other, mb, ptr)                                            \
    {                                                                            \
        a, b, type, sizeof(other), XtOffsetOf(struct XPDFAppResources, mb),      \
            type, (XtPointer)ptr                                                 \
    }

    T("geometry", "Geometry", XtRString, String, geometry, 0),
    T("title", "Title", XtRString, String, title, 0),
    T("installCmap", "InstallCmap", XtRBool, Bool, installCmap, &defInstallCmap),
    T("rgbCubeSize", "RgbCubeSize", XtRInt, int, rgbCubeSize, &defRGBCubeSize),
    T("reverseVideo", "ReverseVideo", XtRBool, Bool, reverseVideo,
      &defReverseVideo),
    T("paperColor", "PaperColor", XtRString, String, paperColor, 0),
    T("matteColor", "MatteColor", XtRString, String, matteColor, "gray50"),
    T("fullScreenMatteColor", "FullScreenMatteColor", XtRString, String,
      fullScreenMatteColor, "black"),
    T("initialZoom", "InitialZoom", XtRString, String, initialZoom, 0)
#undef T
};

XtResource *xResources()
{
    return xResourcesData;
}

size_t xResourcesSize()
{
    return sizeof(xResourcesData) / sizeof(XtResource);
}
