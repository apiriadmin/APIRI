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

#ifndef ATC_SC_H
#define ATC_SC_H

#include <stdbool.h>
#include <pthread.h>
#include "atc_sc_screen.h"  // screen initialization data

#ifdef TEST_DEBUG_MODE  // used for self contained test only
#define PATH_NAME_FPI "./test_file"
#else
#define PATH_NAME_FPI "/dev/sci"
#endif

#define MAX_PROCFILE_LINE_SIZE	256

/* Max size for the command buffer; only one command will be at a time in this buffer */
#define MAX_CMD_BUFFER_SIZE 20

/* SC Input recognized chars */
#define SC_INPUT_ZERO		'0'
#define SC_INPUT_NINE		'9'
#define SC_INPUT_A		'A'
#define SC_INPUT_B		'B'
#define SC_INPUT_C		'C'
#define SC_INPUT_D		'D'
#define SC_INPUT_E		'E'
#define SC_INPUT_F		'F'
#define SC_INPUT_ESC		0x1b
#define SC_INPUT_LEFT_BRACE	0x5b
#define SC_INPUT_ENT		0x0d
#define SC_INPUT_PLUS		'+'
#define SC_INPUT_MINUS		'-'

/* Error codes returned by the SC module */
#define ERR_INIT_SCREEN         -1
#define ERR_CREATE_THREAD	-2
#define ERR_OPEN_CONNECTION	-3
#define ERR_OPEN_PROCFILE	-4
#define ERR_READ_PROCFILE	-5
#define ERR_UNAVAILABLE_PROCFILE	-6

/* Definitions of constants used for locking the screens */
#define SCREEN_LOCK		1
#define NO_SCREEN_LOCK		0

/* Constant indicating if a cursor position is changed or not */
#define CURSOR_NOT_CHANGED	-1

/* Definition of command types */
typedef enum _sc_cmd_type
{
	kNopCmd,	// no op command (will be ignored)
	kChangeCharCmd,	// change field command
	kGoLeftCmd,	// go left to the previous modifiable field, or start of line if none
	kGoRightCmd,	// go right to the next modifiable field, or end of line if none
	kGoUpCmd,	// go up one line to the first modifiable field
	kGoDownCmd,	// go down one line to the first modifiable field
	kGoNextScreenCmd,  // go to first line of the next screen
	kGoPrevScreenCmd,  // go to first line of the previous screen
	kMenuSelectCmd,     // direct entry of numbered menu item
	kCommitCmd,	// commit latest changes on the current line
	kRevertCmd,	// revert changes to field value
	kEscapeCmd,	// Use ESC to escape to top-level SC menu (screen #0)
	kNextCmd,
	kFieldPlusCmd,	// increase field value
	kFieldMinusCmd, // decrease field value
	kMaxCmdType
}sc_cmd_type;

/* The structure of a parsed command; the command types are as in the enum above;
 * The field "value" is used only for the command kChangeCharCmd
 */
typedef struct _sc_cmd_struct
{
	sc_cmd_type 	type;
	unsigned char 	value; // used to store the new char value for kChangeCharCmd
}sc_cmd_struct;

/* Enum containing the screen field types */
typedef enum _sc_field_type
{
	kUnmodifiable, 	// field is unmodifiable by user
	kModifiable,	// field is modifiable by user
	kModified,	// field has been modified by user, not committed yet
	kModified2,	// field has been modified by user, overdue for committing 
	kMaxFieldType
}sc_field_type;

/* Structure of a screen field */
typedef struct _sc_line_field
{
	sc_field_type 	type;	// type of the field (see enum above)
	int 		length;	// length of the field in chars
	int		start; // relative position from the begining the line
	int		internal_data;	// internal field data in binary format
	int		temp_data;	// holding field value until committed/reverted
	int		data_min;	// minimum data value
	int		data_max;	// maximum data value
	char *		string_data[MAX_FIELD_STRING_VALUES]; // for non-integral type fields
}sc_line_field;

/* Structure of a line from the  the internal screen
 * Such a line is composed of several screen fields, each having its own attributes
 */
typedef struct _sc_screen_line
{
	int no_fields; // number of used fields in this line
	bool isScrollable; // determines if the line is fixed or scrollable
	unsigned char line[MAX_INTERNAL_SCREEN_X_SIZE]; // display buffer for this line
	sc_line_field fields[MAX_FIELDS_PER_LINE]; // fields composing this line
}sc_screen_line;

/* Structure of an internal screen; this screen will be mapped onto the external 
 * display screen using x,y offsets;
 * The y_offset is given by display_offset_y and the x_offset is always 0;
 * This means that in the current implementation the horizontal size of the 
 * internal screen is smaller or equal than the horizontal size of the display screen
 */
typedef struct _sc_internal_screen
{
	int dim_x;	// horizontal size of this screen 
	int dim_y;	// vertical size of this screen
	int cursor_x; 	// current position of the cursor in the line
	int cursor_y;	// the current line of the internal screen
	int display_offset_y; // the first internal screen line diaplayed on the external screen
	void (*update)(void*); // pointer to function used to update screen data
	int header_dim_y;	// vertical size of header
	unsigned char header[2][MAX_INTERNAL_SCREEN_X_SIZE];	// display buffer for header lines
	unsigned char footer[MAX_INTERNAL_SCREEN_X_SIZE];	// display buffer for footer line
	sc_screen_line screen_lines[MAX_INTERNAL_SCREEN_Y_SIZE];

}sc_internal_screen;

/* Structure containing the internl data used by the ATC_SC module */
typedef struct _sc_internal_data
{
	pthread_mutex_t screen_mutex; // mutex controling the access to the internal screens
	pthread_t update_thread;	// thread to run update function for screen on display
	int file_descr; // file descriptor used to communicate witht he screen manager
	int crt_screen;	// current internal screen to be displayed on the external screen
	sc_internal_screen *screens[MAX_SCREENS]; //internal screens
	
}sc_internal_data;


#endif	/* ATC_SC_H */
