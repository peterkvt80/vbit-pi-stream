#include "settings.h"

// template string for generating header packets
char headerTemplate[33];

// settings for generation of packet 8/30
uint8_t multiplexedSignalFlag;
uint8_t initialMag;
uint8_t initialPage;
uint16_t initialSubcode;
uint16_t NetworkIdentificationCode;
char serviceStatusString[20];

void initConfigDefaults(void){
	/* keep initialisation of defaults all in one place */
	
	// This is where the default header template is defined.
	sprintf(headerTemplate," VBIT-PI %%%%# %%%%a %%d %%%%b%c%%H:%%M/%%S",0x83); // include alpha yellow code
	
	// 0 indicates teletext is multiplexed with video, 1 means full frame teletext.
	multiplexedSignalFlag = 0;
	
	// here we set the default initial teletext page.
	initialMag = 1;
	initialPage = 0x00;
	initialSubcode = 0x3F7F;
	
	// here we set the default NI code. 0000 is rather a long string of zero bits, but that's what the spect tells us to do.
	NetworkIdentificationCode = 0x0000; // "Where a broadcaster has not been allocated an official NI value, bytes 13 and 14 of packet 8/30 format 1 should be coded with all bits set to 0"
	memcpy(serviceStatusString, "VBIT default", 12); // default service string.
}

int readConfigFile(char *filename){
	FILE *file;
	char str[MAXCONFLINE];
	
	initConfigDefaults(); // first set the default values that config file will override
	
	file=fopen(filename,"rb");
	if (!file) return NOCONFIG; // no such file - keep default settings
	
	/* TODO: invent a simple config file syntax and parse it! */
	while (file && !feof(file)){
		if (!fgets(str,MAXCONFLINE,file)) // read a line
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
	char hexdigits[] = "0123456789ABCDEF";
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
	} else if (!strncmp(configLine, "initial_teletext_page=", 22)){
		// found an initial page, it must be at least three bytes.
		if (strlen(configLine+22) < 3){
			strcpy(configErrorString,"\"initial_teletext_page\" must contain a magazine and page");
			return BADCONFIG; // refuse to deal with malformed page numbers
		}
		// check that it is a valid number
		char *separator;
		long magpage; 
		magpage = strtol(configLine+22, &separator, 16); // try to read some hex digits
		if(magpage > 0xFF && magpage < 0x900){ // allow 0x100 to 0x8FF
			initialMag = magpage / 0x100;
			initialPage = magpage % 0x100;
			if(*separator == ':'){
				if (strlen(configLine+26) != 4){
					strcpy(configErrorString,"initial subcode number must be exactly 4 digits");
					return BADCONFIG; // refuse to deal with malformed subcodes
				}
				int i;
				int subcode = 0; 
				char *nibble;
				for (i=0; i<4; i++){
					nibble = strchr(hexdigits, toupper(*(configLine+26+i)));
					if (!nibble){
						strcpy(configErrorString,"invalid digit in initial subcode number");
						return BADCONFIG; // refuse to deal with malformed subcodes
					}
					subcode |= (nibble-hexdigits) << (4 * (3-i));
				}
				if (subcode & 0xC080){
					strcpy(configErrorString,"initial subcode must not have bits 7, 14, or 15 set");
					return BADCONFIG; // refuse to deal with malformed subcodes
				}
				initialSubcode=subcode;
				
				return 0; // we got a magazine, page number and subcode
				
			} else {
				return 0; // we got a magazine and page number
			}
		}
		// something was invalid that we didn't specifically test for so drop out the bottom
	}
	
	
	// all options have been checked against so input must be some malformed junk
	
	// take care here not to overflow configErrorString
	snprintf(configErrorString,31,"\"%s",configLine); // first 30 characters should be enough to identify a line
	if(strlen(configLine) >= 30)
		strcat(configErrorString,"...");
	strcat(configErrorString,"\"");
	return BADCONFIG;
}