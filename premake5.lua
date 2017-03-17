require 'build'

workspace 'PacketNgin'
    configurations { 'debug', 'release' }

    warnings        'Extra'
    buildoptions    '-ffreestanding'
    linkoptions     '-nostdlib'

include 'loader'
include 'kernel'

project 'Build'
    kind 'Makefile'

    buildcommands {
        'make -C loader',
        'make -C kernel',
        --'make -C lib',
        --'make -C tools',

        'bin/premake5 --file=image.lua',
    }

    cleancommands {
        'make clean -C loader',
        'make clean -C kernel',
        --[[
           ['make clean -C lib',
           ['make clean -C tools'
           ]]

        --'rm -rf kernel.smap kernel.bin system.img'
    }
