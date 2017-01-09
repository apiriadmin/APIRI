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

This module contains all code for the FIO API.

*/
/*****************************************************************************/

/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */
#include	<stdlib.h>				/* STD Library Definitions */
#include	<unistd.h>				/* UNIX Standard Definitions */
#include	<sys/ioctl.h>			/* IOCTL Definitions */
#include	<string.h>				/* String Definitions */
#include	<errno.h>				/* Errno Definitions */
#include	<sys/types.h>			/* System Types Definitions */
#include	<sys/stat.h>			/* System Stat Definitions */
#include	<fcntl.h>				/* File Control Definitions */

/* Local includes. */
#include	"fio.h"					/* FIO API Definitions */
#include        "fiodriver.h"


/*  Definition section.
-----------------------------------------------------------------------------*/
/* For logging purposes. */


/*  Global section.
-----------------------------------------------------------------------------*/


/*  Private API declaration (prototype) section.
-----------------------------------------------------------------------------*/


/*  Public API interface section.
-----------------------------------------------------------------------------*/


/*  Private API implementation section.
-----------------------------------------------------------------------------*/


/*  Public API implementation section.
-----------------------------------------------------------------------------*/

/*****************************************************************************/
/*
Register with the FIO API.
*/
/*****************************************************************************/

FIO_APP_HANDLE
fio_register
(
)
{
	return ( ( FIO_APP_HANDLE )open( FIO_DEV, O_RDWR ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Deregister with the FIO API.
*/
/*****************************************************************************/

int
fio_deregister
(
	FIO_APP_HANDLE	app_handle	/* APP handle from fio_register() */
)
{
								/* Close the FIO API */
	return ( close( (int)( app_handle ) ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Register with the FIO API to access FIOD services.
*/
/*****************************************************************************/

FIO_DEV_HANDLE
fio_fiod_register
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_PORT		port,		/* Port of FIOD */
	FIO_DEVICE_TYPE	dev			/* FIOD to register with */
)
{
	FIO_IOC_FIOD	ioc_fiod;	/* Structure used for ioctl */

	/* Set up structure to pass */
	ioc_fiod.port = port;
	ioc_fiod.fiod = dev;

	/* Register this FIOD on this port */
	return ( ( FIO_DEV_HANDLE )ioctl( (int)app_handle, FIOMAN_IOC_REG_FIOD, &ioc_fiod ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Deregister a FIOD.
*/
/*****************************************************************************/

int
fio_fiod_deregister
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle	/* FIOD Handle from fio_fiod_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_DEREG_FIOD, dev_handle ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Enable a FIOD.
*/
/*****************************************************************************/

int
fio_fiod_enable
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle	/* FIOD Handle from fio_fiod_register() */
)
{
	/* Enable this FIOD on this port */
	return ( ioctl( (int)app_handle, FIOMAN_IOC_ENABLE_FIOD, dev_handle ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Disable a FIOD.
*/
/*****************************************************************************/

int
fio_fiod_disable
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle	/* FIOD Handle from fio_fiod_register() */
)
{
	/* Disable this FIOD on this port */
	return ( ioctl( (int)app_handle, FIOMAN_IOC_DISABLE_FIOD, dev_handle ) );
}

/*****************************************************************************/
/*
Query a FIOD is attached to FIO_PORT.
*/
/*****************************************************************************/
int
fio_query_fiod
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_PORT	port,
	FIO_DEVICE_TYPE	dev
)
{
	FIO_IOC_QUERY_FIOD request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.port = port;
	request.fiod = dev;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_QUERY_FIOD, &request ) );
}

/*****************************************************************************/
static char lkm_version[80] = "";
char *fio_apiver( FIO_APP_HANDLE app_handle, FIO_VERSION which )
{
	if (which == FIO_VERSION_LIBRARY)
		return( "Intelight, 2.01, 2.17" );
	if (which == FIO_VERSION_LKM) {
		if (ioctl( (int)app_handle, FIOMAN_IOC_VERSION_GET, lkm_version) != -1) {
			return lkm_version;
		}
	}
	
	return (NULL);
}

/*****************************************************************************/
/*
This function is used to reserve output points for an application
*/
/*****************************************************************************/

int
fio_fiod_outputs_reservation_set
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,	/* FIOD Handle from fio_fiod_register() */
	unsigned char	*data,		/* Output Pointss to reserve */
	unsigned int	count
)
{
	FIO_IOC_RESERVE_SET	reserve;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	reserve.dev_handle = dev_handle;
	reserve.data = data;
	reserve.count = count;

	/* Reserve these output points */
	return ( ioctl( (int)app_handle, FIOMAN_IOC_RESERVE_SET, &reserve ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get output reservations for this APP or the system.
*/
/*****************************************************************************/

int
fio_fiod_outputs_reservation_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW		view,			/* Type of reservations desired */
	unsigned char		*data,			/* Where to place reservations */
	unsigned int		count
)
{
	FIO_IOC_RESERVE_GET	reserve;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	reserve.dev_handle = dev_handle;
	reserve.view = view;
	reserve.data = data;
	reserve.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_RESERVE_GET, &reserve ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to reserve CMU/MMU channels for an application
*/
/*****************************************************************************/

int
fio_fiod_channel_reservation_set
(
	FIO_APP_HANDLE	app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,	/* FIOD Handle from fio_fiod_register() */
	unsigned char	*data,		/* channels to reserve */
	unsigned int    count       	/* size of channel data */
)
{
	FIO_IOC_CHANNEL_RESERVE_SET	reserve;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	reserve.dev_handle = dev_handle;
	reserve.data = data;
	reserve.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CHANNEL_RESERVE_SET, &reserve ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get channel reservations for this APP or the system.
*/
/*****************************************************************************/

int
fio_fiod_channel_reservation_get
(
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW	view,			/* Type of reservations desired */
	unsigned char	*data,			/* Where to place reservations */
	unsigned int	count			/* Size in bytes of returned data */
)
{
	FIO_IOC_CHANNEL_RESERVE_GET	reserve;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	reserve.dev_handle = dev_handle;
	reserve.view = view;
	reserve.data = data;
	reserve.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CHANNEL_RESERVE_GET, &reserve ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to return the current state of output points.
*/
/*****************************************************************************/

int
fio_fiod_outputs_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW		view,			/* Type of outputs desired */
	unsigned char		*plus,			/* Where to place plus */
	unsigned char		*minus,			/* Where to place minus */
	unsigned int		num_ls_bytes		/* Buffer size */
)
{
	FIO_IOC_OUTPUTS_GET	outputs;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	outputs.dev_handle = dev_handle;
	outputs.view = view;
	outputs.ls_plus  = plus;
	outputs.ls_minus = minus;
	outputs.num_ls_bytes = num_ls_bytes;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_OUTPUTS_GET, &outputs ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the state of reserved output points.
*/
/*****************************************************************************/

int
fio_fiod_outputs_set
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	unsigned char		*plus,			/* Load Switch Plus to set */
	unsigned char		*minus,			/* Load Switch Minus to set */
	unsigned int		num_ls_bytes		/* Buffer size */
)
{
	FIO_IOC_OUTPUTS_SET	outputs;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	outputs.dev_handle = dev_handle;
	outputs.ls_plus  = plus;
	outputs.ls_minus = minus;
	outputs.num_ls_bytes = num_ls_bytes;

	/* Reserve these output points */
	return ( ioctl( (int)app_handle, FIOMAN_IOC_OUTPUTS_SET, &outputs ) );
}

/*****************************************************************************/

/* Begin multi-device outputs set transaction */
int
fio_fiod_begin_outputs_set
(
        FIO_APP_HANDLE app_handle
)
{
        return ( ioctl( (int)app_handle, FIOMAN_IOC_TRANSACTION_BEGIN ) );
}

/* Commit multi-device outputs set transaction */
int
fio_fiod_commit_outputs_set
(
        FIO_APP_HANDLE app_handle
)
{
        return ( ioctl( (int)app_handle, FIOMAN_IOC_TRANSACTION_COMMIT ) );
}

/*****************************************************************************/
/*
This function returns a count of channel mappings for the APP or SYSTEM view.
*/
/*****************************************************************************/

int
fio_fiod_channel_map_count
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW		view			/* Type of outputs desired */
)
{
	FIO_IOC_CHAN_MAP_COUNT	count;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	count.dev_handle = dev_handle;
	count.view = view;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CHAN_MAP_COUNT, &count ) );
}

/*****************************************************************************/
/*
This function is used to return the current channel map.
*/
/*****************************************************************************/

int
fio_fiod_channel_map_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW		view,			/* Type of outputs desired */
	FIO_CHANNEL_MAP		*map,			/* Where to place channel map */
	unsigned int		count			/* Channel map count of entries */
)
{
	FIO_IOC_CHAN_MAP_GET	mappings;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
    mappings.dev_handle = dev_handle;
	mappings.view = view;
    mappings.map = map;
    mappings.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CHAN_MAP_GET, &mappings ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the state of channel mappings.
*/
/*****************************************************************************/

int
fio_fiod_channel_map_set
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_CHANNEL_MAP		*map,			/* channel map to set */
	unsigned int		count			/* count of map entries to set */
)
{
	FIO_IOC_CHAN_MAP_SET	mappings;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	mappings.dev_handle = dev_handle;
    mappings.map = map;
    mappings.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CHAN_MAP_SET, &mappings ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to return the current state of input points.
*/
/*****************************************************************************/

int
fio_fiod_inputs_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_INPUTS_TYPE		inputs_type,		/* Type of inputs desired */
	unsigned char		*data,			/* Where to place data */
	unsigned int		num_data_bytes		/* Size of buffer */
)
{
	FIO_IOC_INPUTS_GET	inputs;			/* IOCTL argument structure */

	/* Set up IOCTL structure */
	inputs.dev_handle = dev_handle;
	inputs.inputs_type = inputs_type;
	inputs.data = data;
	inputs.num_data_bytes = num_data_bytes;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_INPUTS_GET, &inputs ) );
}

/*****************************************************************************/
/*
This function is used to return the current mmu load switch flash bit.
*/
/*****************************************************************************/

int
fio_fiod_mmu_flash_bit_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW			view,			/* Type of outputs desired */
	FIO_MMU_FLASH_BIT	*fb			    /* Where to place mmu flash bit */
)
{
	FIO_IOC_MMU_FLASH_BIT_GET	flash_bit;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
    flash_bit.dev_handle = dev_handle;
	flash_bit.view = view;
    flash_bit.fb = fb;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_MMU_FLASH_BIT_GET, &flash_bit ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the state of mmu load switch flash bit.
*/
/*****************************************************************************/

int
fio_fiod_mmu_flash_bit_set
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_MMU_FLASH_BIT	fb			    /* flash bit state */
)
{
	FIO_IOC_MMU_FLASH_BIT_SET	flash_bit;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
    flash_bit.dev_handle = dev_handle;
    flash_bit.fb = fb;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_MMU_FLASH_BIT_SET, &flash_bit ) );
}

/*****************************************************************************/
/*
This function is used to return the current Fault Monitor state.
*/
/*****************************************************************************/

int
fio_fiod_ts_fault_monitor_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW			view,			/* Type of outputs desired */
	FIO_TS_FM_STATE     *fms		    /* Where to place fault monitor state value */
)
{
	FIO_IOC_TS_FMS_GET	ts_fms;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
    ts_fms.dev_handle = dev_handle;
	ts_fms.view = view;
    ts_fms.fms = fms;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_TS_FMS_GET, &ts_fms ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the state of the Fault Monitor.
*/
/*****************************************************************************/

int
fio_fiod_ts_fault_monitor_set
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_TS_FM_STATE     fms			    /* fault monitor state */
)
{
	FIO_IOC_TS_FMS_SET	ts_fms;	        /* IOCTL argument structure */

	/* Set up IOCTL structure */
    ts_fms.dev_handle = dev_handle;
    ts_fms.fms = fms;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_TS_FMS_SET, &ts_fms ) );
}

/*****************************************************************************/
/*
This function is used to return the current Voltage Monitor state.
*/
/*****************************************************************************/

int
fio_fiod_ts1_volt_monitor_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW			view,			/* Type of outputs desired */
	FIO_TS1_VM_STATE     *vms		    /* Where to place voltage monitor state value */
)
{
	FIO_IOC_TS1_VM_GET	ts1_vms;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	ts1_vms.dev_handle = dev_handle;
	ts1_vms.view = view;
	ts1_vms.vms = vms;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_TS1_VM_GET, &ts1_vms ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the state of the Voltage Monitor.
*/
/*****************************************************************************/

int
fio_fiod_ts1_volt_monitor_set
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_TS1_VM_STATE     vms			/* voltage monitor state */
)
{
	FIO_IOC_TS1_VM_SET	ts1_vms;	        /* IOCTL argument structure */

	/* Set up IOCTL structure */
	ts1_vms.dev_handle = dev_handle;
	ts1_vms.vms = vms;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_TS1_VM_SET, &ts1_vms ) );
}
#ifdef TS2_PORT1_STATE
/*****************************************************************************/
/*
This function is used to return the state of the TS2 port1 disable pin.
*/
/*****************************************************************************/

int
fio_ts2_port1_state
(
	FIO_APP_HANDLE	app_handle,     /* FIO APP Handle from fio_register() */
	FIO_PORT	port,           /* TS2 serial port in use */
        FIO_TS2_PORT1_STATE *state
)
{
        FIO_IOC_TS2_PORT1_STATE port1_state;
        port1_state.port = port;
        port1_state.state = state;
        
	return ( ioctl( (int)app_handle, FIOMAN_IOC_TS2_PORT1_STATE, &port1_state ) );
}
#endif
/*****************************************************************************/
/*
This function is used to return the CMU configuration change count.
*/
/*****************************************************************************/

int
fio_fiod_cmu_config_change_count
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle	/* FIOD Handle fio_fiod_register() */
)
{

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CMU_CFG_CHANGE_COUNT, dev_handle) );
}

/*****************************************************************************/
/*
This function is used to return the CMU dark channel mask.
*/
/*****************************************************************************/

int
fio_fiod_cmu_dark_channel_get
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	FIO_VIEW		view,		/* Type of outputs desired */
	FIO_CMU_DC_MASK     	*mask		/* Where to place dark channel mask value */
)
{
	FIO_IOC_CMU_DC_GET	request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.view = view;
	request.mask = mask;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CMU_DC_GET, &request ) );
}


/*****************************************************************************/
/*
This function is used to set the CMU dark channel mask.
*/
/*****************************************************************************/

int
fio_fiod_cmu_dark_channel_set
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	FIO_CMU_DC_MASK		mask		/* voltage monitor state */
)
{
	FIO_IOC_CMU_DC_SET	request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.mask = mask;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CMU_DC_SET, &request ) );
}
						
/*****************************************************************************/
/*
This function is used to return the CMU failed state (fault) action.
*/
/*****************************************************************************/

int
fio_fiod_cmu_fault_get
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	FIO_VIEW		view,		/* Type of outputs desired */
	FIO_CMU_FSA     	*fsa		/* Where to place fsa value */
)
{
	FIO_IOC_CMU_FSA_GET	request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.view = view;
	request.fsa = fsa;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CMU_FSA_GET, &request ) );
}


/*****************************************************************************/
/*
This function is used to set the CMU failed state (fault) action.
*/
/*****************************************************************************/

int
fio_fiod_cmu_fault_set
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	FIO_CMU_FSA		fsa		/* failed state action */
)
{
	FIO_IOC_CMU_FSA_SET	request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.fsa = fsa;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_CMU_FSA_SET, &request ) );
}

/*****************************************************************************/
/*
This function is used to return the fio device status information.
*/
/*****************************************************************************/

int
fio_fiod_status_get
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_FIOD_STATUS     *status         /* Where to place fiod status information */
)
{
	FIO_IOC_FIOD_STATUS request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
    request.dev_handle = dev_handle;
    request.status = status;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_STATUS_GET, &request ) );
}

/*****************************************************************************/
/*
This function is used to reset the fio device status counts.
*/
/*****************************************************************************/

int
fio_fiod_status_reset
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle		/* FIOD Handle fio_fiod_register() */
)
{
	/* Reset status counts for FIOD on this port */
	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_STATUS_RESET, dev_handle ) );
}

/*****************************************************************************/
/*
This function is used to request a list of frame schedule frequencies.
*/
/*****************************************************************************/

int fio_fiod_frame_schedule_get (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW	view,			/* Type desired */
	FIO_FRAME_SCHD	*frame_schd,
	unsigned int	count
)
{
	FIO_IOC_FIOD_FRAME_SCHD_GET request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.view = view;
	request.frame_schd = frame_schd;
	request.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_SCHD_GET, &request ) );
}

/*****************************************************************************/
/*
This function is used to set a list of frame schedule frequencies.
*/
/*****************************************************************************/

int fio_fiod_frame_schedule_set (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_FRAME_SCHD	*frame_schd,
	unsigned int	count
)
{
	FIO_IOC_FIOD_FRAME_SCHD_SET request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.frame_schd = frame_schd;
	request.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_SCHD_SET, &request ) );
}

/*****************************************************************************/
/*
This function is used to request a list of input filter values.
*/
/*****************************************************************************/

int fio_fiod_inputs_filter_get (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW	view,			/* Type desired */
	FIO_INPUT_FILTER *input_filter,
	unsigned int	count
)
{
	FIO_IOC_FIOD_INPUTS_FILTER_GET request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.view = view;
	request.input_filter = input_filter;
	request.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_INPUTS_FILTER_GET, &request ) );
}

/*****************************************************************************/
/*
This function is used to set a list of input filter values.
*/
/*****************************************************************************/

int fio_fiod_inputs_filter_set (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_INPUT_FILTER *input_filter,
	unsigned int	count
)
{
	FIO_IOC_FIOD_INPUTS_FILTER_SET request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.input_filter = input_filter;
	request.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_INPUTS_FILTER_SET, &request ) );
}


/*****************************************************************************/
/*
This function is used to request the last response frame of given type.
*/
/*****************************************************************************/

int fio_fiod_frame_read
(
	FIO_APP_HANDLE app_handle,  /* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE dev_handle,  /* FIOD Handle fio_fiod_register() */
	unsigned int   rx_frame,
	unsigned int   *seq_number,
	unsigned char  *buf,
	unsigned int   count,
        unsigned int   timeout
)
{
	FIO_IOC_FIOD_FRAME_READ request; /* IOCTL argument structure */

        /* Set up IOCTL structure */
        request.dev_handle = dev_handle;
        request.rx_frame = rx_frame;
        request.seq_number = seq_number;
        request.buf = buf;
        request.count = count;
        request.timeout = timeout;

        return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_READ, &request ) );
}

/*****************************************************************************/
/*
This function is used to request an arbitrary command frame to be sent once.
*/
/*****************************************************************************/
int
fio_fiod_frame_write
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,		/* FIOD Handle fio_fiod_register() */
	unsigned int		tx_frame,
	unsigned char		*payload,
	unsigned int		count
)
{
	FIO_IOC_FIOD_FRAME_WRITE request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.tx_frame = tx_frame;
	request.payload = payload;
	request.count = count;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_WRITE, &request ) );
}

/*****************************************************************************/
/*
This function is used to request the size of last response frame of given type.
*/
/*****************************************************************************/

int
fio_fiod_frame_size
(
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	unsigned int	rx_frame,
	unsigned int	*seq_number
)
{
	FIO_IOC_FIOD_FRAME_SIZE request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.rx_frame = rx_frame;
	request.seq_number = seq_number;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_SIZE, &request ) );
}

/*****************************************************************************/
/*
This function is used to register a frame notification.
*/
/*****************************************************************************/

int fio_fiod_frame_notify_register (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	unsigned int	rx_frame,		/* Frame number to notify receipt of */
	FIO_NOTIFY	notify
)
{
	FIO_IOC_FIOD_FRAME_NOTIFY_REG request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.rx_frame = rx_frame;
	request.notify = notify;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_NOTIFY_REG, &request ) );
}

/*****************************************************************************/
/*
This function is used to deregister a frame notification.
*/
/*****************************************************************************/

int fio_fiod_frame_notify_deregister (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	unsigned int	rx_frame		/* Frame number of notification */
)
{
	FIO_IOC_FIOD_FRAME_NOTIFY_DEREG request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.rx_frame = rx_frame;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_FIOD_FRAME_NOTIFY_DEREG, &request ) );
}

/*****************************************************************************/
/*
This function is used to query a frame notification.
*/
/*****************************************************************************/

int fio_query_frame_notify_status (
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_NOTIFY_INFO *notify_info
)
{
	FIO_IOC_QUERY_NOTIFY_INFO request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.notify_info = notify_info;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_QUERY_NOTIFY_INFO, &request ) );
}

/*****************************************************************************/
/*
This function is used to get the inputs enabled for transition monitoring.
*/
/*****************************************************************************/
int
fio_fiod_inputs_trans_get
(
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	FIO_VIEW	view,			/* Type desired */
	unsigned char	*data,
	unsigned int	count
)
{
	FIO_IOC_INPUTS_TRANS_GET request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.view = view;
	request.data = data;
	request.count = count;
	
	return ( ioctl( (int)app_handle, FIOMAN_IOC_INPUTS_TRANS_GET, &request ) );
}

/*****************************************************************************/
/*
This function is used to set the inputs enabled for transition monitoring.
*/
/*****************************************************************************/
int
fio_fiod_inputs_trans_set
(
	FIO_APP_HANDLE	app_handle,		/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE	dev_handle,		/* FIOD Handle fio_fiod_register() */
	unsigned char	*data,
	unsigned int	count
)
{
	FIO_IOC_INPUTS_TRANS_SET request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.data = data;
	request.count = count;
	
	return ( ioctl( (int)app_handle, FIOMAN_IOC_INPUTS_TRANS_SET, &request ) );
}

/*****************************************************************************/
/*
This function is used to read the input transition entry buffer.
*/
/*****************************************************************************/
int
fio_fiod_inputs_trans_read
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	FIO_TRANS_STATUS	*status,	/* Array to receive transition status */
	FIO_TRANS_BUFFER	*trans_buf,	/* Array to receive transition entries */
	unsigned int		count
)
{
	FIO_IOC_INPUTS_TRANS_READ request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.status = status;
	request.trans_buf = trans_buf;
	request.count = count;
	
	return ( ioctl( (int)app_handle, FIOMAN_IOC_INPUTS_TRANS_READ, &request ) );
}

/*****************************************************************************/
/*
This function is used to set the local time offset in seconds.
*/
/*****************************************************************************/

int
fio_set_local_time_offset
(
	FIO_APP_HANDLE		app_handle,		/* FIO APP Handle from fio_register() */
    const int   *tzsec_offset
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_SET_LOCAL_TIME_OFFSET, tzsec_offset ) );
}

/*****************************************************************************/
/*
This function is used to register for the watchdog service.
*/
/*****************************************************************************/

int
fio_fiod_wd_register
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle	/* FIOD Handle fio_fiod_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_WD_REG, dev_handle) );
}

/*****************************************************************************/
/*
This function is used to deregister for the watchdog service.
*/
/*****************************************************************************/

int
fio_fiod_wd_deregister
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle	/* FIOD Handle fio_fiod_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_WD_DEREG, dev_handle) );
}

/*****************************************************************************/
/*
This function is used to return the reserved watchdog output.
*/
/*****************************************************************************/

int
fio_fiod_wd_reservation_get
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	unsigned int     	*output		/* Where to place output value */
)
{
	FIO_IOC_FIOD_WD_RES_GET	request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.output = output;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_WD_RES_GET, &request ) );
}


/*****************************************************************************/
/*
This function is used to set the watchdog reserved output.
*/
/*****************************************************************************/

int
fio_fiod_wd_reservation_set
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	unsigned int		output		/* watchdog output value */
)
{
	FIO_IOC_FIOD_WD_RES_SET	request;	/* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.output = output;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_WD_RES_SET, &request ) );
}
#ifdef NEW_WATCHDOG
/*****************************************************************************/
/*
This function is used to set the watchdog toggle rate.
*/
/*****************************************************************************/

int
fio_fiod_wd_rate_set
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle,	/* FIOD Handle fio_fiod_register() */
	FIO_HZ		        rate		/* watchdog toggle rate enum */
)
{
	FIO_IOC_FIOD_WD_RATE_SET        request;        /* IOCTL argument structure */

	/* Set up IOCTL structure */
	request.dev_handle = dev_handle;
	request.rate = rate;

	return ( ioctl( (int)app_handle, FIOMAN_IOC_WD_RATE_SET, &request ) );
}
#endif
/*****************************************************************************/
/*
This function is used to toggle the watchdog output.
*/
/*****************************************************************************/

int
fio_fiod_wd_heartbeat
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	FIO_DEV_HANDLE		dev_handle	/* FIOD Handle fio_fiod_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_WD_HB, dev_handle) );
}

/*****************************************************************************/
/*
This function is used to register for the health monitor service.
*/
/*****************************************************************************/

int
fio_hm_register
(
	FIO_APP_HANDLE		app_handle,	/* FIO APP Handle from fio_register() */
	unsigned int		timeout
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_HM_REG, timeout) );
}

/*****************************************************************************/
/*
This function is used to deregister from the health monitor service.
*/
/*****************************************************************************/

int
fio_hm_deregister
(
	FIO_APP_HANDLE		app_handle	/* FIO APP Handle from fio_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_HM_DEREG) );
}

/*****************************************************************************/
/*
This function is used to refresh/heartbeat the health monitor service.
*/
/*****************************************************************************/

int
fio_hm_heartbeat
(
	FIO_APP_HANDLE		app_handle	/* FIO APP Handle from fio_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_HM_HEARTBEAT) );
}

/*****************************************************************************/
/*
This function is used to reset a health monitor service timeout/fault.
*/
/*****************************************************************************/

int
fio_hm_fault_reset
(
	FIO_APP_HANDLE		app_handle	/* FIO APP Handle from fio_register() */
)
{
	return ( ioctl( (int)app_handle, FIOMAN_IOC_HM_RESET) );
}

/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
