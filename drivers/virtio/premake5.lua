project 'virtio'
    kind        'StaticLib'
    targetname  'virtio.ko'

    build.compileProperty('x86_64')

    linkoptions { '-r ../linux.ko' }


