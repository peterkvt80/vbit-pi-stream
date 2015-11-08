#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for recv() and send() */
#include <unistd.h>     /* for close() */

#define RCVBUFSIZE 32   /* Size of receive buffer */

void DieWithError(char *errorMessage);  /* Error handling function */

#define MAXCMD 128
static char* pCmd;
static char cmd[MAXCMD];

void command(char* cmd)
{
	switch (cmd[0])
	{
	case 'Y' : 
		puts( "VBIT\r\n");
		break;
	}
} // command

void clearCmd(void)
{
	pCmd=cmd;
}

/** AddChar
 * Add a char to the command buffer and call command when we get CR 
 */
void addChar(char ch)
{
	if (ch!='\n')
	{
		*pCmd++=ch;
	}
	else
	{
		// Got a complete command
		printf("cmd=%s\n",cmd);
		// TODO command();	// Call the command interpreter
		command(cmd);
		clearCmd();
		// hmm, also want to send back response
	}
}


/** HandleTCPClient
 *  Commands will come in here.
 * They need to be accumulated and when we have a complete line, send it to a command interpreter.
 */
void HandleTCPClient(int clntSocket)
{
    char echoBuffer[RCVBUFSIZE];        /* Buffer for echo string */
    int recvMsgSize;                    /* Size of received message */
	int i;
	clearCmd();
    /* Receive message from client */
    if ((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");
		
	for (i=0;i<recvMsgSize;i++)
		addChar(echoBuffer[i]);

    /* Send received string and receive again until end of transmission */
    while (recvMsgSize > 0)      /* zero indicates end of transmission */
    {
        /* Echo message back to client */
        if (send(clntSocket, echoBuffer, recvMsgSize, 0) != recvMsgSize)
            DieWithError("send() failed");

        /* See if there is more data to receive */
        if ((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0)
            DieWithError("recv() failed");
		for (i=0;i<recvMsgSize;i++)
			addChar(echoBuffer[i]);
    }

    close(clntSocket);    /* Close client socket */
	printf("Done Handle TCP\n");
}
