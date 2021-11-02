libutils_SOURCES = [
    'path.cc',
    'memory.cc',
    'parseargs.cc',
    'string.cc',
    'GHash.cc',
    'GList.cc',
    'GString.cc'
]

libutils = static_library(
    'utils', libutils_SOURCES,
    include_directories : [
        top_INCLUDES,
        fofi_INCLUDES,
        utils_INCLUDES
    ],
    dependencies : boost_dep, install : false)
