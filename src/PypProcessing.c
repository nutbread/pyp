#include <stddef.h>
#include <assert.h>
#include "PypProcessing.h"
#include "PypTags.h"
#include "Memory.h"



// Create processing info for a tag
PypProcessingInfo*
pypProcessingInfoCreate(PypDataBufferModifier selfModifier, PypDataBufferModifier childSuccessModifier, PypDataBufferModifier childFailureModifier, PypDataBufferModifier continuationModifier) {
	PypProcessingInfo* info = memAlloc(PypProcessingInfo);
	if (info == NULL) return NULL; // error

	// Members
	info->selfModifier = selfModifier;
	info->childSuccessModifier = childSuccessModifier;
	info->childFailureModifier = childFailureModifier;
	info->continuationModifier = continuationModifier;

	// Done
	return info;
}

// Delete processing info
void
pypProcessingInfoDelete(PypProcessingInfo* pInfo) {
	assert(pInfo != NULL);

	memFree(pInfo);
}

// Set the processing info of a tag
void
pypSetProcessingInfo(struct PypTag_* tag, const PypProcessingInfo* info) {
	assert(tag != NULL);
	assert(info != NULL);
	assert(tag->processingInfo == NULL);

	tag->processingInfo = info;
}


