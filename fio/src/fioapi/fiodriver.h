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

#include "fioapi.h"

/* File used to open FIO Driver */
#define	FIO_DEV		"/dev/fio"

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
        FIOMAN_IOC_54,                  /* Begin outputs set transaction */
        FIOMAN_IOC_55,                  /* Commit outputs set transaction */	
        FIOMAN_IOC_56,                  /* Get state of TS2 port 1 disable pin */
        FIOMAN_IOC_57,                  /* Set watchdog toggle rate */
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

#ifdef TS2_PORT1_STATE
struct fio_ioc_ts2_port1_state
{
        FIO_PORT                port;
        FIO_TS2_PORT1_STATE     *state;
};
typedef struct fio_ioc_ts2_port1_state FIO_IOC_TS2_PORT1_STATE;

#define FIOMAN_IOC_TS2_PORT1_STATE    _IOR( FIO_IOC_MAGIC, \
                                                FIO_IOCTL( FIOMAN_IOC_56 ), \
                                                FIO_IOC_TS2_PORT1_STATE )
#endif

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
struct fio_ioc_fiod_frame_read
{
	FIO_DEV_HANDLE dev_handle;    /* FIOD being requested */
	unsigned int   rx_frame;      /* Response frame number */
	unsigned int   *seq_number;   /* Sequence number of response frame */
	unsigned char  *buf;          /* Buffer to store response frame */
	unsigned int   count;         /* Size of buffer for response frame */
        unsigned int   timeout;       /* Maximum time to wait in milliseconds */
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
#define FIOMAN_IOC_INPUTS_TRANS_SET		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_51 ), \
						FIO_IOC_INPUTS_TRANS_SET )						

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
#ifdef NEW_WATCHDOG
struct fio_ioc_wd_rate_set
{
	FIO_DEV_HANDLE		dev_handle;	/* FIOD being requested */
	FIO_HZ                  rate;		/* Frequency enum (toggle rate) */
};							
typedef struct fio_ioc_wd_rate_set	FIO_IOC_FIOD_WD_RATE_SET;
#define FIOMAN_IOC_WD_RATE_SET		_IOW( FIO_IOC_MAGIC, \
						FIO_IOCTL( FIOMAN_IOC_57 ), \
						FIO_IOC_FIOD_WD_RATE_SET )                                                
#endif

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
/* Set Outputs Transaction */
#define FIOMAN_IOC_TRANSACTION_BEGIN    _IO( FIO_IOC_MAGIC, \
                                                FIO_IOCTL( FIOMAN_IOC_54 ))
#define FIOMAN_IOC_TRANSACTION_COMMIT   _IO( FIO_IOC_MAGIC, \
                                                FIO_IOCTL( FIOMAN_IOC_55 ))
/*****************************************************************************/

#endif /* #ifndef _FIODRIVER_H_ */


/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
