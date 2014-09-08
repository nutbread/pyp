#ifndef __PYP_TAGS_H
#define __PYP_TAGS_H


#include <stdint.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include "PypTypes.h"
#include "PypProcessing.h"



enum {
	PYP_TAG_FLAGS_NONE = 0x0,
	PYP_TAG_FLAG_CONTINUATION = 0x1,
};



struct PypTag_;
struct PypTagGroup_;

typedef uint32_t PypTagFlags;



typedef struct PypTag_ {
	PypSize textLength;
	PypSize arbitraryChars;
	PypChar* text; // not \x00 terminated unless debug is enabled
	struct PypTag_* nextSibling;
	struct PypTag_* firstChild;
	struct PypTagGroup_* children;
	struct PypTagGroup_* closingGroup;
	const PypProcessingInfo* processingInfo;
	PypTagFlags flagsOptimized;
	PypTagFlags flags;
} PypTag;

typedef struct PypTagGroup_ {
	struct PypTag_* firstChild;
	struct PypTag_** ptrNextChild;
} PypTagGroup;



PypTagGroup* pypTagGroupCreate();
PypTag* pypTagCreate(const char* text, PypSize arbitraryChars, PypTagFlags flags, PypTagGroup* closingGroup, PypTagGroup* children);

void pypTagGroupDelete(PypTagGroup* group);
PypBool pypTagGroupDeleteTree(PypTagGroup* group);

void pypTagGroupAddTag(PypTagGroup* group, PypTag* tag);

PypTagGroup* pypTagGroupOptimize(const PypTagGroup* group);

PypBool pypTagIsContinuation(const PypTag* tag);
PypBool pypTagIsComplete(const PypTag* tag);
PypBool pypTagIsClosing(const PypTag* tag);

#ifndef NDEBUG
void pypTagGroupPrint(const PypTagGroup* group, FILE* output, PypBool mustBeComplete);
#endif



#endif


