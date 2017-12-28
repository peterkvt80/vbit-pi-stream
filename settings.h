#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#define CONFIGFILE "vbit.conf" // default config file name
#define NOCONFIG 1 // failed to open config file
#define BADCONFIG 2 // config file malformed
#define UNKNOWNCONFIG 3 // something unrecognised

#define MAXCONFLINE 100 // max line length in config file

// template string for generating header packets
extern char headerTemplate[];

// settings for generation of packet 8/30
extern uint8_t multiplexedSignalFlag;
extern uint8_t initialMag;
extern uint8_t initialPage;
extern uint16_t initialSubcode;
extern uint16_t NetworkIdentificationCode;
extern char serviceStatusString[21];

// description of last error encountered reading config file
extern char configErrorString[100];

void initConfigDefaults(void);
int readConfigFile(char *filename);
int parseConfigLine(char *configLine);

#endif
