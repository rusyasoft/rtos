#!/usr/bin/lua

function buildRamdiskImage(input)
    return 'initrd.img'
end

function buildSystemImage(loader, kernel, ramdisk)
    return 'system.img'
end

function buildImage(input)
    -- Make PacketNgin binary
    local file = path.join('kernel', input)
    os.execute('bin/smap ' .. file .. ' kernel.smap')
    local kernel = 'kernel.bin'
    os.execute('bin/pnkc ' .. file .. ' kernel.smap ' .. kernel)

    -- Make ramdisk
    local ramdisk = buildRamdiskImage({'drivers/*.ko', 'drivers/*.ko'})

    -- Make system image
    local loader = path.join('loader', 'loader.bin')

    return buildSystemImage(loader, kernel, ramdisk)
end
