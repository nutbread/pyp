#ifndef __PYP_PROCESSING_H
#define __PYP_PROCESSING_H



#include <stdint.h>



struct PypTag_;
struct PypTagGroup_;
struct PypProcessingInfo_;
struct PypDataBuffer_;
struct PypStreamLocation_;
enum PypReadStatus_;

typedef enum PypReadStatus_ (*PypDataBufferModifier)(struct PypDataBuffer_* input, struct PypDataBuffer_** output, const struct PypStreamLocation_* streamLocation, void* data);

typedef struct PypProcessingInfo_ {
	PypDataBufferModifier selfModifier;
	PypDataBufferModifier childSuccessModifier;
	PypDataBufferModifier childFailureModifier;
	PypDataBufferModifier continuationModifier;
} PypProcessingInfo;



PypProcessingInfo* pypProcessingInfoCreate(PypDataBufferModifier selfModifier, PypDataBufferModifier childSuccessModifier, PypDataBufferModifier childFailureModifier, PypDataBufferModifier continuationModifier);
void pypProcessingInfoDelete(PypProcessingInfo* pInfo);
void pypSetProcessingInfo(struct PypTag_* tag, const PypProcessingInfo* info);



#endif


