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

#ifndef __ATC_H__
#define __ATC_H__

// Device File Names
#define ATC_HOST_EEPROM_DEV "/dev/eeprom"
#define ATC_DATAKEY_DEV "/dev/datakey"
#define ATC_GPIO_POWERDOWN_DEV "/dev/powerdown"
#define ATC_GPIO_DATAKEY_DEV "/dev/datakeypresent"
#define ATC_GPIO_CPUACTIVE_DEV "/dev/cpuactive"
#define ATC_GPIO_CPURESET_DEV "/dev/cpureset"
#define ATC_TIMING_TOD_DEV "/dev/tod"

#define ATC_SP1 "/dev/sp1"
#define ATC_SP2 "/dev/sp2"
#define ATC_SP3 "/dev/sp3"
#define ATC_SP4 "/dev/sp4"
#define ATC_SP5 "/dev/sp5"
#define ATC_SP6 "/dev/sp6"
#define ATC_SP8 "/dev/sp8"

#define ATC_SP1S "/dev/sp1s"
#define ATC_SP2S "/dev/sp2s"
#define ATC_SP3S "/dev/sp3s"
#define ATC_SP5S "/dev/sp5s"
#define ATC_SP8S "/dev/sp8s"

// Datakey Driver Definitions
#define ATC_DATAKEY_ERASE_ALL		_IO('D', 1)
#define ATC_DATAKEY_ERASE_SECTOR	_IOW('D', 2, unsigned long)
#define ATC_DATAKEY_READ_PROTECT_BITS	_IOR('D', 3, unsigned long)
#define ATC_DATAKEY_WRITE_PROTECT_BITS	_IOW('D', 4, unsigned long)
#define ATC_DATAKEY_GET_DEVICE_SIZE	_IOR('D', 5, unsigned long)
#define ATC_DATAKEY_GET_SECTOR_SIZE	_IOR('D', 6, unsigned long)

// Time of Day Driver Definitions
#define ATC_TOD_SET_TIMESRC		_IOW('T', 1, unsigned long)
#define ATC_TOD_GET_TIMESRC		_IO('T', 2)
#define ATC_TOD_GET_INPUT_FREQ		_IO('T', 3)
#define ATC_TOD_REQUEST_TICK_SIG	_IOW('T', 4, unsigned long)
#define ATC_TOD_CANCEL_TICK_SIG		_IO('T', 5)
#define ATC_TOD_REQUEST_ONCHANGE_SIG	_IOW('T', 6, unsigned long)
#define ATC_TOD_CANCEL_ONCHANGE_SIG	_IO('T', 7)

enum timesrc_enum
{
	ATC_TIMESRC_LINESYNC,
	ATC_TIMESRC_RTCSQWR,
	ATC_TIMESRC_CRYSTAL,
	ATC_TIMESRC_EXTERNAL1,
	ATC_TIMESRC_EXTERNAL2
};

#endif // __ATC_H__
