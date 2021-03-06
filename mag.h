#ifndef _MAG_H_
#define _MAG_H_
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h> 

#include "thread.h"

// VBIT stuff
#include "page.h"
#include "packet.h"
#include "buffer.h"
#include "delay.h"

/// States that each magazine can be in
#define STATE_BEGIN	0
#define STATE_IDLE	1
#define STATE_HEADER	2
#define STATE_SENDING	3

//#define _DEBUG_

#define MAXPATH 132

extern char pagesPath[MAXPATH];

/** domag - Runs a single thread
 */
void domag(void);

/** magInit - Starts the 8 mag threads (plus ancillary service buffer)
 */
void magInit(void);

// System status
bool get_temp(char* str);
bool get_time(char* str);
bool get_offset_time(char* str);
bool get_net(char* str);

extern bufferpacket magBuffer[9];	// One buffer control block for each magazine (plus one for subtitles and databroadcast!(

#endif
