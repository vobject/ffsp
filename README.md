# FFSP - A Flash File System Prototype

[![Build status](https://ci.appveyor.com/api/projects/status/n7j070e6e4ketrl5/branch/master?svg=true)](https://ci.appveyor.com/project/vobject/ffsp/branch/master)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](http://www.gnu.org/licenses/gpl-3.0)

FFSP is an experimental file system for consumer-level flash devices with the objective of providing improved write speeds over established file systems such as fat32, ext4, or btrfs. The file system respects how flash drives behave internally in order to achieve better write performance. The base for this is the article [Optimizing Linux with cheap flash drives](https://lwn.net/Articles/428584/) that researched the internal characteristics of memory cards and USB flash drives. FFSP is implemented using FUSE 2.6.

## Usage

### mkfs.ffsp
`./mkfs.ffsp [OPTION] DEVICE`

The `mkfs.ffsp` tool creates a ffsp file system inside the given file `DEVICE`. This device be a physical drive or a local file (e.g. created with `dd`). The options provide parameters specific to the underlying flash device.

#### Options
- `-c, --clustersize=N` Use a clusterblock size of N (default:4KiB)
- `-e, --erasesize=N` Use a eraseblock size of N (default:4MiB)
- `-i, --open-ino=N` Support caching of N dirty inodes at a time (default:100)
- `-o, --open-eb=N` Support N open erase blocks at a time (default:5)
- `-r, --reserve-eb=N` Reserve N erase blocks for internal use (default:3)
- `-w, --write-eb=N` Perform garbage collection after N erase blocks have been written (default:5)

### mount.ffsp
`./mount.ffsp DEVICE MOUNTPOINT`

The `mount.ffsp` program mounts a given [DEVICE] that was formatted earlier and mounts it to a directory. A mounted file system must be unmounted using `fusermount -u MOUNTPOINT`.

## TODO
- Implement ffsp_rename()
- Fix garbage collection for cluster indirect data (currently disabled).
- Implement sync() to write the first erase block.
- Take all intelligence out of writing-into-erase-block-indirect-data; otherwise we get into big trouble because GC is not implemented for erase block indirect data. That means: for every write operation into (or append) an erase block indirect file, the fs will start a new/empty erase block - and because the write requests are so small this can quickly occupy all erase blocks.
- Write meta data (the first erase block) more often; it is currently only written on unmount.
- Write unit tests.
- Implement last-write-time functionality (although there is no garbage collection policy that make use of it yet).
- Implement a garbage collection policy that works with the last-write-time instead of just looking at how full the erase blocks are.
- Extended debug functions to print fs state (e.g. /.FFSP file).

## License
This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
