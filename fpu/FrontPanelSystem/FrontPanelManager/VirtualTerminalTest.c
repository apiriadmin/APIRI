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


extern int create_virtual_terminal( int );
extern void destroy_virtual_terminal( int );
extern void virtual_terminal( int, char * );

#define ESC ""

struct command_table_s {
        char * pattern;
	char * token;
} cmdt[] = {
        { "\t",          "HT" },
        { "\r",          "CR" },
        { "\n",          "LF" },
        { "\b",          "BS" },
        { ESC "[01;02f", "HVP" },
        { ESC "[03A",    "CUP" },
        { ESC "[04B",    "CUD" },
        { ESC "[05C",    "CUF" },
        { ESC "[06D",    "CUB" },
        { ESC "[H",      "CUH" },
        { ESC "c",       "SRS" },
        { ESC "P1[255;255;255;255;255;255;255;255f", "SPC" },
        { ESC "[<06V",   "SPD" },
        { ESC "[25h",    "CHR-BK ON" },
        { ESC "[25l",    "CHR-BK OFF" },
        { ESC "[<5h",    "SCR-BL ON" },
        { ESC "[<5l",    "SCR-BL OFF" },
        { ESC "[33h",    "CUR-BK ON" },
        { ESC "[33l",    "CUR-BK OFF" },
        { ESC "[27h",    "CHR-RV ON" },
        { ESC "[27l",    "CHR-RV OFF" },
        { ESC "[24h",    "CHR-UL ON" },
        { ESC "[24l",    "CHR-UL OFF" },
        { ESC "[0m",     "CHR-ALL OFF" },
	{ ESC "H",	 "SHT" },
        { ESC "[08g",    "CHT" },
        { ESC "[?7h",    "SCR-AW ON" },
        { ESC "[?7l",    "SCR-AW OFF" },
        { ESC "[?8h",    "SCR-AR ON" },
        { ESC "[?8l",    "SCR-AR OFF" },
        { ESC "[?25h",   "CUR-VS ON" },
        { ESC "[?25l",   "CUR-VS OFF" },
        { ESC "[<47h",   "SCR-AS ON" },
        { ESC "[<47l",   "SCR-AS OFF" },
        { ESC "[<09S",   "SBTO" },
        { ESC "[6n",     "ICP" },
        { ESC "[Bn",     "ATT" },
        { ESC "[An",     "AUX" },
};

#define CMDT_SIZE ( sizeof( cmdt ) / sizeof( cmdt[0] ) )


int main( int argc, char * argv[] )
{
	int i;
	FILE * log;

	if( (log = fopen("log.file", "w+") ) == NULL ) {
		log = stderr;
	}

	create_virtual_terminal( 0 );

	for( i = 0; i < CMDT_SIZE; i++ ) {
		fprintf( log, "\n%s: sending %s\n", argv[0], cmdt[i].token );
		virtual_terminal( 0, cmdt[i].pattern );
	}

	destroy_virtual_terminal( 0 );
	exit( 0 );
}

void routing_return( int target, char * s, char * t )
{
}


void viewport_copy_out( int term, char * s )
{
}
