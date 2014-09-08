#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "PypTags.h"
#include "Memory.h"



// More headers
enum {
	PYP_TAG_OPTIMIZED_FLAGS_NONE = 0x0, // Empty tag
	PYP_TAG_OPTIMIZED_FLAG_COMPLETE = 0x1, // This tag is complete (used for optimization only)
	PYP_TAG_OPTIMIZED_FLAG_CLOSING = 0x2, // This tag is an ending tag (used for optimization only)
};

typedef struct PypTagGroupMap_ {
	const PypTagGroup* tagGroupOld[2];
	PypTagGroup* tagGroupNew;
	struct PypTagGroupMap_* nextSibling;
} PypTagGroupMap;

static void pypTagDelete(PypTag* tag);
static void pypTagGroupDeleteFromTree(PypTagGroup* group);

static PypTag* pypTagCreateExt(const char* text, PypSize textLength, PypSize arbitraryChars, PypTagFlags flags, PypTagFlags flagsOptimized, PypTag* firstChild, PypTagGroup* closingGroup, PypTagGroup* children, const PypProcessingInfo* processingInfo);
static PypBool pypTagDeleteTree(PypTag* tag, PypTagGroupMap* head);

static PypTagGroupMap* pypTagGroupMapCreateHead(const PypTagGroup* groupOld, PypTagGroup* groupNew);
static PypBool pypTagGroupMapExtendWith(PypTagGroupMap* head, const PypTagGroup* groupOld1, const PypTagGroup* groupOld2, PypTagGroup** ptrGroupNew);
static PypBool pypTagGroupSimpleMapExtendWith(PypTagGroupMap* head, PypTagGroup* group);
static PypBool pypTagGroupMapExtend(PypTagGroupMap* head, const PypTag* sourceTag, PypTag* targetTag);
static PypBool pypTagGroupSimpleMapExtend(PypTagGroupMap* head, PypTag* sourceTag);
static void pypTagGroupMapDelete(PypTagGroupMap* head, PypBool deleteNewGroups);

static PypBool pypTagGroupOptimizeSingle(const PypTagGroup* group, PypTagGroup* optGroup, PypTagGroupMap* queueHead, PypTagFlags newFlags);



// Create a tag group
PypTagGroup*
pypTagGroupCreate() {
	// Create
	PypTagGroup* tg = memAlloc(PypTagGroup);
	if (tg == NULL) return NULL;

	// Members
	tg->firstChild = NULL;
	tg->ptrNextChild = &tg->firstChild;

	// Done
	return tg;
}

// Create a single tag
PypTag*
pypTagCreate(const char* text, PypSize arbitraryChars, PypTagFlags flags, PypTagGroup* closingGroup, PypTagGroup* children) {
	// New tag
	return pypTagCreateExt(
		text,
		strlen(text),
		arbitraryChars,
		flags,
		PYP_TAG_OPTIMIZED_FLAGS_NONE,
		NULL,
		closingGroup,
		children,
		NULL
	);
}

// Create a tag with extended options
PypTag*
pypTagCreateExt(const char* text, PypSize textLength, PypSize arbitraryChars, PypTagFlags flags, PypTagFlags flagsOptimized, PypTag* firstChild, PypTagGroup* closingGroup, PypTagGroup* children, const PypProcessingInfo* processingInfo) {
	// Vars
	PypTag* tag;

	// Assertions
	assert(text != NULL);
	assert(textLength > 0);

	// Create tag
	tag = memAlloc(PypTag);
	if (tag == NULL) return NULL;

	// Set text
	tag->textLength = textLength;
	#ifndef NDEBUG
	++textLength; // Debug only; null terminates without interfering with anything else
	#endif
	tag->text = memAllocArray(PypChar, textLength);
	if (tag->text == NULL) {
		// Cleanup
		memFree(tag);
		return NULL;
	}

	// Copy text
	#ifndef NDEBUG
	tag->text[--textLength] = '\x00'; // Debug only; null terminates without interfering with anything else
	#endif
	assert(sizeof(char) == sizeof(PypChar)); // If PypChar is not a char, the following line needs changing (e.g. if using UTF-16)
	memcpy(tag->text, text, sizeof(PypChar) * textLength);

	// References
	tag->nextSibling = NULL;
	tag->firstChild = firstChild;
	tag->children = children;
	tag->closingGroup = closingGroup;
	tag->processingInfo = processingInfo;

	// Other stuff
	tag->flags = flags;
	tag->flagsOptimized = flagsOptimized;
	tag->arbitraryChars = arbitraryChars;

	// Done
	return tag;
}



// Add a tag to a tag group
void
pypTagGroupAddTag(PypTagGroup* group, PypTag* tag) {
	assert(group != NULL);
	assert(tag != NULL);
	assert(tag->nextSibling == NULL);

	(*group->ptrNextChild) = tag;
	group->ptrNextChild = &tag->nextSibling;
}



// Functions to create/update/destroy a mapping of 1 tag group to another; used in optimization
PypTagGroupMap*
pypTagGroupMapCreateHead(const PypTagGroup* groupOld, PypTagGroup* groupNew) {
	PypTagGroupMap* queueEntry = memAlloc(PypTagGroupMap);
	if (queueEntry == NULL) return NULL;

	queueEntry->tagGroupOld[0] = groupOld;
	queueEntry->tagGroupOld[1] = NULL;
	queueEntry->tagGroupNew = groupNew;
	queueEntry->nextSibling = NULL;

	return queueEntry;
}

PypBool
pypTagGroupMapExtendWith(PypTagGroupMap* head, const PypTagGroup* groupOld1, const PypTagGroup* groupOld2, PypTagGroup** ptrGroupNew) {
	// Vars
	PypTagGroupMap* queueEntry;

	// Assertions
	assert(head != NULL);
	assert(ptrGroupNew != NULL);
	assert(groupOld1 != NULL);

	// Check and see if it already exists within the queue
	while (PYP_TRUE) {
		if (head->tagGroupOld[0] == groupOld1 && head->tagGroupOld[1] == groupOld2) {
			// Already exists
			(*ptrGroupNew) = head->tagGroupNew;

			// Okay
			return PYP_TRUE;
		}

		// Next
		if (head->nextSibling == NULL) break;
		head = head->nextSibling;
	}

	// New group
	(*ptrGroupNew) = pypTagGroupCreate();
	if ((*ptrGroupNew) == NULL) return PYP_FALSE; // error

	// New queue entry
	queueEntry = memAlloc(PypTagGroupMap);
	if (queueEntry == NULL) {
		// Cleanup
		pypTagGroupDelete(*ptrGroupNew);
		return PYP_FALSE;
	}

	// Queue entry members
	queueEntry->tagGroupOld[0] = groupOld1;
	queueEntry->tagGroupOld[1] = groupOld2;
	queueEntry->tagGroupNew = (*ptrGroupNew);
	queueEntry->nextSibling = NULL;

	// Link queue
	head->nextSibling = queueEntry;


	// Okay
	return PYP_TRUE;

}

PypBool
pypTagGroupMapExtend(PypTagGroupMap* head, const PypTag* sourceTag, PypTag* targetTag) {
	assert(head != NULL);
	assert(sourceTag != NULL);
	assert(targetTag != NULL);

	// Nullify
	targetTag->children = NULL;
	targetTag->closingGroup = NULL;

	if (sourceTag->closingGroup != NULL) {
		if (sourceTag->children != NULL) {
			//if (!pypTagGroupMapExtendWith(head, sourceTag->closingGroup, NULL, &targetTag->closingGroup)) return PYP_FALSE;
			//if (!pypTagGroupMapExtendWith(head, sourceTag->children, NULL, &targetTag->children)) return PYP_FALSE;
			if (!pypTagGroupMapExtendWith(head, sourceTag->children, sourceTag->closingGroup, &targetTag->children)) return PYP_FALSE;
		}
		else {
			//if (!pypTagGroupMapExtendWith(head, sourceTag->closingGroup, NULL, &targetTag->closingGroup)) return PYP_FALSE;
			if (!pypTagGroupMapExtendWith(head, sourceTag->closingGroup, NULL, &targetTag->children)) return PYP_FALSE;
		}
	}
	else if (sourceTag->children != NULL) {
		if (!pypTagGroupMapExtendWith(head, sourceTag->children, NULL, &targetTag->children)) return PYP_FALSE;
	}

	return PYP_TRUE;
}

PypBool
pypTagGroupSimpleMapExtendWith(PypTagGroupMap* head, PypTagGroup* group) {
	// Vars
	PypTagGroupMap* queueEntry;

	// Assertions
	assert(group != NULL);
	assert(head != NULL);

	// Check and see if it already exists within the queue
	while (PYP_TRUE) {
		if (head->tagGroupNew == group) {
			// Already exists
			return PYP_TRUE;
		}

		// Next
		if (head->nextSibling == NULL) break;
		head = head->nextSibling;
	}


	// New queue entry
	queueEntry = memAlloc(PypTagGroupMap);
	if (queueEntry == NULL) return PYP_FALSE; // error

	// Queue entry members
	queueEntry->tagGroupOld[0] = NULL;
	queueEntry->tagGroupOld[1] = NULL;
	queueEntry->tagGroupNew = group;
	queueEntry->nextSibling = NULL;

	// Link queue
	head->nextSibling = queueEntry;


	// Okay
	return PYP_TRUE;

}

PypBool
pypTagGroupSimpleMapExtend(PypTagGroupMap* head, PypTag* sourceTag) {
	assert(head != NULL);
	assert(sourceTag != NULL);

	return (
		(sourceTag->children == NULL || pypTagGroupSimpleMapExtendWith(head, sourceTag->children)) &&
		(sourceTag->closingGroup == NULL || pypTagGroupSimpleMapExtendWith(head, sourceTag->closingGroup))
	);
}

void
pypTagGroupMapDelete(PypTagGroupMap* head, PypBool deleteNewGroups) {
	// Vars
	PypTagGroupMap* next;

	// Assertions
	assert(head != NULL);

	while (PYP_TRUE) {
		// Delete groups
		if (deleteNewGroups) {
			assert(head->tagGroupNew != NULL);
			pypTagGroupDeleteFromTree(head->tagGroupNew);
		}


		// Delete
		next = head->nextSibling;
		memFree(head);


		// Next
		if (next == NULL) break;
		head = next;
	}
}



// Create an optimized tag group from a regular tag group
PypBool
pypTagGroupOptimizeSingle(const PypTagGroup* group, PypTagGroup* optGroup, PypTagGroupMap* queueHead, PypTagFlags newFlags) {
	// Vars
	PypSize i;
	PypSize textLength;
	PypSize textLengthMin;
	const PypChar* text;
	PypTag* tag;
	PypTag** next;
	const PypTag* oldTag;

	// Assertions
	assert(group != NULL);
	assert(optGroup != NULL);
	assert(queueHead != NULL);
	assert(group->firstChild != NULL); // no empty groups, because that makes no sense

	// Vars setup
	oldTag = group->firstChild;

	// Add tags
	while (PYP_TRUE) {
		// Make sure no tampering occured
		assert(oldTag->flagsOptimized == PYP_TAG_OPTIMIZED_FLAGS_NONE);
		assert(oldTag->text != NULL);
		assert(oldTag->textLength > 0);

		// Find matching parts of existing tags (if any)
		text = oldTag->text;
		textLength = oldTag->textLength;

		next = &optGroup->firstChild;
		while ((*next) != NULL) {
			// Count how much of the two tags are identical
			textLengthMin = ((*next)->textLength < textLength) ? (*next)->textLength : textLength;
			for (i = 0; i < textLengthMin && (text[i] == (*next)->text[i]); ++i); // Needs no body, the condition covers everything

			// Any match?
			if (i > 0) {
				// Make sure input data is correct; if this fails, it usually means duplicate tags were listed
				assert(((*next)->textLength > i) || (textLength > i) || ((*next)->firstChild != NULL && (*next)->flagsOptimized == PYP_TAG_OPTIMIZED_FLAGS_NONE));

				// Update text offsets
				textLength -= i;

				// Modify existing tag
				if ((*next)->textLength > i) {
					// Create a new child tag
					tag = pypTagCreateExt(
						&((*next)->text[i]),
						(*next)->textLength - i,
						(*next)->arbitraryChars,
						(*next)->flags,
						(*next)->flagsOptimized,
						(*next)->firstChild,
						(*next)->closingGroup,
						(*next)->children,
						(*next)->processingInfo
					);
					if (tag == NULL) {
						// Cleanup
						return PYP_FALSE;
					}

					// Update pre-existing tag
					(*next)->firstChild = tag;
					(*next)->textLength = i;

					// No remaining text?
					if (textLength == 0) {
						// Simply make the if jump to a different position (to reduce code redundancy)
						// Copying the code from the labeled block into this block achieves the same effect (but more error prone)
						goto updateExistingTag;
					}
					else {
						// Nullify unused tag
						(*next)->flags = PYP_TAG_FLAGS_NONE;
						(*next)->flagsOptimized = PYP_TAG_OPTIMIZED_FLAGS_NONE;
						(*next)->arbitraryChars = 0;
						(*next)->closingGroup = NULL;
						(*next)->children = NULL;
						(*next)->processingInfo = NULL;
					}

					// Check children in the next loop, skipping this new tag (since they're not equal at this point)
					next = &tag->nextSibling;
				}
				else if (textLength > 0) {
					// Check children in the next loop
					next = &(*next)->firstChild;
				}
				else {
					updateExistingTag:

					// Use (*next) as the final, completed tag
					(*next)->flags = oldTag->flags;
					(*next)->flagsOptimized = newFlags;
					(*next)->arbitraryChars = oldTag->arbitraryChars;
					if (!pypTagGroupMapExtend(queueHead, oldTag, (*next))) {
						// Cleanup
						return PYP_FALSE;
					}

					// Break inner, continue outer; this is used in place of a break and some extra if checks (similar to a Java labeled continue)
					goto outerContinue;
				}

				// Modify text offset
				text = &(text[i]);

				// Skip "next" below
				continue;
			}

			// Next
			next = &(*next)->nextSibling;
		}

		// A few things that are true at this point
		assert(next != NULL);
		assert((*next) == NULL);
		assert(textLength > 0);

		// Create tag
		tag = pypTagCreateExt(
			text,
			textLength,
			oldTag->arbitraryChars,
			oldTag->flags,
			newFlags,
			NULL,
			NULL,
			NULL,
			oldTag->processingInfo
		);
		if (tag == NULL || !pypTagGroupMapExtend(queueHead, oldTag, tag)) {
			// Cleanup
			return PYP_FALSE;
		}

		// Update lists
		*next = tag;


		// Continue without new tag creation
		outerContinue: oldTag = oldTag->nextSibling; // https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf : 6.8.3 / example 3

		// Terminate condition
		if (oldTag == NULL) break;
	}


	// Okay
	return PYP_TRUE;
}



// Create an optimized tag group tree from a regular tag group tree
PypTagGroup*
pypTagGroupOptimize(const PypTagGroup* group) {
	// Vars
	PypTagGroup* optGroup;
	PypTagGroup* optGroupRoot;
	PypTagGroupMap* queueHead;
	PypTagGroupMap* queueCurrent;

	// Assertions
	assert(group != NULL);

	// Create first
	optGroup = pypTagGroupCreate();
	if (optGroup == NULL) return NULL;
	optGroupRoot = optGroup;

	// Setup queue
	queueCurrent = pypTagGroupMapCreateHead(group, optGroup);
	if (queueCurrent == NULL) {
		// Cleanup and return
		pypTagGroupDelete(optGroup);
		return NULL;
	}
	queueHead = queueCurrent;

	// Loop over the queue
	while (PYP_TRUE) {
		if (
			!pypTagGroupOptimizeSingle(queueCurrent->tagGroupOld[0], queueCurrent->tagGroupNew, queueHead, PYP_TAG_OPTIMIZED_FLAG_COMPLETE) ||
			(queueCurrent->tagGroupOld[1] != NULL && !pypTagGroupOptimizeSingle(queueCurrent->tagGroupOld[1], queueCurrent->tagGroupNew, queueHead, PYP_TAG_OPTIMIZED_FLAG_COMPLETE | PYP_TAG_OPTIMIZED_FLAG_CLOSING))
		) {
			// Cleanup
			pypTagGroupMapDelete(queueHead, PYP_TRUE);
			return NULL;
		}

		// Go to next entry in queue
		queueCurrent = queueCurrent->nextSibling;

		// Termination condition
		if (queueCurrent == NULL) break;
	}

	// Done
	pypTagGroupMapDelete(queueHead, PYP_FALSE);
	return optGroupRoot;
}



// Check if a tag is a continuation tag
PypBool
pypTagIsContinuation(const PypTag* tag) {
	assert(tag != NULL);

	return ((tag->flags & PYP_TAG_FLAG_CONTINUATION) != 0);
}

// Check if an optimized tag represents a completed formation
PypBool
pypTagIsComplete(const PypTag* tag) {
	assert(tag != NULL);

	return ((tag->flagsOptimized & PYP_TAG_OPTIMIZED_FLAG_COMPLETE) != 0);
}

// Check if an optimized tag represents a closing tag
PypBool
pypTagIsClosing(const PypTag* tag) {
	assert(tag != NULL);

	return ((tag->flagsOptimized & PYP_TAG_OPTIMIZED_FLAG_CLOSING) != 0);
}



// Delete a tag group that hasn't been set up yet
void
pypTagGroupDelete(PypTagGroup* group) {
	assert(group != NULL);
	assert(group->firstChild == NULL);

	// Delete group
	memFree(group);
}

// Delete a tag group and its tag formations
void
pypTagGroupDeleteFromTree(PypTagGroup* group) {
	assert(group != NULL);
	assert(group->firstChild != NULL);

	// Delete child list
	pypTagDelete(group->firstChild);

	// Delete group
	memFree(group);
}

// Delete a tag group, its tag formations, and any sub-groups
PypBool
pypTagGroupDeleteTree(PypTagGroup* group) {
	// Vars
	PypTagGroupMap* queueHead;
	PypTagGroupMap* queueCurrent;

	// Assertions
	assert(group != NULL);
	assert(group->firstChild != NULL);

	// Setup queue
	queueCurrent = pypTagGroupMapCreateHead(NULL, group);
	if (queueCurrent == NULL) return PYP_FALSE; // error
	queueHead = queueCurrent;

	// Loop
	while (PYP_TRUE) {
		// No empty groups
		assert(queueCurrent->tagGroupNew != NULL);
		assert(queueCurrent->tagGroupNew->firstChild != NULL);

		// Delete tags and update queue
		if (!pypTagDeleteTree(queueCurrent->tagGroupNew->firstChild, queueHead)) {
			// Error
			pypTagGroupMapDelete(queueHead, PYP_FALSE);
			return PYP_FALSE;
		}


		// Delete
		memFree(queueCurrent->tagGroupNew);


		// Go to next entry in queue
		queueCurrent = queueCurrent->nextSibling;

		// Termination condition
		if (queueCurrent == NULL) break;
	}

	// Done
	pypTagGroupMapDelete(queueHead, PYP_FALSE);
	return PYP_TRUE;
}

// Delete an optimized tag and its children
void
pypTagDelete(PypTag* tag) {
	// Vars
	PypTag* next;

	// Assertions
	assert(tag != NULL);

	while (PYP_TRUE) {
		// Delete children
		if (tag->firstChild != NULL) {
			pypTagDelete(tag->firstChild);
		}

		// Delete it
		next = tag->nextSibling;
		memFree(tag->text);
		memFree(tag);

		// Continue to siblings
		if (next == NULL) break;
		tag = next;
	}
}

// Delete an optimized tag and its children
PypBool
pypTagDeleteTree(PypTag* tag, PypTagGroupMap* head) {
	// Vars
	PypTag* next;
	PypBool returnValue = PYP_TRUE;

	// Assertions
	assert(tag != NULL);

	while (PYP_TRUE) {
		// Delete children
		if (tag->firstChild != NULL) {
			pypTagDeleteTree(tag->firstChild, head);
		}


		// Add child groups to the queue
		if (!pypTagGroupSimpleMapExtend(head, tag)) {
			returnValue = PYP_FALSE;
		}

		// Delete it
		next = tag->nextSibling;
		memFree(tag->text);
		memFree(tag);

		// Continue to siblings
		if (next == NULL) break;
		tag = next;
	}


	// Done
	return returnValue;
}



// Output the contents of an optimized tag group
#ifndef NDEBUG
typedef struct PypTagPrintInfo_ {
	struct PypTagPrintInfo_* parent;
	struct PypTagPrintInfo_* child;
	const PypTag* tag;
	PypSize textOffset;
} PypTagPrintInfo;

static void
pypTagPrint(const PypTag* tag, FILE* output, PypBool mustBeComplete) {
	// Vars
	PypSize textBufferLength;
	char* textBuffer;
	PypTagPrintInfo* pInfo;
	PypTagPrintInfo* pInfoAlt;
	PypTagPrintInfo* pInfoFirst;

	// Assertions
	assert(tag != NULL);
	assert(output != NULL);

	textBufferLength = 128;
	textBuffer = memAllocArray(char, textBufferLength);
	if (textBuffer == NULL) return;

	pInfo = memAlloc(PypTagPrintInfo);
	if (pInfo == NULL) {
		// Cleanup
		memFree(textBuffer);
		return;
	}
	pInfo->parent = NULL;
	pInfo->child = NULL;
	pInfo->textOffset = 0;
	pInfo->tag = tag;

	pInfoFirst = pInfo;

	while (PYP_TRUE) {
		// Output
		if (pypTagIsComplete(tag) || !mustBeComplete) {
			for (pInfoAlt = pInfoFirst; pInfoAlt != NULL; pInfoAlt = pInfoAlt->child) {
				// Space requirement
				if (pInfoAlt->tag->textLength + 1 > textBufferLength) {
					memFree(textBuffer);

					textBufferLength = (pInfoAlt->tag->textLength + 1);
					textBuffer = memAllocArray(char, textBufferLength);
					if (textBuffer == NULL) goto cleanupAndReturn; // Error
				}

				// Copy and null terminate
				memcpy(textBuffer, pInfoAlt->tag->text, sizeof(PypChar) * pInfoAlt->tag->textLength);
				textBuffer[pInfoAlt->tag->textLength] = '\x00';

				// Output separator
				if (pInfoAlt != pInfoFirst) fprintf(output, " ");

				// Output tag
				//fprintf(output, "\"%s\"(0x%X,0x%X)", textBuffer, pInfoAlt->tag->flags, pInfoAlt->tag->flagsOptimized);
				fprintf(output, "\"%s\"", textBuffer);
			}

			fprintf(output, "\n");
		}

		// Next
		if (tag->firstChild == NULL) {
			tag = tag->nextSibling;

			while (tag == NULL) {
				// Shallower
				pInfoAlt = pInfo;
				pInfo = pInfo->parent;

				// Delete
				memFree(pInfoAlt);

				// No tags remain
				if (pInfo == NULL) goto cleanupAndReturn;

				// Nullify child, change tag
				pInfo->child = NULL;
				tag = pInfo->tag->nextSibling;
			}

			// Same depth
			pInfo->tag = tag;
		}
		else {
			// Deeper
			tag = tag->firstChild;

			// New entry
			pInfoAlt = memAlloc(PypTagPrintInfo);
			if (pInfoAlt == NULL) goto cleanupAndReturn; // Error
			pInfoAlt->parent = pInfo;
			pInfoAlt->child = NULL;
			pInfo->child = pInfoAlt;
			pInfoAlt->textOffset = 0;
			pInfoAlt->tag = tag;
			pInfo = pInfoAlt;
		}
	}

	// Cleanup
	cleanupAndReturn:
	if (textBuffer != NULL) {
		memFree(textBuffer);
	}
	for (pInfoAlt = pInfo; pInfoAlt != NULL; pInfoAlt = pInfoAlt->parent) {
		memFree(pInfoAlt);
	}
}

void
pypTagGroupPrint(const PypTagGroup* group, FILE* output, PypBool mustBeComplete) {
	assert(group != NULL);
	assert(output != NULL);

	// Output tags
	pypTagPrint(group->firstChild, output, mustBeComplete);
}
#endif


