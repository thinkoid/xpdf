project('xpdf', 'cpp', version : '3.0.4', license : 'GPL',
        default_options : ['cpp_std=c++20', 'build.cpp_std=c++20'])

cpp_warnings = [
    '-W',
    '-Wall',
    '-Wno-sign-compare',
    '-Wno-parentheses',
    '-Wno-deprecated-declarations',
    '-Wno-empty-body',
    '-Wno-unused-parameter',
    '-Wno-char-subscripts',
    '-Wno-write-strings',
    '-Wno-implicit-fallthrough',
    '-Wno-format-overflow',
    '-Wno-stringop-truncation',
    '-Wno-stringop-overflow'
]

add_global_arguments(cpp_warnings, language : 'cpp')

boost_dep = dependency('boost', modules : [ 'filesystem', 'iostreams', 'program_options', 'system' ])
fmt_dep = dependency('fmt')

freetype2_dep = dependency('freetype2')

libpaper_dep = dependency('libpaper')
libpng_dep = dependency('libpng')

X11_dep = dependency('x11')
Xpm_dep = dependency('xpm')
Xt_dep = dependency('xt')
motif_dep = dependency('motif')

test_dep = dependency('boost', modules : ['unit_test_framework'])

top_INCLUDES       = include_directories('.')
fofi_INCLUDES      = include_directories('fofi')
utils_INCLUDES     = include_directories('utils')
splash_INCLUDES    = include_directories('splash')
iostreams_INCLUDES = include_directories('iostreams')
xpdf_INCLUDES      = include_directories('xpdf')
test_INCLUDES      = include_directories('test')

subdir('fofi')
subdir('utils')
subdir('splash')
subdir('xpdf')
# subdir('test')
