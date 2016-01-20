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

#include "atc_sc.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <ctype.h>

#include <fpui.h>
#include <fio.h>
#include <tod.h>

#define TZCONFIG_FILE	"/etc/localtime"

/* Procedures used by the SC module */
int start_sc_module(void);
void init_internal_screens(void);
void display_screen(int screen_no);
void commit_line(int screen_no, int line_no);
void send_display_change(int crt_cursor_x, int crt_cursor_y, unsigned char display_cmd,
		 unsigned char *value, int field_length, unsigned char lock_screen);
void update_time_screen(void);
void update_ethernet_screen(void *);
void update_linux_screen(void *);
void update_eeprom_screen(void *);
void update_timesrc_screen(void);

int read_cmd(unsigned char *pBuffer);
void parse_cmd(unsigned char *pBuffer, sc_cmd_struct *pCmd);
void process_cmd(sc_cmd_struct *pCmd);
 
 /* Global data containing the database of  the SC module. 
  * This data is accessed directly by the procedures from the SC module.
  */
static sc_internal_data	display_data;
int g_rows = 4, g_cols = 40;
bool panel_change = false;


 /*
  * A simple integer power implementation
  */
int ipow(int base, int exp)
{
	int result = 1;
	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

void signal_handler( int arg )
{
	fprintf( stderr, "SIGWINCH\n" );
	panel_change = true;
}

 int main()
 {
 	return start_sc_module();
 }	

 /* Initialize and start the SC module;
  * After initialization the SC module stays in an infinite loop,
  * where it blocks  waiting for a new command, parse it, and execute it
  */	
 int start_sc_module()
 {
	struct sigaction act;
 	/* Buffers used to store the current command */
	unsigned char cmd_buffer[MAX_CMD_BUFFER_SIZE];
	sc_cmd_struct crt_cmd;

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigaction(SIGWINCH, &act, NULL);

	/* Open the communication channel with the FPM module */
	display_data.file_descr = open(PATH_NAME_FPI, O_RDWR | O_EXCL);
	
	if (display_data.file_descr < 0) /* error opening the channel? */
		return ERR_OPEN_CONNECTION;
	
	/* Get actual panel dimensions */
	if (fpui_get_window_size(display_data.file_descr, &g_rows, &g_cols) != 0) {
		g_rows = EXTERNAL_SCREEN_Y_SIZE;
		g_cols = EXTERNAL_SCREEN_X_SIZE;
	}

 	/* Initialize the default values of the internal screens */
 	init_internal_screens();
		
	/* Display the current screen */
	display_screen(display_data.crt_screen);
	
	/* Stay in the infinite loop read/parse/process command */
	while (1)
	{
		if (panel_change) {
			if (fpui_panel_present(display_data.file_descr)
				&& (fpui_get_window_size(display_data.file_descr, &g_rows, &g_cols) != 0)) {
				g_rows = EXTERNAL_SCREEN_Y_SIZE;
				g_cols = EXTERNAL_SCREEN_X_SIZE;
			}
			display_screen(display_data.crt_screen);
			panel_change = false;
		}
		read_cmd(cmd_buffer);
		parse_cmd(cmd_buffer, &crt_cmd);
		process_cmd(&crt_cmd);
	}
	
 }
 
 /* Initialize one unmodifiable field line, defined by a pointer to a screen and the line number */ 
 void init_one_field_line(sc_internal_screen *pScreen, int lineNo, const char *line_data, bool isScrollable)
 {
 	pScreen->screen_lines[lineNo].no_fields = 1; // 1 field in this line
	pScreen->screen_lines[lineNo].fields[0].type = kUnmodifiable; 
	pScreen->screen_lines[lineNo].fields[0].length = strlen(line_data);
	pScreen->screen_lines[lineNo].fields[0].start = 0;
	memcpy(pScreen->screen_lines[lineNo].line, line_data , 
				pScreen->screen_lines[lineNo].fields[0].length );
	pScreen->screen_lines[lineNo].isScrollable = isScrollable;
 }

void init_alpha_field(sc_line_field *pField, char data_val)
{
	char val;
	
	if (isalpha(data_val))
		val = tolower(data_val) - 'a' + 1;
	else if (isdigit(data_val))
		val = data_val - 0x30 + 27;
	else if (data_val == '-')
		val = 37;
	else
		val = 0;
		
	pField->internal_data = val;
	pField->temp_data = val;
	pField->data_max = 37;
	pField->string_data[0] = " ";
	pField->string_data[1] = "a";
	pField->string_data[2] = "b";
	pField->string_data[3] = "c";
	pField->string_data[4] = "d";
	pField->string_data[5] = "e";
	pField->string_data[6] = "f";
	pField->string_data[7] = "g";
	pField->string_data[8] = "h";
	pField->string_data[9] = "i";
	pField->string_data[10] = "j";
	pField->string_data[11] = "k";
	pField->string_data[12] = "l";
	pField->string_data[13] = "m";
	pField->string_data[14] = "n";
	pField->string_data[15] = "o";
	pField->string_data[16] = "p";
	pField->string_data[17] = "q";
	pField->string_data[18] = "r";
	pField->string_data[19] = "s";
	pField->string_data[20] = "t";
	pField->string_data[21] = "u";
	pField->string_data[22] = "v";
	pField->string_data[23] = "w";
	pField->string_data[24] = "x";
	pField->string_data[25] = "y";
	pField->string_data[26] = "z";
	pField->string_data[27] = "0";
	pField->string_data[28] = "1";
	pField->string_data[29] = "2";
	pField->string_data[30] = "3";
	pField->string_data[31] = "4";
	pField->string_data[32] = "5";
	pField->string_data[33] = "6";
	pField->string_data[34] = "7";
	pField->string_data[35] = "8";
	pField->string_data[36] = "9";
	pField->string_data[37] = "-";
}

 /* Get display data from file (config files or procfs files, etc).
  * Used to initialize info fields in the display screens.
  * The procedure depends on the file format which is subject to parsing;
  * a change of this format will make the procedure not to work properly. 
  */
int get_data_from_file(const char *file_name, const char *match, char *data_value, int no_chars)
{
	char info_line[MAX_PROCFILE_LINE_SIZE];
	int index = 0;

	FILE *fp = fopen(file_name, "r");
	if (fp == NULL) /* error opening the channel? */
		return ERR_OPEN_PROCFILE;
	
	while (1) {
		fgets(info_line, MAX_PROCFILE_LINE_SIZE, fp);
		if (ferror(fp))
			return ERR_READ_PROCFILE;
		else if (feof(fp))
			return ERR_UNAVAILABLE_PROCFILE;
		if( (match == NULL) || strstr(info_line, match)) {
			/* return this line */
			break;
		}
	}

	/* Copy the matched value to the destination */
	index = 0;
	while ((index < no_chars) && (info_line[index] != '\n') && (info_line[index] != '\0'))
	{
		data_value[index] = info_line[index];
		index++;
	}
  
	fclose(fp);
	/* Return number chars copied */
	return (index);
}

void init_remaining_lines(sc_internal_screen* pScreen, int startLine)
{
	int lineNo;
	for(lineNo=startLine; lineNo<MAX_INTERNAL_SCREEN_Y_SIZE; lineNo++) 
	{
		memset(pScreen->screen_lines[lineNo].line, ' ', pScreen->dim_x);
		
	}
}
  
 /* Initialize the internal screens and the mutex protecting them. */
 void init_internal_screens(void)
 {
 	sc_internal_screen * pScreen;
	sc_screen_line *pLine;
	int lineNo;
	int fieldNo; 
	struct utsname utsname;
	struct sysinfo sys_info;
        struct statfs stat_fs;
	char buffer[MAX_INTERNAL_SCREEN_X_SIZE];
	int byteCount;

	/* Initialize the screen mutex protecting the variable part of the screens. */
	pthread_mutex_init(&display_data.screen_mutex, NULL);	
	
	/* Lock the mutex protecting the variable part of the screens. */
	pthread_mutex_lock(&display_data.screen_mutex);

 	/* Initialize the Menu screen */
	pScreen = &display_data.screens[MENU_SCREEN_ID]; 
	pScreen->dim_x = MENU_SCREEN_X_SIZE;
	pScreen->dim_y = MENU_SCREEN_Y_SIZE;
	pScreen->header_dim_y = MENU_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = NULL;
	memcpy(pScreen->header[0], HEADER_LINE_0_MENU,
		strlen(HEADER_LINE_0_MENU));
	memcpy(pScreen->header[1], HEADER_LINE_1_MENU,
		strlen(HEADER_LINE_1_MENU));
	memcpy(pScreen->footer, FOOTER_LINE_MENU, strlen(FOOTER_LINE_MENU));
	init_one_field_line(pScreen, 0, LINE_0_MENU, false);
	init_one_field_line(pScreen, 1, LINE_1_MENU, false);
	init_one_field_line(pScreen, 2, LINE_2_MENU, true);
	init_one_field_line(pScreen, 3, LINE_3_MENU, true);
	init_one_field_line(pScreen, 4, LINE_4_MENU, true);
	init_one_field_line(pScreen, 5, LINE_5_MENU, true);
	init_one_field_line(pScreen, 6, LINE_6_MENU, true);
	init_one_field_line(pScreen, 7, LINE_7_MENU, false);
	init_remaining_lines(pScreen, 8);
	//init_one_field_line(pScreen, 10, LINE_10_MENU, true);


 	/* Initialize the TIME screen */
	pScreen = &display_data.screens[TIME_SCREEN_ID]; 
	pScreen->dim_x = TIME_SCREEN_X_SIZE;
	pScreen->dim_y = TIME_SCREEN_Y_SIZE;
	pScreen->header_dim_y = TIME_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = (void *)update_time_screen;
	memcpy(pScreen->header[0], HEADER_LINE_0_TIME,
		strlen(HEADER_LINE_0_TIME));
	memcpy(pScreen->footer, FOOTER_LINE_TIME,
		strlen(FOOTER_LINE_TIME));
	init_one_field_line(pScreen, 0, LINE_0_TIME, false);
	init_one_field_line(pScreen, 1, LINE_1_TIME, false);
	init_one_field_line(pScreen, 2, LINE_2_TIME, false);
	init_one_field_line(pScreen, 3, LINE_3_TIME, false);
	init_one_field_line(pScreen, 4, LINE_4_TIME, false);
	pScreen->screen_lines[5].isScrollable = false;
	pScreen->screen_lines[5].no_fields = 17; //17 fields in line 5
	memcpy(pScreen->screen_lines[5].line, LINE_5_TIME , strlen(LINE_5_TIME));

	pScreen->screen_lines[5].fields[0].type = kModifiable;
	pScreen->screen_lines[5].fields[0].length = 2;
	pScreen->screen_lines[5].fields[0].data_min = 1;
	pScreen->screen_lines[5].fields[0].data_max = 12;
	pScreen->screen_lines[5].fields[0].string_data[1] = "%02d";
	
	pScreen->screen_lines[5].fields[1].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[1].length = 1;

	pScreen->screen_lines[5].fields[2].type = kModifiable;
	pScreen->screen_lines[5].fields[2].length = 2;
	pScreen->screen_lines[5].fields[2].data_min = 1;
	pScreen->screen_lines[5].fields[2].data_max = 31;
	pScreen->screen_lines[5].fields[2].string_data[1] = "%02d";
	
	pScreen->screen_lines[5].fields[3].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[3].length = 1;

	pScreen->screen_lines[5].fields[4].type = kModifiable;
	pScreen->screen_lines[5].fields[4].length = 4;
	pScreen->screen_lines[5].fields[4].data_max = 9999;
	pScreen->screen_lines[5].fields[4].string_data[1] = "%04d";
	
	pScreen->screen_lines[5].fields[5].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[5].length = 1;
	
	pScreen->screen_lines[5].fields[6].type = kModifiable;
	pScreen->screen_lines[5].fields[6].length = 2;
	pScreen->screen_lines[5].fields[6].data_max = 23;
	pScreen->screen_lines[5].fields[6].string_data[1] = "%02d";
	
	pScreen->screen_lines[5].fields[7].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[7].length = 1;
	
	pScreen->screen_lines[5].fields[8].type = kModifiable;
	pScreen->screen_lines[5].fields[8].length = 2;
	pScreen->screen_lines[5].fields[8].data_max = 59;
	pScreen->screen_lines[5].fields[8].string_data[1] = "%02d";
	
	pScreen->screen_lines[5].fields[9].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[9].length = 4;
	
	pScreen->screen_lines[5].fields[10].type = kModifiable;
	pScreen->screen_lines[5].fields[10].length = 1;
	pScreen->screen_lines[5].fields[10].internal_data = 1;
	pScreen->screen_lines[5].fields[10].temp_data = 1;
	pScreen->screen_lines[5].fields[10].data_max = 1;
	pScreen->screen_lines[5].fields[10].string_data[0] = "-";
	pScreen->screen_lines[5].fields[10].string_data[1] = "+";

	pScreen->screen_lines[5].fields[11].type = kModifiable;
	pScreen->screen_lines[5].fields[11].length = 2;
	pScreen->screen_lines[5].fields[11].data_max = 12;
	pScreen->screen_lines[5].fields[11].string_data[1] = "%02d";

	pScreen->screen_lines[5].fields[12].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[12].length = 1;

	pScreen->screen_lines[5].fields[13].type = kModifiable;
	pScreen->screen_lines[5].fields[13].length = 2;
	pScreen->screen_lines[5].fields[13].data_max = 59;
	pScreen->screen_lines[5].fields[13].string_data[1] = "%02d";

	pScreen->screen_lines[5].fields[14].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[14].length = 1;

	pScreen->screen_lines[5].fields[15].type = kModifiable;
	pScreen->screen_lines[5].fields[15].length = 6;
	pScreen->screen_lines[5].fields[15].internal_data = 0;
	pScreen->screen_lines[5].fields[15].temp_data = 0;
	pScreen->screen_lines[5].fields[15].data_max = 1;
	pScreen->screen_lines[5].fields[15].string_data[0] = "Disabl";
	pScreen->screen_lines[5].fields[15].string_data[1] = "Enable";

	pScreen->screen_lines[5].fields[16].type = kUnmodifiable;
	pScreen->screen_lines[5].fields[16].length = 7;
	
	init_remaining_lines(pScreen, 6);

	/* Update the start var in each field of each line of this default screen */
	for (lineNo = 0; lineNo < pScreen->dim_y; lineNo++)
	{
		pScreen->screen_lines[lineNo].fields[0].start = 0;
		for (fieldNo = 1; fieldNo < pScreen->screen_lines[lineNo].no_fields; fieldNo++)
		{
			pScreen->screen_lines[lineNo].fields[fieldNo].start =
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].start +
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].length;
		}
	}


 	/* Initialize the ETH1 screen */
	pScreen = &display_data.screens[ETH1_SCREEN_ID]; 
	pScreen->dim_x = ETH1_SCREEN_X_SIZE;
	pScreen->dim_y = ETH1_SCREEN_Y_SIZE;
	pScreen->header_dim_y = ETH_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = (void *)update_ethernet_screen;
	memcpy(pScreen->header[0], HEADER_LINE_0_ETH,
		strlen(HEADER_LINE_0_ETH));
	memcpy(pScreen->header[1], HEADER_LINE_1_ETH,
		strlen(HEADER_LINE_1_ETH));	
	memcpy(pScreen->footer, FOOTER_LINE_ETH, strlen(FOOTER_LINE_ETH));
	
	byteCount = get_data_from_file("/sys/class/net/eth0/address", NULL, buffer, 17);
	memcpy((char *)&pScreen->header[1][19], buffer, byteCount);

	pLine = &pScreen->screen_lines[ETH_MODE_LINE];
	pLine->isScrollable = true;
	memcpy(pLine->line, LINE_0_ETH , strlen(LINE_0_ETH));
	pLine->no_fields = 3;
	pLine->fields[0].type = kUnmodifiable;
	pLine->fields[0].length = 17;
	pLine->fields[1].type = kModifiable;
	pLine->fields[1].length = 8;
	pLine->fields[1].internal_data = 0;
	pLine->fields[1].temp_data = 0;
	pLine->fields[1].data_max = 2;
	pLine->fields[1].string_data[0] = "disabled";
	pLine->fields[1].string_data[1] = "static  ";
	pLine->fields[1].string_data[2] = "dhcp    ";
	pLine->fields[2].type = kUnmodifiable;
	pLine->fields[2].length = 15;

	pLine = &pScreen->screen_lines[ETH_IPADDR_LINE];
	pLine->isScrollable = true;
	memcpy(pLine->line, LINE_1_ETH , strlen(LINE_1_ETH));
	pLine->no_fields = 9;
	pLine->fields[0].type = kUnmodifiable;
	pLine->fields[0].length = 17;
	pLine->fields[1].type = kModifiable;
	pLine->fields[1].length = 3;
	pLine->fields[1].data_max = 255;
	pLine->fields[2].type = kUnmodifiable;
	pLine->fields[2].length = 1;
	pLine->fields[3].type = kModifiable;
	pLine->fields[3].length = 3;
	pLine->fields[3].data_max = 255;
	pLine->fields[4].type = kUnmodifiable;
	pLine->fields[4].length = 1;
	pLine->fields[5].type = kModifiable;
	pLine->fields[5].length = 3;
	pLine->fields[5].data_max = 255;
	pLine->fields[6].type = kUnmodifiable;
	pLine->fields[6].length = 1;
	pLine->fields[7].type = kModifiable;
	pLine->fields[7].length = 3;
	pLine->fields[7].data_max = 255;
	pLine->fields[8].type = kUnmodifiable;
	pLine->fields[8].length = 8;

	pLine = &pScreen->screen_lines[ETH_NETMASK_LINE];
	pLine->isScrollable = true;
	memcpy(pLine->line, LINE_2_ETH , strlen(LINE_2_ETH));
	pLine->no_fields = 9;
	pLine->fields[0].type = kUnmodifiable;
	pLine->fields[0].length = 17;
	pLine->fields[1].type = kModifiable;
	pLine->fields[1].length = 3;
	pLine->fields[1].data_max = 255;
	pLine->fields[2].type = kUnmodifiable;
	pLine->fields[2].length = 1;
	pLine->fields[3].type = kModifiable;
	pLine->fields[3].length = 3;
	pLine->fields[3].data_max = 255;
	pLine->fields[4].type = kUnmodifiable;
	pLine->fields[4].length = 1;
	pLine->fields[5].type = kModifiable;
	pLine->fields[5].length = 3;
	pLine->fields[5].data_max = 255;
	pLine->fields[6].type = kUnmodifiable;
	pLine->fields[6].length = 1;
	pLine->fields[7].type = kModifiable;
	pLine->fields[7].length = 3;
	pLine->fields[7].data_max = 255;
	pLine->fields[8].type = kUnmodifiable;
	pLine->fields[8].length = 8;

	pLine = &pScreen->screen_lines[ETH_GWADDR_LINE];
	pLine->isScrollable = true;
	memcpy(pLine->line, LINE_3_ETH , strlen(LINE_3_ETH));
	pLine->no_fields = 9;
	pLine->fields[0].type = kUnmodifiable;
	pLine->fields[0].length = 17;
	pLine->fields[1].type = kModifiable;
	pLine->fields[1].length = 3;
	pLine->fields[1].data_max = 255;
	pLine->fields[2].type = kUnmodifiable;
	pLine->fields[2].length = 1;
	pLine->fields[3].type = kModifiable;
	pLine->fields[3].length = 3;
	pLine->fields[3].data_max = 255;
	pLine->fields[4].type = kUnmodifiable;
	pLine->fields[4].length = 1;
	pLine->fields[5].type = kModifiable;
	pLine->fields[5].length = 3;
	pLine->fields[5].data_max = 255;
	pLine->fields[6].type = kUnmodifiable;
	pLine->fields[6].length = 1;
	pLine->fields[7].type = kModifiable;
	pLine->fields[7].length = 3;
	pLine->fields[7].data_max = 255;
	pLine->fields[8].type = kUnmodifiable;
	pLine->fields[8].length = 8;

	pLine = &pScreen->screen_lines[ETH_NSADDR_LINE];
	pLine->isScrollable = true;
	memcpy(pLine->line, LINE_4_ETH , strlen(LINE_4_ETH));
	pLine->no_fields = 9;
	pLine->fields[0].type = kUnmodifiable;
	pLine->fields[0].length = 17;
	pLine->fields[1].type = kModifiable;
	pLine->fields[1].length = 3;
	pLine->fields[1].data_max = 255;
	pLine->fields[2].type = kUnmodifiable;
	pLine->fields[2].length = 1;
	pLine->fields[3].type = kModifiable;
	pLine->fields[3].length = 3;
	pLine->fields[3].data_max = 255;
	pLine->fields[4].type = kUnmodifiable;
	pLine->fields[4].length = 1;
	pLine->fields[5].type = kModifiable;
	pLine->fields[5].length = 3;
	pLine->fields[5].data_max = 255;
	pLine->fields[6].type = kUnmodifiable;
	pLine->fields[6].length = 1;
	pLine->fields[7].type = kModifiable;
	pLine->fields[7].length = 3;
	pLine->fields[7].data_max = 255;
	pLine->fields[8].type = kUnmodifiable;
	pLine->fields[8].length = 8;

	pLine = &pScreen->screen_lines[ETH_HOSTNAME_LINE];
	pLine->isScrollable = true;
	memcpy(pLine->line, LINE_5_ETH , strlen(LINE_5_ETH));
	pLine->no_fields = 17;
	pLine->fields[0].type = kUnmodifiable;
	pLine->fields[0].length = 11;
	byteCount = get_data_from_file("/proc/sys/kernel/hostname", NULL, buffer, 29);
	if (byteCount > 0)
		memcpy((char *)&pLine->line[11], buffer, byteCount);
	for(fieldNo = ETH_HOSTNAME_FIELD1; fieldNo < pLine->no_fields; fieldNo++) {
		pLine->fields[fieldNo].type = kModifiable;
		pLine->fields[fieldNo].length = 1;
		if (fieldNo <= byteCount)
			init_alpha_field(&pLine->fields[fieldNo], buffer[fieldNo-1]);
		else
			init_alpha_field(&pLine->fields[fieldNo], ' ');
	}
	
	init_one_field_line(pScreen, ETH_TX_LINE, LINE_6_ETH, true);
	init_one_field_line(pScreen, ETH_RX_LINE, LINE_7_ETH, true);
	
	init_remaining_lines(pScreen, 8);

	/* Update the start var in each field of each line of this default screen */
	for (lineNo = 0; lineNo < pScreen->dim_y; lineNo++)
	{
		pScreen->screen_lines[lineNo].fields[0].start = 0;
		for (fieldNo = 1; fieldNo < pScreen->screen_lines[lineNo].no_fields; fieldNo++)
		{
			pScreen->screen_lines[lineNo].fields[fieldNo].start =
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].start +
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].length;
		}
	}


 	/* Initialize the ETH2 screen */
	pScreen = &display_data.screens[ETH2_SCREEN_ID];
	*pScreen = display_data.screens[ETH1_SCREEN_ID];
	memcpy(pScreen->header[1], HEADER_LINE_1_ETH2,
		strlen(HEADER_LINE_1_ETH2));	
	byteCount = get_data_from_file("/sys/class/net/eth1/address", NULL, buffer, 17);
	memcpy((char *)&pScreen->header[1][19], buffer, byteCount);

	/* Update the start var in each field of each line of this default screen */
	for (lineNo = 0; lineNo < pScreen->dim_y; lineNo++)
	{
		pScreen->screen_lines[lineNo].fields[0].start = 0;
		for (fieldNo = 1; fieldNo < pScreen->screen_lines[lineNo].no_fields; fieldNo++)
		{
			pScreen->screen_lines[lineNo].fields[fieldNo].start =
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].start +
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].length;
		}
	}


 	/* Initialize the SRVC screen */
	pScreen = &display_data.screens[SRVC_SCREEN_ID]; 
	pScreen->dim_x = SRVC_SCREEN_X_SIZE;
	pScreen->dim_y = SRVC_SCREEN_Y_SIZE;
	pScreen->header_dim_y = SRVC_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = NULL;
	memcpy(pScreen->header[0], HEADER_LINE_0_SRVC,
		strlen(HEADER_LINE_0_SRVC));
	memcpy(pScreen->header[1], HEADER_LINE_1_SRVC,
		strlen(HEADER_LINE_1_SRVC));
	memcpy(pScreen->footer, FOOTER_LINE_SRVC, strlen(FOOTER_LINE_SRVC));
	init_remaining_lines(pScreen, 0);
	//init_one_field_line(pScreen, 7, LINE_7_SRVC, false);
	{DIR *dir;
	struct dirent *ent;
	struct stat sb;
	char fn[80];

	/* check for service scripts in /etc/init.d */
	if ((dir = opendir("/etc/init.d")) != NULL) {
		sc_screen_line *pLine = &pScreen->screen_lines[0];
		int line_no = 0;
		while ((ent = readdir(dir)) != NULL) {
			/* Skip any file not starting with a 'S' or 'X' */
			if ((ent->d_name[0] != 'S') && (ent->d_name[0] != 'X'))
				continue;

			sprintf(fn, "/etc/init.d/%s", ent->d_name);
			if (stat(fn, &sb) != 0) continue;
			if (!S_ISREG(sb.st_mode)) continue;

			pLine->isScrollable = true;
			pLine->no_fields = 3;
			pLine->fields[0].type = kUnmodifiable;
			pLine->fields[0].length = 23;
			byteCount = snprintf(buffer, 24, "%-23.23s", &ent->d_name[1]);
			memcpy((char *)pLine->line, buffer, byteCount);
			pLine->fields[1].type = kUnmodifiable;
			pLine->fields[1].length = 9;
			pLine->fields[1].internal_data = (ent->d_name[0] == 'S')?1:0;
			pLine->fields[1].temp_data = pLine->fields[1].internal_data;
			pLine->fields[1].data_max = 1;
			pLine->fields[1].string_data[0] = "Disabled";
			pLine->fields[1].string_data[1] = "Enabled ";
			byteCount = snprintf(buffer, 10, "%s", pLine->fields[1].string_data[pLine->fields[1].internal_data]);
			memcpy((char *)&pLine->line[23], buffer, byteCount);
			pLine->fields[2].type = kModifiable;
			pLine->fields[2].length = 8;
			pLine->fields[2].internal_data = (ent->d_name[0] == 'S')?1:0;
			pLine->fields[2].temp_data = pLine->fields[2].internal_data;
			pLine->fields[2].data_max = 1;
			pLine->fields[2].string_data[0] = "Disabled";
			pLine->fields[2].string_data[1] = "Enabled ";
			byteCount = snprintf(buffer, 9, "%s", pLine->fields[2].string_data[pLine->fields[2].internal_data]);
			memcpy((char *)&pLine->line[32], buffer, byteCount);
			pLine++;
			line_no++;
		}

		if (line_no > SRVC_SCREEN_Y_SIZE) {
			pScreen->dim_y = line_no;
		}
	}
	}
	/* Update the start var in each field of each line of this default screen */
	for (lineNo = 0; lineNo < pScreen->dim_y; lineNo++)
	{
		pScreen->screen_lines[lineNo].fields[0].start = 0;
		for (fieldNo = 1; fieldNo < pScreen->screen_lines[lineNo].no_fields; fieldNo++)
		{
			pScreen->screen_lines[lineNo].fields[fieldNo].start =
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].start +
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].length;
		}
	}

 	/* Initialize the LNUX screen */
	pScreen = &display_data.screens[LNUX_SCREEN_ID]; 
	pScreen->dim_x = LNUX_SCREEN_X_SIZE;
	pScreen->dim_y = LNUX_SCREEN_Y_SIZE;
	pScreen->header_dim_y = LNUX_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = (void *)update_linux_screen;
	memcpy(pScreen->header[0], HEADER_LINE_0_LNUX,
		strlen(HEADER_LINE_0_LNUX));
	memcpy(pScreen->footer, FOOTER_LINE_LNUX, strlen(FOOTER_LINE_LNUX));
	init_one_field_line(pScreen, 0, LINE_0_LNUX, true);
	init_one_field_line(pScreen, 1, LINE_1_LNUX, true);
	init_one_field_line(pScreen, 2, LINE_2_LNUX, true);
	init_one_field_line(pScreen, 3, LINE_3_LNUX, true);
	init_one_field_line(pScreen, 4, LINE_4_LNUX, true);
	init_one_field_line(pScreen, 5, LINE_5_LNUX, true);
	init_one_field_line(pScreen, 6, LINE_6_LNUX, true);
	init_remaining_lines(pScreen, 7);
	if( uname(&utsname) == 0 ) {
		byteCount = snprintf(buffer, 24, "%-14.14s", utsname.release);
		memcpy((char *)&pScreen->screen_lines[0].line[15], buffer, byteCount);
		byteCount = snprintf(buffer, 25, "%-25.25s", utsname.version);
		memcpy((char *)&pScreen->screen_lines[1].line[15], buffer, byteCount);
		byteCount = snprintf(buffer, 17, "%-17.17s", utsname.machine);
		memcpy((char *)&pScreen->screen_lines[2].line[23], buffer, byteCount);
	}
	if( sysinfo(&sys_info) == 0 ) {
		int updays = sys_info.uptime/(60*60*24);
		int uphours = (sys_info.uptime/(60*60))%24;
		int upmins = (sys_info.uptime/60)%60;
		if( sys_info.mem_unit == 0 )
			sys_info.mem_unit = 1;
		byteCount = snprintf(buffer, 32, "%4dMB", (int)((sys_info.totalram*sys_info.mem_unit)>>20));
		memcpy((char *)&pScreen->screen_lines[3].line[14], buffer, byteCount);
		byteCount = snprintf(buffer, 32, "%4dMB", (int)((sys_info.freeram*sys_info.mem_unit)>>20));
		memcpy((char *)&pScreen->screen_lines[3].line[27], buffer, byteCount);
		byteCount = get_data_from_file("/proc/loadavg", NULL, buffer, 26);
		memcpy((char *)&pScreen->screen_lines[5].line[14], buffer, byteCount);
		byteCount = snprintf(buffer, 26, "%d day%s, %2d hour%s, %2d min%s",
                    updays, ((updays!=1)?"s":""), uphours, ((uphours!=1)?"s":""), upmins, ((upmins!=1)?"s":"") );
		memcpy((char *)&pScreen->screen_lines[6].line[8], buffer, byteCount);
	}
        if (statfs("/", &stat_fs) == 0) {
		byteCount = snprintf(buffer, 32, "%4dMB", (int)((stat_fs.f_blocks*stat_fs.f_bsize)>>20));
		memcpy((char *)&pScreen->screen_lines[4].line[18], buffer, byteCount);
		byteCount = snprintf(buffer, 32, "%4dMB", (int)((stat_fs.f_bavail*stat_fs.f_bsize)>>20));
		memcpy((char *)&pScreen->screen_lines[4].line[31], buffer, byteCount);                
        }


 	/* Initialize the APIV screen */
	pScreen = &display_data.screens[APIV_SCREEN_ID]; 
	pScreen->dim_x = APIV_SCREEN_X_SIZE;
	pScreen->dim_y = APIV_SCREEN_Y_SIZE;
	pScreen->header_dim_y = APIV_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = NULL;
	memcpy(pScreen->header[0], HEADER_LINE_0_APIV,
		strlen(HEADER_LINE_0_APIV));
	memcpy(pScreen->footer, FOOTER_LINE_APIV, strlen(FOOTER_LINE_APIV));
	init_one_field_line(pScreen, 0, LINE_0_APIV, false);
	init_one_field_line(pScreen, 1, LINE_1_APIV, false);
	init_one_field_line(pScreen, 2, LINE_2_APIV, false);
	init_one_field_line(pScreen, 3, LINE_3_APIV, false);
	init_one_field_line(pScreen, 4, LINE_4_APIV, false);
	init_one_field_line(pScreen, 5, LINE_5_APIV, false);
	init_remaining_lines(pScreen, 6);
	byteCount = snprintf(buffer, 23, "%-23.23s", fpui_apiver(display_data.file_descr,1));
	memcpy(&pScreen->screen_lines[0].line[17], buffer, byteCount);
	byteCount = snprintf(buffer, 19, "%-19.19s", fpui_apiver(display_data.file_descr,2));
	memcpy((char *)&pScreen->screen_lines[1].line[21], buffer, byteCount);
	// Connect to fio api to get version info
	FIO_APP_HANDLE fiod = -1;
	if ((fiod = fio_register()) >= 0) { 
		byteCount = snprintf(buffer, 24, "%-24.24s", fio_apiver(fiod, FIO_VERSION_LIBRARY));
		memcpy(&pScreen->screen_lines[2].line[16], buffer, byteCount);
		byteCount = snprintf(buffer, 20, "%-20.20s", fio_apiver(fiod, FIO_VERSION_LKM));
		memcpy((char *)&pScreen->screen_lines[3].line[20], buffer, byteCount);
		fio_deregister(fiod);
	}
	byteCount = snprintf(buffer, 24, "%-24.24s", tod_apiver());
	memcpy(&pScreen->screen_lines[4].line[16], buffer, byteCount);
	
 	/* Initialize the EPRM screen */
	pScreen = &display_data.screens[EPRM_SCREEN_ID];
	pScreen->dim_x = EPRM_SCREEN_X_SIZE;
	pScreen->dim_y = EPRM_SCREEN_Y_SIZE;
	pScreen->header_dim_y = EPRM_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = (void *)update_eeprom_screen;
	memcpy(pScreen->header[0], HEADER_LINE_0_EPRM,
		strlen(HEADER_LINE_0_EPRM));
	memcpy(pScreen->footer, FOOTER_LINE_EPRM, strlen(FOOTER_LINE_EPRM));
	init_remaining_lines(pScreen, 0);
	for(lineNo=0; lineNo<MAX_INTERNAL_SCREEN_Y_SIZE; lineNo++) {
		memset(pScreen->screen_lines[lineNo].line, ' ', EPRM_SCREEN_X_SIZE);
	}
	int screen_no = EPRM_SCREEN_ID;
	update_eeprom_screen(&screen_no);


 	/* Initialize the TSRC screen */
	pScreen = &display_data.screens[TSRC_SCREEN_ID]; 
	pScreen->dim_x = TSRC_SCREEN_X_SIZE;
	pScreen->dim_y = TSRC_SCREEN_Y_SIZE;
	pScreen->header_dim_y = TSRC_SCREEN_HEADER_SIZE;
	pScreen->cursor_x = 0;
	pScreen->cursor_y = 0;
	pScreen->display_offset_y = 0;
	pScreen->update = (void *)update_timesrc_screen;
	memcpy(pScreen->header[0], HEADER_LINE_0_TSRC,
		strlen(HEADER_LINE_0_TSRC));
	memcpy(pScreen->footer, FOOTER_LINE_TSRC, strlen(FOOTER_LINE_TSRC));
	init_one_field_line(pScreen, 0, LINE_0_TSRC, false);

        //int tsrc = tod_get_timesrc();
        //printf("Initial tod_get_timesrc: %d\n", tsrc);
        //if ((tsrc < 0) || (tsrc > 4))
        //        tsrc = 0;
	pLine = &pScreen->screen_lines[TSRC_TSRC_LINE];
	memcpy(pLine->line, LINE_1_TSRC, strlen(LINE_1_TSRC));
	pLine->isScrollable = false;
	pLine->no_fields = 12;
	pLine->fields[0].type = kModifiable;
	pLine->fields[0].length = 8;
	pLine->fields[0].temp_data = pLine->fields[0].internal_data = 0;
	pLine->fields[0].data_max = 4;
	pLine->fields[0].string_data[0] = "LINESYNC";
	pLine->fields[0].string_data[1] = "RTCSQWR ";
	pLine->fields[0].string_data[2] = "CRYSTAL ";
	pLine->fields[0].string_data[3] = "GPS     ";
	pLine->fields[0].string_data[4] = "NTP     ";
	pLine->fields[1].type = kUnmodifiable;
	pLine->fields[1].length = 4;
	pLine->fields[2].type = kModifiable;
	pLine->fields[2].length = 3;
	pLine->fields[2].data_max = 255;
	pLine->fields[3].type = kUnmodifiable;
	pLine->fields[3].length = 1;
	pLine->fields[4].type = kModifiable;
	pLine->fields[4].length = 3;
	pLine->fields[4].data_max = 255;
	pLine->fields[5].type = kUnmodifiable;
	pLine->fields[5].length = 1;
	pLine->fields[6].type = kModifiable;
	pLine->fields[6].length = 3;
	pLine->fields[6].data_max = 255;
	pLine->fields[7].type = kUnmodifiable;
	pLine->fields[7].length = 1;
	pLine->fields[8].type = kModifiable;
	pLine->fields[8].length = 3;
	pLine->fields[8].data_max = 255;
	pLine->fields[9].type = kUnmodifiable;
	pLine->fields[9].length = 3;
	pLine->fields[10].type = kModifiable;
	pLine->fields[10].length = 3;
	pLine->fields[10].temp_data = pLine->fields[0].internal_data = 0;
	pLine->fields[10].data_max = 4;
	pLine->fields[10].string_data[0] = "N/A";
	pLine->fields[10].string_data[1] = "sp1";
	pLine->fields[10].string_data[2] = "sp2";
	pLine->fields[10].string_data[3] = "sp3";
	pLine->fields[10].string_data[4] = "sp8";
        
	init_remaining_lines(pScreen, 2);

	/* Update the start var in each field of each line of this default screen */
	for (lineNo = 0; lineNo < pScreen->dim_y; lineNo++)
	{
		pScreen->screen_lines[lineNo].fields[0].start = 0;
		for (fieldNo = 1; fieldNo < pScreen->screen_lines[lineNo].no_fields; fieldNo++)
		{
			pScreen->screen_lines[lineNo].fields[fieldNo].start =
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].start +
				pScreen->screen_lines[lineNo].fields[fieldNo - 1].length;
		}
	}

	/* Initialize the Help screen */
	/* TODO: more info may be needed to be displayed here */
 	pScreen = &display_data.screens[HELP_SCREEN_ID]; 
	pScreen->dim_x = HELP_SCREEN_X_SIZE;
	pScreen->dim_y = HELP_SCREEN_Y_SIZE;	
	pScreen->header_dim_y = HELP_SCREEN_HEADER_SIZE;
	pScreen->update = NULL;
	memcpy(pScreen->header[0], HEADER_LINE_0_HELP,
		strlen(HEADER_LINE_0_HELP));
	memcpy(pScreen->footer, FOOTER_LINE_HELP, strlen(FOOTER_LINE_HELP));
	init_one_field_line(pScreen, 0, LINE_0_HELP, false);
	init_one_field_line(pScreen, 1, LINE_1_HELP, true);
	init_one_field_line(pScreen, 2, LINE_2_HELP, true);
	init_one_field_line(pScreen, 3, LINE_3_HELP, true);
	init_one_field_line(pScreen, 4, LINE_4_HELP, true);
	init_one_field_line(pScreen, 5, LINE_5_HELP, true);
	init_remaining_lines(pScreen, 6);

	/* Set the Menu screen as the current screen */
 	display_data.crt_screen = MENU_SCREEN_ID;


	/* Unlock the mutex protecting the variable part of the screens. */
	pthread_mutex_unlock(&display_data.screen_mutex);
 
 }
 
 /* Display screen with ID "screen_no" */
  void display_screen(int screen_no)
  {

 	/* Get screen to be displayed */
  	sc_internal_screen *pCrt_screen = &display_data.screens[screen_no];
 	sc_screen_line *pCrt_line = NULL; /* Current line pointer */
 	int line = 0;

 	int crt_cursor_y = 0;

	 /* Lock the screens and call send_display_change with NO_SCREEN_LOCK as a parameter. */
	pthread_mutex_lock(&display_data.screen_mutex);
	/*printf("%s: screen #%d, y-dim=%d, y-offset=%d\n", __func__, screen_no,
		pCrt_screen->dim_y, pCrt_screen->display_offset_y);*/
/*int i;
for(i=0; i<pCrt_screen->dim_y; i++)
	printf("%*.*s\n", EPRM_SCREEN_X_SIZE, EPRM_SCREEN_X_SIZE, pCrt_screen->screen_lines[i].line);*/
	/* Write the header lines */
	for(line=0;line<pCrt_screen->header_dim_y;line++) {
		send_display_change(0, line,
			CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
		/* Write the line to the screen */
		if (pCrt_screen->header[line] != NULL)
			write(display_data.file_descr, pCrt_screen->header[line], pCrt_screen->dim_x);
	}

	/* Write the screen data */
	crt_cursor_y = pCrt_screen->header_dim_y;
	line = 0;
	while (crt_cursor_y < (g_rows-1)) {
	 	pCrt_line = &pCrt_screen->screen_lines[line+pCrt_screen->display_offset_y];
		/* Set the cursor at the line start */
		send_display_change(0, crt_cursor_y,
			CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
		/* Write the line to the screen */
		if (pCrt_line != NULL)
			write(display_data.file_descr, pCrt_line->line, pCrt_screen->dim_x);
		line++;
		crt_cursor_y++;
 	}

	/* Write the fixed footer at the last line */
	if (pCrt_screen->footer != NULL) {
		send_display_change(0, g_rows-1,
			CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
		write(display_data.file_descr, pCrt_screen->footer,
			pCrt_screen->dim_x);
	}
	
 	//send_display_change(pCrt_screen->cursor_x, crt_cursor_y, CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	pCrt_screen->cursor_x = pCrt_screen->cursor_y = 0;
fpui_set_cursor_pos(display_data.file_descr, /*pCrt_screen->cursor_y*/1, /*pCrt_screen->cursor_x*/1);
fpui_set_cursor(display_data.file_descr, true);

// Set the cursor at the first editable field visible; set to 0,0 or hide if no editable fields

 	/* Set the cursor in the right position, compensating for the vertical scrolling. */
 	crt_cursor_y = pCrt_screen->header_dim_y;
	line = 0;
	while (crt_cursor_y < (g_rows-1)) {
	 	pCrt_line = &pCrt_screen->screen_lines[line+pCrt_screen->display_offset_y];
		int crt_field = -1;
		while ((++crt_field < pCrt_line->no_fields) &&
			(pCrt_line->fields[crt_field].type == kUnmodifiable));
		if (crt_field < pCrt_line->no_fields) /* found a modifiable field */{
			sc_line_field *pCrt_field = &pCrt_line->fields[crt_field];
			pCrt_screen->cursor_y = line+pCrt_screen->display_offset_y;
			pCrt_screen->cursor_x = pCrt_field->start + pCrt_field->length - 1;
/*printf("%s: screen #%d, x=%d, y=%d, %*.*s\n", __func__, screen_no, pCrt_field->start, crt_cursor_y,
	pCrt_field->length,pCrt_field->length,(pCrt_line->line + pCrt_field->start));*/
			send_display_change(pCrt_field->start,
				crt_cursor_y,
				CS_DISPLAY_MAKE_BLINKING,
				(pCrt_line->line + pCrt_field->start), pCrt_field->length,
				NO_SCREEN_LOCK);
			/* set the cursor on the last position */
/*printf("%s: screen #%d, x=%d, y=%d\n", __func__, screen_no, pCrt_screen->cursor_x, crt_cursor_y);*/
			send_display_change(pCrt_screen->cursor_x,
				crt_cursor_y,
				CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			break;
		}
		line++;
		crt_cursor_y++;
 	}
	/*printf("%s: screen #%d, cursor_y=%d, cursor_x=%d\n", __func__, screen_no,
		pCrt_screen->cursor_y, pCrt_screen->cursor_x);*/

 	/* Create the thread that will update the variable screen part
 	 */
 	//pthread_cancel(display_data.update_thread);
 	if (display_data.crt_screen != screen_no) {
 		display_data.crt_screen = screen_no;
 		if ((display_data.screens[screen_no].update != NULL)
 			&& pthread_create(&display_data.update_thread, NULL,
 				(void *)display_data.screens[screen_no].update, &screen_no)) {
 			perror("pthread_create");
 		}
 	}

 	/* Unlock the screens. */
 	pthread_mutex_unlock(&display_data.screen_mutex);
  }


 /* Commit a specified line of a given screen id. 
  * TODO: add error codes and error handling for the commit operation.
  */
 void commit_line(int screen_no, int line_no)
 {
	sc_internal_screen *pCrt_screen = &display_data.screens[screen_no]; /* Sceen to be committed */
	sc_screen_line *pCrt_line = &pCrt_screen->screen_lines[line_no]; /* Current line pointer */
	sc_line_field * pCrt_field; /* Current field of line pointer */
	int field_value = 0; /* Place where to read various field values */


	/* Lock the screen. */
	pthread_mutex_lock(&display_data.screen_mutex);

	switch (screen_no)
	{
	case TIME_SCREEN_ID:
	{
		/* Various time-date formats needed to read/update the time/date in the system. */
		struct tm local_time;
		struct tm * pLocalDateTime;
		time_t t_of_day;
		struct timeval time_sec;
		int local_tz = 0;
		char tzVar[128];
		char *dstVar = &tzVar[54];
		bool time_is_changed = false; /* Flag to indicate if some date/time field is modified. */

		/* For the moment we commit only the modified fields from the date-time line. */
		if (line_no != DATE_TIME_EDIT_LINE) // nothing to commit
			break;

		/* Commit the date-time line. */
		/* Get the system date-time first. */
               	t_of_day = time(NULL);
                pLocalDateTime = localtime(&t_of_day);
		local_time = *pLocalDateTime;

		/* Check if some date-time fields are modified. */

		/* Process the MONTH field. */
		pCrt_field = &pCrt_line->fields[DATE_MONTH_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2))
		{
			sscanf((char *)(pCrt_line->line + pCrt_field->start),
				"%2d", &field_value);
			if ((field_value > 0) && (field_value <= 12))
			{
				local_time.tm_mon = field_value - 1;
				pCrt_field->internal_data = pCrt_field->temp_data;
				time_is_changed = true;
			}
		}

		/* Process the DAY field. */
		pCrt_field = &pCrt_line->fields[DATE_DAY_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2))
                {
                        sscanf((char *)(pCrt_line->line + pCrt_field->start),
                                "%2d", &field_value);
			if ((field_value > 0) && (field_value <= 31))
			{
                        	local_time.tm_mday = field_value;
				pCrt_field->internal_data = pCrt_field->temp_data;
				time_is_changed = true;
			}
                }
		
		/* Process the YEAR field. */
		pCrt_field = &pCrt_line->fields[DATE_YEAR_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2))
                {
                        sscanf((char *)(pCrt_line->line + pCrt_field->start),
                                "%4d", &field_value);
                        if ((field_value >= 1900) && (field_value <= 2050))
			{
                                local_time.tm_year = field_value - 1900;
				pCrt_field->internal_data = pCrt_field->temp_data;
				time_is_changed = true;
			}
                }
		
		/* Process the HOUR field. */
		pCrt_field = &pCrt_line->fields[TIME_HOUR_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2))
                {
                        sscanf((char *)(pCrt_line->line + pCrt_field->start),
                                "%2d", &field_value);
                        if ((field_value >= 0) && (field_value <= 24))
			{
                                local_time.tm_hour = field_value;
				local_time.tm_sec = 0;
				pCrt_field->internal_data = pCrt_field->temp_data;
				time_is_changed = true;
			}
                }

		/* Process the MINUTE field. */
		pCrt_field = &pCrt_line->fields[TIME_MIN_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2))
                {
                        sscanf((char *)(pCrt_line->line + pCrt_field->start),
                                "%2d", &field_value);
                        if ((field_value >= 0) && (field_value <= 60))
			{
                                local_time.tm_min = field_value;
                                /* zero second when changing minute value */
                                local_time.tm_sec = 0;
				pCrt_field->internal_data = pCrt_field->temp_data;
				time_is_changed = true;
			}
                }
		
		if (time_is_changed)
		{
			/* Update the new date-time to the system. */
			t_of_day = mktime(&local_time);

			time_sec.tv_sec = t_of_day;
			time_sec.tv_usec = 0;
			if (settimeofday(&time_sec, NULL) != 0) {
				/* TODO: Handle some commit errors here. */
				break;
			}
		}

		/* See if the timezone/DST settings are changed */
                if (((pCrt_line->fields[ZONE_HOUR_FIELD].type == kModified) ||
                		(pCrt_line->fields[ZONE_HOUR_FIELD].type == kModified2))
                        || ((pCrt_line->fields[ZONE_MIN_FIELD].type == kModified) ||
                        	(pCrt_line->fields[ZONE_MIN_FIELD].type == kModified2))
                        || ((pCrt_line->fields[ZONE_SIGN_FIELD].type == kModified) ||
                                (pCrt_line->fields[ZONE_SIGN_FIELD].type == kModified2))
                        || ((pCrt_line->fields[DST_ENABLE_FIELD].type == kModified) ||
                        	(pCrt_line->fields[DST_ENABLE_FIELD].type == kModified2)))
                {
                        sscanf((char *)(pCrt_line->line + pCrt_line->fields[ZONE_HOUR_FIELD].start),
                                "%2d", &field_value);
                                local_tz = field_value*3600;
                        sscanf((char *)(pCrt_line->line + pCrt_line->fields[ZONE_MIN_FIELD].start),
                                "%2d", &field_value);
                                local_tz += field_value*60;
			// Sanity check the timezone offset
			if ((abs(local_tz) <= (14*60*60)) && (local_tz%(15*60) == 0)) {
				char c;
				sscanf((char *)(pCrt_line->line + pCrt_line->fields[ZONE_SIGN_FIELD].start),
					"%c", &c);
				if (c != SC_INPUT_MINUS)
					local_tz = -local_tz;
				if ( ((pCrt_line->fields[DST_ENABLE_FIELD].type == kModified) ||
					(pCrt_line->fields[DST_ENABLE_FIELD].type == kModified2))
					|| (local_tz != timezone) )
				{
					// test if /usr/share/zoneinfo present and link to matching tzfile if so.
					// else convert to POSIX and write out to /etc/localtime
					sprintf(tzVar,"TZif2");
					*dstVar++ = '\n';
					dstVar += sprintf(dstVar, "ATCST%2.2d:%2.2d:%2.2d",
						(local_tz/3600), ((local_tz%3600)/60),
						((local_tz%3600)%60));
					// cannot infer the dst rule, assume the most likely (usa rule)
					if (pCrt_line->fields[DST_ENABLE_FIELD].temp_data) {
						dstVar += sprintf(dstVar, "ATCDT,M3.2.0,M11.1.0");
					}
					*dstVar++ ='\n';
					FILE* file = fopen(TZCONFIG_FILE, "wb");
					if (file != NULL) {
						fwrite(tzVar, sizeof(char), (dstVar-tzVar), file);
						fclose(file);
					}
				}
			}
			pCrt_line->fields[ZONE_HOUR_FIELD].internal_data = pCrt_line->fields[ZONE_HOUR_FIELD].temp_data;
			pCrt_line->fields[ZONE_MIN_FIELD].internal_data = pCrt_line->fields[ZONE_MIN_FIELD].temp_data;
			pCrt_line->fields[ZONE_SIGN_FIELD].internal_data = pCrt_line->fields[ZONE_SIGN_FIELD].temp_data;
			pCrt_line->fields[DST_ENABLE_FIELD].internal_data = pCrt_line->fields[DST_ENABLE_FIELD].temp_data;
                }
		break;
	}

	case ETH1_SCREEN_ID:
	case ETH2_SCREEN_ID:
	{
		char sh_cmd[400];
		bool eth_if_changed = false, eth_dns_changed = false;
		bool eth_enable = true;
		char *eth_name = (screen_no == ETH1_SCREEN_ID)?"eth0":"eth1";
		int i;

		switch (line_no) {
		case ETH_MODE_LINE:
			pCrt_field = &pCrt_line->fields[ETH_MODE_FIELD];
			if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {

				if (pCrt_field->temp_data) {
					/* Make sure "auto" entry in /etc/network/interfaces */
					sprintf(sh_cmd,	"awk '{if($1==\"auto\"&&$2==\"%s\")"
							"{found=1};if($1==\"iface\"&&$2==\"%s\")"
							"{if(!found){print \"auto %s\"};"
							"print \"iface %s inet %s\"}"
							"else{print $0}}'"
							" /etc/network/interfaces >/tmp/ifs",
							eth_name, eth_name, eth_name, eth_name,
							(pCrt_field->temp_data == 2)?"dhcp":"static");
				} else {
					/* Remove "auto" entry in /etc/network/interfaces */
					sprintf(sh_cmd,	"awk '!($1==\"auto\"&&$2==\"%s\")' "
							"/etc/network/interfaces >/tmp/ifs", eth_name);
					eth_enable = false;
				}
				pCrt_field->internal_data = pCrt_field->temp_data;
				eth_if_changed = true;
			}
			break;
#if 1
		case ETH_IPADDR_LINE:
		case ETH_NETMASK_LINE:
		case ETH_GWADDR_LINE:
			for (i=0; i<4; i++) {
				pCrt_field = &pCrt_line->fields[1 + (2*i)];
				if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
					pCrt_field->internal_data = pCrt_field->temp_data;
					break;
				}
			}
			sprintf(sh_cmd,
				"awk '{if($1==\"iface\"){if($2==\"%s\")"
				"{iface=1;if(match($0, / static/)){static=1;}else{static=0;}}else{iface=0;}print $0;next;}}"
				"{if(iface&&static&&$1==\"address\")"
				"{print \"\t\" \"address %d.%d.%d.%d\";"
				"print \"\t\" \"netmask %d.%d.%d.%d\";"
				"print \"\t\" \"gateway %d.%d.%d.%d\";"
				"print \"\";}"
				"else{if($1==\"iface\"){iface=0;}else{if(!iface){print $0}}};next;}' /etc/network/interfaces >/tmp/ifs", eth_name,
				pCrt_screen->screen_lines[ETH_IPADDR_LINE].fields[ETH_IPADDR1_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_IPADDR_LINE].fields[ETH_IPADDR2_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_IPADDR_LINE].fields[ETH_IPADDR3_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_IPADDR_LINE].fields[ETH_IPADDR4_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_NETMASK_LINE].fields[ETH_NETMASK1_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_NETMASK_LINE].fields[ETH_NETMASK2_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_NETMASK_LINE].fields[ETH_NETMASK3_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_NETMASK_LINE].fields[ETH_NETMASK4_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_GWADDR_LINE].fields[ETH_GWADDR1_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_GWADDR_LINE].fields[ETH_GWADDR2_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_GWADDR_LINE].fields[ETH_GWADDR3_FIELD].internal_data,
				pCrt_screen->screen_lines[ETH_GWADDR_LINE].fields[ETH_GWADDR4_FIELD].internal_data);
			eth_if_changed = true;
			break;
#else
		case ETH_IPADDR_LINE:
			for (i=0; i<4; i++) {
				pCrt_field = &pCrt_line->fields[ETH_IPADDR1_FIELD + (2*i)];
				if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
					// change eth ip address in /etc/network/interfaces
					sprintf(sh_cmd,
						"awk '{if($1==\"iface\"){if($2==\"%s\")"
						"{iface=1;if(match($0, / static/)){static=1;}else{static=0;}}else{iface=0;}print $0;next;}}"
						"{if(iface&&static&&$1==\"address\")"
						"{print \"\t\" \"address %d.%d.%d.%d\";static=0;}"
						"else{print $0};next;}' /etc/network/interfaces >/tmp/ifs", eth_name,
						pCrt_line->fields[ETH_IPADDR1_FIELD].temp_data,
						pCrt_line->fields[ETH_IPADDR2_FIELD].temp_data,
						pCrt_line->fields[ETH_IPADDR3_FIELD].temp_data,
						pCrt_line->fields[ETH_IPADDR4_FIELD].temp_data);
					pCrt_field->internal_data = pCrt_field->temp_data;
					eth_if_changed = true;
					break; /* from loop */
				}
			}
			break;
		case ETH_NETMASK_LINE:
			for (i=0; i<4; i++) {
				pCrt_field = &pCrt_line->fields[ETH_NETMASK1_FIELD + (2*i)];
				if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
					// change eth netmask in /etc/network/interfaces
					sprintf(sh_cmd,
						"awk '{if($1==\"iface\"){if($2==\"%s\")"
						"{iface=1;if(match($0, / static/)){static=1;}else{static=0;}}else{iface=0;}print $0;next;}}"
						"{if(iface&&static&&$1==\"netmask\")"
						"{print \"\t\" \"netmask %d.%d.%d.%d\";static=0;}"
						"else{print $0};next;}' /etc/network/interfaces >/tmp/ifs", eth_name,
						pCrt_line->fields[ETH_NETMASK1_FIELD].temp_data,
						pCrt_line->fields[ETH_NETMASK2_FIELD].temp_data,
						pCrt_line->fields[ETH_NETMASK3_FIELD].temp_data,
						pCrt_line->fields[ETH_NETMASK4_FIELD].temp_data);
					pCrt_field->internal_data = pCrt_field->temp_data;
					eth_if_changed = true;
					break; /* from loop */
				}
			}
			break;
		case ETH_GWADDR_LINE:
			for (i=0; i<4; i++) {
				pCrt_field = &pCrt_line->fields[ETH_GWADDR1_FIELD + (2*i)];
				if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
					// change eth gateway in /etc/network/interfaces
					sprintf(sh_cmd,
						"awk '{if($1==\"iface\"){if($2==\"%s\")"
						"{iface=1;if(match($0, / static/)){static=1;}else{static=0;}}else{iface=0}print $0;next;}}"
						"{if(iface&&static&&$1==\"gateway\")"
						"{print \"\t\" \"gateway %d.%d.%d.%d\";static=0;}"
						"else{print $0};next;}' /etc/network/interfaces >/tmp/ifs", eth_name,
						pCrt_line->fields[ETH_GWADDR1_FIELD].temp_data,
						pCrt_line->fields[ETH_GWADDR2_FIELD].temp_data,
						pCrt_line->fields[ETH_GWADDR3_FIELD].temp_data,
						pCrt_line->fields[ETH_GWADDR4_FIELD].temp_data);
					pCrt_field->internal_data = pCrt_field->temp_data;
					eth_if_changed = true;
					break; /* from loop */
				}
			}
			break;
#endif
		case ETH_NSADDR_LINE:
			for (i=0; i<4; i++) {
				pCrt_field = &pCrt_line->fields[ETH_NSADDR1_FIELD + (2*i)];
				if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
					// change nameserver in /etc/resolv.conf
					sprintf(sh_cmd,
						"awk '{if($1==\"nameserver\")"
						"{print \"nameserver %d.%d.%d.%d\";}else{print $0};next;}'"
						" /etc/resolv.conf >/tmp/dns",
						pCrt_line->fields[ETH_NSADDR1_FIELD].temp_data,
						pCrt_line->fields[ETH_NSADDR2_FIELD].temp_data,
						pCrt_line->fields[ETH_NSADDR3_FIELD].temp_data,
						pCrt_line->fields[ETH_NSADDR4_FIELD].temp_data);
					pCrt_field->internal_data = pCrt_field->temp_data;
					eth_dns_changed = true;
					break; /* from loop */
				}
			}
			break;
		case ETH_HOSTNAME_LINE:
		{
			char hostname[ETH_HOSTNAME_FIELDS + 1];
			bool spc = false, valid = true;
			
			// parse the hostname alpha fields for valid name
			for (i=0;i<ETH_HOSTNAME_FIELDS;i++) {
				char ch = pCrt_line->line[pCrt_line->fields[ETH_HOSTNAME_FIELD1+i].start];
				hostname[i] = ch;
				if ( isalnum(ch) || ((ch == '-') && i) || ((ch == ' ') && i)) {
					if (ch == ' ')
						spc = true;
					if (spc)	
						hostname[i] = '\0';
					continue;
				} else {
					valid = false;
					break;
				}
			}
			if (valid) {
				// write to /etc/hostname
				sprintf(sh_cmd, "sed -i 's/.*/%s/' /etc/hostname", hostname);
				system(sh_cmd);
				// change system hostname
				sprintf(sh_cmd, "/bin/hostname -F /etc/hostname");
				system(sh_cmd);
			}
			for (i=0; i<ETH_HOSTNAME_FIELDS; i++) {
				pCrt_field = &pCrt_line->fields[ETH_HOSTNAME_FIELD1 + i];
				if (valid) {
					if (hostname[i] == '\0')
						pCrt_field->temp_data = 0;
					pCrt_field->internal_data = pCrt_field->temp_data;
				} else
					// not valid, revert change
					pCrt_field->temp_data = pCrt_field->internal_data;
	                        memcpy(pCrt_line->line + pCrt_field->start, pCrt_field->string_data[pCrt_field->temp_data],
	                        	pCrt_field->length);
				pCrt_field->type = kModifiable;
			}
			/* Send change to display. */
			send_display_change(pCrt_line->fields[ETH_HOSTNAME_FIELD1].start,
				pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
				CS_DISPLAY_MAKE_BLINKING,
				&pCrt_line->line[pCrt_line->fields[ETH_HOSTNAME_FIELD1].start],
				ETH_HOSTNAME_FIELDS, NO_SCREEN_LOCK);
			/* Restore cursor */
			send_display_change(pCrt_screen->cursor_x,
				pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
				CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			break;
		}
		default:
			break;
		}

		if (eth_if_changed || eth_dns_changed) {
			/*printf("%s\n",sh_cmd);*/
			system(sh_cmd);
			// stop network interface
			sprintf(sh_cmd, "/sbin/ifdown -f %s", eth_name);
			system(sh_cmd);
			if (eth_if_changed) {
				// rewrite interfaces config file
				sprintf(sh_cmd, "mv /tmp/ifs /etc/network/interfaces");
				system(sh_cmd);
			}
			if (eth_dns_changed) {
				// rewrite resolv.conf file
				sprintf(sh_cmd, "mv /tmp/dns /etc/resolv.conf");
				system(sh_cmd);
			}
			// start network interface (if enabled)
			if (eth_enable) {
				sprintf(sh_cmd, "/sbin/ifup -f %s", eth_name);
				system(sh_cmd);
			}
		}

		break;
	}

	case SRVC_SCREEN_ID:
		pCrt_field = &pCrt_line->fields[SRVC_ENABLE_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
			// rename service file as required
			bool enable = (pCrt_field->temp_data == 1);
			char fmt[8], srvcname[40], oldfname[80], newfname[80];
			pCrt_field = &pCrt_line->fields[SRVC_NAME_FIELD];
			sprintf(fmt, "%%%ds", pCrt_field->length);
                        sscanf((char *)(pCrt_line->line + pCrt_line->fields[SRVC_NAME_FIELD].start),
                                fmt, srvcname);
			sprintf(oldfname, "/etc/init.d/%c%s", enable?'X':'S', srvcname);
			sprintf(newfname, "/etc/init.d/%c%s", enable?'S':'X', srvcname); 
			rename(oldfname, newfname);
			pCrt_field->internal_data = pCrt_field->temp_data;
		printf("set service %s %s\n", srvcname, enable?"on":"off");
			// call service restart/stop?
			//sprintf(cmdline, "%s %s", newfname, enable ? "restart" : "stop");
			//system(cmdline);			
		}
		break;

        case TSRC_SCREEN_ID:
        {
                int tsrc = 0;
                char params[32];
                char cmdstr[80];
                tsrc = tod_get_timesrc();
                pCrt_field = &pCrt_line->fields[TSRC_TSRC_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
                        if ( (tsrc >= 0) && (tsrc != pCrt_field->temp_data)) {
                                // Use "timesrc" utility to make changes
                                if (pCrt_field->temp_data == TOD_TIMESRC_EXTERNAL1) {
                                        int gps_port = pCrt_line->fields[TSRC_PORT_FIELD].internal_data;
                                        if (gps_port == 4)
                                                gps_port = 8;
                                        sprintf(params, "EXTERNAL1 %d", gps_port);
                                } else if (pCrt_field->temp_data == TOD_TIMESRC_EXTERNAL2) {
                                        sprintf(params, "EXTERNAL2 %d.%d.%d.%d",
                                                pCrt_line->fields[TSRC_IPADDR1_FIELD].internal_data,
                                                pCrt_line->fields[TSRC_IPADDR2_FIELD].internal_data,
                                                pCrt_line->fields[TSRC_IPADDR3_FIELD].internal_data,
                                                pCrt_line->fields[TSRC_IPADDR4_FIELD].internal_data);
                                } else
                                        sprintf(params, "%s", pCrt_field->string_data[pCrt_field->temp_data]);
                                printf("Setting timesrc %s\n", params);
                                sprintf(cmdstr, "timesrc %s\n", params);
                                system(cmdstr);
                                printf("Set timesrc %d\n", pCrt_field->temp_data);
                                pCrt_field->internal_data = pCrt_field->temp_data;
                        }
                }
                
                pCrt_field = &pCrt_line->fields[TSRC_PORT_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
                        if (tsrc >= 0) {
                                pCrt_field->internal_data = pCrt_field->temp_data;
                        }
                }
                
                pCrt_field = &pCrt_line->fields[TSRC_IPADDR1_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
                        if (tsrc >= 0) {
                                pCrt_field->internal_data = pCrt_field->temp_data;
                        }
                }
                pCrt_field = &pCrt_line->fields[TSRC_IPADDR2_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
                        if (tsrc >= 0) {
                                pCrt_field->internal_data = pCrt_field->temp_data;
                        }
                }
                pCrt_field = &pCrt_line->fields[TSRC_IPADDR3_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
                        if (tsrc >= 0) {
                                pCrt_field->internal_data = pCrt_field->temp_data;
                        }
                }
                pCrt_field = &pCrt_line->fields[TSRC_IPADDR4_FIELD];
		if ((pCrt_field->type == kModified) || (pCrt_field->type == kModified2)) {
                        if (tsrc >= 0) {
                                pCrt_field->internal_data = pCrt_field->temp_data;
                        }
                }
                
                break;
        }
	case HELP_SCREEN_ID:
		/* Nothing to commit for these screens */
	default:
		break;
	}

	/* Unlock the screen. */
	pthread_mutex_unlock(&display_data.screen_mutex);
 }
 
 /* Set the cursor at the position specified by (crt_cursor_x, crt_cursor_y), updating
  * the value displayed at that position using the parameter "value" and making the cursor blinking or 
  * not depending on the parameter "display_cmd".
  */
 void send_display_change(int crt_cursor_x, int crt_cursor_y, unsigned char display_cmd,
		 unsigned char *value, int field_length, unsigned char lock_screen)
 {
 	/* TODO debug this procedure using the real board */
 	int noBytes = 0;
 	unsigned char pCursor[MAX_CMD_BUFFER_SIZE]; /* Buffer for variable length command */
	unsigned char pCursorStartBlink[MAX_CMD_BUFFER_SIZE] = {0x1b, 0x5b,0x32, 0x35, 0x68};
	unsigned char pCursorStopBlink[MAX_CMD_BUFFER_SIZE] = {0x1b, 0x5b,0x32, 0x35, 0x6c};
	
	/* Set the cursor at the right position 
	 * Note that the cursor positions have to be provided in ASCII starting with 1 (not 0) so we
	 * have to adjust these values by 1 */
	crt_cursor_x++;
	crt_cursor_y++;	

	/* Prepare the format of the cursor's command to set its  x,y coordinates. */
	sprintf((char *)pCursor, "%c%c%u%c%u%c",0x1b,0x5b,(unsigned char)crt_cursor_y,0x3b,(unsigned char)crt_cursor_x,0x66);
	/* The no of bytes in this command depends on the size of crt_cursor_x which could have a digit or two
	 * so compute this number and stored it in "noBytes". */
	//noBytes = (crt_cursor_x >= 10) ? 7 : 6;
	noBytes = strlen((char*)pCursor);
	
	/* Depending on the value of the input parameter "lock_screen", lock the screen before
	 * sending commands to the terminal.
	 */
	if (lock_screen == SCREEN_LOCK)
	{	
		pthread_mutex_lock(&display_data.screen_mutex);
	}

	/* Send the positioning command to the terminal. */
	noBytes = write(display_data.file_descr, pCursor, noBytes);
	
	if (display_cmd == CS_DISPLAY_SET_CURSOR)
	{
		/* Make sure that the cursor does not blink.  */
		write(display_data.file_descr, pCursorStopBlink, 5);
	}
	else if (display_cmd == CS_DISPLAY_MAKE_BLINKING)
	{
		/* Start blinking the char pointed by the cursor, setting the cursor in blinking mode,
		 * rewriting the char after this, undo one char right move of the cursor, and setting
		 * the cursor in non-blinking mode at the end. 
		 */
		write(display_data.file_descr, pCursorStartBlink, 5);
		write(display_data.file_descr, value , field_length);
		noBytes = write(display_data.file_descr, pCursor, noBytes);
		write(display_data.file_descr, pCursorStopBlink, 5); 
	}
	else if (display_cmd == CS_DISPLAY_STOP_BLINKING)
	{
		/* Stop blinking the char pointed by the cursor, setting the cursor in non-blinking
		 * mode, ewriting the char after this, and undo one char right move of the cursor.
		 */
		write(display_data.file_descr, pCursorStopBlink, 5);
		write(display_data.file_descr, value, field_length);
		noBytes = write(display_data.file_descr, pCursor, noBytes);
	}
 
	if (lock_screen == SCREEN_LOCK)
        {
                /* Unlock the screen. */
                pthread_mutex_unlock(&display_data.screen_mutex);
        }

 }

/*
 * Cause front panel alarm bell sound
 */
void send_alarm_bell(void)
{
	char bel = 0x7;
	write(display_data.file_descr, &bel, 1);
}

/* Update the variable part of the screen, i.e., the part of the screen that is changed 
 * by the operating system independent of the user control.
 * This procedure is supposed to run in  another thread.
 */
void update_time_screen(void)
{
	/* Currently the variable part of the screen consists of only Time/Date
	 * since only these fields do change and have to be updated in real time 
	 * when the screen displaying them has the focus.
	 */
	sc_internal_screen * pDateTime_screen = &display_data.screens[TIME_SCREEN_ID];
	sc_screen_line * pDateTimeDisp_line = &(pDateTime_screen->screen_lines[DATE_TIME_DISP_LINE]);
	sc_screen_line * pDateTimeEdit_line = &(pDateTime_screen->screen_lines[DATE_TIME_EDIT_LINE]);
	struct tm *pLocalDateTime;
	time_t t_msec;
	unsigned char buffer[10];
	bool dateTimeUpdate = true;

	/* The saved position of the cursor that has to be restored if screen changes are performed. */
	int saved_cursor_x = CURSOR_NOT_CHANGED;
	int saved_cursor_y = CURSOR_NOT_CHANGED;


	while(display_data.crt_screen == TIME_SCREEN_ID)	/* while we are displayed */
	{
		t_msec = time(NULL);
		pLocalDateTime = localtime(&t_msec);
		
		/* Lock the screen before updates. */
		pthread_mutex_lock(&display_data.screen_mutex);

		if (dateTimeUpdate) {
			dateTimeUpdate = false;
			
		/* Update the current date/time in the internal structures if not modified recently by user. */
		/* Update the TIMEZONE sign field */
		if (((((pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].temp_data != 1) && (timezone <= 0))
			|| ((pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].temp_data != 0) && (timezone > 0)))
				&& (pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].type == kModified2))
		{
			/* If second chance expired, overwrite the change done by the user and make the field
			 * modifiable again.
			 */
			if (pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].temp_data = 
			pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].internal_data = (timezone > 0)? 0:1;
			sprintf( (char *)buffer, "%s", pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].string_data[pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].internal_data]);
			memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].start, buffer, 1);

			/* Display the changes on the terminal if TIME_SCREEN is the current screen. */
			if (display_data.crt_screen == TIME_SCREEN_ID)
			{	/* Saved the initial position of the cursor if this has not been done yet. */
				if (saved_cursor_x == CURSOR_NOT_CHANGED)
				{
					saved_cursor_x = pDateTime_screen->cursor_x;
					saved_cursor_y = pDateTime_screen->cursor_y;
				}

		           	send_display_change(pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].start,
						 	pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                		 /* Write the new zone sign to the screen */
                		write(display_data.file_descr,
					pDateTimeEdit_line->line + pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].start, 1);
			}
		}
		else if  (pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].type == kModified)
		{
			/* Don't change the field, give a second chance to the user to commit his change. */
		 	pDateTimeEdit_line->fields[ZONE_SIGN_FIELD].type = kModified2;
		}

		/* Update the TIMEZONE HOUR field */
		if (((pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].temp_data != (abs(timezone)/3600) ) &&
                       (pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].type == kModified2))
		{
                        /* If second chance expired, overwrite the change done by the user and make the field
                         * modifiable again.
                         */
                        if (pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].temp_data =
			pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].internal_data = (abs(timezone)/3600);
                 	sprintf((char *)buffer, "%02d", pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].internal_data);
                        memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].start, buffer, 2);

                        /* Display the changes on the terminal if TIME_SCREEN is the current screen. */
                        if (display_data.crt_screen == TIME_SCREEN_ID)
                        {       /* Saved the initial position of the cursor if this has not been done yet. */
                                if (saved_cursor_x == CURSOR_NOT_CHANGED)
                                {
                                        saved_cursor_x = pDateTime_screen->cursor_x;
                                        saved_cursor_y = pDateTime_screen->cursor_y;
                                }

                                send_display_change(pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].start,
                                                        pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                 /* Write the new timezone hour to the screen */
                                write(display_data.file_descr,
                                        pDateTimeEdit_line->line + pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].start, 2);
                        }
		}
                else if  (pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].type == kModified)
                {
                        /* Don't change the field, give a second chance to the user to commit his change. */
                        pDateTimeEdit_line->fields[ZONE_HOUR_FIELD].type = kModified2;
                }

		/* Update the TIMEZONE MINUTE field */
		if (((pDateTimeEdit_line->fields[ZONE_MIN_FIELD].temp_data != ((abs(timezone)%3600)/60) ) &&
                       (pDateTimeEdit_line->fields[ZONE_MIN_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[ZONE_MIN_FIELD].type == kModified2))
		{
                        /* If second chance expired, overwrite the change done by the user and make the field
                         * modifiable again.
                         */
                        if (pDateTimeEdit_line->fields[ZONE_MIN_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[ZONE_MIN_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[ZONE_MIN_FIELD].temp_data =
			pDateTimeEdit_line->fields[ZONE_MIN_FIELD].internal_data = ((abs(timezone)%3600)/60);
                        sprintf((char *)buffer, "%02d", pDateTimeEdit_line->fields[ZONE_MIN_FIELD].internal_data);
                        memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[ZONE_MIN_FIELD].start, buffer, 2);

                        /* Display the changes on the terminal if TIME_SCREEN is the current screen. */
                        if (display_data.crt_screen == TIME_SCREEN_ID)
                        {       /* Saved the initial position of the cursor if this has not been done yet. */
                                if (saved_cursor_x == CURSOR_NOT_CHANGED)
                                {
                                        saved_cursor_x = pDateTime_screen->cursor_x;
                                        saved_cursor_y = pDateTime_screen->cursor_y;
                                }

                                send_display_change(pDateTimeEdit_line->fields[ZONE_MIN_FIELD].start,
                                                        pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                 /* Write the new minute to the screen */
                                write(display_data.file_descr,
                                        pDateTimeEdit_line->line + pDateTimeEdit_line->fields[ZONE_MIN_FIELD].start, 2);
                        }
		}
		else if (pDateTimeEdit_line->fields[ZONE_MIN_FIELD].type == kModified)
                {
                        /* Don't change the field, give a second chance to the user to commit his change. */
                        pDateTimeEdit_line->fields[ZONE_MIN_FIELD].type = kModified2;
                }

		/* Update the DST enable field */
		if (((((pDateTimeEdit_line->fields[DST_ENABLE_FIELD].temp_data != 0) && (daylight == 0))
			|| ((pDateTimeEdit_line->fields[DST_ENABLE_FIELD].temp_data != 1) && (daylight > 0)))
				&& (pDateTimeEdit_line->fields[DST_ENABLE_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[DST_ENABLE_FIELD].type == kModified2))
		{
			/* If second chance expired, overwrite the change done by the user and make the field
			 * modifiable again.
			 */
			if (pDateTimeEdit_line->fields[DST_ENABLE_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[DST_ENABLE_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[DST_ENABLE_FIELD].temp_data =
			pDateTimeEdit_line->fields[DST_ENABLE_FIELD].internal_data = (daylight > 0)? 1 :0;
			sprintf( (char *)buffer, "%s", pDateTimeEdit_line->fields[DST_ENABLE_FIELD].string_data[pDateTimeEdit_line->fields[DST_ENABLE_FIELD].internal_data]);
			memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DST_ENABLE_FIELD].start, buffer, 6);

			/* Display the changes on the terminal if TIME_SCREEN is the current screen. */
			if (display_data.crt_screen == TIME_SCREEN_ID)
			{	/* Saved the initial position of the cursor if this has not been done yet. */
				if (saved_cursor_x == CURSOR_NOT_CHANGED)
				{
					saved_cursor_x = pDateTime_screen->cursor_x;
					saved_cursor_y = pDateTime_screen->cursor_y;
				}

		           	send_display_change(pDateTimeEdit_line->fields[DST_ENABLE_FIELD].start,
						 	pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                		 /* Write the new DST enable state to the screen */
                		write(display_data.file_descr,
					pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DST_ENABLE_FIELD].start, 6);
			}
		}
		else if  (pDateTimeEdit_line->fields[DST_ENABLE_FIELD].type == kModified)
		{
			/* Don't change the field, give a second chance to the user to commit his change. */
		 	pDateTimeEdit_line->fields[DST_ENABLE_FIELD].type = kModified2;
		}

		/* Update the YEAR field */
		if (((pDateTimeEdit_line->fields[DATE_YEAR_FIELD].temp_data != (pLocalDateTime->tm_year + 1900))
			&& (pDateTimeEdit_line->fields[DATE_YEAR_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[DATE_YEAR_FIELD].type == kModified2))
		{	
			/* If second chance expired, overwrite the change done by the user and make the field
			 * modifiable again. 
			 */
			if (pDateTimeEdit_line->fields[DATE_YEAR_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[DATE_YEAR_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[DATE_YEAR_FIELD].temp_data =
			pDateTimeEdit_line->fields[DATE_YEAR_FIELD].internal_data = (pLocalDateTime->tm_year + 1900);
			sprintf( (char *)buffer, "%4d", pDateTimeEdit_line->fields[DATE_YEAR_FIELD].internal_data );
			memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DATE_YEAR_FIELD].start, buffer, 4);

			/* Display the changes on the terminal if TIME_SCREEN is the current screen. */
			if (display_data.crt_screen == TIME_SCREEN_ID)
			{	/* Saved the initial position of the cursor if this has not been done yet. */
				if (saved_cursor_x == CURSOR_NOT_CHANGED) 
				{
					saved_cursor_x = pDateTime_screen->cursor_x;
					saved_cursor_y = pDateTime_screen->cursor_y;
				}
		            	
		           	send_display_change(pDateTimeEdit_line->fields[DATE_YEAR_FIELD].start,
						 	pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y, 
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                		 /* Write the new year to the screen */
                		write(display_data.file_descr, 
					pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DATE_YEAR_FIELD].start, 4);
			}
		}
		else if  (pDateTimeEdit_line->fields[DATE_YEAR_FIELD].type == kModified)
		{
			/* Don't change the field, give a second chance to the user to commit his change. */
		 	pDateTimeEdit_line->fields[DATE_YEAR_FIELD].type = kModified2;
		}

		/* Update the MONTH field */
		if (((pDateTimeEdit_line->fields[DATE_MONTH_FIELD].temp_data != (pLocalDateTime->tm_mon+1)) &&
		        (pDateTimeEdit_line->fields[DATE_MONTH_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[DATE_MONTH_FIELD].type == kModified2))
		{
			/* If second chance expired, overwrite the change done by the user and make the field
                         * modifiable again.
                         */
                        if (pDateTimeEdit_line->fields[DATE_MONTH_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[DATE_MONTH_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[DATE_MONTH_FIELD].temp_data =
			pDateTimeEdit_line->fields[DATE_MONTH_FIELD].internal_data = pLocalDateTime->tm_mon + 1;
	                sprintf( (char *)buffer, "%02d", pDateTimeEdit_line->fields[DATE_MONTH_FIELD].internal_data);
                        memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DATE_MONTH_FIELD].start, buffer, 2);

                        /* Display the changes on the terminal if TIME_SCREEN is the current screen. */
                        if (display_data.crt_screen == TIME_SCREEN_ID)
                        {       /* Saved the initial position of the cursor if this has not been done yet. */
                                if (saved_cursor_x == CURSOR_NOT_CHANGED)
                                {
                                        saved_cursor_x = pDateTime_screen->cursor_x;
                                        saved_cursor_y = pDateTime_screen->cursor_y;
                                }

                                send_display_change(pDateTimeEdit_line->fields[DATE_MONTH_FIELD].start,
                                                        pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                 /* Write the month to the screen */
                                write(display_data.file_descr,
                                        pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DATE_MONTH_FIELD].start, 2);
                        }
		}
                else if  (pDateTimeEdit_line->fields[DATE_MONTH_FIELD].type == kModified)
                {
                        /* Don't change the field, give a second chance to the user to commit his change. */
                        pDateTimeEdit_line->fields[DATE_MONTH_FIELD].type = kModified2;
                }

		/* Update the DAY field */ 
		if (((pDateTimeEdit_line->fields[DATE_DAY_FIELD].temp_data != pLocalDateTime->tm_mday) &&
		        (pDateTimeEdit_line->fields[DATE_DAY_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[DATE_DAY_FIELD].type == kModified2))
		{
			/* If second chance expired, overwrite the change done by the user and make the field
                         * modifiable again.
                         */
                        if (pDateTimeEdit_line->fields[DATE_DAY_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[DATE_DAY_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[DATE_DAY_FIELD].temp_data =
			pDateTimeEdit_line->fields[DATE_DAY_FIELD].internal_data = pLocalDateTime->tm_mday;
			sprintf((char *)buffer, "%02d", pDateTimeEdit_line->fields[DATE_DAY_FIELD].internal_data);
                        memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DATE_DAY_FIELD].start, buffer, 2);

                        /* Display the changes on the terminal if TIME_SCREEN is the current screen. */
                        if (display_data.crt_screen == TIME_SCREEN_ID)
                        {       /* Saved the initial position of the cursor if this has not been done yet. */
                                if (saved_cursor_x == CURSOR_NOT_CHANGED)
                                {
                                        saved_cursor_x = pDateTime_screen->cursor_x;
                                        saved_cursor_y = pDateTime_screen->cursor_y;
                                }

                                send_display_change(pDateTimeEdit_line->fields[DATE_DAY_FIELD].start,
                                                        pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                 /* Write the day to the screen */
                                write(display_data.file_descr,
                                        pDateTimeEdit_line->line + pDateTimeEdit_line->fields[DATE_DAY_FIELD].start, 2);
                        }
		}
                else if  (pDateTimeEdit_line->fields[DATE_DAY_FIELD].type == kModified)
                {
                        /* Don't change the field, give a second chance to the user to commit his change. */
                        pDateTimeEdit_line->fields[DATE_DAY_FIELD].type = kModified2;
                }
	
		/* Update the HOUR field */
		if (((pDateTimeEdit_line->fields[TIME_HOUR_FIELD].temp_data != pLocalDateTime->tm_hour) &&
                       (pDateTimeEdit_line->fields[TIME_HOUR_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[TIME_HOUR_FIELD].type == kModified2))
		{
                        /* If second chance expired, overwrite the change done by the user and make the field
                         * modifiable again.
                         */
                        if (pDateTimeEdit_line->fields[TIME_HOUR_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[TIME_HOUR_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[TIME_HOUR_FIELD].temp_data =
			pDateTimeEdit_line->fields[TIME_HOUR_FIELD].internal_data = pLocalDateTime->tm_hour;
                 	sprintf((char *)buffer, "%02d", pDateTimeEdit_line->fields[TIME_HOUR_FIELD].internal_data);
                        memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[TIME_HOUR_FIELD].start, buffer, 2);

                        /* Display the changes on the terminal if TIME_SCREEN is the current screen. */
                        if (display_data.crt_screen == TIME_SCREEN_ID)
                        {       /* Saved the initial position of the cursor if this has not been done yet. */
                                if (saved_cursor_x == CURSOR_NOT_CHANGED)
                                {
                                        saved_cursor_x = pDateTime_screen->cursor_x;
                                        saved_cursor_y = pDateTime_screen->cursor_y;
                                }

                                send_display_change(pDateTimeEdit_line->fields[TIME_HOUR_FIELD].start,
                                                        pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                 /* Write the new year to the screen */
                                write(display_data.file_descr,
                                        pDateTimeEdit_line->line + pDateTimeEdit_line->fields[TIME_HOUR_FIELD].start, 2);
                        }
		}
                else if  (pDateTimeEdit_line->fields[TIME_HOUR_FIELD].type == kModified)
                {
                        /* Don't change the field, give a second chance to the user to commit his change. */
                        pDateTimeEdit_line->fields[TIME_HOUR_FIELD].type = kModified2;
                }

		/* Update the MINUTE field */
		if (((pDateTimeEdit_line->fields[TIME_MIN_FIELD].temp_data != pLocalDateTime->tm_min) &&
                       (pDateTimeEdit_line->fields[TIME_MIN_FIELD].type != kModified))
			|| (pDateTimeEdit_line->fields[TIME_MIN_FIELD].type == kModified2))
		{
                        /* If second chance expired, overwrite the change done by the user and make the field
                         * modifiable again.
                         */
                        if (pDateTimeEdit_line->fields[TIME_MIN_FIELD].type == kModified2)
				pDateTimeEdit_line->fields[TIME_MIN_FIELD].type = kModifiable;

			pDateTimeEdit_line->fields[TIME_MIN_FIELD].temp_data =
			pDateTimeEdit_line->fields[TIME_MIN_FIELD].internal_data = pLocalDateTime->tm_min;
                        sprintf((char *)buffer, "%02d", pDateTimeEdit_line->fields[TIME_MIN_FIELD].internal_data);
                        memcpy(pDateTimeEdit_line->line + pDateTimeEdit_line->fields[TIME_MIN_FIELD].start, buffer, 2);

                        /* Display the changes on the terminal if TIME_SCREEN is the current screen. */
                        if (display_data.crt_screen == TIME_SCREEN_ID)
                        {       /* Saved the initial position of the cursor if this has not been done yet. */
                                if (saved_cursor_x == CURSOR_NOT_CHANGED)
                                {
                                        saved_cursor_x = pDateTime_screen->cursor_x;
                                        saved_cursor_y = pDateTime_screen->cursor_y;
                                }

                                send_display_change(pDateTimeEdit_line->fields[TIME_MIN_FIELD].start,
                                                        pDateTime_screen->header_dim_y+DATE_TIME_EDIT_LINE-pDateTime_screen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                 /* Write the new minute to the screen */
                                write(display_data.file_descr,
                                        pDateTimeEdit_line->line + pDateTimeEdit_line->fields[TIME_MIN_FIELD].start, 2);
                        }
		}
		else if (pDateTimeEdit_line->fields[TIME_MIN_FIELD].type == kModified)
                {
                        /* Don't change the field, give a second chance to the user to commit his change. */
                        pDateTimeEdit_line->fields[TIME_MIN_FIELD].type = kModified2;
                }
		}

		// Update the date/time display line
		char line[MAX_INTERNAL_SCREEN_X_SIZE+1];
		snprintf(line, MAX_INTERNAL_SCREEN_X_SIZE+1,
			"%02d/%02d/%4d %02d:%02d:%02d %c%02d:%02d %s/%s",
			pLocalDateTime->tm_mon+1, pLocalDateTime->tm_mday, pLocalDateTime->tm_year+1900,
			pLocalDateTime->tm_hour, pLocalDateTime->tm_min, pLocalDateTime->tm_sec,
			(timezone > 0)? SC_INPUT_MINUS :SC_INPUT_PLUS, abs(timezone)/3600,
			(abs(timezone)%3600)/60, (daylight > 0)?"Enable":"Disabl",
			(pLocalDateTime->tm_isdst > 0)?"Active":"Inactv");
		memcpy(pDateTimeDisp_line->line, line, MAX_INTERNAL_SCREEN_X_SIZE);
		/* Display the changes on the terminal */
		/* Saved the initial position of the cursor if this has not been done yet. */
		if (saved_cursor_x == CURSOR_NOT_CHANGED) {
			saved_cursor_x = pDateTime_screen->cursor_x;
			saved_cursor_y = pDateTime_screen->cursor_y;
		}
		send_display_change(0, pDateTime_screen->header_dim_y+DATE_TIME_DISP_LINE-pDateTime_screen->display_offset_y,
			CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
		write(display_data.file_descr, pDateTimeDisp_line->line, MAX_INTERNAL_SCREEN_X_SIZE);
#if 0
		if( dateTimeUpdate ) {
			//update edit line date/time once only
			dateTimeUpdate = false;
			memcpy(pDateTimeEdit_line->line, line,
				pDateTimeEdit_line->fields[DST_ENABLE_FIELD].start +
				pDateTimeEdit_line->fields[DST_ENABLE_FIELD].length);
			send_display_change(0, DATE_TIME_EDIT_LINE - pDateTime_screen->display_offset_y,
				CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			write(display_data.file_descr, pDateTimeEdit_line->line, MAX_INTERNAL_SCREEN_X_SIZE);
		}
#endif
		/* Restore the saved cursor position if screen updates have been made. */
		if (saved_cursor_x != CURSOR_NOT_CHANGED)
		{
			send_display_change(saved_cursor_x, pDateTime_screen->header_dim_y+saved_cursor_y,
                                              CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			saved_cursor_x = CURSOR_NOT_CHANGED;
			/* TODO: make the cursor blinking if positioned on a modifiable char. */

		}

		/* Unlock the screen after updates. */
		pthread_mutex_unlock(&display_data.screen_mutex);

		/* Sleep for 1 sec before the new updates. */
		sleep(1/*60*/);
	}
}


struct eth_dev_stats {
        unsigned long long rx_packets;  /* total packets received       */
        unsigned long long tx_packets;  /* total packets transmitted    */
        unsigned long long rx_bytes;    /* total bytes received         */
        unsigned long long tx_bytes;    /* total bytes transmitted      */
        unsigned long rx_errors;        /* bad packets received         */
        unsigned long tx_errors;        /* packet transmit problems     */
        unsigned long rx_dropped;       /* no space in linux buffers    */
        unsigned long tx_dropped;       /* no space available in linux  */
        unsigned long rx_multicast;     /* multicast packets received   */
        unsigned long rx_compressed;
        unsigned long tx_compressed;
        unsigned long collisions;

        /* detailed rx_errors: */
        unsigned long rx_length_errors;
        unsigned long rx_over_errors;   /* receiver ring buff overflow  */
        unsigned long rx_crc_errors;    /* recved pkt with crc error    */
        unsigned long rx_frame_errors;  /* recv'd frame alignment error */
        unsigned long rx_fifo_errors;   /* recv'r fifo overrun          */
        unsigned long rx_missed_errors; /* receiver missed packet     */
        /* detailed tx_errors */
        unsigned long tx_aborted_errors;
        unsigned long tx_carrier_errors;
        unsigned long tx_fifo_errors;
        unsigned long tx_heartbeat_errors;
        unsigned long tx_window_errors;
};

/* Update the variable part of the screen, i.e., the part of the screen that is changed
 * by the operating system independent of the user control.
 * This procedure is supposed to run in  another thread.
 */
void update_ethernet_screen(void *arg)
{
	int *screen_id = arg, eth_screen_id = *screen_id;
	sc_internal_screen *pEthernet_screen = &display_data.screens[eth_screen_id];
	sc_screen_line *pEthEditLine;
	sc_line_field *pField;
	char buffer[32], *match;
	char *eth_name = (eth_screen_id == ETH1_SCREEN_ID)?"eth0":"eth1";
	struct ifreq ifr;
	int skfd;

	/* The saved position of the cursor that has to be restored if screen changes are performed. */
	int saved_cursor_x = CURSOR_NOT_CHANGED;
	int saved_cursor_y = CURSOR_NOT_CHANGED;

	skfd = socket(AF_INET, SOCK_DGRAM, 0);

	while(display_data.crt_screen == eth_screen_id)	/* while we are displayed */
	{
		char ip_str[24];
	        int octet;
	        struct in_addr gw_addr;
	        char rt_str[256];
	        int rt_flgs;

		/* Lock the screen before updates. */
		pthread_mutex_lock(&display_data.screen_mutex);
		/*
		 *  Interface Mode Setting
		 */
		pEthEditLine = &(pEthernet_screen->screen_lines[ETH_MODE_LINE]);
	        pField = &(pEthEditLine->fields[ETH_MODE_FIELD]);
		strncpy(ifr.ifr_name, eth_name, IFNAMSIZ);
		if( ioctl(skfd, SIOCGIFFLAGS, &ifr) == 0 ) {
			if( (((((ifr.ifr_flags & IFF_UP) == 0) && (pField->temp_data > 0))
				|| ((ifr.ifr_flags & IFF_UP) && (pField->temp_data == 0)))
					&& (pField->type != kModified))
						|| (pField->type == kModified2) )
			{

				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = 0;
				if (ifr.ifr_flags & IFF_UP) {
					match = (eth_screen_id == ETH1_SCREEN_ID)?"eth0 inet dhcp":"eth1 inet dhcp";
					if (get_data_from_file("/etc/network/interfaces", match, rt_str, 256) > 0)
						pField->temp_data = pField->internal_data = 2;
					else
						pField->temp_data = pField->internal_data = 1;
				}
				sprintf( (char *)buffer, "%s", pField->string_data[pField->internal_data]);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);

				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
							pEthernet_screen->header_dim_y+ETH_MODE_LINE-pEthernet_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}
		}
		/*
		 *  IP Address
		 */
		strncpy(ifr.ifr_name, eth_name, IFNAMSIZ);
		ifr.ifr_addr.sa_family = AF_INET;
		if( ioctl(skfd, SIOCGIFADDR, &ifr) == 0 ) {
		        snprintf(ip_str, 16, "%s",
		        		inet_ntoa(((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr));
			pEthEditLine = &(pEthernet_screen->screen_lines[ETH_IPADDR_LINE]);
		        pField = &(pEthEditLine->fields[ETH_IPADDR1_FIELD]);
		        octet = atoi(strtok(ip_str,"."));
		        if ( ((octet != pField->temp_data)
		        		&& (pField->type != kModified))
		        			|| (pField->type == kModified2) )
		        {
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
							pEthernet_screen->header_dim_y+ETH_IPADDR_LINE-pEthernet_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
		        }
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_IPADDR2_FIELD]);
		        octet = atoi(strtok(NULL, "."));
		        if ( ((octet != pField->temp_data)
		        		&& (pField->type != kModified))
		        			|| (pField->type == kModified2) )
		        {
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
							pEthernet_screen->header_dim_y+ETH_IPADDR_LINE-pEthernet_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
		        }
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_IPADDR3_FIELD]);
		        octet = atoi(strtok(NULL, "."));
		        if ( ((octet != pField->temp_data)
		        		&& (pField->type != kModified))
		        			|| (pField->type == kModified2) )
		        {
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
							pEthernet_screen->header_dim_y+ETH_IPADDR_LINE-pEthernet_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
		        }
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_IPADDR4_FIELD]);
		        octet = atoi(strtok(NULL,"."));
		        if ( ((octet != pField->temp_data)
		        		&& (pField->type != kModified))
		        			|| (pField->type == kModified2) )
		        {
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
							pEthernet_screen->header_dim_y+ETH_IPADDR_LINE-pEthernet_screen->display_offset_y,
							CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
		        }
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}
		}
		/*
		 *  Netmask
		 */
		strncpy(ifr.ifr_name, eth_name, IFNAMSIZ);
		if( ioctl(skfd, SIOCGIFNETMASK, &ifr) == 0 ) {
			snprintf(ip_str, 16, "%s",
					inet_ntoa(((struct sockaddr_in *)(&ifr.ifr_netmask))->sin_addr) );
			pEthEditLine = &(pEthernet_screen->screen_lines[ETH_NETMASK_LINE]);
		        pField = &(pEthEditLine->fields[ETH_NETMASK1_FIELD]);
		        octet = atoi(strtok(ip_str,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NETMASK_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_NETMASK2_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
					&& (pField->type != kModified))
						|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NETMASK_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_NETMASK3_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
					&& (pField->type != kModified))
						|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NETMASK_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_NETMASK4_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
					&& (pField->type != kModified))
						|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}

			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NETMASK_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}
		}
		/*
		 * Default gateway address.
		 */
		match = (eth_screen_id == ETH1_SCREEN_ID)?"eth0\t00000000\t":"eth1\t00000000\t";
		if ( (get_data_from_file("/proc/net/route", match, rt_str, 256) > 0)
			&& (sscanf(&rt_str[14],"%lx%X%*[^\n]\n", (unsigned long *)&gw_addr.s_addr, &rt_flgs) == 2)
			&& (rt_flgs & (RTF_UP|RTF_GATEWAY)) ) {

			snprintf(ip_str,16,"%s", inet_ntoa(gw_addr));
			pEthEditLine = &(pEthernet_screen->screen_lines[ETH_GWADDR_LINE]);
		        pField = &(pEthEditLine->fields[ETH_GWADDR1_FIELD]);
		        octet = atoi(strtok(ip_str,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_GWADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_GWADDR2_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_GWADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_GWADDR3_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_GWADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_GWADDR4_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_GWADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}
		}

		/*
		 * DNS Nameserver address.
		 */
		// Parse /etc/resolv.conf for (first) nameserver entry
		memset(rt_str, 0, sizeof(rt_str));
		if ( (get_data_from_file("/etc/resolv.conf", "nameserver", rt_str, 256) > 0)
			&& (sscanf(rt_str, "%*s %s", ip_str) == 1) ) {
			pEthEditLine = &(pEthernet_screen->screen_lines[ETH_NSADDR_LINE]);
		        pField = &(pEthEditLine->fields[ETH_NSADDR1_FIELD]);
		        octet = atoi(strtok(ip_str,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NSADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_NSADDR2_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NSADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_NSADDR3_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NSADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}

		        pField = &(pEthEditLine->fields[ETH_NSADDR4_FIELD]);
		        octet = atoi(strtok(NULL,"."));
			if ( ((octet != pField->temp_data)
			        	&& (pField->type != kModified))
			        		|| (pField->type == kModified2) )
			{
				if (pField->type == kModified2)
					pField->type = kModifiable;
				pField->temp_data = pField->internal_data = octet;
				sprintf(buffer, "%3d", octet);
				memcpy(pEthEditLine->line + pField->start, buffer, pField->length);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
				if (display_data.crt_screen == eth_screen_id)
				{	/* Saved the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED)
					{
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
			           	send_display_change(pField->start,
						pEthernet_screen->header_dim_y+ETH_NSADDR_LINE-pEthernet_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		 /* Write the new zone sign to the screen */
	                		write(display_data.file_descr,
						pEthEditLine->line + pField->start, pField->length);
				}
			}
			else if  (pField->type == kModified)
			{
				/* Don't change the field, give a second chance to the user to commit his change. */
			 	pField->type = kModified2;
			}
		}

		/*
		 * Interface statistics from /proc/net/dev.
		 */
		struct eth_dev_stats stats;
		memset(rt_str, 0, sizeof(rt_str));
		match = (eth_screen_id == ETH1_SCREEN_ID)?"eth0:":"eth1:";
		if (get_data_from_file("/proc/net/dev", match, rt_str, 256) > 0) {
#if INT_MAX == LONG_MAX
			match = "%*s%llu%llu%u%u%u%u%u%u%llu%llu%u%u%u%u%u%u";
#else
			match = "%*s%llu%llu%lu%lu%lu%lu%lu%lu%llu%llu%lu%lu%lu%lu%lu%lu"
#endif
			int count = 0;
			if ((count = sscanf(rt_str, match,
					&stats.rx_bytes, &stats.rx_packets, &stats.rx_errors,
					&stats.rx_dropped, &stats.rx_fifo_errors, &stats.rx_frame_errors,
					&stats.rx_compressed, &stats.rx_multicast,
					&stats.tx_bytes, &stats.tx_packets, &stats.tx_errors,
					&stats.tx_dropped, &stats.tx_fifo_errors,
					&stats.collisions, &stats.tx_carrier_errors, &stats.tx_compressed)) == 16) {
				char line[MAX_INTERNAL_SCREEN_X_SIZE+1];
				pEthEditLine = &(pEthernet_screen->screen_lines[ETH_TX_LINE]);
				snprintf(line, MAX_INTERNAL_SCREEN_X_SIZE+1,
					"Packets Sent GD:%10llu BD:%10lu", stats.tx_packets, stats.tx_errors);
				memcpy(pEthEditLine->line, line, MAX_INTERNAL_SCREEN_X_SIZE);
				pEthEditLine = &(pEthernet_screen->screen_lines[ETH_RX_LINE]);
				snprintf(line, MAX_INTERNAL_SCREEN_X_SIZE+1,
					"Packets Rcvd GD:%10llu BD:%10lu", stats.rx_packets, stats.rx_errors);
				memcpy(pEthEditLine->line, line, MAX_INTERNAL_SCREEN_X_SIZE);
				/* Display the changes on the terminal if ETH_SCREEN is the current screen. */
#if 0
		//only if line is visible on external display!
				if (display_data.crt_screen == eth_screen_id)
				{	/* Save the initial position of the cursor if this has not been done yet. */
					if (saved_cursor_x == CURSOR_NOT_CHANGED) {
						saved_cursor_x = pEthernet_screen->cursor_x;
						saved_cursor_y = pEthernet_screen->cursor_y;
					}
					send_display_change(0, ETH_TX_LINE - pEthernet_screen->display_offset_y,
								CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
					write(display_data.file_descr, pEthernet_screen->screen_lines[ETH_TX_LINE].line,
							MAX_INTERNAL_SCREEN_X_SIZE);
			           	send_display_change(0, ETH_RX_LINE - pEthernet_screen->display_offset_y,
								CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
	                		write(display_data.file_descr, pEthernet_screen->screen_lines[ETH_RX_LINE].line,
	                				MAX_INTERNAL_SCREEN_X_SIZE);
				}
#endif
			}
		}

		/* Restore the saved cursor position if screen updates have been made. */
		if (saved_cursor_x != CURSOR_NOT_CHANGED)
		{
			send_display_change(saved_cursor_x, pEthernet_screen->header_dim_y+saved_cursor_y,
                                              CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			saved_cursor_x = CURSOR_NOT_CHANGED;
			/* TODO: make the cursor blinking if positioned on a modifiable char. */
		}

		/* Unlock the screen after updates. */
		pthread_mutex_unlock(&display_data.screen_mutex);

		/* Sleep for 5 sec before the new updates. */
		sleep(5/*60*/);
	}

	close (skfd);
}

void update_linux_screen(void *arg)
{
	int *screen_id = arg, lnx_screen_id = *screen_id;
	sc_internal_screen *pLinux_screen = &display_data.screens[lnx_screen_id];
	char buffer[32];
	int count, updays, uphours, upmins;
	struct sysinfo sys_info;
        struct statfs stat_fs;

	/* The saved position of the cursor that has to be restored if screen changes are performed. */
	int saved_cursor_x = CURSOR_NOT_CHANGED;
	int saved_cursor_y = CURSOR_NOT_CHANGED;

	while(display_data.crt_screen == lnx_screen_id)	/* while we are displayed */
	{
		/* Lock the screen before updates. */
		pthread_mutex_lock(&display_data.screen_mutex);
		if (sysinfo(&sys_info) == 0) {
			/* update system uptime */
			updays = sys_info.uptime/(60*60*24),
			uphours = (sys_info.uptime/(60*60))%24,
			upmins = (sys_info.uptime/60)%60;
			count = snprintf(buffer, 34, "%d day%s, %2d hour%s, %2d min%s",
				updays, ((updays!=1)?"s":""), uphours, ((uphours!=1)?"s":""),
				upmins, ((upmins!=1)?"s":"") );
			memcpy(&pLinux_screen->screen_lines[LNX_UPT_LINE].line[8], buffer, count);
			/* Save the initial position of the cursor if this has not been done yet. */
			if (saved_cursor_x == CURSOR_NOT_CHANGED) {
				saved_cursor_x = pLinux_screen->cursor_x;
				saved_cursor_y = pLinux_screen->cursor_y;
			}
			send_display_change(0, pLinux_screen->header_dim_y+LNX_UPT_LINE-pLinux_screen->display_offset_y,
				CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			/* Write the new data to the screen */
			write(display_data.file_descr, pLinux_screen->screen_lines[LNX_UPT_LINE].line, MAX_INTERNAL_SCREEN_X_SIZE);
			/* update Load Average */
			count = get_data_from_file("/proc/loadavg", NULL, buffer, 26);
			memcpy((char *)&pLinux_screen->screen_lines[LNX_AVG_LINE].line[14], buffer, count);
			send_display_change(0, pLinux_screen->header_dim_y+LNX_AVG_LINE-pLinux_screen->display_offset_y,
				CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			/* Write the new data to the screen */
			write(display_data.file_descr, pLinux_screen->screen_lines[LNX_AVG_LINE].line, MAX_INTERNAL_SCREEN_X_SIZE);
                        /* update Free Memory */
               		count = snprintf(buffer, 32, "%4dMB", (int)((sys_info.freeram*sys_info.mem_unit)>>20));
                        memcpy((char *)&pLinux_screen->screen_lines[LNX_MEM_LINE].line[27], buffer, count);
			send_display_change(0, pLinux_screen->header_dim_y+LNX_MEM_LINE-pLinux_screen->display_offset_y,
				CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			/* Write the new data to the screen */
			write(display_data.file_descr, pLinux_screen->screen_lines[LNX_MEM_LINE].line, MAX_INTERNAL_SCREEN_X_SIZE);
                        /* update Free Storage on rootfs */
                        if (statfs("/", &stat_fs) == 0) {
                                count = snprintf(buffer, 32, "%4dMB", (int)((stat_fs.f_bavail*stat_fs.f_bsize)>>20));
                                memcpy((char *)&pLinux_screen->screen_lines[LNX_FS_LINE].line[31], buffer, count);                
                                send_display_change(0, pLinux_screen->header_dim_y+LNX_FS_LINE-pLinux_screen->display_offset_y,
                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                /* Write the new data to the screen */
                                write(display_data.file_descr, pLinux_screen->screen_lines[LNX_FS_LINE].line, MAX_INTERNAL_SCREEN_X_SIZE);
                        }

			/* restore cursor position */
			if (saved_cursor_x != CURSOR_NOT_CHANGED) {
				send_display_change(saved_cursor_x, pLinux_screen->header_dim_y+saved_cursor_y, CS_DISPLAY_SET_CURSOR, 0, 0,
					NO_SCREEN_LOCK);
				saved_cursor_x = CURSOR_NOT_CHANGED;
			}
		}

		/* Unlock the screen after updates. */
		pthread_mutex_unlock(&display_data.screen_mutex);

		/* Sleep for 5 sec before the new updates. */
		sleep(5);
	}
}

void update_eeprom_screen(void *arg)
{
	int *screen_id = arg, eprm_screen_id = *screen_id;
	sc_internal_screen *pScreen = &display_data.screens[eprm_screen_id];
	char buffer[32];
	char eeprom[256];
	int fd, eprm_sz = 0;
	
	/* update eeprom content from current state before display */
	sprintf(buffer, "/usr/bin/eeprom -u");
	system(buffer);
	
	if ((fd = open("/dev/eeprom", O_RDONLY)) != -1) {
		eprm_sz = read(fd, eeprom, 256);
		close(fd);	
	}

	int i = 0, j = 0, count = 0, line_no = 0, byteCount = 0;
	uint16_t sz = 0;
	if (eprm_sz >= 256) {
		/* parse eeprom version */
		byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Host EEPROM Version: %d", eeprom[j++]);
		memcpy(pScreen->screen_lines[line_no++].line, buffer, byteCount);
		/* parse eeprom size */
		sz = *(uint16_t *)&eeprom[j];
		j += 2;
		byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Host EEPROM Size (bytes): %d", sz);
		memcpy(pScreen->screen_lines[line_no++].line, buffer, byteCount);
	}
	/* if version is ATC Standard v6, and not 2070 default, and size < eprm_sz, and  crc is good ... */
	/* (assume size value is from eprm start to end of CRC, so crc is at eeprom[size-2] */
	if ((eeprom[0] == 2) && sz && (sz <= eprm_sz) /*&& (crc16(eeprom, &eeprom[sz-2]))*/) {
		count = eeprom[j++];
		byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "#Modules: %d", count);
		memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
		/* for each module */
		for(i=0; i<count; i++) {
			/* module N location */
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Mod %d Location: %d", i+1, eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* module N make */
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Mod %d Make: %d", i+1, eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* module N Model (octet string) */
			byteCount = eeprom[j];
			if (byteCount > 27)
				byteCount = 27;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Mod %d Model: %*.*s", i+1,
					byteCount, byteCount, &eeprom[j+1]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			j += 1 + eeprom[j];
			if (j > 255)
				break;
			/* module N version (BCD) */
			char eprm_ver[15];
			int ii;
			for(ii=0; ii<7; ii++, j++) {
				eprm_ver[ii*2] = ((eeprom[j]>>4)&0xf) + 0x30;
				eprm_ver[(ii*2)+1] = (eeprom[j]&0xf) + 0x30;
			}
			eprm_ver[14] = '\0';
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Mod %d Version: %s", i+1, eprm_ver);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* module N type */
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Mod %d Type: %d", i+1, eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
		}
		if( j<255) {
			/* Display Properties */
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Display #Char Lines: %d", eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Display #Char Columns: %d", eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Display #Graphic Rows-1 (ymax): %d", eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			uint16_t *gfx_cols = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Display #Graphic Cols-1 (xmax): %d", *gfx_cols);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Ethernets */
			count = eeprom[j++];
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "#Ethernets: %d", count);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			for(i=0; i<count; i++) {
				/* Ethernet N Type */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Enet %d Type: %d", i+1, eeprom[j++]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				/* Ethernet N IP Address */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Enet %d IP Address:       %d.%d.%d.%d", i+1,
						eeprom[j], eeprom[j+1], eeprom[j+2], eeprom[j+3]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				j += 4;
				/* Ethernet N Switch/Router MAC */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Enet %d Switch/Router MAC Address:", i+1);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "       %0X:%0X:%0X:%0X:%0X:%0X",
						eeprom[j], eeprom[j+1], eeprom[j+2], eeprom[j+3], eeprom[j+4], eeprom[j+5]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				j += 6;
				/* Ethernet N netmask */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Enet %d Subnet Mask:      %d.%d.%d.%d",
						i+1, eeprom[j], eeprom[j+1], eeprom[j+2], eeprom[j+3]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				j += 4;
				/* Ethernet N default gateway */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Enet %d Default Gateway:  %d.%d.%d.%d",
						i+1, eeprom[j], eeprom[j+1], eeprom[j+2], eeprom[j+3]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				j += 4;
				/* Ethernet N interface type */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Enet %d Engine Board Interface: %d",
						i+1, eeprom[j++]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			}
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "SPI3 Purpose: %d", eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "SPI4 Purpose: %d", eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Host Board Serial Ports Used: %0X %0X", eeprom[j], eeprom[j+1]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			j += 2;
			int count = eeprom[j++];
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "#Ports Used For I/O: %d", count);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			for(i=0; i<count; i++) {
				/* Port N ID */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Port %d ID: %d", i+1, eeprom[j++]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				/* Port N Mode */
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Port %d Mode: %d", i+1, eeprom[j++]);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
				/* Port N Baudrate */
				uint32_t *baudrate = (uint32_t *)&eeprom[j];
				j += 4;
				byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Port %d Baud Rate: %d", i+1, *baudrate);
				memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			}
			/* Host Board Serial Ports Present */
			j += 2;
			/* Serial Bus #1 Port */
			uint16_t *port_no = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Serial Bus #1 Port: %d", *port_no);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Serial Bus #2 Port */
			port_no = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Serial Bus #2 Port: %d", *port_no);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* TS2 Port */
			port_no = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "TS2 Port 1 Port: %d", *port_no);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Expansion bus type */
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Expansion Bus Type: %d", eeprom[j++]);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Encoded SPI Addressing */
			j++;
			/* CRC #1 */
			uint16_t *crc_16 = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "CRC #1 (16-bit): %d", *crc_16);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Latitude from Datakey */
			float *degrees = (float *)&eeprom[j];
			j += 4;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Latitude:  %f", *degrees);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Longitude from Datakey */
			degrees = (float *)&eeprom[j];
			j += 4;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Longitude: %f", *degrees);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Controller ID from Datakey */
			uint16_t *temp = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Controller ID: %d", *temp);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Comm drop address from Datakey */
			temp = (uint16_t *)&eeprom[j];
			j += 2;
			byteCount = snprintf(buffer, EPRM_SCREEN_X_SIZE, "Communication Drop#: %d", *temp);
			memcpy((char *)pScreen->screen_lines[line_no++].line, buffer, byteCount);
			/* Agency Reserved Bytes from Datakey */
		}
		if (line_no > EPRM_SCREEN_Y_SIZE) {
			pScreen->dim_y = line_no;
		}
		for(i=0; i<pScreen->dim_y; i++) {
			pScreen->screen_lines[i].isScrollable = true;
			pScreen->screen_lines[i].no_fields = 1;
			pScreen->screen_lines[i].fields[0].type = kUnmodifiable;
			pScreen->screen_lines[i].fields[0].length = EPRM_SCREEN_X_SIZE;
			pScreen->screen_lines[i].fields[0].start = 0;
		}
	}
}

#if 0
void update_service_screen(void *arg)
{
	int *screen_id = arg, srvc_screen_id = *screen_id;
	sc_internal_screen *pScreen = &display_data.screens[srvc_screen_id];
	char buffer[32];

	/* The saved position of the cursor that has to be restored if screen changes are performed. */
	int saved_cursor_x = CURSOR_NOT_CHANGED;
	int saved_cursor_y = CURSOR_NOT_CHANGED;

	while(display_data.crt_screen == srvc_screen_id)	/* while we are displayed */
	{
		/* Lock the screen before updates. */
		pthread_mutex_lock(&display_data.screen_mutex);

		
		/* Unlock the screen after updates. */
		pthread_mutex_unlock(&display_data.screen_mutex);

		/* Sleep for 30 sec before the new updates. */
		sleep(30);
	}
}
#endif
void update_timesrc_screen(void)
{
	sc_internal_screen *pScreen = &display_data.screens[TSRC_SCREEN_ID];
	sc_screen_line *pEditLine, *pCrt_line;
	sc_line_field *pField, *pCrt_field;
	char buffer[32];
        int tsrc;

	/* The saved position of the cursor that has to be restored if screen changes are performed. */
	int crt_cursor_x, saved_cursor_x = CURSOR_NOT_CHANGED;
	int crt_cursor_y, saved_cursor_y = CURSOR_NOT_CHANGED;
        int crt_field = 0;

	while(display_data.crt_screen == TSRC_SCREEN_ID)	/* while we are displayed */
	{
		/* Lock the screen before updates. */
		pthread_mutex_lock(&display_data.screen_mutex);

		pEditLine = &(pScreen->screen_lines[TSRC_TSRC_LINE]);
	        pField = &(pEditLine->fields[TSRC_TSRC_FIELD]);
                
                if ( (tsrc = tod_get_timesrc()) >= 0) {
                        /* Is the tsrc field value different from what is displayed? */
                        if ( ((tsrc != pField->temp_data) && (pField->type != kModified))
					|| (pField->type == kModified2) )
                        {
                                if (pField->type == kModified2)
                                        pField->type = kModifiable;
                                
                                pField->temp_data = pField->internal_data = tsrc;
                                sprintf( (char *)buffer, "%s", pField->string_data[pField->internal_data]);
                                memcpy(pEditLine->line + pField->start, buffer, pField->length);
                                /* Display the changes on the terminal if TSRC_SCREEN is the current screen. */
                                if (display_data.crt_screen == TSRC_SCREEN_ID)
                                {	/* Save the initial position of the cursor if this has not been done yet. */
                                        if (saved_cursor_x == CURSOR_NOT_CHANGED) {
                                                saved_cursor_x = pScreen->cursor_x;
                                                saved_cursor_y = pScreen->cursor_y;
                                        }
                                        send_display_change(pField->start,
                                                        pScreen->header_dim_y+TSRC_TSRC_LINE-pScreen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                        /* Write the new value to the screen */
                                        write(display_data.file_descr,
                                                pEditLine->line + pField->start, pField->length);
                                }
                        }
                        else if  (pField->type == kModified) {
                                /* Don't change the field, give a second chance to the user to commit his change. */
                                pField->type = kModified2;
                        }
                        /* enable/disable  modification of other fields according to current timesrc ? */
                        /* Is the NTP peer field value different from what is displayed? */
                        /* Show NTP peer (if any) */
                        if (tsrc == TOD_TIMESRC_EXTERNAL2) {
                                char peer_str[80];
                                int peer_addr[4];
                                if ( (get_data_from_file("/etc/ntp.conf", "iburst", peer_str, 80) > 0)
                                        && (sscanf(peer_str, "server %d.%d.%d.%d", &peer_addr[0], &peer_addr[1], &peer_addr[2], &peer_addr[3]) == 4) ) {
                                        pEditLine->fields[TSRC_IPADDR1_FIELD].internal_data = peer_addr[0];
                                        pEditLine->fields[TSRC_IPADDR2_FIELD].internal_data = peer_addr[1];
                                        pEditLine->fields[TSRC_IPADDR3_FIELD].internal_data = peer_addr[2];
                                        pEditLine->fields[TSRC_IPADDR4_FIELD].internal_data = peer_addr[3];
                                        sprintf(peer_str, "%3d.%3d.%3d.%3d",
                                                peer_addr[0], peer_addr[1], peer_addr[2], peer_addr[3]);
                                        printf("New NTP Peer %s\n", peer_str);
                                        memcpy(&pEditLine->line[pEditLine->fields[TSRC_IPADDR1_FIELD].start],
                                                peer_str, strlen(peer_str));
                                        if (saved_cursor_x == CURSOR_NOT_CHANGED) {
                                                saved_cursor_x = pScreen->cursor_x;
                                                saved_cursor_y = pScreen->cursor_y;
                                        }
                                        pField = &(pEditLine->fields[TSRC_IPADDR1_FIELD]);
                                        send_display_change(pField->start,
                                                        pScreen->header_dim_y+TSRC_TSRC_LINE-pScreen->display_offset_y,
                                                        CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                        /* Write the new value to the screen */
                                        write(display_data.file_descr,
                                                pEditLine->line + pField->start, pField->length);
                                }
                        }
                        
                        /* Is the GPS Port field value different from what is displayed? */
                        /* Show GPS serial port (if any) */
                        if (tsrc == TOD_TIMESRC_EXTERNAL1) {
                                char buf[16];
                                int gps_dev = 0;
                                //find link of /dev/gps0
                                if ( (readlink("/dev/gps0", buf, sizeof(buf)) > 0)
                                        && (sscanf(buf, "/dev/sp%d", &gps_dev) == 1) ) {
                                        switch (gps_dev) {
                                        case 1: case 2: case 3: case 8:
                                                break;
                                        default:
                                                gps_dev = 0;
                                                break;
                                        }
                                }
                                printf("New GPS port %d\n", gps_dev);
                                pEditLine->fields[TSRC_PORT_FIELD].internal_data = gps_dev;
                                memcpy(&pEditLine->line[pEditLine->fields[TSRC_PORT_FIELD].start],
                                        pEditLine->fields[TSRC_PORT_FIELD].string_data[gps_dev],
                                        pEditLine->fields[TSRC_PORT_FIELD].length);
                                if (saved_cursor_x == CURSOR_NOT_CHANGED) {
                                        saved_cursor_x = pScreen->cursor_x;
                                        saved_cursor_y = pScreen->cursor_y;
                                }
                                pField = &(pEditLine->fields[TSRC_PORT_FIELD]);
                                send_display_change(pField->start,
                                                pScreen->header_dim_y+TSRC_TSRC_LINE-pScreen->display_offset_y,
                                                CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
                                /* Write the new value to the screen */
                                write(display_data.file_descr,
                                        pEditLine->line + pField->start, pField->length);
                        }
                }

		/* Restore the saved cursor position if screen updates have been made. */
		if (saved_cursor_x != CURSOR_NOT_CHANGED)
		{
//			send_display_change(saved_cursor_x, pScreen->header_dim_y+saved_cursor_y,
//                                              CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
			saved_cursor_x = CURSOR_NOT_CHANGED;
			/* TODO: make the field blinking if positioned on a modifiable field. */
                        crt_cursor_x = pScreen->cursor_x;	/* cursor position inside the screen */
                        crt_cursor_y = pScreen->cursor_y;
                        pCrt_line = &(pScreen->screen_lines[crt_cursor_y]); /* current line where the cursor is */
                        /* detect current field inside the line where the cursor is */
                        crt_field = 0;
                        while ((crt_cursor_x >= pCrt_line->fields[crt_field].start + 
					pCrt_line->fields[crt_field].length) &&
				(crt_field < pCrt_line->no_fields))
                                crt_field++;
                        pCrt_field = &pCrt_line->fields[crt_field];
                        pScreen->cursor_y = crt_cursor_y;
                        pScreen->cursor_x = pCrt_field->start + pCrt_field->length - 1;
                        send_display_change(pCrt_field->start,
                                pScreen->header_dim_y+pScreen->cursor_y-pScreen->display_offset_y,
                                CS_DISPLAY_MAKE_BLINKING,
                                (pCrt_line->line + pCrt_field->start), pCrt_field->length,
                                NO_SCREEN_LOCK);
                        /* set the cursor on the last position */
                        send_display_change(pScreen->cursor_x,
                                pScreen->header_dim_y+pScreen->cursor_y-pScreen->display_offset_y,
                                CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
		}
                /* Unlock the screen after any updates. */
		pthread_mutex_unlock(&display_data.screen_mutex);

                sleep(5);
        }
}

/* Read a command and store it to pBuffer */
 int read_cmd(unsigned char *pBuffer)
 {
#ifdef TEST_DEBUG_MODE
 	// Test only commands
#define NO_TEST_COMMANDS 	11
 	static int cmd_no = 0;
 	unsigned char test_cmd[NO_TEST_COMMANDS][10] = {{0x31},
					 {0x1b,0x5b,0x42},
					 {0x1b,0x5b,0x42},
					 {0x1b,0x5b,0x43},
					 {0x1b,0x5b,0x43},
					 {0x35},
					 {0x1b,0x5b,0x43},
					 {0x1b,0x5b,0x43},
					 {0x36},
					 {0x0d},
					 {0x1b,0x5b,0x42}
					 };
	memcpy(pBuffer, test_cmd[cmd_no], 6);
	cmd_no++;
	if (cmd_no == NO_TEST_COMMANDS)
		cmd_no = 0;
	return 0;
#else
	/* The command data is read one data byte a a time, each byte followed by a 0.
	 * This section packs a multi-byte command (which is sequence of such bytes)
	 * eliminating the trailing zeroes. 
	 */
 	read(display_data.file_descr, pBuffer, MAX_CMD_BUFFER_SIZE);
	/*
	if (*pBuffer == SC_INPUT_ESC)
	{
		read(display_data.file_descr, pBuffer + 1, MAX_CMD_BUFFER_SIZE - 1);
		read(display_data.file_descr, pBuffer + 2, MAX_CMD_BUFFER_SIZE - 2);
	}
	*/

	return 0;
#endif 
 }
 
 /* Parse the command from pBuffer and convert it to internal format in pCmd
  * Illegal commands are converted to kNopCmd and will be ignored
  */
 void parse_cmd(unsigned char *pBuffer, sc_cmd_struct *pCmd)
 {
 
//printf("%s\n", __func__);
	pCmd->type = kNopCmd;
	if ((*pBuffer >= SC_INPUT_ZERO) && (*pBuffer <= SC_INPUT_NINE)) {/* input digits */
		if (display_data.crt_screen == MENU_SCREEN_ID) {
			pCmd->type = kMenuSelectCmd;
			pCmd->value = (*pBuffer - SC_INPUT_ZERO) + 1;       /* start after menu screen */
		} else {
			pCmd->type = kChangeCharCmd;
			pCmd->value = (*pBuffer - SC_INPUT_ZERO);
		}
	} else if ((*pBuffer >= SC_INPUT_A) && (*pBuffer <= SC_INPUT_F)) {/* input hex digits */
		if (display_data.crt_screen == MENU_SCREEN_ID) {
			pCmd->type = kMenuSelectCmd;
			pCmd->value = 10 + (*pBuffer - SC_INPUT_A) + 1;     /* start after menu screen */
		}
	} else if (*pBuffer == 0x80)
		pCmd->type = kGoUpCmd;/* up arrow */
	else if (*pBuffer == 0x81)
		pCmd->type = kGoDownCmd;/* down arrow */
	else if (*pBuffer == 0x82)
		pCmd->type = kGoRightCmd;/* right arrow */
	else if (*pBuffer == 0x83)
		pCmd->type = kGoLeftCmd;/* left arrow */
	else if (*pBuffer == 0x84)
		pCmd->type = kNextCmd; /* NEXT key */
	else if (*pBuffer == 0x85)
		pCmd->type = kCommitCmd; /* YES key */
	else if (*pBuffer == 0x86)
		pCmd->type = kRevertCmd; /* NO key */
	else if (*pBuffer == SC_INPUT_ESC) {
		if(pBuffer[1] == SC_INPUT_LEFT_BRACE) {
			if (pBuffer[2] == SC_INPUT_A) /* up arrow */
				pCmd->type = kGoUpCmd;
			else if (pBuffer[2] == SC_INPUT_B) /* down arrow */
				pCmd->type = kGoDownCmd;
			else if (pBuffer[2] == SC_INPUT_C) /* right arrow */
				pCmd->type = kGoRightCmd;
			else if (pBuffer[2] == SC_INPUT_D) /* left arrow */
				pCmd->type = kGoLeftCmd;
		} else if (pBuffer[1] == 'O') {
			if (pBuffer[2] == 'S')
				pCmd->type = kEscapeCmd; /* ESC key */
			else if (pBuffer[2] == 'P')
				pCmd->type = kNextCmd; /* NEXT key */
			else if (pBuffer[2] == 'Q')
				pCmd->type = kCommitCmd; /* YES key */
			else if (pBuffer[2] == 'R')
				pCmd->type = kRevertCmd; /* NO key */
		}
	}
	else if (*pBuffer == SC_INPUT_ENT) /* enter cmd used to commit changes */
 		pCmd->type = kCommitCmd;
	else if (*pBuffer == SC_INPUT_PLUS) /* "plus" cmd used to increment or advance to next field value */
		pCmd->type = kFieldPlusCmd;
	else if (*pBuffer == SC_INPUT_MINUS) /* "minus" cmd used to decrement or go to earlier field value */
		pCmd->type = kFieldMinusCmd;
 
 }
 
 /* Execute the command stored in internal format at pCmd */
 void process_cmd(sc_cmd_struct *pCmd)
 {
 	/* TODO: for optimization make some local variables static */
 	sc_internal_screen * pCrt_screen = NULL;
	sc_screen_line * pCrt_line = NULL;
	sc_line_field *pCrt_field = NULL;
	int crt_cursor_x = 0;
	int crt_cursor_y = 0;
	int crt_field = 0;
	unsigned char buffer[10];
	int screen_no = display_data.crt_screen;
	
	if (pCmd->type != kNopCmd) /* functional command */
	{
 		pCrt_screen = &display_data.screens[screen_no];/* current screen */
		crt_cursor_x = pCrt_screen->cursor_x;	/* cursor position inside the screen */
		crt_cursor_y = pCrt_screen->cursor_y;
		pCrt_line = &(pCrt_screen->screen_lines[crt_cursor_y]); /* current line where the cursor is */
		
		/* detect current field inside the line where the cursor is */
		crt_field = 0;
		while ((crt_cursor_x >= pCrt_line->fields[crt_field].start + 
					pCrt_line->fields[crt_field].length) &&
				(crt_field < pCrt_line->no_fields))
			crt_field++;
		if (crt_field == pCrt_line->no_fields) {
			/* some alignment error here, skip the command */
			pCmd->type = kNopCmd;
		} else {
			pCrt_field = &pCrt_line->fields[crt_field];
		}
	};
	
	/*printf("%s: cmd type=%d, crt curs xy=(%d,%d) field=%d\n",
			__func__, pCmd->type, crt_cursor_x, crt_cursor_y, crt_field);*/
	switch (pCmd->type)
	{
		case kChangeCharCmd: /* change field command */
		{
			int new_value;
			if (((pCrt_field->type == kModifiable) ||
				(pCrt_field->type == kModified) ||
				(pCrt_field->type == kModified2))
				&& (pCrt_field->string_data[0] == NULL) ) /* modifiable and numeric field */
			{
				if (pCrt_field->type == kModifiable) {
					new_value = pCmd->value;
				} else {
					/* if already modified, shift field left and add the new char at right */
					new_value = (pCrt_field->temp_data * 10) + pCmd->value;
					/* field width check - did the new char cause the field to overflow? */
					if (new_value >= ipow(10, pCrt_field->length)) {
						/* truncate */
						new_value %= ipow(10, pCrt_field->length);
					}
				}
				/* range check */
				if ((new_value < pCrt_field->data_min) || (new_value > pCrt_field->data_max)) {
					send_alarm_bell();
					break; /* sound error alarm */
				}
				pCrt_field->temp_data = new_value;
				pCrt_field->type = kModified;

				// update visible field on screeen
				// check for a format string
				if ((pCrt_field->string_data[1] != NULL) && (pCrt_field->string_data[1][0] == '%'))
					sprintf((char *)buffer, pCrt_field->string_data[1], pCrt_field->temp_data);
				else
					sprintf((char *)buffer, "%*d", pCrt_field->length, pCrt_field->temp_data);
				memcpy(pCrt_line->line + pCrt_field->start, buffer, pCrt_field->length);

				/* Send change to display. */
				send_display_change(pCrt_field->start,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_MAKE_BLINKING,
						(pCrt_line->line + pCrt_field->start), pCrt_field->length,
						SCREEN_LOCK); 
			}
			else
			{
				/* TODO: display on the screen an error for the current cmd */
				send_alarm_bell();
			}
		
			break;
		}
		case kFieldPlusCmd: /* increment field command */
		case kFieldMinusCmd: /* decrement field command */
			if ((pCrt_field->type == kModifiable) ||
				(pCrt_field->type == kModified) ||
				(pCrt_field->type == kModified2))
			{
				if ((pCmd->type == kFieldPlusCmd) && (pCrt_field->temp_data < pCrt_field->data_max))
					pCrt_field->temp_data++;
				else if ((pCmd->type == kFieldMinusCmd) && (pCrt_field->temp_data > pCrt_field->data_min))
					pCrt_field->temp_data--;
				else
					break;	//sound error alarm?

				pCrt_field->type = kModified;
				if (pCrt_field->string_data[0] == NULL) {
					// check for a format string
					if ((pCrt_field->string_data[1] != NULL) && (pCrt_field->string_data[1][0] == '%'))
						sprintf((char *)buffer, pCrt_field->string_data[1], pCrt_field->temp_data);
					else
						sprintf((char *)buffer, "%*d", pCrt_field->length, pCrt_field->temp_data);
				} else {
					sprintf((char *)buffer, "%s", pCrt_field->string_data[pCrt_field->temp_data]);
				}
	                        memcpy(pCrt_line->line + pCrt_field->start, buffer,
	                        	pCrt_field->length);
				/* Send change to display. */
				send_display_change(pCrt_field->start,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_MAKE_BLINKING,
						(pCrt_line->line + pCrt_field->start), pCrt_field->length,
						SCREEN_LOCK);
			}
			break;

		case kGoLeftCmd:	/* go left to the previous modifiable field, or start of line if none */
			/* commit any changes to the current field */
			if (pCrt_field->temp_data != pCrt_field->internal_data)
				commit_line(screen_no, pCrt_screen->cursor_y);

			/* go to the previous modifiable field in this line or previous lines if any */
			while (crt_cursor_y >= pCrt_screen->display_offset_y) {
				pCrt_line = &(pCrt_screen->screen_lines[crt_cursor_y]);
				while ((--crt_field >= 0) &&
						(pCrt_line->fields[crt_field].type == kUnmodifiable));
						
				if (crt_field < 0) /* no previous modifiable chars in this line */
				{
					crt_cursor_y--;
					if (crt_cursor_y >= 0)
						crt_field = pCrt_screen->screen_lines[crt_cursor_y].no_fields /*- 1*/;
					else
						break;
				}
				else /* Found a modifiable field */
				{	
					/* Stop blinking the current field */
					send_display_change(pCrt_field->start,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_STOP_BLINKING,
						pCrt_screen->screen_lines[pCrt_screen->cursor_y].line + pCrt_field->start,
						pCrt_field->length,
						SCREEN_LOCK);
					pCrt_field = &pCrt_line->fields[crt_field];
					pCrt_screen->cursor_y = crt_cursor_y;
					pCrt_screen->cursor_x = pCrt_field->start + pCrt_field->length - 1;
					send_display_change(pCrt_field->start,
							pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
							CS_DISPLAY_MAKE_BLINKING,
							(pCrt_line->line + pCrt_field->start), pCrt_field->length,
							SCREEN_LOCK);
					/* set the cursor on the last position */
					send_display_change(pCrt_screen->cursor_x,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
					break;
				}
			};
			break;
			
		case kGoRightCmd:	/* Go right to the next modifiable field, or start of last field if none */
			/* commit any changes to the current field */
			if (pCrt_field->temp_data != pCrt_field->internal_data)
				commit_line(screen_no, pCrt_screen->cursor_y);
			
			/* Go to the next modifiable field in this line or next lines if any. */
			while ((crt_cursor_y < pCrt_screen->dim_y) &&
					((pCrt_screen->header_dim_y+crt_cursor_y-pCrt_screen->display_offset_y) < (g_rows-1/*for footer*/))) {
				pCrt_line = &(pCrt_screen->screen_lines[crt_cursor_y]);
				while ((++crt_field < pCrt_line->no_fields) &&
						(pCrt_line->fields[crt_field].type == kUnmodifiable));
						
				if (crt_field < pCrt_line->no_fields) /* found a modifiable field */
				{
					/* Stop blinking the current field */
					send_display_change(pCrt_field->start,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_STOP_BLINKING,
						pCrt_screen->screen_lines[pCrt_screen->cursor_y].line + pCrt_field->start,
						pCrt_field->length,
						SCREEN_LOCK);
					/* Start blinking the new field */
					pCrt_field = &pCrt_line->fields[crt_field];
					pCrt_screen->cursor_y = crt_cursor_y;
					pCrt_screen->cursor_x = pCrt_field->start + pCrt_field->length - 1;
					send_display_change(pCrt_field->start,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_MAKE_BLINKING,
						(pCrt_line->line + pCrt_field->start), pCrt_field->length,
						SCREEN_LOCK);
					/* set the cursor on the last position */
					send_display_change(pCrt_screen->cursor_x,
						pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
						CS_DISPLAY_SET_CURSOR, 0, 0, NO_SCREEN_LOCK);
					break;
				} else {
					/* no more fields in this line */
					crt_cursor_y++;
					if (crt_cursor_y < pCrt_screen->dim_y)
						crt_field = -1;
					else
						break;
				}
			}
			break;
			
		case kGoUpCmd:		/* scroll up one line */
			/* commit any changes to the current field */
			if (pCrt_field->temp_data != pCrt_field->internal_data)
				commit_line(screen_no, pCrt_screen->cursor_y);
			/* Set the cursor position */
			if (pCrt_screen->display_offset_y > 0) {
				pCrt_screen->display_offset_y--;
				/* re-display screen */
				display_screen(screen_no);
			}

			break;
			
		case kGoDownCmd:	/* scroll down one line */
			/* commit any changes to the current field */
			if (pCrt_field->temp_data != pCrt_field->internal_data)
				commit_line(screen_no, pCrt_screen->cursor_y);
			if (pCrt_screen->display_offset_y <= (pCrt_screen->dim_y - (g_rows-pCrt_screen->header_dim_y))) {
				pCrt_screen->display_offset_y++;
				/* re-display screen */
				display_screen(screen_no);
			}

			break;
			
		case kGoNextScreenCmd:  /* go to the next screen */

			if (++screen_no >= NO_INTERNAL_SCREENS)
				screen_no = 0;
			display_screen(screen_no);
			break;
			
		case kGoPrevScreenCmd:  /* go to the previous screen */
			if (--screen_no < 0)
				screen_no = NO_INTERNAL_SCREENS - 1;
			display_screen(screen_no);
			break;
			
		case kMenuSelectCmd:  /* go to the selected screen */
#if 0
			if ( pCmd->value < NO_INTERNAL_SCREENS ) {
				display_data.crt_screen = pCmd->value;

				display_screen(display_data.crt_screen);
			}
			break;
#else
			if ( pCmd->value < NO_INTERNAL_SCREENS ) {
				screen_no = pCmd->value;

				display_screen(screen_no);
			}
			break;
#endif
		case kEscapeCmd:	/* go to the top-level menu (screen #0) */
			display_screen(0);
			break;
			
		case kCommitCmd:	/* commit latest changes on the current line */
			/* commit any changes to the current field */
			if (pCrt_field->temp_data != pCrt_field->internal_data)
				commit_line(screen_no, pCrt_screen->cursor_y);
			break;
			
		case kRevertCmd:	/* revert changes to current field */
			pCrt_field->type = kModifiable;
			pCrt_field->temp_data = pCrt_field->internal_data;
			/* re-display original field data ? */ 
			if (pCrt_field->string_data[0] == NULL) {
				// check for a format string
				if ((pCrt_field->string_data[1] != NULL) && (pCrt_field->string_data[1][0] == '%'))
					sprintf((char *)buffer, pCrt_field->string_data[1], pCrt_field->temp_data);
				else
					sprintf((char *)buffer, "%*d", pCrt_field->length, pCrt_field->temp_data);
			} else {
				sprintf((char *)buffer, "%s", pCrt_field->string_data[pCrt_field->temp_data]);
			}
			memcpy(pCrt_line->line + pCrt_field->start, buffer, pCrt_field->length);
			/* Send change to display. */
			send_display_change(pCrt_field->start,
				pCrt_screen->header_dim_y+pCrt_screen->cursor_y-pCrt_screen->display_offset_y,
				CS_DISPLAY_MAKE_BLINKING,
				(pCrt_line->line + pCrt_field->start), pCrt_field->length,
				SCREEN_LOCK);
			break;
		default:		/* no op command (will be ignored) */
			pCmd->type = kNopCmd;
			//send "bel" alarm beep
			send_alarm_bell();
			break;

	}
 
 }
 
 
