
local includePath = {
    'core'='../lib/core/include',
}

function buildLibraryProperty(libs)
	local root = '..'
    libdirs { root .. '/lib' }

	for lib, path in inclduePath do
		includedirs(path)
	end
		--[[
           ['../lib/core/include',
           ['../lib/TLSF/src',
           ['../lib/lwip/src/include',
           ['../lib/lwip/src/include/ipv4'
		   ]]

    links(libs)
end

-- Add assembly file build command
function buildAssemblyProperty(arch)
    filter 'files:src/**.asm'
		buildmessage
			'Compiling %{file.relpath}'
		buildcommands
			{ 'nasm -o "%{cfg.objdir}/%{file.basename}.o" "%{file.relpath}" -f ' .. arch }
		buildoutputs
			'%{cfg.objdir}/%{file.basename}.o'

	filter {}
end

-- Add default build property for each architecture
function buildProperty(arch, libs)
    architecture(arch)
    location	'.'
	targetdir	'.'

    files		{ 'src/**.c', 'src/**.asm' }
	removefiles { 'src/test/**.c', 'src/test/**.asm' }

	local format
	if(arch == 'x86') then
		format = 'elf32'
	elseif(arch == 'x86_64') then
		format = 'elf64'
	end

    buildAssemblyProperty(format)
	buildLibraryProperty(libs)
end
