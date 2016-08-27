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

#include "libffsp/mkfs.hpp"
#include "libffsp/ffsp.hpp"

#include "spdlog/spdlog.h"

#include <string>

#include <cstdio>
#include <cstdlib>
#include <getopt.h>

struct mkfs_arguments
{
    const char* device;
    unsigned int clustersize;
    unsigned int erasesize;
    unsigned int ninoopen;
    unsigned int neraseopen;
    unsigned int nerasereserve;
    unsigned int nerasewrites;
};

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
                args.clustersize = std::stoul(optarg);
                break;
            case 'e':
                args.erasesize = std::stoul(optarg);
                break;
            case 'i':
                args.ninoopen = std::stoul(optarg);
                break;
            case 'o':
                args.neraseopen = std::stoul(optarg);
                break;
            case 'r':
                args.nerasereserve = std::stoul(optarg);
                break;
            case 'w':
                args.nerasewrites = std::stoul(optarg);
                break;
            case '?':
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
    mkfs_arguments args = {};
    if (!parse_arguments(argc, argv, args))
    {
        return EXIT_FAILURE;
    }

    auto console = spdlog::stdout_logger_mt("console");
    console->info("Setup file system:");
    console->info("\tdevice={}", args.device);
    console->info("\tclustersize={}", args.clustersize);
    console->info("\terasesize={}", args.erasesize);
    console->info("\tninoopen={}", args.ninoopen);
    console->info("\tneraseopen={}", args.neraseopen);
    console->info("\tnerasereserve={}", args.nerasereserve);
    console->info("\tnerasewrites={}", args.nerasewrites);

    if (!ffsp_mkfs(args.device, {args.clustersize,
                                 args.erasesize,
                                 args.ninoopen,
                                 args.neraseopen,
                                 args.nerasereserve,
                                 args.nerasewrites}))
    {
        fprintf(stderr, "%s: failed to setup file system", argv[0]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
