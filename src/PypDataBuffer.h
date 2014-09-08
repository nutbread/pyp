#ifndef __PYP_DATA_BUFFER_H
#define __PYP_DATA_BUFFER_H



#include "PypTypes.h"



struct PypDataBuffer_;
struct PypDataBufferEntry_;



typedef struct PypDataBuffer_ {
	PypSize totalSize;
	struct PypDataBufferEntry_* firstChild;
	struct PypDataBufferEntry_** lastChild;
} PypDataBuffer;

typedef struct PypDataBufferEntry_ {
	PypSize bufferLength;
	PypChar* buffer;
	struct PypDataBufferEntry_* nextSibling;
} PypDataBufferEntry;



PypDataBuffer* pypDataBufferCreate();
void pypDataBufferDelete(PypDataBuffer* dataBuffer);
void pypDataBufferEmpty(PypDataBuffer* dataBuffer);
PypDataBufferEntry* pypDataBufferExtend(PypDataBuffer* dataBuffer, PypSize dataLength);
PypDataBufferEntry* pypDataBufferExtendWithData(PypDataBuffer* dataBuffer, const PypChar* data, PypSize dataLength);
PypDataBufferEntry* pypDataBufferExtendWithString(PypDataBuffer* dataBuffer, const PypChar* data);
void pypDataBufferExtendWithDataBufferAndDelete(PypDataBuffer* dataBuffer, PypDataBuffer* other);
PypBool pypDataBufferUnify(PypDataBuffer* dataBuffer, PypBool nullTerminate, PypDataBufferEntry** ptrNewEntry);



#endif


