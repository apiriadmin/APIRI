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

/*****************************************************************************/
/*

This module contains definitions for the FIO API used by both kernel space
and user space.

*/
/*****************************************************************************/

#ifndef _FIOAPI_H_
#define _FIOAPI_H_

/*  Include section.
-----------------------------------------------------------------------------*/

/* System includes. */
#ifndef __KERNEL__
#include <stdbool.h>
#endif

/* Local includes. */


/*  Definition section.
-----------------------------------------------------------------------------*/
/*  Module public structure/enum definition.*/


/*  Global section.
-----------------------------------------------------------------------------*/

/* Macros for managing bit arrays */
/* These macros assume that the bit array is ordered as followes:
   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 |
   Such that the msb of each byte is on the left end, and the lsb is
   on the right */
/* Clear a bit array */
/* a = bit array address, s = size, in bytes, of bit array */
#define	FIO_BITS_CLEAR( a, s )	( memset( (a), 0, (s) ) )

/* Set Bit */
/* a = bit array address, n = bit number */
#define	FIO_BIT_SET( a, n )		( (a)[ (n) / 8 ] |= ( 1 << ( (n) % 8 ) ) )

/* Clear Bit */
/* a = bit array address, n = bit number */
#define	FIO_BIT_CLEAR( a, n )	( (a)[ (n) / 8 ] &= ~( 1 << ( (n) % 8 ) ) )

/* Test Bit */
/* a = bit array address, n = bit number */
#define	FIO_BIT_TEST( a, n )	( (a)[ (n) / 8 ] & ( 1 << ( (n) % 8 ) ) )

/*  Public Interface section.
-----------------------------------------------------------------------------*/

typedef	int		FIO_DEV_HANDLE;		/* FIOD Handle from fio_fiod_register */

/* Port definitions */
enum fio_port
{
	FIO_PORT_SP3 = 	0,
	FIO_PORT_SP5 = 	1,
	FIO_PORT_SP8 = 	2,
	FIO_PORTS_MAX =	3
};
typedef	enum fio_port	FIO_PORT;

/* FIOD definitions */
enum fio_device_type
{
	FIO_UNDEF,			/* Don't use 0 */
	FIO332,				/* Caltrans 332 */
	FIOTS1,				/* NEMA TS1 or TS2-T2 */
	FIOTS2,				/* NEMA TS2-T1 */
	FIOMMU,				/* MMU */
	FIODR1,				/* Detector BIU 1 */
	FIODR2,				/* Detector BIU 2 */
	FIODR3,				/* Detector BIU 3 */
	FIODR4,				/* Detector BIU 4 */
	FIODR5,				/* Detector BIU 5 */
	FIODR6,				/* Detector BIU 6 */
	FIODR7,				/* Detector BIU 7 */
	FIODR8,				/* Detector BIU 8 */
	FIOTF1,				/* T&F BIU 1 */
	FIOTF2,				/* T&F BIU 2 */
	FIOTF3,				/* T&F BIU 3 */
	FIOTF4,				/* T&F BIU 4 */
	FIOTF5,				/* T&F BIU 5 */
	FIOTF6,				/* T&F BIU 6 */
	FIOTF7,				/* T&F BIU 7 */
	FIOTF8,				/* T&F BIU 8 */
	FIOCMU,				/* CMU */
	FIOINSIU1,			/* Input SIU 1 */
	FIOINSIU2,			/* Input SIU 2 */
	FIOINSIU3,			/* Input SIU 3 */
	FIOINSIU4,			/* Input SIU 4 */
	FIOINSIU5,			/* Input SIU 5 */
	FIOOUT6SIU1,		/* Output 6p SIU 1 */
	FIOOUT6SIU2,		/* Output 6p SIU 2 */
	FIOOUT6SIU3,		/* Output 6p SIU 3 */
	FIOOUT6SIU4,		/* Output 6p SIU 4 */
	FIOOUT14SIU1,		/* Output 14p SIU 1 */
	FIOOUT14SIU2,		/* Output 14p SIU 2 */
	FIOD_MAX,			/* Total number of FIODs defined */
	FIOD_ALL = FIOD_MAX	/* All FIOD targets */
};
typedef	enum fio_device_type	FIO_DEVICE_TYPE;

#define IS_TFBIU(x)		((x>=FIOTF1)&&(x<=FIOTF8))
#define IS_OUTSIU(x)	((x>=FIOOUT6SIU1)&&(x<=FIOOUT14SIU2))

/* HZ Definitions for Apps */
enum fio_hz
{
	FIO_HZ_0,				/* Frame is currently not scheduled */
	FIO_HZ_ONCE,			/* Send frame once */
	FIO_HZ_1,				/* 1 Hz */
	FIO_HZ_2,				/* 2 Hz */
	FIO_HZ_5,				/* 5 Hz */
	FIO_HZ_10,				/* 10 Hz */
	FIO_HZ_20,				/* 20 Hz */
	FIO_HZ_30,				/* 30 Hz */
	FIO_HZ_40,				/* 40 Hz */
	FIO_HZ_50,				/* 50 Hz */
	FIO_HZ_60,				/* 60 Hz */
	FIO_HZ_70,				/* 70 Hz */
	FIO_HZ_80,				/* 80 Hz */
	FIO_HZ_90,				/* 90 Hz */
	FIO_HZ_100,				/* 100 Hz */
	FIO_HZ_MAX				/* Maximum Hz entires */
};
typedef	enum fio_hz	FIO_HZ;

enum fio_version
{
	FIO_VERSION_LIBRARY,
	FIO_VERSION_LKM
};
typedef enum fio_version FIO_VERSION;

/*-----------------------*/
/* Input & Output Points */
/*-----------------------*/
/* The following defines are used to configure the minimum size of
   the buffers used for input and output points.  The numbers 119 & 103
   are used as the number of input & output points used by ITS cabinets. */
#define	FIO_OUTPUT_POINTS_BYTES	( ( 103 + 1 ) / 8 )	/* Min number of bytes */
#define	FIO_INPUT_POINTS_BYTES	( ( 119 + 1 ) / 8 )	/* Min number of bytes */

enum	fio_view
{
	FIO_VIEW_APP,		/* Return my view */
	FIO_VIEW_SYSTEM		/* Return system view */
};
typedef	enum fio_view	FIO_VIEW;

enum	fio_inputs_type
{
	FIO_INPUTS_RAW,			/* Return unfiltered inputs */
	FIO_INPUTS_FILTERED		/* Return filtered inputs */
};
typedef	enum fio_inputs_type	FIO_INPUTS_TYPE;

/* Channel Mapping support */
#define FIO_CHANNELS		(32)				/* 32 channels per CMU/MMU */
#define FIO_CHANNEL_BYTES	(FIO_CHANNELS/8)

enum fio_color
{
    FIO_GREEN,
    FIO_YELLOW,
    FIO_RED
};
typedef enum fio_color  FIO_COLOR;

struct fio_channel_map
{
    unsigned int    output_point;
    FIO_DEV_HANDLE  fiod;
    unsigned int    channel;
    FIO_COLOR       color;
};
typedef struct fio_channel_map  FIO_CHANNEL_MAP;


enum fio_mmu_flash_bit
{
	FIO_MMU_FLASH_BIT_OFF,
	FIO_MMU_FLASH_BIT_ON
};
typedef enum fio_mmu_flash_bit FIO_MMU_FLASH_BIT;

enum fio_ts_fm_state
{
	FIO_TS_FM_OFF,
	FIO_TS_FM_ON
};
typedef enum fio_ts_fm_state	FIO_TS_FM_STATE;

/* Voltage Monitor state */
enum fio_ts1_vm_state
{
    FIO_TS1_VM_OFF,
    FIO_TS1_VM_ON
};
typedef enum fio_ts1_vm_state FIO_TS1_VM_STATE;

#ifdef TS2_PORT1_STATE
/* TS2 Port 1 state */
enum fio_ts2_port1_state
{
        FIO_TS2_PORT1_DISABLED,
        FIO_TS2_PORT1_ENABLED
};
typedef enum fio_ts2_port1_state FIO_TS2_PORT1_STATE;
#endif

enum fio_cmu_dc_mask
{
	FIO_CMU_DC_MASK1,
	FIO_CMU_DC_MASK2,
	FIO_CMU_DC_MASK3,
	FIO_CMU_DC_MASK4
};
typedef enum fio_cmu_dc_mask 	FIO_CMU_DC_MASK;

enum fio_cmu_fsa
{
	FIO_CMU_FSA_NONE,
	FIO_CMU_FSA_NON_LATCHING,
	FIO_CMU_FSA_LATCHING
};
typedef enum fio_cmu_fsa	FIO_CMU_FSA;

/* FIO device status */
struct fio_frame_info
{
    FIO_HZ          frequency;      /* Current frame frequency */
    unsigned int    success_rx;     /* Cumulative Successful Rx�s */
    unsigned int    error_rx;       /* Cumulative Error Rx�s */
    unsigned int    error_last_10;  /* Errors in last 10 frames */
    unsigned int    last_seq;       /* Last frame sequence # */
};
typedef struct fio_frame_info FIO_FRAME_INFO;

#define FIO_TX_FRAME_COUNT  128
struct fio_fiod_status
{
    bool            comm_enabled;   /* Is communication enabled? */
    unsigned int    success_rx;     /* Cumulative Successful Rx�s */
    unsigned int    error_rx;       /* Cumulative Error Rx�s */
                                    /* Frame information array */
    FIO_FRAME_INFO  frame_info[ FIO_TX_FRAME_COUNT ];
};
typedef struct fio_fiod_status FIO_FIOD_STATUS;

/* Frame schedule related */
struct fio_frame_schd
{
	unsigned int	req_frame;	/* frame # */
	FIO_HZ		frequency;	/* frame scheduled frequency */
};
typedef struct fio_frame_schd FIO_FRAME_SCHD;

/* Filtered inputs */
#define FIO_FILTER_DEFAULT      5
struct fio_input_filter
{
	unsigned int	input_point;	/* input # */
	unsigned int	leading;	/* leading edge filter */
	unsigned int	trailing;	/* trailing edge filter */
};
typedef struct fio_input_filter FIO_INPUT_FILTER;

#define	FIO_SIGIO 	(SIGRTMIN + 4)
enum fio_notify
{
	FIO_NOTIFY_ONCE,
	FIO_NOTIFY_ALWAYS,
};
typedef	enum fio_notify		FIO_NOTIFY;

enum fio_frame_status
{
	FIO_FRAME_ERROR,
	FIO_FRAME_RECEIVED
};
typedef	enum fio_frame_status FIO_FRAME_STATUS;

struct fio_notify_info
{
	unsigned int     rx_frame;        /* Response Frame # */
	FIO_FRAME_STATUS status;          /* Status of response frame */
	unsigned int     seq_number;      /* Sequence Number of frame */
	unsigned int     count;           /* # of bytes in response frame */
	FIO_DEV_HANDLE   fiod;            /* FIOD of response frame */
};
typedef	struct	fio_notify_info	FIO_NOTIFY_INFO;

enum fio_trans_status
{
	FIO_TRANS_SUCCESS,
	FIO_TRANS_FIOD_OVERRUN,
	FIO_TRANS_APP_OVERRUN
};
typedef	enum fio_trans_status		FIO_TRANS_STATUS;

struct fio_trans_buffer
{
	unsigned int	input_point:7;		/* Input number */
	unsigned int	state:1;			/* Input state 0 = off, 1 = on */
	unsigned int	timestamp : 16;		/* 16 bit timestamp for FIOD */
};
typedef	struct	fio_trans_buffer	FIO_TRANS_BUFFER;

#endif /* #ifndef _FIOAPI_H_ */
