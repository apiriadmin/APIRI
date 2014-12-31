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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <front_panel.h>

#define BLOCKING

int main( int argc, char * argv[] )
{
	int fd;
	char buf[4];

#ifdef BLOCKING
	fprintf( stderr, "%s: opening device (blocking)\n", argv[0] );
	if( (fd = open( "/dev/aux", O_RDONLY )) < 0 ) {
	        perror( argv[0] );
		exit( 99 );
	}

	for( ;; ) {
		read( fd, buf, sizeof( buf ) );
		printf("AUX Switch: state is %s\n", (buf[0])?"ON":"OFF" );
	}
	close( fd );
#endif

#ifdef NONBLOCKING
	fprintf( stderr, "%s: opening device (nonblocking)\n", argv[0] );
	if( (fd = open( "/dev/aux", O_RDONLY | O_NONBLOCK )) < 0 ) {
	        perror( argv[0] );
		exit( 99 );
	}

	for( ;; ) {
		read( fd, buf, sizeof( buf ) );
		printf("AUX Switch: state is %s\n", (buf[0])?"ON":"OFF" );
		sleep( 3 );
	}
	close( fd );
#endif

	printf("%s: Exiting\n", argv[0] );

	exit( 0 );
}
