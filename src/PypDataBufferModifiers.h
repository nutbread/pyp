#ifndef __PYP_DATA_BUFFER_MODIFIERS_H
#define __PYP_DATA_BUFFER_MODIFIERS_H



#include "PypReader.h"
#include "PypDataBuffer.h"



PypReadStatus pypDataBufferModifyToString(PypDataBuffer* input, PypDataBuffer** output, const PypStreamLocation* streamLocation, void* data);
PypReadStatus pypDataBufferModifyToEscapedHTML(PypDataBuffer* input, PypDataBuffer** output, const PypStreamLocation* streamLocation, void* data);



#endif


