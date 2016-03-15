/*
 * Copyright 2014, 2015 AASHTO/ITE/NEMA.
 * American Association of State Highway and Transportation Officials,
 * Institute of Transportation Engineers and
 * National Electrical Manufacturers Association.
 *  
 * This file is part of the Advanced Transportation Controller (ATC)
 * Application Programming Interface Reference Implementation (APIRI).
 *  
 * The APIRI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *  
 * The APIRI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with the APIRI.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 *  Implemented from Advanced Transportation Controller Standard v06.xx
 */
#ifndef _ATC_SPXS_H
#define _ATC_SPXS_H

#define ATC_LKM_SP1S 1
#define ATC_LKM_SP2S 2
#define ATC_LKM_SP3S 3
#define ATC_LKM_SP5S 5
#define ATC_LKM_SP8S 8

#define ATC_SPXS_WRITE_CONFIG 0
#define ATC_SPXS_READ_CONFIG  1

#define ATC_SDLC 0
#define ATC_SYNC 1
#define ATC_HDLC 2

#define ATC_B1200   0
#define ATC_B2400   1
#define ATC_B4800   2
#define ATC_B9600   3
#define ATC_B19200  4
#define ATC_B38400  5
#define ATC_B57600  6
#define ATC_B76800  7
#define ATC_B115200 8
#define ATC_B153600 9
#define ATC_B614400 10

const int ATC_B[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600,
76800, 115200, 153600, 614400};

#define ATC_CLK_INTERNAL 0
#define ATC_CLK_EXTERNAL 1

#define ATC_GATED      0
#define ATC_CONTINUOUS 1

typedef struct atc_spxs_config {
        unsigned char protocol;
        unsigned char baud;
        unsigned char transmit_clock_source;
        unsigned char transmit_clock_mode;
} atc_spxs_config_t;

#endif /* _ATC_SPXS_H */
