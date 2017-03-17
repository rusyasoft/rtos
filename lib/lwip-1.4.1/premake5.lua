project "lwip"
    kind "StaticLib"

    build.compileProperty('x86_64')
    build.exportPath('lwip', {
        'src/include',
        'src/include/ipv4',
    })

    removefiles { 'src/core/ipv6/**.c' }
    includedirs { '../core/include' }

