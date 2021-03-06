/** ***************************************************************************
 * Name				: vbit.c
 * Description       : VBIT-Pi Teletext Inserter. Main loop.
 *
 * Copyright (C) 2010-2015, Peter Kwan
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 *************************************************************************** **/
 /*
  * This is how to run it
  
  pi@raspberrypi ~/raspi $ /home/pi/vbit-pi/vbit | /home/pi/raspi/teletext -

  */

#include "vbit.h"

void DieWithError(char *errorMessage);  /* Error handling function */

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

void HandleTCPClient(int clntSocket);   /* TCP client handling function */
#define MAXPENDING 5    /* Maximum outstanding connection requests */

/* hic sunt globals */

/* These are set by everyFieldInterrupt
 * and cleared by [name of function that fills the FIFO]
 */
volatile uint8_t gVBIStarted;
struct timespec gFieldTimespec;


volatile uint8_t vbiDone; // Set when the timer reckons that the vbi is over. Cleared by main.
volatile uint8_t FIFOBusy;	// When set, the FillFIFO process is required to release the FIFO.
volatile uint8_t fifoReadIndex; /// maintains the tx block index 0..MAXFIFOINDEX-1
volatile uint8_t fifoWriteIndex; /// maintains the load index 0..MAXFIFOINDEX-1



/** Look for a client 
 * 	This will handle commands but doesn't do anything ATM.
 * Think this will need to go in a thread
 */
//void runClient(void)
PI_THREAD(runClient)
{
	// Network stuff from http://cs.baylor.edu/~donahoo/practical/CSockets/code/TCPEchoServer.c
    int serverSock;                    /* Socket descriptor for server */
    int clientSock;                    /* Socket descriptor for client */	
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */	
    unsigned short echoServPort;     /* Server port */
	#ifdef WIN32
    int clntLen;                     /* needs to be signed int for winsock */
	#else
	unsigned int clntLen;            /* Length of client address data structure */
	#endif

	echoServPort = 5570;  /* This is the local port */

	#ifdef WIN32
	WSADATA wsaData;
    int iResult;
	
	// Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
		DieWithError("WSAStartup failed");
    }
	#endif
	
	// System initialisations
	/* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Create socket for incoming connections */
    if ((serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed\n");	
	

    /* Bind to the local address */
    if (bind(serverSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");

    /* Mark the socket so it will listen for incoming connections */
    if (listen(serverSock, MAXPENDING) < 0)
        DieWithError("listen() failed");	
	


	/* Set the size of the in-out parameter */
	clntLen = sizeof(echoClntAddr);
	
	while(1)
	{

		/* Wait for a client to connect */
		if ((clientSock = accept(serverSock, (struct sockaddr *) &echoClntAddr, 
							   &clntLen)) < 0)
			DieWithError("accept() failed");

		/* clientSock is connected to a client! */

		// printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

		HandleTCPClient(clientSock);
	}
	return NULL;
} // runClient

int main (int argc, char** argv)
{
	
	int i;
	char filename[MAXPATH];
	
	if(argc > 2){
		if(!strcmp(argv[1],"--dir")){
			#ifdef _DEBUG_
			fprintf(stderr,"got directory %s from command line\n",argv[2]);
			#endif
			strncpy(pagesPath, argv[2], MAXPATH-1); /* copy directory string to global */
		}
	}
	
	// construct default config file filename from pages directory and default config file name
	// TODO: allow specifying any file from command line
	strncpy(filename,pagesPath,MAXPATH-1);
	i = filename[(strlen(filename)-1)];
	if (i != '/' && i != '\\' && strlen(filename) + 1 < MAXPATH)
		strcat(filename, "/"); // append missing trailing slash
	i = strlen(filename) + strlen(CONFIGFILE);
	if (i < MAXPATH){
		strncat(filename,CONFIGFILE, i);
	}
	#ifdef _DEBUG_
	fprintf(stderr,"using config file %s\n",filename);
	#endif
	
	i = readConfigFile(filename); // read in config from config file
	switch (i){
		case NOCONFIG:
			fprintf(stderr,"No config file found. Using default settings.\n");
			break;
		case UNKNOWNCONFIG:
			fprintf(stderr,"Ignoring unrecognised config line: %s\n",configErrorString);
			break;
		case BADCONFIG:
			fprintf(stderr,"Config file contains invalid setting: %s\n",configErrorString);
			return 1;
	}
	
	/* initialize the mutexes we're going to use! */
	init_mutex(0);
	init_mutex(1);
	
	InitNu4(); // Prepare the buffers used by Newfor subtitles
	
	// Start the network port (commands and subtitles)
	i=piThreadCreate(runClient);
	//while(1);
	
	// Set up the eight magazine threads
	magInit();

	// TODO: Test that the threads started.
	
	// Sequence streams of packets into VBI fields
	i=piThreadCreate(Stream);
	if (i != 0)
	{
		// printf ("Stream thread! It didn't start\n");		
		return 1;
	}
	// Copy VBI to stdout
	i=piThreadCreate(OutputStream);
	if (i != 0)
	{
		// printf ("OutputStream thread! It didn't start\n");
		return 1;
	}
	while (1)
	{
	}
	fputs("Finished\n",stderr); // impossible to get here
	return 1;
}

