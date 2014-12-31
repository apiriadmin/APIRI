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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <fpui.h>

int main( int argc, char * argv[] )
{
	int id = 0;
	int i;
	int fd;
	FILE * fp;
	char buf[64];

	if( argc > 1 ) {
		sscanf( argv[1], "%d", &id );
	} 

	fprintf( stderr, "%s: opening device\n", argv[0] );
	sprintf( buf, "Sample %d Application", id );
	if( (fd = fpui_open(O_RDWR|O_SYNC, buf)) < 0 ) {
	        fprintf( stderr, "%s: Fopen failed (%s)\n", argv[0], strerror( errno ) );
		exit( 99 );
	}
fprintf(stderr, "%s: fd=%d\n", argv[0], fd);
	//fprintf( fp, "\f" );
	fpui_write(fd, "\f", 1);
	//fprintf( fp, "This is Sample Application %d\n\r", id );
	sprintf( buf, "This is Sample Application %d\n\r", id );
	fpui_write_string(fd, buf);
	for( i = 0; i < id; i++ ) {
		fpui_write_char( fd, '\n' );
	}
	sprintf( buf, "     here is another line\n\r");
	fpui_write_string(fd, buf);
	for( i = 0; i < 5 - id; i++ ) {
		fpui_write_char( fd, '\n' );
	}
	sprintf( buf, "          and this is the final line\n\r");
	fpui_write_string(fd, buf);

	for( ;; ) {
		char ch;
		//fscanf( fp, "%c", &ch );
		ch = fpui_read_char(fd);
		printf("Sample Application %d: %c (0x%2.2x)\n", id, ch, ch );
		sleep( 1 );
	}

	close( fd );
	printf("%s: Exiting\n", argv[0] );

	exit( 0 );
}
