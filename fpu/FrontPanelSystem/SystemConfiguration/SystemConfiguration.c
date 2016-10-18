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
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/wait.h>

#include <fpui.h>
#include <front_panel.h>

#define CHAR_ESC '\x1b'

#define ATC_CONFIG_MENU_FILE "/etc/default/ATCConfigurationMenu.txt"

static int scm;				// globaled for signal handler
static int default_screen = SCM_DEV;	// temporary storage for default screen
static bool panel_change = true;

void window_handler( int arg )
{
	panel_change = true;
}


void alldone( int arg )
{
	fprintf( stderr, "Exiting on HUP\n");
	close( scm );
	exit( 0 );
}


static char *regtab[16];
static char *apptab[16];
static bool emergency[APP_OPENS] = {0};
#define MAX_MENU_ROWS 8
static char menu[MAX_MENU_ROWS][41];
int g_rows = 8, g_cols = 40;
static char pad[16];

void clear_menu( void )
{
	memset( &menu[0][0], ' ', sizeof(menu) );
}


char *reg_name( int n )
{
	if( regtab[n] != NULL ) {
		if( *regtab[n] == '\0' ) return("<empty>");
		if( *regtab[n] == ' ' ) return("<erased>");
		return( regtab[n] );
	}
	return( "" );
}


char isdefault( int i )
{
	return( (i == default_screen)?'*':' ' );
}

bool has_emergency( int i )
{
	return emergency[i];
}

void display( int fd, int row_index )
{
	int i, row;
	int menu_start_row = ((g_rows-3) < MAX_MENU_ROWS)?row_index:0;
	char buf[100];

	fpui_write_string_at(fd, "     ATC CONFIGURATION INFORMATION      ", 1, 1);
	fpui_write_string_at(fd, " SELECT ITEM [0-F]                      ", 2, 1);
	for( i=menu_start_row, row=3; (i<MAX_MENU_ROWS) && (row<g_rows); i++ ) {
		sprintf(buf, "%1X %-18.18s%1X %-18.18s",
			i*2, reg_name(i*2),
			i*2+1, reg_name(i*2+1));
		fpui_write_string_at(fd, buf, row++, 1);
	}
	fpui_write_string_at(fd, "[UP/DN ARROW]        [FRONT PANEL- NEXT]", g_rows, 1);
	fpui_set_cursor_pos(fd, 3, 10);
}

void get_screen_size(int fd)
{
	char buf[1024];
	read_packet *rp = (read_packet *)buf;
	char type = 0;
	int count = 0;

	// Query panel type: A (4 lines); B (8 lines); D (16 lines)
	fpui_write_string( fd, "\x1b[c" );
	// response should be in read_packet format
	while ( (count = read(fd, buf, sizeof(buf))) > 0 ) {
		fprintf(stderr, "SCM: get_screen_size: read %d bytes\n", count);
		if (rp->command == DATA) {
			if (sscanf((const char *)rp->data, "\x1b[%cR", &type) == 1) {
				if (toupper(type) == 'A')
					g_rows = 4;
				else if (toupper(type) == 'B')
					g_rows = 8;
				else if (toupper(type) == 'D')
					g_rows = 16;
			}
			break;
		}
	}
	fprintf( stderr, "SCM: panel type = %x, g_rows = %d\n", type, g_rows);	
}

#define KEY_ENT   0x0d
#define KEY_SPLAT 0x2a
#define KEY_PLUS  0x2b
#define KEY_MINUS 0x2d

typedef enum { KEY_NONE=0x00, KEY_UP=0x80,  KEY_DOWN=0x81, KEY_RIGHT=0x82, KEY_LEFT=0x83,
	       KEY_NEXT=0x84,  KEY_YES=0x85, KEY_NO=0x86 } key_types;

struct {
	key_types   key;
	char      * seq;
} keymap[] = {
	{ KEY_UP,    "\x1b[A" },	// up arrow
	{ KEY_DOWN,  "\x1b[B" },	// down arrow
	{ KEY_RIGHT, "\x1b[C" },	// right arrow
	{ KEY_LEFT,  "\x1b[D" },	// left arrow
	{ KEY_NEXT,  "\x1bOP" },	// NEXT
	{ KEY_YES,   "\x1bOQ" },	// YES
	{ KEY_NO,    "\x1bOR" },	// NO
};
#define KEYMAP_SIZE (sizeof( keymap ) / sizeof( keymap[0] ))

/* Initialize menu from configuration file */
void init_menu(void)
{
	FILE *fp = NULL;
	char linestr[255];
	char *appName, *appPath;
	int i;

	if ((fp = fopen(ATC_CONFIG_MENU_FILE, "r")) == NULL) {
		printf("init_menu: could not open %s\n", ATC_CONFIG_MENU_FILE);
		return;
	}
	/* Process entries and add to menu */
	for (i=0; (i<16) && (fgets(linestr, sizeof(linestr), fp) != NULL); i++) {
		if ((appName = strtok(linestr, ",")) == NULL)
			continue;
		if ((appPath = strtok(NULL, "\n")) == NULL)
			continue;
		printf("init_menu: init slot %d for %s, %s\n", i, appName, appPath);
		if ((regtab[i] = calloc(1, strlen(appName))) == NULL) {
			printf("init_menu: could not allocate memory for app #%d regname\n", i);
			break;
		}
		memcpy(regtab[i], appName, strlen(appName));
		if ((apptab[i] = calloc(1, strlen(appPath))) == NULL) {
			printf("init_menu: could not allocate memory for app #%d pathname\n", i);
			break;
		}
		memcpy(apptab[i], appPath, strlen(appPath));
	}
	fclose(fp);
}

pid_t childPID;
int currentApp;
int start_app(int appnum)
{
	if ((regtab[appnum] != NULL) && (apptab[appnum] != NULL) && (access(apptab[appnum],X_OK) != -1)) {
		/* try to exec application */
		pid_t pid;	
		printf("start_app: #%d starting utility %s\n", appnum, apptab[appnum]);
		if ((pid = fork()) < 0) {
			/* Error forking */
			return -1;
		} else if (pid == 0) {
			/* child process */
			char *pname = strdup(apptab[appnum]);
			char *argv[] = {basename(pname), NULL};
			execvp(apptab[appnum], argv);
		} else {
			/* parent process */
			printf("start_app: forked utility id %d\n", pid);
			childPID = pid;
			currentApp = appnum;
		}
		return 0;
	}
	return -1;
}

int terminate_prompt(int fd)
{
	int i, count = 0;
	char buf[100];
	read_packet *rp = (read_packet *)buf;
	int focus_dev = SCM_DEV;

	printf("SCM: show terminate prompt\n");
	fpui_clear(fd);
	
	fpui_write_string_at(fd, "****************************************", 2, 1);
	fpui_write_string_at(fd, "        The Configuration Utility       ", 3, 1);
	sprintf(buf, "   %18.18s has not closed    ", reg_name(currentApp));
	fpui_write_string_at(fd, buf, 4, 1);
	fpui_write_string_at(fd, "      Do you wish to terminate it?      ", 5, 1);
	fpui_write_string_at(fd, "             Press Yes or No            ", 6, 1);
	fpui_write_string_at(fd, "****************************************", 7, 1);

	// put our prompt screen in focus
	ioctl( fd, FP_IOC_SET_FOCUS, &focus_dev );
	
	// response should be in read_packet format
	count = read(fd, buf, sizeof(buf));
	if ((count < 0) || (count < sizeof(read_packet))) {
		return -1;
	}
	if (rp->command == DATA) {
		for(i=0; (i<rp->raw_offset) && (rp->data[i] != '\0'); i++) {
			switch( rp->data[i] ) {
			case KEY_YES:
				return 1;
			case KEY_NO:
				return 0;
			default:
				return -1;
			};
		}
	}
	return -1;
}

int main( int argc, char * argv[] )
{
	int i, res;
	int row_index = 0;
	char buf[1024];
	read_packet *rp = (read_packet *)buf;
	struct sigaction act;
	unsigned int *p_state;
	
	signal( SIGHUP, alldone );
	
	memset (&act, 0, sizeof(act));
	act.sa_handler = window_handler;
	act.sa_flags = 0;
	if (sigaction(SIGWINCH, &act, NULL) != 0) {
	        fprintf( stderr, "%s: sigaction error - %s\n", argv[0], strerror( errno ) );
		exit( 99 );		
	}

	// initialize the arrays of application names and executable paths
	for(i=0; i<16; i++) { 
		regtab[i] = NULL;
		apptab[i] = NULL;
	}

	pad[0] = 0;

	//
	// NOTE when reading, we are reading preformatted read_packets.
	//
	if( (scm = open( "/dev/scm", O_RDWR | O_EXCL )) < 0) {
	        fprintf( stderr, "%s: Open error - %s\n", argv[0], strerror( errno ) );
		exit( 99 );
	}

	printf("%s Ready scm=%d\n", argv[0], scm );

	init_menu();
	
	// Use default key mappings
	for( ;; ) {
		if (panel_change) {
			// if we regained focus, begin timeout of utility if any
			if (childPID > 0) {
				printf("SCM: timing out utility...\n");
				int ret = 0;
				if ((ret = waitpid(childPID, 0, WNOHANG)) == 0) {
					printf( "SCM: utility not terminated, restore focus\n");
					unsigned int focus_dev = SCI_DEV;
					ioctl( scm, FP_IOC_SET_FOCUS, &focus_dev );
					sleep(2);
					// if child has not terminated 
					if ((ret = waitpid(childPID, 0, WNOHANG)) == 0) {
						printf( "SCM: utility not terminated after 2 secs, remove focus\n");
						// prompt user to terminate
						res = terminate_prompt(scm);
						if (res == 0) {
							// user requested continue
							printf( "SCM: user requests continue, restore focus\n");
							focus_dev = SCI_DEV;
							ioctl( scm, FP_IOC_SET_FOCUS, &focus_dev );
						} else if (res == 1) {
							// user requested terminate
							printf( "SCM: user requests terminate, send kill signal\n");
							kill(childPID, SIGKILL);
							ret = waitpid(childPID, 0, 0);
						} else {
							printf( "SCM: error result from terminate_prompt()\n");
							kill(childPID, SIGKILL);
							ret = waitpid(childPID, 0, 0);
						}
					}
				}
				if (ret == childPID) {
					printf( "SCM: utility has terminated\n");
					childPID = -1;
				}
			}
			// get actual panel dimensions
			row_index = 0;
			fprintf(stderr, "SCM: panel change\n");
			get_screen_size( scm );
			fpui_clear( scm );
			display( scm, row_index );
			panel_change = false;
		}
			
		i = read(scm, buf, sizeof(buf));
		printf( "SCM: read() return %d\n", i );
		if ( (i < 0) || (i < sizeof(read_packet)) ) {
			if (errno == EINTR)
				continue;
			perror( argv[0] );
			exit( 30 );
		}
		switch( rp->command ) {
			case NOOP:
				printf("%s: NOOP packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
			case DATA:
				printf("%s: DATA packet from %d to %d\n", argv[0], rp->from, rp->to );
				// Read each byte of "mapped" data from combined mapped+raw packet.
				for( i = 0; (i < rp->raw_offset) && (rp->data[i] != '\0'); i++ ) {
					switch( rp->data[i] ) {
						case KEY_NEXT:	{// to select the Master Selection screen
							//unsigned int focus_dev = MS_DEV;
							//fprintf( stderr, "SC: Setting focus to Master Selection\n");
							//ioctl( msi, FP_IOC_SET_FOCUS, &focus_dev );
							break;
						}
						case KEY_UP:
							fprintf( stderr, "SCM: keycode KEY_UP\n");
							if( (g_rows-3) < MAX_MENU_ROWS ) {
								if (row_index > 0) 
									row_index--;
							}
							break;
						case KEY_DOWN:
							fprintf( stderr, "SCM: keycode KEY_DOWN\n");
							if( (g_rows-3) < MAX_MENU_ROWS ) {
								if ((row_index+(g_rows-3)) < MAX_MENU_ROWS)
									row_index++;
							}
							break;
						default:	// [0..F] to select an application screen
						{
							fprintf( stderr, "SCM: keycode 0x%2.2x\n", rp->data[i]);
							if(isxdigit(rp->data[i])) {
								int app = isdigit(rp->data[i])?
                                                                        (rp->data[i]-0x30):
                                                                        (toupper(rp->data[i])-55);
								if (regtab[app] != NULL) {
									fprintf( stderr, "SCM: starting child process for app %x\n", app);
									if (start_app(app) == 0) {
										break;
									}
								}
							}
							// invalid selection
							fprintf( stderr, "SCM: invalid keypress\n" );
							// Issue "bell" alert to front panel
							char bel = 0x7;
							write(scm, &bel, 1);						
							break;
						}
					}
				}
				display( scm, row_index );
                                break;
                        case CREATE: {
				unsigned int focus_dev = SCI_DEV;
				printf("%s: CREATE packet from %d to %d\n", argv[0], rp->from, rp->to );
				// Config Utility window is open, switch focus to it
				ioctl( scm, FP_IOC_SET_FOCUS, &focus_dev );
                                break;
			}
                        case REGISTER:
				printf("%s: REGISTER packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
                        case DESTROY:
				printf("%s: DESTROY packet from %d to %d\n", argv[0], rp->from, rp->to );
				// Config Utility window has closed, wait on child exit
				if (childPID > 0) {
					waitpid(childPID, 0 , 0);
					childPID = -1;
					currentApp = -1;
				}
                                break;
                        case FOCUS:
				printf("%s: FOCUS packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
                        default:
				printf("%s: Undefined packet from %d to %d\n", argv[0], rp->from, rp->to );
				exit( 99 );
                                break;

		}
	}
}
