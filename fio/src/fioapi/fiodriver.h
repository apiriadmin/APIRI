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

#ifndef _FIODRIVER_H_
#define _FIODRIVER_H_

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

/* IOCTL Commands */
#define	FIO_IOC_MAGIC	'f'						/* Magic number used */
#define	FIO_IOC_SEQ		0x80					/* Start of sequence number */
#define	FIO_IOCTL( x )	( FIO_IOC_SEQ + ( x ) )	/* macro for IOCTL # */

/* Allow request frame frequencies to be changed */
struct fio_ioc_freq
{
	FIO_PORT		port;		/* Port FIOD is located on */
	FIO_DEVICE_TYPE	fiod;		/* FIOD to address */
	unsigned long	tx_frame;	/* Request frame number to change */
	FIO_HZ			freq;		/* new frequency, 0 is disable frame */
};
typedef	struct fio_ioc_freq	FIO_IOC_FREQ;

/* FIOD status counts */
struct fio_ioc_status
{
	FIO_PORT		port;		/* Port FIOD is located on */
	FIO_DEVICE_TYPE	fiod;		/* FIOD interested in */
};
typedef	struct fio_ioc_status	FIO_IOC_STATUS;

enum fiomsg_iocs
{
	FIOMSG_IOC_MAX					/* Maximum FIOMSG IOC */
};
typedef	enum fiomsg_iocs	FIOMSG_IOCS;

/*****************************************************************************/

enum fioman_iocs
{
	FIOMAN_IOC_0 = FIOMSG_IOC_MAX,	/* Register FIOD */
	FIOMAN_IOC_1,			/* Deregister FIOD */
	FIOMAN_IOC_2,			/* Enable FIOD */
	FIOMAN_IOC_3,			/* Disable FIOD */
	FIOMAN_IOC_4,			/* Set Reservations */
	FIOMAN_IOC_5,			/* Get Reservations */
	FIOMAN_IOC_6,			/* Clear Reservations */
	FIOMAN_IOC_7,			/* Set Outputs */
	FIOMAN_IOC_8,			/* Get Outputs */
	FIOMAN_IOC_9,			/* Get Inputs */
	FIOMAN_IOC_10,			/* Set Channel Reservations */
	FIOMAN_IOC_11,			/* Get Channel Reservations */
	FIOMAN_IOC_12,			/* Set Channel Map */
	FIOMAN_IOC_13,          	/* Get Channel Map */
	FIOMAN_IOC_14,          	/* Get Channel Map count */
	FIOMAN_IOC_15,          	/* Set MMU flash bit */
	FIOMAN_IOC_16,          	/* Get MMU flash bit */
	FIOMAN_IOC_17,          	/* Get FIOD Status information */
	FIOMAN_IOC_18,          	/* Read Frame Data */
	FIOMAN_IOC_19,          	/* Set TS Fault Monitor State */
	FIOMAN_IOC_20,          	/* Get TS Fault Monitor State */
	FIOMAN_IOC_21,			/* Set Local Time Offset */
	FIOMAN_IOC_22,          	/* Set TS1 Volt Monitor State */
	FIOMAN_IOC_23,          	/* Get TS1 Volt Monitor State */
	FIOMAN_IOC_24,			/* Set Frame Schedule */
	FIOMAN_IOC_25,			/* Get Frame Schedule */
	FIOMAN_IOC_26,			/* Set Input filter values */
	FIOMAN_IOC_27,			/* Get Input filter values */
	FIOMAN_IOC_28,			/* Get FIO driver API version */
	FIOMAN_IOC_29,			/* Reset cumulative FIOD status counts */
	FIOMAN_IOC_30,			/* Get size of response frame */
	FIOMAN_IOC_31,			/* Register a frame notification */
	FIOMAN_IOC_32,			/* Deregister a frame notification */
	FIOMAN_IOC_33,			/* Request frame notification status info */
	FIOMAN_IOC_34,			/* Query FIO device is attached to port */
	FIOMAN_IOC_35,			/* Get CMU configuration change count */
	FIOMAN_IOC_36,			/* Get CMU dark channel map */
	FIOMAN_IOC_37,			/* Set CMU dark channel map */
	FIOMAN_IOC_38,			/* Get CMU fault status */
	FIOMAN_IOC_39,			/* Set CMU fault configuration */
	FIOMAN_IOC_40,			/* Register for watchdog service */
	FIOMAN_IOC_41,			/* Deregister from watchdog service */	
	FIOMAN_IOC_42,			/* Get reserved watchdog pin */
	FIOMAN_IOC_43,			/* Set reserved watchdog pin */	
	FIOMAN_IOC_44,			/* Request toggle of watchdog pin */
	FIOMAN_IOC_45,			/* Register for health monitor service */
	FIOMAN_IOC_46,			/* Deregister from health monitor service */
	FIOMAN_IOC_47,			/* Health monitor refresh/heartbeat */
	FIOMAN_IOC_48,			/* Health monitor fault reset */
	FIOMAN_IOC_49,			/* Send arbitrary command frame */
	FIOMAN_IOC_50,			/* Get inputs enabled for transition monitoring */	
	FIOMAN_IOC_51,			/* Set inputs enabled for transition monitoring */
	FIOMAN_IOC_52,			/* Read input transition buffer */
	FIOMAN_IOC_53,			/* Query if device exists on port */	
	FIOMAN_IOC_MAX			/* End of FIOMSG Range */
};
typedef	enum fioman_iocs	FIOMAN_IOCS;

/* Request enabling / disabling of a FIOD on a port */
struct fio_ioc_fiod
{
	FIO_PORT		port;		/* Port to enable / disable FIOD on */
	FIO_DEVICE_TYPE	fiod;		/* FIOD to enable / disable */
};
typedef struct fio_ioc_fiod	FIO_IOC_FIOD;

/* FIOD Register / Deregister */
#define	FIOMAN_IOC_REG_FIOD	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_0 ), \
					FIO_IOC_FIOD )
#define	FIOMAN_IOC_DEREG_FIOD	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_1 ), \
					FIO_DEV_HANDLE )

/* FIOD Enable / Disable */
#define	FIOMAN_IOC_ENABLE_FIOD	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_2 ), \
					FIO_IOC_FIOD )
#define	FIOMAN_IOC_DISABLE_FIOD	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_3 ), \
					FIO_IOC_FIOD )

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

struct	fio_ioc_reserve_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	unsigned char		*data;			/* Bit array of reservations */
	unsigned int		count;
};
typedef	struct fio_ioc_reserve_set	FIO_IOC_RESERVE_SET;

struct	fio_ioc_reserve_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;			/* Reservations desired */
	unsigned char		*data;			/* Where to put reservations */
	unsigned int		count;
};
typedef	struct fio_ioc_reserve_get	FIO_IOC_RESERVE_GET;

#define	FIOMAN_IOC_RESERVE_SET	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_4 ), \
					FIO_IOC_RESERVE_SET )
#define	FIOMAN_IOC_RESERVE_GET	_IOR( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_5 ), \
					FIO_IOC_RESERVE_GET )

struct	fio_ioc_outputs_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	unsigned char		*ls_plus;		/* Load Switch Plus to set */
	unsigned char		*ls_minus;		/* Load Switch Minus to set */
	unsigned int		num_ls_bytes;		/* Size of buffer */
};
typedef	struct fio_ioc_outputs_set	FIO_IOC_OUTPUTS_SET;

struct	fio_ioc_outputs_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;			/* Outputs desired */
	unsigned char		*ls_plus;		/* Load Switch Plus to set */
	unsigned char		*ls_minus;		/* Load Switch Minus to set */
	unsigned int		num_ls_bytes;		/* Size of buffer */
};
typedef	struct fio_ioc_outputs_get	FIO_IOC_OUTPUTS_GET;

#define	FIOMAN_IOC_OUTPUTS_SET	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_6 ), \
					FIO_IOC_OUTPUTS_SET )
#define	FIOMAN_IOC_OUTPUTS_GET	_IOR( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_7 ), \
					FIO_IOC_OUTPUTS_GET )

/* Channel Reservation support */
struct	fio_ioc_channel_reserve_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	unsigned char		*data;			/* Bit array of reservations */
	unsigned int		count;
};
typedef	struct fio_ioc_channel_reserve_set	FIO_IOC_CHANNEL_RESERVE_SET;

struct	fio_ioc_channel_reserve_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;			/* Reservations desired */
	unsigned char		*data;			/* Where to put reservations */
	unsigned int		count;
};
typedef	struct fio_ioc_channel_reserve_get	FIO_IOC_CHANNEL_RESERVE_GET;

#define	FIOMAN_IOC_CHANNEL_RESERVE_SET	_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_10 ), \
						FIO_IOC_CHANNEL_RESERVE_SET )
#define	FIOMAN_IOC_CHANNEL_RESERVE_GET	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_11 ), \
						FIO_IOC_CHANNEL_RESERVE_GET )

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
    FIO_DEV_HANDLE  dev_handle;
    unsigned int    channel;
    FIO_COLOR       color;
};
typedef struct fio_channel_map  FIO_CHANNEL_MAP;

struct	fio_ioc_channel_map_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_CHANNEL_MAP		*map;
	unsigned int		count;			/* number of mappings */
};
typedef	struct fio_ioc_channel_map_set	FIO_IOC_CHAN_MAP_SET;

struct	fio_ioc_channel_map_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_CHANNEL_MAP		*map;
	unsigned int		count;			/* number of mappings */
};
typedef	struct fio_ioc_channel_map_get	FIO_IOC_CHAN_MAP_GET;

struct  fio_ioc_channel_map_count
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
};
typedef struct fio_ioc_channel_map_count FIO_IOC_CHAN_MAP_COUNT;

#define	FIOMAN_IOC_CHAN_MAP_SET		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_12 ), \
						FIO_IOC_CHAN_MAP_SET )
#define	FIOMAN_IOC_CHAN_MAP_GET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_13 ), \
						FIO_IOC_CHAN_MAP_GET )
#define FIOMAN_IOC_CHAN_MAP_COUNT       _IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_14 ), \
						FIO_IOC_CHAN_MAP_COUNT )
/* CMU/MMU specific definitions */
#define FIOMAN_IOC_CMU_CFG_CHANGE_COUNT	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_35 ), \
						FIO_DEV_HANDLE )
enum fio_cmu_dc_mask
{
	FIO_CMU_DC_MASK1,
	FIO_CMU_DC_MASK2,
	FIO_CMU_DC_MASK3,
	FIO_CMU_DC_MASK4
};
typedef enum fio_cmu_dc_mask 	FIO_CMU_DC_MASK;
struct	fio_ioc_cmu_dc_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_CMU_DC_MASK		*mask;
};
typedef	struct fio_ioc_cmu_dc_get	FIO_IOC_CMU_DC_GET;
#define FIOMAN_IOC_CMU_DC_GET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_36 ), \
						FIO_IOC_CMU_DC_GET )
struct fio_ioc_cmu_dc_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_CMU_DC_MASK		mask;
};
typedef	struct fio_ioc_cmu_dc_set	FIO_IOC_CMU_DC_SET;
#define FIOMAN_IOC_CMU_DC_SET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_37 ), \
						FIO_IOC_CMU_DC_SET )
enum fio_cmu_fsa
{
	FIO_CMU_FSA_NONE,
	FIO_CMU_FSA_NON_LATCHING,
	FIO_CMU_FSA_LATCHING
};
typedef enum fio_cmu_fsa	FIO_CMU_FSA;
struct	fio_ioc_cmu_fsa_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_CMU_FSA		*fsa;
};
typedef	struct fio_ioc_cmu_fsa_get	FIO_IOC_CMU_FSA_GET;
#define FIOMAN_IOC_CMU_FSA_GET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_38 ), \
						FIO_IOC_CMU_FSA_GET )
struct fio_ioc_cmu_fsa_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_CMU_FSA		fsa;
};
typedef	struct fio_ioc_cmu_fsa_set	FIO_IOC_CMU_FSA_SET;
#define FIOMAN_IOC_CMU_FSA_SET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_39 ), \
						FIO_IOC_CMU_FSA_SET )

enum fio_mmu_flash_bit
{
	FIO_MMU_FLASH_BIT_OFF,
	FIO_MMU_FLASH_BIT_ON
};
typedef enum fio_mmu_flash_bit FIO_MMU_FLASH_BIT;

struct	fio_ioc_mmu_flash_bit_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_MMU_FLASH_BIT	fb;
};
typedef	struct fio_ioc_mmu_flash_bit_set	FIO_IOC_MMU_FLASH_BIT_SET;

struct	fio_ioc_mmu_flash_bit_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_MMU_FLASH_BIT	*fb;
};
typedef	struct fio_ioc_mmu_flash_bit_get	FIO_IOC_MMU_FLASH_BIT_GET;

#define	FIOMAN_IOC_MMU_FLASH_BIT_SET	_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_15 ), \
						FIO_IOC_MMU_FLASH_BIT_SET )
#define	FIOMAN_IOC_MMU_FLASH_BIT_GET	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_16 ), \
						FIO_IOC_MMU_FLASH_BIT_GET )

enum fio_ts_fm_state
{
	FIO_TS_FM_OFF,
	FIO_TS_FM_ON
};
typedef enum fio_ts_fm_state	FIO_TS_FM_STATE;

struct	fio_ioc_ts_fms_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_TS_FM_STATE		fms;
};
typedef	struct fio_ioc_ts_fms_set FIO_IOC_TS_FMS_SET;

struct	fio_ioc_ts_fms_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_TS_FM_STATE		*fms;
};
typedef	struct fio_ioc_ts_fms_get FIO_IOC_TS_FMS_GET;

#define	FIOMAN_IOC_TS_FMS_SET	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_19 ), \
					FIO_IOC_TS_FMS_SET )
#define	FIOMAN_IOC_TS_FMS_GET	_IOR( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_20 ), \
					FIO_IOC_TS_FMS_GET )

/* Input point processing */
struct	fio_ioc_inputs_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_INPUTS_TYPE		inputs_type;		/* Inputs desired */
	unsigned char		*data;			/* Data to set */
	unsigned int		num_data_bytes;		/* Size of data buffer */
};
typedef	struct fio_ioc_inputs_get	FIO_IOC_INPUTS_GET;

#define	FIOMAN_IOC_INPUTS_GET	_IOR( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_8 ), \
					FIO_IOC_INPUTS_GET )

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

struct	fio_ioc_fiod_status
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_FIOD_STATUS		*status;
};
typedef	struct fio_ioc_fiod_status	FIO_IOC_FIOD_STATUS;

#define	FIOMAN_IOC_FIOD_STATUS_GET	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_17 ), \
						FIO_IOC_FIOD_STATUS )

#define	FIOMAN_IOC_FIOD_STATUS_RESET	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_29 ), \
					FIO_IOC_FIOD )

/* Voltage Monitor state */
enum fio_ts1_vm_state
{
    FIO_TS1_VM_OFF,
    FIO_TS1_VM_ON
};
typedef enum fio_ts1_vm_state FIO_TS1_VM_STATE;

struct	fio_ioc_ts1_vm_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_TS1_VM_STATE	vms;
};
typedef	struct fio_ioc_ts1_vm_set FIO_IOC_TS1_VM_SET;

struct	fio_ioc_ts1_vm_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_TS1_VM_STATE		*vms;
};
typedef	struct fio_ioc_ts1_vm_get FIO_IOC_TS1_VM_GET;

#define	FIOMAN_IOC_TS1_VM_SET	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_22 ), \
					FIO_IOC_TS1_VM_SET )
#define	FIOMAN_IOC_TS1_VM_GET	_IOR( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_23 ), \
					FIO_IOC_TS1_VM_GET )

/* Frame schedule related */
struct fio_frame_schd
{
	unsigned int	req_frame;	/* frame # */
	FIO_HZ		frequency;	/* frame scheduled frequency */
};
typedef struct fio_frame_schd FIO_FRAME_SCHD;

struct	fio_ioc_fiod_frame_schd_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_FRAME_SCHD		*frame_schd;
	unsigned int		count;			/* Size of array for passed data */
};
typedef	struct fio_ioc_fiod_frame_schd_set FIO_IOC_FIOD_FRAME_SCHD_SET;

struct	fio_ioc_fiod_frame_schd_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_FRAME_SCHD		*frame_schd;
	unsigned int		count;			/* Size of array for returned data */
};
typedef	struct fio_ioc_fiod_frame_schd_get FIO_IOC_FIOD_FRAME_SCHD_GET;

#define	FIOMAN_IOC_FIOD_FRAME_SCHD_SET	_IOW( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_24 ), \
					FIO_IOC_FIOD_FRAME_SCHD_SET )
#define	FIOMAN_IOC_FIOD_FRAME_SCHD_GET	_IOR( FIO_IOC_MAGIC, \
					FIO_IOCTL( FIOMAN_IOC_25 ), \
					FIO_IOC_FIOD_FRAME_SCHD_GET )

/* Filtered inputs */
struct fio_input_filter
{
	unsigned int	input_point;	/* input # */
	unsigned int	leading;	/* leading edge filter */
	unsigned int	trailing;	/* trailing edge filter */
};
typedef struct fio_input_filter FIO_INPUT_FILTER;

struct	fio_ioc_fiod_inputs_filter_set
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_INPUT_FILTER	*input_filter;
	unsigned int		count;			/* Size of array for passed data */
};
typedef	struct fio_ioc_fiod_inputs_filter_set FIO_IOC_FIOD_INPUTS_FILTER_SET;

struct	fio_ioc_fiod_inputs_filter_get
{
	FIO_DEV_HANDLE		dev_handle;		/* FIOD being requested */
	FIO_VIEW		view;
	FIO_INPUT_FILTER	*input_filter;
	unsigned int		count;			/* Size of array for returned data */
};
typedef	struct fio_ioc_fiod_inputs_filter_get FIO_IOC_FIOD_INPUTS_FILTER_GET;

#define	FIOMAN_IOC_FIOD_INPUTS_FILTER_SET	_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_26 ), \
						FIO_IOC_FIOD_INPUTS_FILTER_SET )
#define	FIOMAN_IOC_FIOD_INPUTS_FILTER_GET	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_27 ), \
						FIO_IOC_FIOD_INPUTS_FILTER_GET )

/* Frame related */
struct  fio_ioc_fiod_frame_read
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		rx_frame;       /* Response frame number */
	unsigned int		*seq_number;    /* Sequence number of response frame */
	unsigned char		*buf;           /* Buffer to store response frame */
	unsigned int		count;          /* Size of buffer for response frame */
};
typedef	struct fio_ioc_fiod_frame_read	FIO_IOC_FIOD_FRAME_READ;
#define	FIOMAN_IOC_FIOD_FRAME_READ	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_18 ), \
						FIO_IOC_FIOD_FRAME_READ )

struct  fio_ioc_fiod_frame_write
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		tx_frame;       /* Command frame number */
	unsigned char		*payload;       /* Buffer to store command frame */
	unsigned int		count;          /* Size of buffer for command frame */
};
typedef	struct fio_ioc_fiod_frame_write	FIO_IOC_FIOD_FRAME_WRITE;
#define	FIOMAN_IOC_FIOD_FRAME_WRITE	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_49 ), \
						FIO_IOC_FIOD_FRAME_WRITE )
struct  fio_ioc_fiod_frame_size
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		rx_frame;       /* Response frame number */
	unsigned int		*seq_number;    /* Sequence number of response frame */
};
typedef struct fio_ioc_fiod_frame_size	FIO_IOC_FIOD_FRAME_SIZE;
#define FIOMAN_IOC_FIOD_FRAME_SIZE		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_30 ), \
						FIO_IOC_FIOD_FRAME_SIZE )

#define	FIO_SIGIO 	(SIGRTMIN + 4)
enum fio_notify
{
	FIO_NOTIFY_ONCE,
	FIO_NOTIFY_ALWAYS,
};
typedef	enum fio_notify		FIO_NOTIFY;
struct fio_ioc_fiod_frame_notify_reg
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		rx_frame;       /* Response frame number */
	FIO_NOTIFY		notify;		/* Notification type */
};
typedef struct fio_ioc_fiod_frame_notify_reg	FIO_IOC_FIOD_FRAME_NOTIFY_REG;
#define FIOMAN_IOC_FIOD_FRAME_NOTIFY_REG	_IOW( FIO_IOC_MAGIC, \
							FIO_IOCTL( FIOMAN_IOC_31 ), \
							FIO_IOC_FIOD_FRAME_NOTIFY_REG )
							
struct fio_ioc_fiod_frame_notify_dereg
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		rx_frame;       /* Response frame number */
};							
typedef struct fio_ioc_fiod_frame_notify_dereg	FIO_IOC_FIOD_FRAME_NOTIFY_DEREG;
#define FIOMAN_IOC_FIOD_FRAME_NOTIFY_DEREG	_IOW( FIO_IOC_MAGIC, \
							FIO_IOCTL( FIOMAN_IOC_31 ), \
							FIO_IOC_FIOD_FRAME_NOTIFY_DEREG )

enum fio_frame_status
{
	FIO_FRAME_ERROR,
	FIO_FRAME_RECEIVED
};
typedef	enum fio_frame_status FIO_FRAME_STATUS;

struct fio_notify_info
{
	unsigned int	rx_frame;			/* Response Frame # */
	FIO_FRAME_STATUS	status;			/* Status of response frame */
	unsigned int	seq_number;			/* Sequence Number of frame */
	unsigned int	count;				/* # of bytes in response frame */
	FIO_DEV_HANDLE	fiod;				/* FIOD of response frame */
};
typedef	struct	fio_notify_info	FIO_NOTIFY_INFO;
struct fio_ioc_query_notify_info
{
	FIO_NOTIFY_INFO *notify_info;
};
typedef struct fio_ioc_query_notify_info FIO_IOC_QUERY_NOTIFY_INFO;
#define FIOMAN_IOC_QUERY_NOTIFY_INFO	_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_33 ), \
						FIO_IOC_QUERY_NOTIFY_INFO )
						
#define	FIOMAN_IOC_SET_LOCAL_TIME_OFFSET	_IOW( FIO_IOC_MAGIC, \
							FIO_IOCTL( FIOMAN_IOC_21 ), \
							const int * )
typedef char* FIO_IOC_VERSION_GET;
#define	FIOMAN_IOC_VERSION_GET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_28 ), \
						FIO_IOC_VERSION_GET )

typedef FIO_IOC_FIOD	FIO_IOC_QUERY_FIOD;
#define FIOMAN_IOC_QUERY_FIOD		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_34 ), \
						FIO_IOC_QUERY_FIOD )

struct fio_ioc_inputs_trans_get
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	FIO_VIEW		view;
	unsigned char		*data;		/* Array of input points enabled for transitions */
	unsigned int		count;		/* Size of array for returned data */	
};
typedef struct fio_ioc_inputs_trans_get FIO_IOC_INPUTS_TRANS_GET;
#define FIOMAN_IOC_INPUTS_TRANS_GET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_50 ), \
						FIO_IOC_INPUTS_TRANS_GET )						
struct fio_ioc_inputs_trans_set
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned char		*data;		/* Array of input points enabled for transitions */
	unsigned int		count;		/* Size of data array passed */	
};
typedef struct fio_ioc_inputs_trans_set FIO_IOC_INPUTS_TRANS_SET;
#define FIOMAN_IOC_INPUTS_TRANS_SET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_51 ), \
						FIO_IOC_INPUTS_TRANS_SET )						

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
struct fio_ioc_inputs_trans_read
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	FIO_TRANS_STATUS	*status;	/* Array transition status structs */
	FIO_TRANS_BUFFER	*trans_buf;	/* Array of transition entries */
	unsigned int		count;		/* Size of arrays for returned data */	
};
typedef struct fio_ioc_inputs_trans_read FIO_IOC_INPUTS_TRANS_READ;
#define FIOMAN_IOC_INPUTS_TRANS_READ		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_52 ), \
						FIO_IOC_INPUTS_TRANS_READ )						


/* Watchdog service */
#define FIOMAN_IOC_WD_REG		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_40 ), \
						FIO_DEV_HANDLE )						
#define FIOMAN_IOC_WD_DEREG		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_41 ), \
						FIO_DEV_HANDLE )
struct fio_ioc_wd_res_get
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		*output;	/* Output number */
};							
typedef struct fio_ioc_wd_res_get	FIO_IOC_FIOD_WD_RES_GET;
#define FIOMAN_IOC_WD_RES_GET		_IOR( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_42 ), \
						FIO_IOC_FIOD_WD_RES_GET )						
struct fio_ioc_wd_res_set
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	unsigned int		output;		/* Output number */
};							
typedef struct fio_ioc_wd_res_set	FIO_IOC_FIOD_WD_RES_SET;
#define FIOMAN_IOC_WD_RES_SET		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_43 ), \
						FIO_IOC_FIOD_WD_RES_SET )
#define FIOMAN_IOC_WD_HB		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_44 ), \
						FIO_DEV_HANDLE )
						
#define FIOMAN_IOC_HM_REG		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_45 ), \
						unsigned int )
#define FIOMAN_IOC_HM_DEREG		_IO( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_46 ))
#define FIOMAN_IOC_HM_HEARTBEAT		_IO( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_47 ))
#define FIOMAN_IOC_HM_RESET		_IO( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_48 ))

/*****************************************************************************/

#endif /* #ifndef _FIODRIVER_H_ */


/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
