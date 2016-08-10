#include "outputstream.h"

char mydata[45];

PI_THREAD (OutputStream)
{
	#ifdef WIN32
	_setmode(_fileno(stdout), _O_BINARY); // binary mode stdout to avoid pesky line ending conversion
	#endif
	while(1)
	{
		// Loop if we have a buffer under-run
		while (bufferGet(streamBuffer,mydata)==BUFFER_EMPTY) delay(10);
		fwrite(&mydata[3],1,42,stdout);
	}
}
