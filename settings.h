#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdio.h>

#define CONFIGFILE "vbit.conf" // default config file name
#define NOCONFIG 1 // failed to open config file
#define BADCONFIG 2 // config file malformed

int readConfigFile(char *filename);

#endif
