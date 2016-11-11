/*
 * Copyright (C) 2011-2012 IBM Corporation
 *
 * Author: Volker Schneider <volker.schneider@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "libffsp/ffsp.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/io_raw.hpp"
#include "libffsp/log.hpp"

#include "spdlog/fmt/ostr.h"
#include "spdlog/spdlog.h"

#include <string>

#include <cstdio>
#include <cstdlib>
#include <getopt.h>

struct mkfs_arguments
{
    const char* device;
    uint32_t clustersize;
    uint32_t erasesize;
    uint32_t ninoopen;
    uint32_t neraseopen;
    uint32_t nerasereserve;
    uint32_t nerasewrites;
};

std::ostream& operator<<(std::ostream& os, const mkfs_arguments& args)
{
    return os << "{"
              << "device=" << args.device
              << ", clustersize=" << args.clustersize
              << ", erasesize=" << args.erasesize
              << ", ninoopen=" << args.ninoopen
              << ", neraseopen=" << args.neraseopen
              << ", nerasereserve=" << args.nerasereserve
              << ", nerasewrites=" << args.nerasewrites
              << "}";
}

static void show_usage(const char* progname)
{
    printf("%s [OPTION] DEVICE\n", progname);
    printf("create a ffsp file system inside the given file [DEVICE]\n\n");
    printf("-c, --clustersize=N use a clusterblock size of N (default:4KiB)\n");
    printf("-e, --erasesize=N use a eraseblock size of N (default:4MiB)\n");
    printf("-i, --open-ino=N support caching of N dirty inodes at a time (default:100)\n");
    printf("-o, --open-eb=N support N open erase blocks at a time (default:5)\n");
    printf("-r, --reserve-eb=N reserve N erase blocks for internal use (default:3)\n");
    printf("-w, --write-eb=N Perform garbage collection after N erase blocks have been written (default:5)\n");
}

static bool parse_arguments(int argc, char** argv, mkfs_arguments& args)
{
    static const option long_options[] =
    {
        { "clustersize", 1, NULL, 'c' },
        { "erasesize", 1, NULL, 'e' },
        { "open-ino", 1, NULL, 'i' },
        { "open-eb", 1, NULL, 'o' },
        { "reserve-eb", 1, NULL, 'r' },
        { "write-eb", 1, NULL, 'w' },
        { NULL, 0, NULL, 0 },
    };

    args = {};
    // Default size for clusters is 4 KiB
    args.clustersize = 1024 * 32;
    // Default size for erase blocks is 4 MiB
    args.erasesize = 1024 * 1024 * 4;
    // Default number of cached dirty inodes is 100
    args.ninoopen = 128;
    // Default number of open erase blocks is 5
    args.neraseopen = 5;
    // Default number of reserved erase blocks is 3
    args.nerasereserve = 3;
    // Default number of erase block finalizations before GC is 5
    args.nerasewrites = 5;

    while (true)
    {
        int c = getopt_long(argc, argv, "c:e:i:o:r:w:", long_options, &optind);
        if (c == -1)
            break;

        switch (c)
        {
            case 'c':
                args.clustersize = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'e':
                args.erasesize = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'i':
                args.ninoopen = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'o':
                args.neraseopen = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'r':
                args.nerasereserve = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'w':
                args.nerasewrites = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'h':
                show_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }

    if (optind != (argc - 1))
    {
        fprintf(stderr, "%s: invalid arguments\n", argv[0]);
        return false;
    }
    args.device = argv[optind];
    return true;
}

int main(int argc, char* argv[])
{
    ffsp::log_init("ffsp_mkfs", spdlog::level::debug);

    mkfs_arguments args = {};
    if (!parse_arguments(argc, argv, args))
    {
        fprintf(stderr, "%s: failed to parse command line arguments", argv[0]);
        return EXIT_FAILURE;
    }

    ffsp::log().info("Setup file system: {})", args);

    auto ret = EXIT_SUCCESS;
    auto* io_ctx = ffsp::io_context_init(args.device);
    if (!io_ctx || !ffsp::mkfs(*io_ctx, {args.clustersize,
                                         args.erasesize,
                                         args.ninoopen,
                                         args.neraseopen,
                                         args.nerasereserve,
                                         args.nerasewrites}))
    {
        perror("failed to setup file system");
        ret = EXIT_FAILURE;
    }
    ffsp::io_context_uninit(io_ctx);
    ffsp::log_deinit();
    return ret;
}
