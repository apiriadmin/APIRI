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

This file contains all definitions for the FIO API.

*/
/*****************************************************************************/

#ifndef _FIO_API_H_
#define _FIO_API_H_

/*  Include section.
-----------------------------------------------------------------------------*/

/* System includes. */
#include <stdbool.h>
#include <signal.h>


/* Local includes. */
#include "fioapi.h"	/* FIO Type Definitions */


/*  Definition section.
-----------------------------------------------------------------------------*/
/*  Module public structure/enum definition.*/


/*  Global section.
-----------------------------------------------------------------------------*/

typedef	int FIO_APP_HANDLE;	/* FIO API Handle from fio_register */

// This will not compile, saying that it does not generate a constant
// #define	FIO_SIGIO				(SIGRTMIN + 4)
// So the following is defined
//#define	FIO_SIGIO				(32 + 4)

/*  Public Interface section.
-----------------------------------------------------------------------------*/

/* Register with the FIO API */
extern	FIO_APP_HANDLE fio_register( );

/* Deregister with the FIO API */
extern	int fio_deregister( FIO_APP_HANDLE );

/* Register with the FIO API to access FIOD services */
extern FIO_DEV_HANDLE fio_fiod_register( FIO_APP_HANDLE,
					FIO_PORT,
					FIO_DEVICE_TYPE);

/* Deregister a FIOD */
extern	int fio_fiod_deregister( FIO_APP_HANDLE, FIO_DEV_HANDLE );

/* Enable a FIOD */
extern	int fio_fiod_enable( FIO_APP_HANDLE, FIO_DEV_HANDLE );

/* Disable a FIOD */
extern	int fio_fiod_disable( FIO_APP_HANDLE, FIO_DEV_HANDLE );

/* Query for FIOD on FIO_PORT */
extern int fio_query_fiod( FIO_APP_HANDLE, FIO_PORT, FIO_DEVICE_TYPE );

/* Get API implementation version info. */
extern char *fio_apiver( FIO_APP_HANDLE, FIO_VERSION );

/* Read state of input points */
extern	int fio_fiod_inputs_get( FIO_APP_HANDLE, 
				FIO_DEV_HANDLE,
				FIO_INPUTS_TYPE,
				unsigned char *,
				unsigned int );

/* Read state of output points */
extern	int fio_fiod_outputs_get( FIO_APP_HANDLE,
				FIO_DEV_HANDLE,
				FIO_VIEW,
				unsigned char *,
				unsigned char *,
				unsigned int );

/* Set state of output points */
extern	int fio_fiod_outputs_set( FIO_APP_HANDLE,
				FIO_DEV_HANDLE,
				unsigned char *,
				unsigned char *,
				unsigned int );

/* Reserve Output points */
extern	int fio_fiod_outputs_reservation_set( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					unsigned char *,
					unsigned int );

/* Get Output points reserved */
extern	int fio_fiod_outputs_reservation_get( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_VIEW,
					unsigned char *,
					unsigned int );

/* Begin multi-device outputs set transaction */
extern int fio_fiod_begin_outputs_set( FIO_APP_HANDLE );

/* Commit multi-device outputs set transaction */
extern int fio_fiod_commit_outputs_set( FIO_APP_HANDLE );

extern int fio_fiod_channel_map_count( FIO_APP_HANDLE,
                                       FIO_DEV_HANDLE,
                                       FIO_VIEW );

extern int fio_fiod_channel_map_get( FIO_APP_HANDLE,
                                     FIO_DEV_HANDLE,
                                     FIO_VIEW,
                                     FIO_CHANNEL_MAP *,
                                     unsigned int );

extern int fio_fiod_channel_map_set( FIO_APP_HANDLE,
                                     FIO_DEV_HANDLE,
                                     FIO_CHANNEL_MAP *,
                                     unsigned int );

extern int fio_fiod_channel_reservation_get( FIO_APP_HANDLE,
                                             FIO_DEV_HANDLE,
                                             FIO_VIEW,
                                             unsigned char *,
                                             unsigned int );

extern int fio_fiod_channel_reservation_set( FIO_APP_HANDLE,
                                             FIO_DEV_HANDLE,
                                             unsigned char *,
                                             unsigned int );   

extern int fio_fiod_mmu_flash_bit_get( FIO_APP_HANDLE,
                                       FIO_DEV_HANDLE,
                                       FIO_VIEW,
                                       FIO_MMU_FLASH_BIT *);   

extern int fio_fiod_mmu_flash_bit_set( FIO_APP_HANDLE,
                                       FIO_DEV_HANDLE,
                                       FIO_MMU_FLASH_BIT );

extern int fio_fiod_ts_fault_monitor_get( FIO_APP_HANDLE,
                                          FIO_DEV_HANDLE,
                                          FIO_VIEW,
                                          FIO_TS_FM_STATE *);

extern int fio_fiod_ts_fault_monitor_set( FIO_APP_HANDLE,
                                          FIO_DEV_HANDLE,
                                          FIO_TS_FM_STATE );

extern int fio_fiod_ts1_volt_monitor_get( FIO_APP_HANDLE,
                                          FIO_DEV_HANDLE,
                                          FIO_VIEW,
                                          FIO_TS1_VM_STATE *);

extern int fio_fiod_ts1_volt_monitor_set( FIO_APP_HANDLE,
                                          FIO_DEV_HANDLE,
                                          FIO_TS1_VM_STATE );

extern int fio_fiod_cmu_config_change_count( FIO_APP_HANDLE,
						FIO_DEV_HANDLE );
						
extern int fio_fiod_cmu_dark_channel_get( FIO_APP_HANDLE,
						FIO_DEV_HANDLE,
						FIO_VIEW,
						FIO_CMU_DC_MASK *);

extern int fio_fiod_cmu_dark_channel_set( FIO_APP_HANDLE,
						FIO_DEV_HANDLE,
						FIO_CMU_DC_MASK );
						
extern int fio_fiod_cmu_fault_get( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_VIEW,
					FIO_CMU_FSA *);

extern int fio_fiod_cmu_fault_set( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_CMU_FSA );
					
extern int fio_fiod_status_get( FIO_APP_HANDLE,
                                FIO_DEV_HANDLE,
                                FIO_FIOD_STATUS *);

extern int fio_fiod_status_reset( FIO_APP_HANDLE,
                                FIO_DEV_HANDLE);

extern int fio_fiod_frame_schedule_get( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_VIEW,
					FIO_FRAME_SCHD *,
					unsigned int );
					
extern int fio_fiod_frame_schedule_set( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_FRAME_SCHD *,
					unsigned int );
					
extern int fio_fiod_inputs_filter_get( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_VIEW,
					FIO_INPUT_FILTER *,
					unsigned int );
					
extern int fio_fiod_inputs_filter_set( FIO_APP_HANDLE,
					FIO_DEV_HANDLE,
					FIO_INPUT_FILTER *,
					unsigned int );
					
extern int fio_fiod_frame_read( FIO_APP_HANDLE fh,
                                FIO_DEV_HANDLE dh,
                                unsigned int rx_frame,
                                unsigned int *seq_number,
                                unsigned char *buf,
                                unsigned int count,
                                unsigned int timeout);
                                
extern int fio_fiod_frame_size( FIO_APP_HANDLE,
                                FIO_DEV_HANDLE,
                                unsigned int,
                                unsigned int *);

extern int fio_fiod_frame_notify_register( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					unsigned int rx_frame,
					FIO_NOTIFY notify);

extern int fio_fiod_frame_notify_deregister( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					unsigned int rx_frame);

extern int fio_query_frame_notify_status( FIO_APP_HANDLE,
					FIO_NOTIFY_INFO *);
					
extern int fio_set_local_time_offset( FIO_APP_HANDLE,
					const int * );

extern int fio_fiod_inputs_trans_get( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					FIO_VIEW view,
					unsigned char *data,
					unsigned int count);

extern int fio_fiod_inputs_trans_read( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					FIO_TRANS_STATUS *status,
					FIO_TRANS_BUFFER *buf,
					unsigned int count);

extern int fio_fiod_inputs_trans_set( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					unsigned char *data,
					unsigned int count);

extern int fio_fiod_wd_register( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh);

extern int fio_fiod_wd_deregister( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh);
					
extern int fio_fiod_wd_reservation_get( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					unsigned int *op);

extern int fio_fiod_wd_reservation_set( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					unsigned int op);

#ifdef NEW_WATCHDOG
extern int fio_fiod_wd_rate_set( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					FIO_HZ rate);
#endif

extern int fio_fiod_wd_heartbeat( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh);
					
extern int fio_hm_register( FIO_APP_HANDLE fh, unsigned int timeout);

extern int fio_hm_deregister( FIO_APP_HANDLE fh );

extern int fio_hm_heartbeat( FIO_APP_HANDLE fh );

extern int fio_hm_fault_reset( FIO_APP_HANDLE fh );

extern int fio_fiod_frame_write( FIO_APP_HANDLE fh,
					FIO_DEV_HANDLE dh,
					unsigned int tx_frame,
					unsigned char *payload,
					unsigned int count);

#ifdef TS2_PORT1_STATE
extern int fio_ts2_port1_state( FIO_APP_HANDLE fh, FIO_PORT port,
                                        FIO_TS2_PORT1_STATE * );
#endif				
#endif /* #ifndef _FIO_API_H_ */


/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
