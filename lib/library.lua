-- [[ 1. Core library ]]
project "core"
    kind "StaticLib"
    location "core/build"
    targetdir "."
    includedirs { "core/include", "TLSF/src", "jsmn/" }
    files { "core/**.asm", "core/**.S", "core/**.h", "core/**.c" }
    -- Enable exntension instruction for SSE. Do not need stack protector 
    buildoptions { "-msse4.1 -fno-stack-protector" }

    filter 'files:core/**.asm'
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            'nasm -f elf64 -o "%{cfg.objdir}/%{file.basename}.o" "%{file.relpath}"'
        }
        buildoutputs { '%{cfg.objdir}/%{file.basename}.o' }

-- [[ 2. Core library for Linux ]]
project "core_linux"
    kind "StaticLib"
    location "core/build"
    targetdir "."
    includedirs { "core/include", "TLSF/src", "jsmn/" }
    files { "core/**.asm", "core/**.S", "core/**.h", "core/**.c" }
    buildoptions { "-msse4.1 -fno-stack-protector" }
    -- Define "LINUX" to make core library for Linux OS
    defines { "LINUX" }

    filter 'files:src/**.asm'
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            'nasm -f elf64 -o "%{cfg.objdir}/%{file.basename}.o" "%{file.relpath}"'
        }
        buildoutputs { '%{cfg.objdir}/%{file.basename}.o' }

-- [[ 3. TLSF library ]]
project "tlsf"
    kind "StaticLib"
    location "TLSF/build"
    targetdir "."
    includedirs { "TLSF/src" }
    files { "TLSF/**.h", "TLSF/**.c" }
    removefiles { "TLSF/examples/*" }
    -- Enable extra waring flag
    buildoptions { "-Wextra -Wwrite-strings -Wstrict-prototypes \
        -Wmissing-prototypes -Wno-long-long -Wstrict-aliasing=1"}
    -- Define flags related TLSF
    defines { "USE_MMAP=0", "US_SBRK=0", "USE_PRINTF=0", "TLSF_STATISTIC=1", "TLSF_USE_LOCKS=1" }

-- [[ 4. LWIP library ]]
project "lwip"
    kind "StaticLib"
    location "lwip/build"
    targetdir "."
    includedirs { "core/include", "lwip/src/**" }
    files { "lwip/src/**.h", "lwip/src/**.c" }
    removefiles { "lwip/src/core/ipv6/**", "lwip/src/include/ipv6/**" }

-- [[ 5. JSMN library ]]
project "jsmn"
    kind "StaticLib"
    location "jsmn/build"
    targetdir "."
    files { "jsmn/**.h", "jsmn/**.c" }

-- [[ X. External Makefile libraries ]]
project "external"
    kind "Makefile"
    location "build"

    buildcommands {
        "cd ..; make -f External.Make" 
    }

    cleancommands {
        "cd ..; make -f External.Make clean" 
    }


-- [[ X+1 . Integration ]]
project "integration"
    kind "Makefile"
    location "build"

    buildcommands {
        -- PacketNgin Application Library
        "ar x ../libc.a",
        "ar x ../libm.a",
        "ar x ../libtlsf.a",
        "ar x ../libcore.a",
        "ar x ../libexpat.a",
        "ar rcs ../libpacketngin.a *.o",

        "cp ../libpacketngin.a ../../sdk/lib/",
        "rm ./*.o -r",

        -- Linux Application library
        "ar x ../libtlsf.a",
        "ar x ../libcore_linux.a",
        "ar rcs ../libumpn.a *.o",
        "rm ./*.o -r"
    }

    cleancommands {
        "rm *.o -rf"
    }
