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

#ifndef BITOPS_H
#define BITOPS_H

#include <stdint.h>

static inline int test_bit(uint32_t* data, uint32_t n)
{
    return ((1 << (n % sizeof(uint32_t))) &
            (data[n / sizeof(uint32_t)])) != 0;
}

static inline void set_bit(uint32_t* data, uint32_t n)
{
    data[n / sizeof(uint32_t)] |= 1 << (n % sizeof(uint32_t));
}

static inline void clear_bit(uint32_t* data, uint32_t n)
{
    data[n / sizeof(uint32_t)] &= ~(1 << (n % sizeof(uint32_t)));
}

#endif /* BITOPS_H */
