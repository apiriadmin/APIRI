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
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include <front_panel.h>

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define RELEASE 12

extern void routing( int );
extern void *viewport_listener( void * );
extern void dump_virtual_terminals( void );

int fd;
pthread_cond_t window_changed = PTHREAD_COND_INITIALIZER;

void alldone( int arg )
{
	fprintf( stderr, "Exiting on HUP\n");
	close( fd );
	exit( 0 );
}

void dumpvt( int arg )
{
	dump_virtual_terminals();
}

struct option options[] = { {"file",    required_argument, 0, 'f' },
			    {"help",    no_argument,       0, 'h' },
			    {"version", no_argument,       0, 'v' },
			    { 0, 0, 0, 0 } };

void version( char * argv )
{
	fprintf( stderr, "%s: Version %2.2d:%2.2d:%2.2d\n", argv, MAJOR_VERSION, MINOR_VERSION, RELEASE );
}


void usage( char * argv )
{
	fprintf( stderr, "Usage: %s [-v] [--version] [-h] [--help] [-f <device path>] [--file <device path>] [<device path>]\n", argv );
	fprintf( stderr, "Where:	-v or --version		display the version number of this module\n" );
	fprintf( stderr, "		-h or --help		display this help text\n" );
	fprintf( stderr, "		-f <device path>	device path to communicate over\n" );
	fprintf( stderr, "		--file <device path>	device path to communicate over\n" );
	fprintf( stderr, "		<device path>		device path to communicate over\n" );
	fprintf( stderr, "		only one of -f, --file or <device path> need be specified\n");
}


int main( int argc, char * argv[] )
{
	int i;
	int option_index = 0;
	char * filepath  = NULL;
	pthread_t viewport;

	signal( SIGHUP, alldone );
	signal( SIGUSR1, dumpvt );

	while( (i = getopt_long( argc, argv, "f:hv", options, &option_index)) >= 0 ) {
		switch( i ) {
			case 'f':
				if( optarg != NULL )
					filepath  = optarg;
				break;
			case 'h':
			case '?':
			case ':':
				usage( argv[0] );
				break;
			case 'v':
				version( argv[0] );
				break;
			default:
				break;
		}

	}

	if (optind < argc) {
		filepath  = argv[optind++];
	}

	if( (filepath == NULL) || (filepath[0] != '/') ) {
		fprintf( stderr, "%s: No device path specified\n", argv[0] );
		usage( argv[0] );
		exit( 98 );
	}

	if( (fd = open( "/dev/fpm", O_RDWR | O_EXCL )) < 0 ) {
	        perror( argv[0] );
		exit( 97 );
	}

	fprintf( stderr, "%s Ready serial port %s\n", argv[0], filepath );

	//daemon( 1, 1 );

	pthread_create( &viewport, NULL, viewport_listener, filepath );

	sleep( 0 );
	fprintf( stderr, "%s: calling routing()\n", argv[0] );
	routing( fd );

	pthread_join( viewport, NULL );
	fprintf( stderr, "%s: Exiting\n", argv[0] );
	exit( 0 );
}
