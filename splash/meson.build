libsplash_SOURCES = [
    'Splash.cc',
    'SplashBitmap.cc',
    'SplashClip.cc',
    'SplashFTFont.cc',
    'SplashFTFontEngine.cc',
    'SplashFTFontFile.cc',
    'SplashFont.cc',
    'SplashFontEngine.cc',
    'SplashFontFile.cc',
    'SplashFontFileID.cc',
    'SplashPath.cc',
    'SplashPattern.cc',
    'SplashScreen.cc',
    'SplashState.cc',
    'SplashXPath.cc',
    'SplashXPathScanner.cc'
]

libsplash = static_library(
    'splash', libsplash_SOURCES,
    include_directories : [
        top_INCLUDES,
        fofi_INCLUDES,
        utils_INCLUDES,
        splash_INCLUDES,
    ],
    dependencies : [freetype2_dep], install : false)
