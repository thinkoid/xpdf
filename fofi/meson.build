libfofi_SOURCES = [
    'FoFiBase.cc',
    'FoFiEncodings.cc',
    'FoFiIdentifier.cc',
    'FoFiTrueType.cc',
    'FoFiType1.cc',
    'FoFiType1C.cc'
]

libfofi = static_library(
    'fofi', libfofi_SOURCES,
    include_directories : [
        top_INCLUDES, fofi_INCLUDES
    ],
    dependencies : boost_dep, install : false)
