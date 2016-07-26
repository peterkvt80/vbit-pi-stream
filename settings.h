#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdio.h>
#include <string.h>

#define CONFIGFILE "vbit.conf" // default config file name
#define NOCONFIG 1 // failed to open config file
#define BADCONFIG 2 // config file malformed

#define MAXCONFLINE 100 // max line length in config file

extern char headerTemplate[]; // template string for generating header packets

extern char configErrorString[100];

void initConfigDefaults(void);
int readConfigFile(char *filename);
int parseConfigLine(char *configLine);

#endif
