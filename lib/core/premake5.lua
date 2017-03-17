project 'core'
    kind 'StaticLib'

    build.compileProperty('x86_64')
    build.linkingProperty { 'tlsf' }

    postbuildcommands {
        -- "doxygen doxyfiles/Doxyfile",
        -- "make -C doc/latex",
        -- "doc/latexy/refman.pdf"
    }

--[[
   [group "tests"
   [    project "cache"
   [        filename    "cache"
   [        kind        "ConsoleApp"
   [        links       { "cmocka", "tlsf" }
   [        libdirs     { ".." }
   [        linkoptions { "../libtlsf.a" }
   [        includedirs { "./include", "../TLSF/src" }
   [        files       { "src/cache.c", "src/test/cache.c", "src/_malloc.c" }
   ]]
