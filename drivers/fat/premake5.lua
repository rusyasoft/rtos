project 'fat'
    kind        'StaticLib'
    targetname  'fat.ko'

    build.compileProperty('x86_64')

    linkoptions { '-r ../linux.ko' }
    includedirs {  '../../kernel/src', '../../lib/core/include' }
