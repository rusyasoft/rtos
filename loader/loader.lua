-- [[ Premake script for loader workspace ]] 
workspace "Loader"
    -- Assembly compiler build commands --
    -- Since there are no implicit compile commands for assembly files. We have to designate it
    filter 'files:src/**.asm'
        buildmessage 'Compiling %{file.relpath}'
        buildcommands {
            'nasm -f elf32 -o "%{cfg.objdir}/%{file.basename}.o" "%{file.relpath}"'
        }
        buildoutputs { '%{cfg.objdir}/%{file.basename}.o' }


    -- [[ Temporary loader project ]]
    project "loader.tmp"
        -- Generate binary object for console application
        kind "ConsoleApp"
        -- Set the destination direcotry for a generated file related to build
        location "build"
        -- Force to make symbol table
        flags { "Symbols" }
        targetdir "."

        -- We cares about files below
        files { "src/entry_tmp.asm", "src/**.h", "src/**.c" }

        prebuildcommands {
            "./set_tmp"
        }

        postbuildcommands {
            "./set_value"
        }

    -- [[ Loader project ]]
    project "loader.bin"
        -- Generate binary object for console application
        kind "ConsoleApp"
        -- Set the destination direcotry for a generated file related to build
        location "build"
        targetdir "."
        flags { "Symbols" }

        -- We cares about files below
        files { "src/entry_tmp.asm", "src/**.h", "src/**.c" }

        linkoptions { "-Wl,--build-id=none -T linker.ld -e start" }

