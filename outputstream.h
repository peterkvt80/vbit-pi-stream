#ifndef _OUTPUTSTREAM_H_
#define _OUTPUTSTREAM_H_

#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
#include <stdint.h>

//#include <stdint.h>

#include "thread.h"

// VBIT
#include "buffer.h"
#include "vbit.h"


/** outputstream is a thread that 
1) sinks packets from stream.c
2) sources packets to stdout
*/
PI_THREAD (OutputStream);
#define STREAMBUFFERSIZE 50
extern bufferpacket streamBuffer[1];

#endif

