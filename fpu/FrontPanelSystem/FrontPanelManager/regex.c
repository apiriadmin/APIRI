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

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <regex.h>


#define ESC ""

 int main( int argc, char * argv[] )
 {
	regex_t preg;
	char errbuf[128];
	int errcode;
	regmatch_t pmatch[3];

	char * s1 = ESC "[12;34f";
	char * s2 = ESC "[1;3f";
	char * s3 = ESC "12;34f";
	char * s4 = ESC "[;34f";
	char * s5 = "[12;34f";
 	char * p =  ESC "[[][0-9]+;[0-9]+f";

	if( (errcode = regcomp( &preg, p, REG_EXTENDED | REG_NOSUB)) != 0 ) {
		regerror( errcode, &preg, errbuf, sizeof( errbuf ));
		fprintf( stderr, "%s: regcomp failed ( %s)\n", argv[0], errbuf );
		exit( 1 );
	}

	printf( "regexec of s1 = %d\n", regexec( &preg, s1, 1, pmatch, 0 ) );
	printf( "regexec of s2 = %d\n", regexec( &preg, s2, 1, pmatch, 0 ) );
	printf( "regexec of s3 = %d\n", regexec( &preg, s3, 1, pmatch, 0 ) );
	printf( "regexec of s4 = %d\n", regexec( &preg, s4, 1, pmatch, 0 ) );
	printf( "regexec of s5 = %d\n", regexec( &preg, s5, 1, pmatch, 0 ) );
 }
