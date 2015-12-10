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
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fpui.h>

volatile sig_atomic_t sig_flag = 0;
static char pid_file[100];
static pid_t child_pid = 0;

void clean_up()
{
	if(child_pid != 0)
	{
		kill(child_pid, SIGTERM);
		child_pid=0;
	}
	if(strlen(pid_file) > 0)
	{
		remove(pid_file);
	}
}

void signal_handler(int sig)
{
	clean_up();
	exit(EXIT_FAILURE);
}

void print_usage()
{
	printf("Usage: fpuiexec [OPTIONS] program\n");
	printf("  -n name                   application name for FrontPanelManager\n");
	printf("  -p file                   process id file\n");
	printf("  -d sec                    delay time in sec before starting program\n");
	printf("  -r                        restart program if it exits\n");
	printf("\n");
}

int main( int argc, char * argv[] )
{
	int opt;
	int fpi;
	int restart=0;
	char name[50];
	char pid_buffer[50];
	char delay_buffer[50];
	char* program = 0;
	FILE* pid_file_handle = 0;
	long int delay;
	char* end;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	name[0]=0;
	pid_file[0]=0;
	delay_buffer[0]=0;
	sprintf(pid_buffer, "%d\n", getpid());

	while ((opt = getopt(argc, argv, "rn:p:d:")) != -1) 
	{
		switch (opt) 
		{
			case 'r': restart=1; break;
			case 'n': strcpy(name, optarg); break;	
			case 'p': strcpy(pid_file, optarg); break;
			case 'd': strcpy(delay_buffer, optarg); break;		
			default:
				print_usage();
				exit(EXIT_FAILURE);
		}
 	}

	if(optind < argc) 
	{
		program=argv[optind];
	} 
	else 
	{
		print_usage();
		exit(EXIT_FAILURE);
	}

	if(strlen(pid_file) > 0)
	{
		pid_file_handle = fopen(pid_file, "w");
		if(pid_file_handle)
		{
			fwrite(pid_buffer, 1, strlen(pid_buffer), pid_file_handle);
			fclose(pid_file_handle);
		}
	}

	if(delay_buffer[0] != 0)
	{
		delay = strtol(delay_buffer,&end,10);
		sleep(delay);
	}
	
	fpi = fpui_open( O_RDWR, name);
	if( fpi <= 0 ) 
	{
		fprintf( stderr, "open /dev/fpi failed (%s)\n", strerror( errno ) );
		exit( 1 );
	}
	
	do
	{
		child_pid = fork();
		if (child_pid == 0) 
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
		else if(child_pid > 0)
		{
			int childExitStatus;
			pid_t ws = waitpid( child_pid, &childExitStatus, 0);
			child_pid=0;
		}
	} while(restart);
	
	fpui_close(fpi);
	clean_up();
	return 0;
}
	
