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

#ifndef LOG_H
#define LOG_H

#ifdef WIN32
void FFSP_DEBUG(const char *format, ...); // FIXME
void FFSP_ERROR(const char *format, ...); // FIXME
#else
void FFSP_DEBUG(const char *format, ...); __attribute__((format(printf, 1, 2)));
void FFSP_ERROR(const char *format, ...); __attribute__((format(printf, 1, 2)));
#endif

#endif /* LOG_H */
