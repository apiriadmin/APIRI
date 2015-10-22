/*
 * atcapi-test.c
 * 
 * Copyright 2015 Intelight Inc. <mgall@arch1>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fio.h>			/* FIO API Definitions */
#include <fpui.h>
#include <tod.h>

int main(int argc, char **argv)
{
        unsigned char output_map[FIO_OUTPUT_POINTS_BYTES];
        unsigned char outputs_plus[FIO_OUTPUT_POINTS_BYTES];
        unsigned char outputs_minus[FIO_OUTPUT_POINTS_BYTES] = {0};
        unsigned char inputs[FIO_INPUT_POINTS_BYTES] = {0};
        FIO_APP_HANDLE fio_handle;
        FIO_DEV_HANDLE dev_handle;
        fpui_handle fpi;
        int i, err = 0;
        int appno = 0;
        char appname[32];
        bool toggle = false;
        struct timeval tval = {0,0};
        int tzone_off = 0, dst_off = 0;
        char tmpstr[40];

	// register with FIOAPI
	if ((fio_handle = fio_register()) < 0) {
		printf( "Failed to fio_register, err(%d): %s\n",
				fio_handle, strerror( errno ) );
		return ( -1 );
	}
	printf( "fio_register() successful\n" );

	/* Register fio device "FIO332" */
	if ((dev_handle = fio_fiod_register(fio_handle, FIO_PORT_SP5, FIO332)) < 0) {
		printf("Failed to fio_fiod_register(FIO_PORT_SP3, FIOTF1),err(%d),errno(%s)\n",
			dev_handle, strerror( errno ) );
		return( -1 );
	}
	printf( "dev_handle(%d) = fio_fiod_register() successful\n", dev_handle );

        // reserve next available output point
	/* get all system output reservations */
	FIO_BITS_CLEAR(output_map, sizeof(output_map));
	if ( (err = fio_fiod_outputs_reservation_get(fio_handle, dev_handle, FIO_VIEW_SYSTEM, output_map, sizeof(output_map))) < 0) {
		printf ("Failed to fio_fiod_outputs_reservation_get( FIO_VIEW_SYSTEM ), "
			"errno(%s)\n", strerror( errno ) );
		return( -1 );
	}

        for(i=0; i<(FIO_OUTPUT_POINTS_BYTES*8); i++) {
                if (!FIO_BIT_TEST(output_map, i)) {
                        FIO_BITS_CLEAR(output_map, sizeof(output_map));
                        FIO_BIT_SET(output_map, i);
                        if ((err = fio_fiod_outputs_reservation_set(fio_handle, dev_handle, output_map, sizeof(output_map))) < 0) {
                                printf ("Failed to fio_fiod_outputs_reservation_set( fio_handle(%d), dev_handle(%d) ), "
                                        "errno(%s)\n", fio_handle, dev_handle, strerror( errno ) );
                                return( -1 );
                        }
                        printf( "fio_fiod_outputs_reservation_set(%d) successful\n", i);
                        appno = i;
                        break;
                }
        }

        // register with FPUAPI using app name based on output point
        sprintf(appname,"atcapi_test%d", appno);
	if ((fpi = fpui_open(O_RDWR, appname)) <= 0) {
		printf( "open /dev/fpi failed (%s)\n", strerror(errno) );
		exit( 1 );
	}

       	fpui_clear(fpi);
        fpui_write_string_at(fpi, "ATCAPI Test", 1, 14);
        
        snprintf(tmpstr, 40, "Output %d State: ", appno);
        fpui_write_string_at(fpi, tmpstr, 5, 8);
        snprintf(tmpstr, 40, "Input %d State: ", appno);
        fpui_write_string_at(fpi, tmpstr, 6, 8);
        for(;;) {
                // display the current date/time/dst from TODAPI
                if ( (err = tod_get(&tval, &tzone_off, &dst_off)) != 0) {
                        printf( "tod_get failed (%s)\n", strerror(errno) );
                }
                ctime_r(&tval.tv_sec, tmpstr);
                fpui_write_string_at(fpi, tmpstr, 2, 8);
                sprintf(tmpstr, "TZ:GMT%+d DST:%+d", tzone_off/3600, dst_off/3600);
                fpui_write_string_at(fpi, tmpstr, 3, 12);
                
                // display the current state of the output and corresponding input
                FIO_BITS_CLEAR(outputs_plus, sizeof(outputs_plus));
                if (toggle)
                        FIO_BIT_SET(outputs_plus, appno);
                fio_fiod_outputs_set(fio_handle, dev_handle, outputs_plus, outputs_minus, sizeof(outputs_plus));
                fpui_write_char_at(fpi, toggle?'1':'0', 5, 26);
                toggle = !toggle;
                fio_fiod_inputs_get(fio_handle, dev_handle, FIO_INPUTS_FILTERED, inputs, sizeof(inputs));
                fpui_write_char_at(fpi, FIO_BIT_TEST(inputs,appno)?'1':'0', 6, 26);
                
                usleep(500000);
        }
        

	return 0;
}

