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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <fpui.h>

int main( int argc, char * argv[] )
{
	const char* regname = 0;
	const char* program = 0;

	if( argc > 2 ) 
	{	
		regname = argv[1];
		program = argv[2];
	}
	else
	{
		printf("must supply regname and program arguments\n");
		exit(1);
	}
	
	int fpi;
	fpi = fpui_open( O_RDWR, regname);
	if( fpi <= 0 ) 
	{
		fprintf( stderr, "open /dev/fpi failed (%s)\n", strerror( errno ) );
		exit( 1 );
	}
	printf("registered child as %s\n", regname);
	
	pid_t child;
	child = fork();
	if (child == 0) 
	{
		// Child process
		// Redirect standard input
		close(STDIN_FILENO);
		dup2(fpi, STDIN_FILENO);

		// Redirect standard output
		close(STDOUT_FILENO);
		dup2(fpi, STDOUT_FILENO);

		// Redirect standard error
		close(STDERR_FILENO);
		dup2(fpi, STDERR_FILENO);

		execvp(program, &argv[2]);
	}
	else if(child > 0)
	{
		int childExitStatus;
		pid_t ws = waitpid( child, &childExitStatus, 0);
	}
	
	fpui_close(fpi);
	return 0;
}
	
