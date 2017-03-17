rootPath = os.getcwd()
build = require 'build'

workspace 'PacketNgin'
    configurations  { 'debug', 'release' }

    filter 'configurations:debug'
        symbols 'On'
    filter 'configurations:release'
        optimize 'On'
    filter {}

    warnings        'Extra'
    buildoptions    '-ffreestanding -std=gnu99'
    linkoptions     '-nostdlib'

include 'lib'
include 'loader'
include 'kernel'
include 'drivers'

project 'build'
    kind 'Makefile'

    buildcommands {
        'make -C loader',
        'make -C kernel',
        'make -C lib',
        'make -C tools',

        --'bin/premake5 --file=image.lua',
    }

    cleancommands {
        'make clean -C loader',
        'make clean -C kernel',
        'make clean -C lib',
        'make clean -C tools'

        --'rm -rf kernel.smap kernel.bin system.img'
    }
