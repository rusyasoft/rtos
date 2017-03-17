project 'loader'
    kind        'ConsoleApp'

    build.compileProperty('x86')

    targetname  'loader.bin'
    linkoptions '-T linker.ld -e start'
