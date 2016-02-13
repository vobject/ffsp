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

#ifndef BYTE_ORDER_H
#define BYTE_ORDER_H

#include <stdint.h>

#ifdef _WIN32
#include <intrin.h>
#define __LITTLE_ENDIAN __MACHINE
#define __BIG_ENDIAN 0
#define __BYTE_ORDER __MACHINE
#define bswap_16 _byteswap_ushort
#define bswap_32 _byteswap_ulong
#define bswap_64 _byteswap_uint64
#else
#include <endian.h>
#include <byteswap.h>
#endif

typedef struct { uint16_t v; } be16_t;
typedef struct { uint32_t v; } be32_t;
typedef struct { uint64_t v; } be64_t;

static inline uint16_t be16_to_cpu(be16_t b)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return bswap_16(b.v);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return b.v;
#endif
}

static inline uint32_t be32_to_cpu(be32_t b)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return bswap_32(b.v);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return b.v;
#endif
}

static inline uint64_t be64_to_cpu(be64_t b)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return bswap_64(b.v);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return b.v;
#endif
}

static inline be16_t cpu_to_be16(uint16_t v)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	const be16_t b = { bswap_16(v) };
#elif __BYTE_ORDER == __BIG_ENDIAN
	const be16_t b = { v };
#endif
	return b;
}

static inline be32_t cpu_to_be32(uint32_t v)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	const be32_t b = { bswap_32(v) };
#elif __BYTE_ORDER == __BIG_ENDIAN
	const be32_t b = { v };
#endif
	return b;
}

static inline be64_t cpu_to_be64(uint64_t v)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	const be64_t b = { bswap_64(v) };
#elif __BYTE_ORDER == __BIG_ENDIAN
	const be64_t b = { v };
#endif
	return b;
}

static inline uint16_t get_be16(be16_t b)
{
	return be16_to_cpu(b);
}

static inline uint32_t get_be32(be32_t b)
{
	return be32_to_cpu(b);
}

static inline uint64_t get_be64(be64_t b)
{
	return be64_to_cpu(b);
}

static inline be16_t put_be16(uint16_t v)
{
	return cpu_to_be16(v);
}

static inline be32_t put_be32(uint32_t v)
{
	return cpu_to_be32(v);
}

static inline be64_t put_be64(uint64_t v)
{
	return cpu_to_be64(v);
}

static inline void inc_be16(be16_t *b)
{
	*b = put_be16(get_be16(*b) + 1);
}

static inline void inc_be32(be32_t *b)
{
	*b = put_be32(get_be32(*b) + 1);
}

static inline void inc_be64(be64_t *b)
{
	*b = put_be64(get_be64(*b) + 1);
}

static inline void dec_be16(be16_t *b)
{
	*b = put_be16(get_be16(*b) - 1);
}

static inline void dec_be32(be32_t *b)
{
	*b = put_be32(get_be32(*b) - 1);
}

static inline void dec_be64(be64_t *b)
{
	*b = put_be64(get_be64(*b) - 1);
}

#endif /* BYTE_ORDER_H */
