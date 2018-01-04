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
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <fpui.h>
#include <signal.h>

static pid_t child_pid = 0;
static int restart = 0;

void exit_handler(int sig) {
	restart = 0;
	if(child_pid != 0) {
		kill(child_pid, SIGTERM);
		child_pid=0;
	}
}

void print_usage() {
	printf("Usage: fpuiexec [OPTIONS] program\n");
	printf("  -n name             application name for FrontPanelManager\n");
	printf("  -p file             process id file\n");
	printf("  -r                  restart program if it exits\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	int i;
	int slave = 0;
	int program_start_arg = 0;
	char* name = "app";
	char* pid_file = 0;

	signal(SIGINT, exit_handler);
	signal(SIGTERM, exit_handler);

	for(i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-n")) {
        	name = argv[++i];
			program_start_arg = i+1;
        }
		else if (!strcmp(argv[i], "-p")) {
        	pid_file = argv[++i];
			program_start_arg = i+1;
        }
		else if (!strcmp(argv[i], "-r")) {
        	restart = 1;
			program_start_arg = i+1;
        }
		else if (!strcmp(argv[i], "-h")) {
			print_usage();
			exit(0);
        }
	}

	if(program_start_arg == 0) {
		fprintf(stderr, "child process not specified\n");
		print_usage();
		exit(-1);
	} 

	slave = fpui_open( O_RDWR, name);
	if( slave <= 0 ) {
		perror("open /dev/fpi failed");
		exit(-1);
	}

	if(pid_file != 0) {
		FILE* pid_file_handle = fopen(pid_file, "w");
		if(pid_file_handle) {
			fprintf(pid_file_handle, "%d\n", getpid());
			fclose(pid_file_handle);
		}
	}

	do {

	struct winsize winp = {16, 40, 240, 128};
	int master;
	child_pid = forkpty(&master, NULL, NULL, &winp);    // opentty + login_tty + fork

	if (child_pid < 0) {
		return 1; // fork with pseudo terminal failed
	}

	else if (child_pid == 0) {   // child
		execvp(argv[program_start_arg], &argv[program_start_arg]);
	}

	else {   // parent
		char buf[1024];
		int escapeCounter = 0;
		struct termios tios;
		tcgetattr(master, &tios);
		tios.c_lflag &= ~(ECHO | ECHONL);
		tcsetattr(master, TCSAFLUSH, &tios);

		for(;;) {
			fd_set read_fd, write_fd, err_fd;

			FD_ZERO(&read_fd);
			FD_ZERO(&write_fd);
			FD_ZERO(&err_fd);
			FD_SET(master, &read_fd);
			FD_SET(slave, &read_fd);

			select(master+1, &read_fd, &write_fd, &err_fd, NULL);

			if (FD_ISSET(master, &read_fd))
			{
				char ch;
				int c = read(master, buf, 1024); // read from program
				if (c > 0)
					write(slave, buf, c);    // write to fpi
				else if (c == -1)
					break; // exit when end of communication channel with program
			}

			if (FD_ISSET(slave, &read_fd))
			{
				char ch;
				int c=read(slave, &ch, 1);   // read from fpi
				if(ch == '\x1b') {
					escapeCounter = 3;
				} else if (escapeCounter == 1 && ch == 'S') {
					// convert fpui esc key "ESC O S" to standard linux tty
					ch = '\x1b';
					write(master, &ch, 1);
				} else if (escapeCounter == 0) {
					if(ch >= '\x80' && ch <= '\x83') {
						// convert fpui arrow keys into standard linux tty
						char arrow[3];
						arrow[0] = '\x1b';
						arrow[1] = '[';
						arrow[2] = ch - '\x80' + 'A';
						write(master, &arrow, 3);
					} else {
						write(master, &ch, 1); // write to program
					}
				}
				if(escapeCounter > 0) {
					escapeCounter--;
				}
			}
		}
	}
	} while(restart);
	return 0;
}
