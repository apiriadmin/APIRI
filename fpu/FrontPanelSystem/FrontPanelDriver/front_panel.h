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

#ifndef _FRONT_PANEL_H_
#define _FRONT_PANEL_H_


// The desired major number, zero forces the kernel to assign one
#define FP_GENERIC_MAJOR 0

//
// a convience definition for boolean operators
//
#ifndef __KERNEL__
#if !(__bool_true_false_are_defined)
typedef enum { false = (0!=0), true  = (0==0) } bool;
#endif
#endif

//
// this structure holds a single read buffer, and a pointer to next one
//

typedef enum {
	NOOP = 0,				// place holder to avoid uninitialized command
	DATA = 1,				// simple data transfer
	CREATE = 2,				// create a virtual terminal for this device
	REGISTER = 3,				// Registration data
	DESTROY = 4,				// Destroy this devices virtual terminal
	FOCUS = 5,				// Change focus information
	DIRECT = 6,				// command bypasses virtual terminal
	SIGNAL_ALL = 7, 			// signal all processes that the state of the front panel display has changed
	SIGNAL = 8,				// signal the 'to' application that something in it's windowing system has changed
	ATTRIBUTES = 9,				// request for attributes held by virtual display
	REFRESH = 10,				// request to redraw on front panel device
	EMERGENCY = 11				// set emergency mode on/off
} command_t;

#define COMMAND_NAMES { \
	"NOOP", \
	"DATA", \
	"CREATE", \
	"REGISTER", \
	"DESTROY", \
	"FOCUS", \
	"DIRECT", \
	"SIGNAL_ALL", \
	"SIGNAL", \
	"ATTRIBUTES", \
	"REFRESH", \
	"EMERGENCY" \
};

typedef struct read_packet {
	command_t		command;	// what this packet is for
	int			from;		// the interface this packet came from
	int			to;		// the interface this packet is destin for
	size_t			size;		// length of data buffer
	size_t			raw_offset;	// offset in data array to raw data
	unsigned char		data[0];	// reference to payload area
} read_packet;


//
// the fp_devtab array holds pointers to the fp_device structures
// for each device that has successfully opened it's interface.
// Enteries are NULL to indicate the device is closed or otherwise
// available to be claimed. To claim an interface, a reference to
// its fp_device structure is entered into this array at an empty
// location. To release an interface, NULL is writen into the location
// previously claimed
//
#define APP_OPENS   (16)		// this is the maximum number of application opens allowed
#define MS_DEV      (APP_OPENS)		// this is the index of the Master Selection device entry
#define SC_DEV      (MS_DEV+1)		// this is the index of the System Configuration device entry
#define AUX_DEV     (SC_DEV+1)		// this is the index of the AUX Switch device entry
#define FPM_DEV     (AUX_DEV+1)		// this is the index of the Front Panel device entry
#define FP_MAX_DEVS (FPM_DEV+1)		// this is the maximum number of possible interfaces

#define AUX_SWITCH_ON  (0xff)
#define AUX_SWITCH_OFF (0x00)

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif

/* Use 'k' as magic number */
#define FP_IOC_MAGIC  'f'

#define FP_IOC_REGISTER		_IOC( _IOC_WRITE, FP_IOC_MAGIC, 0, sizeof( char * ) )
#define FP_IOC_GET_FOCUS	_IOC( _IOC_READ,  FP_IOC_MAGIC, 1, sizeof( unsigned int * ) )
#define FP_IOC_SETDEFAULT	_IOC( _IOC_WRITE, FP_IOC_MAGIC, 2, sizeof( int ) )
#define FP_IOC_VERSION		_IOC( _IOC_READ,  FP_IOC_MAGIC, 3, sizeof( int * ) )
#define FP_IOC_WINDOW_SIZE	_IOC( _IOC_READ,  FP_IOC_MAGIC, 4, sizeof( int * ) )
#define FP_IOC_EMERGENCY	_IOC( _IOC_WRITE, FP_IOC_MAGIC, 5, sizeof( int ) )
#define FP_IOC_ATTRIBUTES	_IOC( _IOC_READ,  FP_IOC_MAGIC, 6, sizeof( int * ) )
#define FP_IOC_SET_FOCUS	_IOC( _IOC_WRITE, FP_IOC_MAGIC, 7, sizeof( unsigned int * ) )
#define FP_IOC_REFRESH		_IO( FP_IOC_MAGIC, 8 )
// #define FP_IOCREGISTER _IOW(FP_IOC_MAGIC,  0, * )

enum { BIT_CHARACTER_BLINK, BIT_CURSOR_BLINK, BIT_UNDERLINE, BIT_CURSOR };



#endif
