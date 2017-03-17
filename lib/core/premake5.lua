project "core"
    kind        "StaticLib"
    targetdir   "."

    includedirs { "./include", "../TLSF/src", "../jsmn/" }
    files       { "src/*.c", "src/*.asm", "src/*.S" }

group "tests"
    project "cache"
        filename    "cache"
        kind        "ConsoleApp"
        links       { "cmocka", "tlsf" }
        libdirs     { ".." }
        linkoptions { "../libtlsf.a" }
        includedirs { "./include", "../TLSF/src" }
        files       { "src/cache.c", "src/test/cache.c", "src/_malloc.c" }

newaction {
    trigger     = "doc",
    description = "generate library documents",
    execute     = function ()
        local target = "manual.pdf"
        os.remove(target)
        os.execute("doxygen doxyfiles/Doxyfile")
        os.execute("make -C doc/latex")
        os.rename("doc/latexy/refman.pdf", target)
    end
}
