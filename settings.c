#include "settings.h"

int readConfigFile(char *filename){
	FILE *file;
	
	file=fopen(filename,"rb");
	if (!file) return NOCONFIG; // no such file - keep default settings
	
	/* TODO: invent a simple config file syntax and parse it! */
	
	fclose(file);
	return 0;
}
