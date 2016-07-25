#include "settings.h"

int readConfigFile(char *filename){
	FILE *file;
	char str[MAXLINE]; // may as well use the same max line length as the tti files
	
	file=fopen(filename,"rb");
	if (!file) return NOCONFIG; // no such file - keep default settings
	
	/* TODO: invent a simple config file syntax and parse it! */
	while (file && !feof(file)){
		if (!fgets(str,MAXLINE,file)) // read a line
			break; // end if read failed
		if (parseConfigLine(str)){ // split parsing out to another function
			// if parsing line failed just bail out
			fclose(file);
			return BADCONFIG;
		}
	}
	
	fclose(file);
	return 0;
}

char configErrorString[100];

int parseConfigLine(char *configLine){
	/* Function to deal with anything we find in a config file */
	/* If we have a lot of options this could turn into one gigantic pile of if statements which I suspect is not the correct way to go about writing a file parser... */
	
	if (!strtok(configLine, "\r\n")) // chop off any newline charaters
		return 0; // ignore blank lines
	
	if(configLine[0] == ';')
		return 0; // just ignore any line starting with ; as a comment
	
	if(!strncmp(configLine, "header_template=", 16)){
		// found a header template, try to copy 32 bytes into the bufferMove's template
		if (strlen(configLine+16) == 32){ // see how long the value is - it should be exactly 32 bytes
			strncpy(headerTemplate,configLine+16,32);
			return 0;
		} else {
			strcpy(configErrorString,"\"header_template\" must be exactly 32 bytes");
			return BADCONFIG; // refuse to deal with malformed header templates
		}
	}
	
	
	// all options have been checked against so input must be some malformed junk
	
	// take care here not to overflow configErrorString
	snprintf(configErrorString,31,"\"%s",configLine); // first 30 characters should be enough to identify a line
	if(strlen(configLine) >= 30)
		strcat(configErrorString,"...");
	strcat(configErrorString,"\"");
	return BADCONFIG;
}