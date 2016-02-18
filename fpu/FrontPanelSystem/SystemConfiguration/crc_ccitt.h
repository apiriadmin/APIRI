/*
 * Copyright 2015 AASHTO/ITE/NEMA.
 * American Association of State Highway and Transportation Officials,
 * Institute of Transportation Engineers and
 * National Electrical Manufacturers Association.
 *
 * This file is part of the Advanced Transportation Controller (ATC)
 * Application Programming Interface Validation Suite (APIVS).
 *
 * The APIRI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * The APIRI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the APIRI.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Adapted from Linux kernel for user space. 
 */
#ifndef __CRC_CCITT_H__
#define __CRC_CCITT_H__

#include <stdlib.h>
#include <stdint.h>

extern uint16_t const crc_ccitt_table[256];

extern uint16_t crc_ccitt(uint16_t crc, const uint8_t *buffer, size_t len);

static inline uint16_t crc_ccitt_byte(uint16_t crc, const uint8_t data)
{
	return (crc >> 8) ^ crc_ccitt_table[(crc ^ data) & 0xff];
}

#endif /* __CRC_CCITT_H__ */
