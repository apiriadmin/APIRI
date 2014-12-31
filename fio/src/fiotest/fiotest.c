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

This module is the test driver for the FIO Driver.

*/
/*****************************************************************************/

/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */
#include	<stdio.h>		/* STDIO Definitions */
#include	<unistd.h>		/* UNIX standaes definitions */
#include	<errno.h>		/* ERRNO Definitions */
#include	<fcntl.h>		/* File operation definitions */
#include	<string.h>		/* String Definitions */
#include	<stdbool.h>

/* Local includes. */
#include	<fio.h>			/* FIO API Definitions */

/*  Definition section.
-----------------------------------------------------------------------------*/
/* For logging purposes. */


/*  Global section.
-----------------------------------------------------------------------------*/
#define	BIT_ARRAY_SIZE	(FIO_OUTPUT_POINTS_BYTES)	/* Array for bits */
#define	TEST_BIT_1		(11)	/* Bit used as a test bit */
#define	TEST_BIT_2		(23)	/* Bit used as a test bit */
#define	TEST_BIT_3		(1)		/* Not reserved */

#define	SLEEP_SECONDS	(10)		/* How long to run for */

/*  Private API declaration (prototype) section.
-----------------------------------------------------------------------------*/


/*  Public API interface section.
-----------------------------------------------------------------------------*/


/*  Private API implementation section.
-----------------------------------------------------------------------------*/

/* TEG - hexdump */

#define TEG_hexdump(str, ptr, len)   {printf("TEG_hexdump: %s(%d) %s\n", __FUNCTION__, __LINE__, str ); TEG_hex((char *)ptr, (size_t)len); }

static void TEG_hex(char *, size_t);

static void
TEG_hex
(
	char	*ptr,
	size_t	len
)
{
	size_t	ii;
	char	*end;
	char	*tmpptr;
	char	*tmpend;

	printf("HEXDUMP: %p, %d bytes\n", ptr, (int)len);
	printf("0x???????? ");
	for (ii = 0; ii < 16; ii++)
	{
		printf("%02x ", (unsigned int)ii);
	}
	printf("\n");
	printf("---------- ");
	for (ii = 0; ii < 16; ii++)
	{
		printf("-- ");
	}
	printf("\n");


	end = ptr + len;
	while (ptr < end)
	{
		printf("%p ", ptr);
		tmpptr = ptr;
		tmpend = ptr + 16;
		if (tmpend > end)
		{
			tmpend = end;
		}
		ii = 0;
		while (tmpptr < tmpend)
		{
			printf("%02x ", *(unsigned char *)tmpptr);
			tmpptr++;
		}
		for (; ii < 16; ii++)
		{
			printf("   ");
		}

		tmpptr = ptr;
		while (tmpptr < tmpend)
		{
			if (   (' ' > *(unsigned char *)tmpptr)
				|| (0x7f < *(unsigned char *)tmpptr))
			{
				printf(".");
			}
			else
			{
				printf("%c", *(unsigned char *)tmpptr);
			}
			tmpptr++;
		}
		printf("\n");
		ptr = tmpend;
	}
}

/* TEG - hexdump */

/*****************************************************************************/
/*
This function is where the real work of the test program is done
*/
/*****************************************************************************/

void
fio_test_run
(
	FIO_APP_HANDLE	fio_handle,		/* APP Handle */
	FIO_DEV_HANDLE	dev_handle		/* FIOD Handle */
)
{
	int				err;	/* Returned error code */
	unsigned char	plus[ FIO_OUTPUT_POINTS_BYTES ];
	unsigned char	minus[ FIO_OUTPUT_POINTS_BYTES ];
	unsigned char	raw[ FIO_INPUT_POINTS_BYTES ];
	FIO_FRAME_SCHD	frame_schedules[3];
	FIO_INPUT_FILTER input_filters[3];
	
	printf( "\nRunning ...\n" );
#if 0
	/* Get frame schedule for frames #52 (raw inputs), #53 (filtered inputs) and #55 (outputs) */
	frame_schedules[0].req_frame = 52;
	frame_schedules[1].req_frame = 53;
	frame_schedules[2].req_frame = 55;
	if (fio_fiod_frame_schedule_get(fio_handle, dev_handle, FIO_VIEW_APP, frame_schedules, 3) < 0) {
		printf ("fio_fiod_frame_schedule_get( FIO_VIEW_APP ) failed, errno(%s)\n", strerror(errno));
		return;
	}
	printf("fio_fiod_frame_schedule_get( FIO_VIEW_APP ): #52:%dHz, #53:%dHz, #55:%dHz\n",
		frame_schedules[0].frequency, frame_schedules[1].frequency, frame_schedules[2].frequency);
	
	/* Set new frame schedule for frames #52 (raw inputs), #53 (filtered inputs) and #55 (outputs) */
	frame_schedules[0].req_frame = 52; frame_schedules[0].frequency = FIO_HZ_100;
	frame_schedules[1].req_frame = 53; frame_schedules[1].frequency = FIO_HZ_100;
	frame_schedules[2].req_frame = 55; frame_schedules[2].frequency = FIO_HZ_100;
	if (fio_fiod_frame_schedule_set(fio_handle, dev_handle, frame_schedules, 3) < 0) {
		printf ("fio_fiod_frame_schedule_set() failed, errno(%s)\n", strerror(errno));
		return;
	}
	printf("fio_fiod_frame_schedule_set(): #52:100Hz, #53:100Hz, #55:100Hz\n");

	/* Get input filter values */
	input_filters[0].input_point = TEST_BIT_1;
	input_filters[1].input_point = TEST_BIT_2;
	input_filters[2].input_point = TEST_BIT_3;
	if (fio_fiod_inputs_filter_get(fio_handle, dev_handle, FIO_VIEW_APP, input_filters, 3) < 0) {
		printf("fio_fiod_inputs_filter_get( FIO_VIEW_APP ) failed, errno(%s)\n", strerror(errno));
		return;
	}
	printf("fio_fiod_inputs_filter_get( FIO_VIEW_APP ): #%d:lead=%d,trail=%d; #%d:lead=%d,trail=%d; #%d:lead=%d,trail=%d",
		input_filters[0].input_point, input_filters[0].leading, input_filters[0].trailing,
		input_filters[1].input_point, input_filters[1].leading, input_filters[1].trailing,
		input_filters[2].input_point, input_filters[2].leading, input_filters[2].trailing);		
	
	/* Set new input filter values */
	input_filters[0].input_point = TEST_BIT_1; input_filters[0].leading = 3; input_filters[0].trailing = 3;
	input_filters[1].input_point = TEST_BIT_2; input_filters[1].leading = 3; input_filters[1].trailing = 3;
	input_filters[2].input_point = TEST_BIT_3; input_filters[2].leading = 3; input_filters[2].trailing = 3;
	if (fio_fiod_inputs_filter_set(fio_handle, dev_handle, input_filters, 3) < 0) {
		printf("fio_fiod_inputs_filter_set() failed, errno(%s)\n", strerror(errno));
		return;
	}
	printf("fio_fiod_inputs_filter_set(): #%d:lead=%d,trail=%d; #%d:lead=%d,trail=%d; #%d:lead=%d,trail=%d",
		input_filters[0].input_point, input_filters[0].leading, input_filters[0].trailing,
		input_filters[1].input_point, input_filters[1].leading, input_filters[1].trailing,
		input_filters[2].input_point, input_filters[2].leading, input_filters[2].trailing);
		
#endif
		
	memset( raw, 0xff, sizeof( raw ) );
	if ( 0 > ( err = fio_fiod_inputs_get( fio_handle, dev_handle, FIO_INPUTS_RAW, raw, sizeof(raw) ) ) ) {
		printf( "ERR(%d), errno(%s): fio_fiod_inputs_get( FIO_INPUTS_RAW ) failed\n",
				err, strerror( errno ) );
		return;
	}
	printf( "fio_fiod_inputs_get( FIO_INPUTS_RAW ) successful\n" );

	if ( !FIO_BIT_TEST( raw, 100 )
		&& !FIO_BIT_TEST( raw, TEST_BIT_1 )
		&& !FIO_BIT_TEST( raw, TEST_BIT_2 ) ) {
		printf( "TEST_BITS set appropriately\n" );
	} else {
		printf( "ERR: TEST_BITS NOT set correctly\n" );
		TEG_hexdump( "INPUTS: raw", raw, sizeof( raw ) );
	}

	printf( "calling fio_fiod_enable( fio_handle(%d), dev_handle(%d) )\n", fio_handle, dev_handle );
	if ( 0 > ( err = fio_fiod_enable( fio_handle, dev_handle ) ) ) {
		printf ("Failed to fio_fiod_enable( fio_handle(%d), dev_handle(%d) ), errno(%s)\n",
			fio_handle, dev_handle, strerror( errno ) );
		return;
	}
	printf( "err(%s) = fio_fiod_enable() successful\n", strerror( errno ) );

	memset( plus, 0, sizeof( plus ) );
	memset( minus, 0, sizeof( minus ) );
	FIO_BIT_SET( plus, TEST_BIT_1 );
	FIO_BIT_SET( plus, TEST_BIT_3 );
	FIO_BIT_SET( minus, TEST_BIT_2 );
	FIO_BIT_SET( minus, TEST_BIT_3 );
	if ( 0 > ( err = fio_fiod_outputs_set(fio_handle, dev_handle, plus, minus, sizeof(plus)))) {
		printf( "ERR(%d), errno(%s): fio_fiod_outputs_set() failed\n", 
				err, strerror(errno));
		return;
	}
	printf( "fio_fiod_outputs_set() successful\n" );

	if ( 0 > ( err = fio_fiod_outputs_get( fio_handle, dev_handle, FIO_VIEW_APP, plus, minus, sizeof(plus) ) ) ) {
		printf( "ERR(%d), errno(%s): fio_fiod_outputs_get( FIO_VIEW_APP ) failed\n",
				err, strerror( errno ) );
		return;
	}
	printf( "fio_fiod_outputs_get( FIO_VIEW_APP ) successful\n" );
	
	if ( FIO_BIT_TEST( plus, TEST_BIT_1 )
		&& FIO_BIT_TEST( minus, TEST_BIT_2 )
		&& !FIO_BIT_TEST( plus, TEST_BIT_3 )
		&& !FIO_BIT_TEST( minus, TEST_BIT_3 ) ) {
		printf( "TEST_BITS set appropriately\n" );
	} else {
		printf( "ERR: TEST_BITS NOT set correctly\n" );
		TEG_hexdump( "APP: outputs plus", plus, sizeof( plus ) );
		TEG_hexdump( "APP: outputs minus", minus, sizeof( minus ) );
	}

	if ( 0 > ( err = fio_fiod_outputs_get( fio_handle, dev_handle, FIO_VIEW_SYSTEM, plus, minus, sizeof(plus) ) ) ) {
		printf( "ERR(%d), errno(%s): fio_fiod_outputs_get( FIO_VIEW_SYSTEM ) failed\n",
				err, strerror( errno ) );
		return;
	}
	printf( "fio_fiod_outputs_get( FIO_VIEW_SYSTEM ) successful\n" );
	
	if ( FIO_BIT_TEST( plus, TEST_BIT_1 )
		&& FIO_BIT_TEST( minus, TEST_BIT_2 )
		&& !FIO_BIT_TEST( plus, TEST_BIT_3 )
		&& !FIO_BIT_TEST( minus, TEST_BIT_3 ) ) {
		printf( "TEST_BITS set appropriately\n" );
	} else {
		printf( "ERR: TEST_BITS NOT set correctly\n" );
		TEG_hexdump( "SYSTEM: outputs plus", plus, sizeof( plus ) );
		TEG_hexdump( "SYSTEM: outputs minus", minus, sizeof( minus ) );
	}

	/* Let the system run */
	printf( "\nSleeping ...\n" );
	sleep( SLEEP_SECONDS );
	printf( "\n\n" );

	FIO_BIT_CLEAR( plus, TEST_BIT_1 );
	FIO_BIT_CLEAR( minus, TEST_BIT_2 );
	if ( 0 > ( err = fio_fiod_outputs_set(fio_handle, dev_handle, plus, minus, sizeof(plus)))) {
		printf( "ERR(%d), errno(%s): fio_fiod_outputs_set() failed\n", 
				err, strerror(errno));
		return;
	}

	if ( 0 > ( err = fio_fiod_outputs_get( fio_handle, dev_handle, FIO_VIEW_APP, plus, minus, sizeof(plus) ) ) ) {
		printf( "ERR(%d), errno(%s): fio_fiod_outputs_get( FIO_VIEW_APP ) failed\n",
				err, strerror( errno ) );
		return;
	}
	
	if ( !FIO_BIT_TEST( plus, TEST_BIT_1 )
		&& !FIO_BIT_TEST( minus, TEST_BIT_2 )
		&& !FIO_BIT_TEST( plus, TEST_BIT_3 )
		&& !FIO_BIT_TEST( minus, TEST_BIT_3 ) ) {
		printf( "TEST_BITS set appropriately\n" );
	} else {
		printf( "ERR: TEST_BITS NOT set correctly\n" );
		TEG_hexdump( "APP: outputs plus", plus, sizeof( plus ) );
		TEG_hexdump( "APP: outputs minus", minus, sizeof( minus ) );
	}

	if ( 0 > ( err = fio_fiod_outputs_get( fio_handle, dev_handle, FIO_VIEW_SYSTEM, plus, minus, sizeof(plus) ) ) ) {
		printf( "ERR(%d), errno(%s): fio_fiod_outputs_get( FIO_VIEW_SYSTEM ) failed\n",
				err, strerror( errno ) );
		return;
	}
	
	if ( !FIO_BIT_TEST( plus, TEST_BIT_1 )
		&& !FIO_BIT_TEST( minus, TEST_BIT_2 )
		&& !FIO_BIT_TEST( plus, TEST_BIT_3 )
		&& !FIO_BIT_TEST( minus, TEST_BIT_3 ) ) {
		printf( "TEST_BITS set appropriately\n" );
	} else {
		printf( "ERR: TEST_BITS NOT set correctly\n" );
		TEG_hexdump( "SYSTEM: outputs plus", plus, sizeof( plus ) );
		TEG_hexdump( "SYSTEM: outputs minus", minus, sizeof( minus ) );
	}

	/* Read RAW inputs */
	memset( raw, 0, sizeof( raw ) );
	if ( 0 > ( err = fio_fiod_inputs_get( fio_handle, dev_handle, FIO_INPUTS_RAW, raw, sizeof(raw) ) ) ) {
		printf( "ERR(%d), errno(%s): fio_fiod_inputs_get( FIO_INPUTS_RAW ) failed\n",
				err, strerror( errno ) );
		return;
	}
	printf( "fio_fiod_inputs_get( FIO_INPUTS_RAW ) successful\n" );
	
	if ( FIO_BIT_TEST( raw, 100 )
		&& !FIO_BIT_TEST( raw, TEST_BIT_1 )
		&& !FIO_BIT_TEST( raw, TEST_BIT_2 ) ) {
		printf( "TEST_BITS set appropriately\n" );
	} else {
		printf( "ERR: TEST_BITS NOT set correctly\n" );
		TEG_hexdump( "INPUTS: raw", raw, sizeof( raw ) );
	}

	printf( "\nEnding ...\n" );

	if ( 0 > ( err = fio_fiod_disable( fio_handle, dev_handle ) ) ) {
		printf ("Failed to fio_fiod_disable( fio_handle(%d), dev_handle(%d) ), errno(%s)\n",
			fio_handle, dev_handle, strerror( errno ) );
		return;
	}

	printf( "fio_fiod_disable(dev_handle(%d)) successful\n", dev_handle );
}

/*****************************************************************************/

/*  Public API implementation section.
-----------------------------------------------------------------------------*/

/*****************************************************************************/
/*
This is the main function for the FIO API Test Program.
*/
/*****************************************************************************/

int main (int argc, char **argv)
{
	FIO_APP_HANDLE		fio_handle;
	FIO_DEV_HANDLE		dev_handle;
	int			err;
	unsigned char		set_bits[ BIT_ARRAY_SIZE ];
	unsigned char		get_bits[ BIT_ARRAY_SIZE ];
	unsigned char channel_reservations[FIO_CHANNEL_BYTES] = {0};

	printf( "Running test\n" );

	/* Register with fio API */
	if ( 0 > ( fio_handle = fio_register() ) ) {
		printf( "Failed to fio_register %s, err(%d), errno(%s)\n",
				FIO_DEV, fio_handle, strerror( errno ) );
		return ( -1 );
	}
	printf( "fio_register() successful\n" );

	/* Register fio device "FIO332" */
	if ( 0 > ( dev_handle = fio_fiod_register( fio_handle, FIO_PORT_SP5, FIO332 ) ) ) {
		printf("Failed to fio_fiod_register(FIO_PORT_SP3, FIOTF1),err(%d),errno(%s)\n",
			dev_handle, strerror( errno ) );
		return( -1 );
	}
	printf( "dev_handle(%d) = fio_fiod_register() successful\n", dev_handle );

	/* Reserve test set of outputs */
	FIO_BITS_CLEAR( set_bits, sizeof( set_bits ) );
	FIO_BIT_SET( set_bits, TEST_BIT_1 );
	if ( 0 > ( err = fio_fiod_outputs_reservation_set( fio_handle, dev_handle, set_bits, sizeof(set_bits) ) ) ) {
		printf ("Failed to fio_fiod_outputs_reservation_set( fio_handle(%d), dev_handle(%d) ), "
			"errno(%s)\n", fio_handle, dev_handle, strerror( errno ) );
		return( -1 );
	}
	printf( "errno(%s) = fio_fiod_outputs_reservation_set() successful\n",
			strerror( errno ) );

	FIO_BIT_SET( set_bits, TEST_BIT_2 );
	if ( 0 > ( err = fio_fiod_outputs_reservation_set( fio_handle, dev_handle, set_bits, sizeof(set_bits) ) ) ) {
		printf ("Failed to fio_fiod_outputs_reservation_set( fio_handle(%d), dev_handle(%d) ), "
			"errno(%s)\n", fio_handle, dev_handle, strerror( errno ) );
		return( -1 );
	}
	printf( "errno(%s) = fio_fiod_outputs_reservation_set() successful\n",
			strerror( errno ) );

	/* Read this app's view of output reservations */
	FIO_BITS_CLEAR( get_bits, sizeof( get_bits ) );
	if ( 0 > ( err = fio_fiod_outputs_reservation_get( fio_handle, dev_handle, FIO_VIEW_APP, get_bits, sizeof(get_bits) ) ) )	{
		printf ("Failed to fio_fiod_outputs_reservation_get( FIO_VIEW_APP ), "
			"errno(%s)\n", strerror( errno ) );
		return( -1 );
	}
	printf( "err(%s) = fio_fiod_outputs_reservation_get( FIO_VIEW_APP ) successful\n",
			strerror( errno ) );

	/* Confirm test output reservations  (app view) */
	/* See if TEST_BIT_1 & TEST_BIT_2 was set in APP reservations */
	if ( FIO_BIT_TEST( get_bits, TEST_BIT_1 )
		&& FIO_BIT_TEST( get_bits, TEST_BIT_2 ) ) {
		printf( "TEST_BITS(%d, %d) set in APP reservations\n",
				TEST_BIT_1, TEST_BIT_2 );
	} else {
		printf( "ERROR: TEST_BITS(%d, %d) NOT set in APP reservations\n",
				TEST_BIT_1, TEST_BIT_2 );
		TEG_hexdump( "APP array", get_bits, sizeof( get_bits ) );
	}

	/* Read all system output reservations */
	FIO_BITS_CLEAR( get_bits, sizeof( get_bits ) );
	if ( 0 > ( err = fio_fiod_outputs_reservation_get( fio_handle, dev_handle, FIO_VIEW_SYSTEM, get_bits, sizeof(get_bits) ) ) ) {
		printf ("Failed to fio_fiod_outputs_reservation_get( FIO_VIEW_SYSTEM ), "
			"errno(%s)\n", strerror( errno ) );
		return( -1 );
	}
	printf( "err(%s) = fio_fiod_outputs_reservation_get( FIO_VIEW_SYSTEM ) successful\n",
			strerror( errno ) );

	/* Confirm test output reservations (system view) */
	/* See if TEST_BIT_1 & TEST_BIT_2 was set in APP reservations */
	if (FIO_BIT_TEST( get_bits, TEST_BIT_1 )
		&& FIO_BIT_TEST( get_bits, TEST_BIT_2 ) ) {
		printf( "TEST_BITS(%d, %d) set in SYSTEM reservations\n",
				TEST_BIT_1, TEST_BIT_2 );
	} else {
		printf( "ERROR: TEST_BITS(%d, %d) NOT set in SYSTEM reservations\n",
				TEST_BIT_1, TEST_BIT_2 );
		TEG_hexdump( "SYSTEM array", get_bits, sizeof( get_bits ) );
	}


	/* Reserve test set of channels */
	FIO_BIT_SET( channel_reservations, 0 );
	FIO_BIT_SET( channel_reservations, 4 );
	FIO_BIT_SET( channel_reservations, 8 );
	FIO_BIT_SET( channel_reservations, 12 );
	if ( 0 > ( err = fio_fiod_channel_reservation_set( fio_handle, dev_handle, channel_reservations, sizeof(channel_reservations) ) ) ) {
		printf ("Failed fio_fiod_channel_reservation_set( fio_handle(%d), dev_handle(%d) ), "
			"errno(%s)\n", fio_handle, dev_handle, strerror( errno ) );
		return( -1 );
	}
	printf( "errno(%s) = fio_fiod_channel_reservation_set() successful %0x,%0x,%0x,%0x\n", strerror( errno ),
		channel_reservations[0],channel_reservations[1],channel_reservations[2],channel_reservations[3] );

	/* Let the system run */
	fio_test_run( fio_handle, dev_handle );
	
	/* See if TEST_BITS still set */
	/* First clear TEST_BIT_1, TEST_BIT_2 should still be set */
	FIO_BIT_CLEAR( set_bits, TEST_BIT_1 );
	if ( 0 > ( err = fio_fiod_outputs_reservation_set( fio_handle, dev_handle, set_bits, sizeof(set_bits) ) ) ) {
		printf ("Failed to fio_fiod_outputs_reservation_set( fio_handle(%d), dev_handle(%d) ), "
			"errno(%s)\n", fio_handle, dev_handle, strerror( errno ) );
		return( -1 );
	}
	printf( "err(%s) = fio_fiod_outputs_reservation_set() successful\n",
			strerror( errno ) );

	FIO_BITS_CLEAR( get_bits, sizeof( get_bits ) );
	if ( 0 > ( err = fio_fiod_outputs_reservation_get( fio_handle, dev_handle, FIO_VIEW_APP, get_bits, sizeof(get_bits) ) ) ) {
		printf ("Failed to fio_fiod_outputs_reservation_get( FIO_VIEW_APP ), "
			"errno(%s)\n", strerror( errno ) );
		return( -1 );
	}
	printf( "err(%s) = fio_fiod_outputs_reservation_get( FIO_VIEW_APP ) successful\n",
			strerror( errno ) );

	/* See if TEST_BIT_1 & TEST_BIT_2 was set in APP reservations */
	if ( !FIO_BIT_TEST( get_bits, TEST_BIT_1 )
		&&  FIO_BIT_TEST( get_bits, TEST_BIT_2 ) ) {
		printf( "TEST_BITS(%d, %d) set in APP reservations correctly\n",
				TEST_BIT_1, TEST_BIT_2 );
	} else {
		printf( "ERROR: TEST_BITS(%d, %d) NOT set in APP reservations correctly\n",
				TEST_BIT_1, TEST_BIT_2 );
		TEG_hexdump( "APP array", get_bits, sizeof( get_bits ) );
	}

	FIO_BITS_CLEAR( get_bits, sizeof( get_bits ) );
	if ( 0 > ( err = fio_fiod_outputs_reservation_get( fio_handle, dev_handle, FIO_VIEW_SYSTEM, get_bits, sizeof(get_bits) ) ) ) {
		printf ("Failed to fio_fiod_outputs_reservation_get( FIO_VIEW_SYSTEM ), "
			"errno(%s)\n", strerror( errno ) );
		return( -1 );
	}
	printf( "err(%s) = fio_fiod_outputs_reservation_get( FIO_VIEW_SYSTEM ) successful\n",
			strerror( errno ) );

	/* See if TEST_BIT_1 & TEST_BIT_2 was set in APP reservations */
	if ( !FIO_BIT_TEST( get_bits, TEST_BIT_1 )
		&&  FIO_BIT_TEST( get_bits, TEST_BIT_2 ) ) {
		printf( "TEST_BITS(%d, %d) set in SYSTEM reservations correctly\n",
				TEST_BIT_1, TEST_BIT_2 );
	} else {
		printf( "ERROR: TEST_BITS(%d, %d) NOT set in SYSTEM reservations correctly\n",
				TEST_BIT_1, TEST_BIT_2 );
		TEG_hexdump( "SYSTEM array", get_bits, sizeof( get_bits ) );
	}

	/* de-register fio device "FIO332" */
	if ( 0 > ( err = fio_fiod_deregister( fio_handle, dev_handle ) ) ) {
		printf ("Failed to fio_fiod_deregister( FIO_PORT_SP5, FIO332 ), errno(%s)\n",
			strerror( errno ) );
		return( -1 );
	}
	printf( "fio_fiod_deregister(dev_handle(%d)) successful\n", dev_handle );

	/* de-register fio API */
	if ( 0 > ( err = fio_deregister( fio_handle ) ) ) {
		printf( "Failed to deregister %s, errno(%s)\n",
				FIO_DEV, strerror( errno ) );
		return ( -1 );
	}

	printf( "fio_deregister() successful\n" );

	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
