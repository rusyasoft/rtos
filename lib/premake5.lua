include 'tlsf'
include 'lwip'
include 'core'

project 'lib'
    kind        'Makefile'
    location    '.'

    buildcommands {
        'make -C tlsf',
        'make -C core',
        'make -C lwip',

        'make -f extern.make'
    }

    cleancommands {
        'make clean -C core',
        'make clean -C lwip',

        'make -f extern.make clean'
    }
