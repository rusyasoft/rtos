project 'linux'
    kind        'StaticLib'
    targetname  'linux.ko'

    build.compileProperty('x86_64')

    includedirs { '.', '../../kernel/src', '../../lib/core/include' }
    removefiles { 'packetngin/**.c' }
