#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "PypReader.h"
#include "Memory.h"
#include "PypDataBuffer.h"
#include "PypTags.h"



#ifdef NDEBUG
#define DEBUG_PRINT(format, ...)
#define DEBUG_VAR(setup)
#else
#define DEBUG_PRINT(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#define DEBUG_VAR(setup) setup
#endif



// Structs
typedef struct PypReadBlock_ {
	PypSize bufferSize;
	PypSize readLength;
	PypChar* buffer;

	struct PypReadBlock_* nextSibling;
	struct PypReadBlock_* previousSibling;
} PypReadBlock;

typedef struct PypReadRollbackEntry_ {
	PypStreamPosition streamPosition;
	PypSize blockPosition;
	PypSize arbitraryChars;
	PypReadBlock* block;
	const PypTag* tag;
} PypReadRollbackEntry;

typedef struct PypReadRollback_ {
	union {
		struct {
			PypReadRollbackEntry start;
			PypReadRollbackEntry mostRecent;
		};
		PypReadRollbackEntry entries[2];
	};
	PypBool active;
} PypReadRollback;

typedef struct PypTagStackEntry_ {
	const PypTag* tag;
	const PypTag* tagListFirst;
	struct PypTagStackEntry_* parent;
} PypTagStackEntry;

typedef struct PypTagStack_ {
	struct PypTagStackEntry_ head;
	struct PypTagStackEntry_* tail;
} PypTagStack;

typedef struct PypProcessingStackEntry_ {
	PypDataBuffer* dataBuffer;
	const PypProcessingInfo* processingInfo;
	PypProcessingInfo* processingInfoCustom;
	struct PypProcessingStackEntry_* parent;
	PypTagStackEntry* tagStackEntry;
	PypSize errorId;
	PypBool isContinuation;
	PypStreamLocation streamPositionFirst;
	PypStreamLocation* streamPositionLast;
} PypProcessingStackEntry;

typedef struct PypProcessingStack_ {
	struct PypProcessingStackEntry_ head;
	struct PypProcessingStackEntry_* tail;
} PypProcessingStack;

typedef struct PypReader_ {
	FILE* outputStream;
	FILE* errorStream;
	const PypReaderSettings* settings;

	PypReadStatus status;

	PypStreamPosition streamPosition;

	PypReadRollback rollback;
	PypTagStack tagStack;
	PypProcessingStack processingStack;

	PypSize processingPosition;

	PypReadBlock** ptrCurrentBlock;
	PypSize* ptrCurrentBlockPosition;
	PypSize* ptrArbitraryChars;

	void* data;

	PypChar mostRecentChar;
} PypReader;



// Headers
static PypReadBlock* pypReadBlockCircularListCreate(PypSize length, PypSize bufferSize);
static PypBool pypReadBlockExtendAfter(PypReadBlock* node, PypSize bufferSize);
static void pypReadBlockCircularListDelete(PypReadBlock* block);

static void pypTagStackEntryDelete(PypTagStackEntry* stackEntry);
static PypBool pypTagStackPush(PypReader* reader, const PypTag* tagListFirst);
static void pypTagStackPop(PypReader* reader);

static void pypProcessingStackEntryDelete(PypProcessingStackEntry* stackEntry);
static void pypProcessingStackEntryStreamPositionsDelete(PypProcessingStackEntry* stackEntry);
static PypBool pypProcessingStackPush(PypReader* reader, PypBool isContinuation, const PypProcessingInfo* processingInfo, PypDataBuffer* dataBuffer);
static PypBool pypProcessingStackTailUpdateOpening(PypReader* reader);
static PypBool pypProcessingStackPop(PypReader* reader);

static void pypProcessingStackTailUpdateStartPositionIncludingTag(PypReader* reader);
static void pypProcessingStackTailUpdateEndPositionIncludingTag(PypReader* reader);
static void pypProcessingStackTailUpdateStartPositionExcludingTag(PypReader* reader);
static void pypProcessingStackTailUpdateEndPositionExcludingTag(PypReader* reader);

static void pypUpdateStreamPosition(PypStreamPosition* streamPosition, PypChar c);

static PypBool pypProcessingStackPopProcess(PypReader* reader, PypProcessingStackEntry* source);
static PypBool pypProcessingStackModifyDataBuffer(PypReader* reader, PypProcessingStackEntry* source);
static PypBool pypProcessingStackModifyDataBufferUsingParent(PypReader* reader, PypProcessingStackEntry* source, PypBool success);
static PypBool pypReadProcessBlock(PypReader* reader, PypSize positionStart, PypSize positionEnd, const PypReadBlock* block);
static PypBool pypReadProcessTag(PypReader* reader, PypSize positionStart, const PypReadBlock* blockStart, PypSize positionEnd, const PypReadBlock* blockEnd);

static void pypReadRollbackReset(PypReader* reader);
static PypBool pypReadPerformAction(PypReader* reader);
static PypBool pypReadRollback(PypReader* reader);
static PypBool pypReadTagMatched(PypReader* reader, PypBool allowArbitraryChars);

static void pypReaderInit(PypReader* reader, FILE* inputStream, FILE* outputStream, FILE* errorStream, PypDataBuffer* dataBuffer, const PypProcessingInfo* processingInfo, const PypTagGroup* group, const PypReaderSettings* settings, PypReadBlock** ptrCurrentBlock, PypSize* ptrCurrentBlockPosition, PypSize* ptrArbitraryChars, void* data);
static void pypReaderClean(PypReader* reader);



// Create a circular list of stream reading blocks
PypReadBlock*
pypReadBlockCircularListCreate(PypSize length, PypSize bufferSize) {
	// Vars
	PypReadBlock* head;
	PypReadBlock* pre;
	PypReadBlock* next;

	// Assertions
	assert(length > 0);
	assert(bufferSize > 0);

	// Create head
	head = memAlloc(PypReadBlock);
	if (head == NULL) return NULL; // error

	// More vars
	pre = head;
	while (PYP_TRUE) {
		// Set data
		pre->bufferSize = bufferSize;
		pre->readLength = 0;
		pre->buffer = memAllocArray(PypChar, bufferSize);
		if (pre->buffer == NULL) break; // error

		// Decrement and terminate if 0
		if (--length == 0) break;

		next = memAlloc(PypReadBlock);
		if (next == NULL) break; // error

		pre->nextSibling = next;
		next->previousSibling = pre;

		// To next
		pre = next;
	}

	// Final link
	head->previousSibling = pre;
	pre->nextSibling = head;

	// Cleanup if error
	if (length > 0) {
		// Cleanup
		pypReadBlockCircularListDelete(head);
		return NULL;
	}

	// Done
	return head;
}

// Extend a circular list after a specified node
PypBool
pypReadBlockExtendAfter(PypReadBlock* node, PypSize bufferSize) {
	// Vars
	PypReadBlock* next;

	// Assertions
	assert(node != NULL);
	assert(bufferSize > 0);

	// Create new node
	next = memAlloc(PypReadBlock);
	if (next == NULL) return PYP_FALSE; // error

	// More vars
	next->bufferSize = bufferSize;
	next->readLength = 0;
	next->buffer = memAllocArray(PypChar, bufferSize);
	if (next->buffer == NULL) {
		// Cleanup
		memFree(next);
		return PYP_FALSE;
	}

	// Link
	next->nextSibling = node->nextSibling;
	next->previousSibling = node;
	node->nextSibling = next;
	next->nextSibling->previousSibling = next;

	// Okay
	return PYP_TRUE;
}

// Delete a circular list of stream reading blocks
void
pypReadBlockCircularListDelete(PypReadBlock* block) {
	// Vars
	PypReadBlock* start;
	PypReadBlock* next;

	// Assertions
	assert(block != NULL);

	// Start
	start = block;
	while (PYP_TRUE) {
		// Delete
		next = block->nextSibling;
		memFree(block->buffer);
		memFree(block);

		// Terminate
		if (next == start) break;

		// Next
		block = next;
	}
}



// Delete a stream reading stack entry
void
pypTagStackEntryDelete(PypTagStackEntry* stackEntry) {
	assert(stackEntry != NULL);

	memFree(stackEntry);
}

// Push onto the stream reading stack
PypBool
pypTagStackPush(PypReader* reader, const PypTag* tagListFirst) {
	// Vars
	PypTagStackEntry* stackEntryNew;

	// Assertions
	assert(reader != NULL);
	assert(tagListFirst != NULL);

	// New stack entry
	stackEntryNew = memAlloc(PypTagStackEntry);
	if (stackEntryNew == NULL) {
		// Error
		reader->status = PYP_READ_ERROR_MEMORY;
		return PYP_FALSE;
	}

	// Members
	stackEntryNew->parent = reader->tagStack.tail;
	stackEntryNew->tag = NULL;
	stackEntryNew->tagListFirst = tagListFirst;

	// Update stack
	reader->tagStack.tail = stackEntryNew;

	// Okay
	return PYP_TRUE;
}

// Pop off of the stream reading stack
void
pypTagStackPop(PypReader* reader) {
	// Vars
	PypTagStackEntry* stackEntryPre;

	// Assertions
	assert(reader != NULL);
	assert(reader->tagStack.tail != NULL);
	assert(reader->tagStack.tail->parent != NULL);

	// Update
	stackEntryPre = reader->tagStack.tail;
	reader->tagStack.tail = reader->tagStack.tail->parent;

	// Delete
	pypTagStackEntryDelete(stackEntryPre);
}



// Delete a processing stack entry
void
pypProcessingStackEntryDelete(PypProcessingStackEntry* stackEntry) {
	assert(stackEntry != NULL);

	// Delete
	if (stackEntry->dataBuffer != NULL) pypDataBufferDelete(stackEntry->dataBuffer);
	if (stackEntry->processingInfoCustom != NULL) pypProcessingInfoDelete(stackEntry->processingInfoCustom);
	pypProcessingStackEntryStreamPositionsDelete(stackEntry);
	memFree(stackEntry);
}

// Delete a processing stack entry
void
pypProcessingStackEntryStreamPositionsDelete(PypProcessingStackEntry* stackEntry) {
	// Vars
	PypStreamLocation* sp;
	PypStreamLocation* next;

	// Assertions
	assert(stackEntry != NULL);

	// Delete
	for (sp = stackEntry->streamPositionFirst.nextSibling; sp != NULL; sp = next) {
		// Delete
		next = sp->nextSibling;
		memFree(sp);
	}
}

// Push onto the processing stack
PypBool
pypProcessingStackPush(PypReader* reader, PypBool isContinuation, const PypProcessingInfo* processingInfo, PypDataBuffer* dataBuffer) {
	// Vars
	PypProcessingStackEntry* stackEntryNew;

	// Assertions
	assert(reader != NULL);
	assert(processingInfo != NULL);
	assert(dataBuffer != NULL);

	// New stack entry
	stackEntryNew = memAlloc(PypProcessingStackEntry);
	if (stackEntryNew == NULL) {
		// Error
		reader->status = PYP_READ_ERROR_MEMORY;
		return PYP_FALSE;
	}

	// Members
	stackEntryNew->parent = reader->processingStack.tail;
	stackEntryNew->dataBuffer = dataBuffer;
	stackEntryNew->processingInfo = processingInfo;
	stackEntryNew->processingInfoCustom = NULL;
	stackEntryNew->isContinuation = isContinuation;
	stackEntryNew->errorId = PYP_READER_ERROR_ID_NO_ERROR;
	stackEntryNew->tagStackEntry = reader->tagStack.tail;

	stackEntryNew->streamPositionFirst.nextSibling = NULL;
	stackEntryNew->streamPositionLast = &stackEntryNew->streamPositionFirst;

	// Update stack
	reader->processingStack.tail = stackEntryNew;

	// Okay
	return PYP_TRUE;
}

// Pop off of the processing stack
PypBool
pypProcessingStackPop(PypReader* reader) {
	// Vars
	PypProcessingStackEntry* stackEntryPre;

	// Assertions
	assert(reader != NULL);
	assert(reader->processingStack.tail != NULL);
	assert(reader->processingStack.tail->parent != NULL);
	assert(reader->processingStack.tail->dataBuffer != NULL);

	// Update
	stackEntryPre = reader->processingStack.tail;
	reader->processingStack.tail = reader->processingStack.tail->parent;

	// Process
	if (stackEntryPre->dataBuffer->totalSize > 0) {
		if (!pypProcessingStackPopProcess(reader, stackEntryPre)) return PYP_FALSE; // error
	}

	// Delete
	pypProcessingStackEntryDelete(stackEntryPre);

	// Okay
	return PYP_TRUE;
}

// Update the tail of the processing stack
PypBool
pypProcessingStackTailUpdateOpening(PypReader* reader) {
	// Vars
	PypStreamLocation* nextLocation;

	// Assertions
	assert(reader != NULL);
	assert(reader->processingStack.tail != NULL);
	assert(reader->processingStack.tail->parent != NULL);
	assert(reader->processingStack.tail->dataBuffer != NULL);

	// Create new tag location
	nextLocation = memAlloc(PypStreamLocation);
	if (nextLocation == NULL) return PYP_FALSE;

	// Link
	nextLocation->nextSibling = NULL;
	reader->processingStack.tail->streamPositionLast->nextSibling = nextLocation;
	reader->processingStack.tail->streamPositionLast = nextLocation;

	// Update position
	pypProcessingStackTailUpdateStartPositionIncludingTag(reader);
	pypProcessingStackTailUpdateEndPositionIncludingTag(reader);

	// Update tag entry
	reader->processingStack.tail->tagStackEntry = reader->tagStack.tail;

	// Okay
	return PYP_TRUE;
}



// Update line/position counters
void
pypProcessingStackTailUpdateStartPositionIncludingTag(PypReader* reader) {
	assert(reader != NULL);

	// Update tag position starting at the most recent tag
	reader->processingStack.tail->streamPositionLast->start = reader->rollback.start.streamPosition;
}

void
pypProcessingStackTailUpdateEndPositionIncludingTag(PypReader* reader) {
	assert(reader != NULL);

	// Update tag position starting at the most recent tag
	reader->processingStack.tail->streamPositionLast->end = reader->rollback.start.streamPosition;
}

void
pypProcessingStackTailUpdateStartPositionExcludingTag(PypReader* reader) {
	assert(reader != NULL);

	// Update tag position starting at the current position
	reader->processingStack.tail->streamPositionLast->start = reader->streamPosition;
	pypUpdateStreamPosition(&reader->processingStack.tail->streamPositionLast->start, reader->mostRecentChar);
}

void
pypProcessingStackTailUpdateEndPositionExcludingTag(PypReader* reader) {
	assert(reader != NULL);

	// Update tag position starting at the current position
	reader->processingStack.tail->streamPositionLast->end = reader->streamPosition;
	pypUpdateStreamPosition(&reader->processingStack.tail->streamPositionLast->end, reader->mostRecentChar);
}



// Update the line/position counter given a char
void
pypUpdateStreamPosition(PypStreamPosition* streamPosition, PypChar c) {
	if (c == '\r') {
		++streamPosition->lineNumber;
		streamPosition->linePosition = 0;
		streamPosition->newlineCompletion = 1;
	}
	else if (c == '\n') {
		if (streamPosition->newlineCompletion != 1) {
			++streamPosition->lineNumber;
			streamPosition->linePosition = 0;
		}
		streamPosition->newlineCompletion = 2;
	}
	else {
		++streamPosition->linePosition;
		streamPosition->newlineCompletion = 0;
	}
	++streamPosition->charPosition;
}



// Functions for processing the data
PypBool
pypProcessingStackModifyDataBuffer(PypReader* reader, PypProcessingStackEntry* source) {
	// Vars
	PypDataBuffer* modifiedData = NULL;
	PypReadStatus status = PYP_READ_OKAY;

	// Assertions
	assert(reader != NULL);
	assert(source != NULL);

	// Modify data
	if (source->processingInfo->selfModifier != NULL) {
		// Modify
		status = (source->processingInfo->selfModifier)(source->dataBuffer, &modifiedData, &source->streamPositionFirst, reader->data);

		// Delete old
		assert(modifiedData != source->dataBuffer);
		pypDataBufferDelete(source->dataBuffer);

		if (status != PYP_READ_OKAY && status != PYP_READ_ERROR_CODE_EXECUTION) {
			// Error
			assert(modifiedData == NULL);
			source->dataBuffer = NULL;
			reader->status = status;
			return PYP_FALSE;
		}
		else {
			// Delete old
			assert(modifiedData != NULL);
			source->dataBuffer = modifiedData;
		}
	}

	// Modify again
	return pypProcessingStackModifyDataBufferUsingParent(reader, source, status == PYP_READ_OKAY);
}

PypBool
pypProcessingStackModifyDataBufferUsingParent(PypReader* reader, PypProcessingStackEntry* source, PypBool success) {
	// Vars
	PypDataBufferModifier modifier;
	PypDataBuffer* modifiedData;
	PypReadStatus status;

	// Assertions
	assert(reader != NULL);
	assert(source != NULL);

	// Modify data
	modifier = (success ? source->parent->processingInfo->childSuccessModifier : source->parent->processingInfo->childFailureModifier);
	modifiedData = NULL;
	status = PYP_READ_OKAY;


	// Change modifier
	if (modifier != NULL) {
		// Modify
		status = (modifier)(source->dataBuffer, &modifiedData, &source->streamPositionFirst, reader->data);

		// Delete old
		assert(modifiedData != source->dataBuffer);
		pypDataBufferDelete(source->dataBuffer);

		if (status != PYP_READ_OKAY && status != PYP_READ_ERROR_CODE_EXECUTION) {
			// Error
			assert(modifiedData == NULL);
			source->dataBuffer = NULL;
			reader->status = status;
			return PYP_FALSE;
		}
		else {
			// Delete old
			assert(modifiedData != NULL);
			source->dataBuffer = modifiedData;
		}
	}


	// Done
	return PYP_TRUE;
}

PypBool
pypProcessingStackPopProcess(PypReader* reader, PypProcessingStackEntry* source) {
	// Vars

	// Assertions
	assert(reader != NULL);
	assert(source->dataBuffer != NULL);
	assert(source->dataBuffer->totalSize > 0);
	assert(source->processingInfo != NULL);
	assert(source->parent == reader->processingStack.tail);
	assert(reader->processingStack.tail->processingInfo != NULL);


	// Modify data
	if (source->errorId == PYP_READER_ERROR_ID_NO_ERROR) {
		// No errors
		if (!pypProcessingStackModifyDataBuffer(reader, source)) return PYP_FALSE;
	}
	else {
		// Overwrite
		if (reader->errorStream == NULL) {
			// Delete contents
			pypDataBufferEmpty(source->dataBuffer);
			if (reader->settings->errorMessages[source->errorId] != NULL) {
				// Replace content with error message
				PypDataBufferEntry* newEntry = pypDataBufferExtendWithString(source->dataBuffer, reader->settings->errorMessages[source->errorId]);
				if (newEntry == NULL) {
					// Error
					reader->status = PYP_READ_ERROR_MEMORY;
					return PYP_FALSE;
				}
			}

			// Process error message
			if (!pypProcessingStackModifyDataBufferUsingParent(reader, source, (reader->settings->flags & PYP_READER_FLAG_TREAT_SYNTAX_ERRORS_AS_SUCCESS) != 0)) return PYP_FALSE;
		}
		else {
			// Direct to error stream
			PypSize writeLength;
			const PypChar* errorMessage = reader->settings->errorMessages[source->errorId];
			PypSize errorMessageLength = strlen(errorMessage);

			writeLength = fwrite(errorMessage, sizeof(PypChar), errorMessageLength, reader->errorStream);
			if (writeLength != errorMessageLength) {
				// Error
				reader->status = PYP_READ_ERROR_WRITE;
				return PYP_FALSE;
			}

			// Done
			return PYP_TRUE;
		}
	}
	assert(source->dataBuffer != NULL);


	// Process and feed data back into parent
	if (reader->processingStack.tail->dataBuffer == NULL) {
		PypSize writeLength;
		PypDataBufferEntry* bufferEntry;

		assert(reader->outputStream != NULL);

		// Add to the output stream
		for (bufferEntry = source->dataBuffer->firstChild; bufferEntry != NULL; bufferEntry = bufferEntry->nextSibling) {
			// Output
			writeLength = fwrite(bufferEntry->buffer, sizeof(PypChar), bufferEntry->bufferLength, reader->outputStream);
			if (writeLength != bufferEntry->bufferLength) {
				// Error
				reader->status = PYP_READ_ERROR_WRITE;
				return PYP_FALSE;
			}
		}
	}
	else {
		// Add to the buffer
		pypDataBufferExtendWithDataBufferAndDelete(reader->processingStack.tail->dataBuffer, source->dataBuffer);
		source->dataBuffer = NULL;
	}

	// Okay
	return PYP_TRUE;
}

PypBool
pypReadProcessBlock(PypReader* reader, PypSize positionStart, PypSize positionEnd, const PypReadBlock* block) {
	// Vars
	PypSize bufferLength;
	const PypChar* buffer;

	// Assertions
	assert(reader != NULL);
	assert(block != NULL);

	// Nothing to do
	if (positionStart == positionEnd) return PYP_TRUE;

	// Get the buffer
	bufferLength = positionEnd - positionStart;
	buffer = &(block->buffer[positionStart]);

	// Process the data
	if (reader->processingStack.tail->dataBuffer == NULL) {
		PypSize writeLength;

		assert(reader->outputStream != NULL);

		// Add to the output stream
		writeLength = fwrite(buffer, sizeof(PypChar), bufferLength, reader->outputStream);
		if (writeLength != bufferLength) {
			// Error
			reader->status = PYP_READ_ERROR_WRITE;
			return PYP_FALSE;
		}
	}
	else {
		// Add to the buffer
		PypDataBufferEntry* entry = pypDataBufferExtendWithData(reader->processingStack.tail->dataBuffer, buffer, bufferLength);
		if (entry == NULL) {
			// Error
			reader->status = PYP_READ_ERROR_MEMORY;
			return PYP_FALSE;
		}
	}

	// Okay
	return PYP_TRUE;
}

PypBool
pypReadProcessTag(PypReader* reader, PypSize positionStart, const PypReadBlock* blockStart, PypSize positionEnd, const PypReadBlock* blockEnd) {
	// Assertions
	assert(reader != NULL);
	assert(blockStart != NULL);
	assert(blockEnd != NULL);

	while (blockStart != blockEnd) {
		// Process
		if (!pypReadProcessBlock(reader, positionStart, blockStart->readLength, blockStart)) return PYP_FALSE;

		// Next
		positionStart = 0;
		blockStart = blockStart->nextSibling;
	}
	if (!pypReadProcessBlock(reader, positionStart, positionEnd + 1, blockStart)) return PYP_FALSE;

	// Okay
	return PYP_TRUE;
}



// Supplementary actions
void
pypReadRollbackReset(PypReader* reader) {
	assert(reader != NULL);
	assert(reader->rollback.active);
	assert(reader->rollback.start.tag != NULL);

	// Update stack
	reader->tagStack.tail->tag = NULL;
	reader->tagStack.tail->tagListFirst = reader->rollback.start.tag;

	// Update rollback
	reader->rollback.active = PYP_FALSE;
	reader->rollback.start.block = NULL;
	reader->rollback.mostRecent.arbitraryChars = 0;
	// other stuff not nullified because not necessary; would also mess up "pypReadPerformAction"
}

PypBool
pypReadPerformAction(PypReader* reader) {
	// Vars
	PypReadBlock* blockStart;
	PypReadBlock* blockEnd;
	PypSize positionStart;
	PypSize positionEnd;
	PypDataBuffer* dataBuffer;
	PypProcessingInfo* processingInfoNext;
	PypSize errorId;

	// Assertions
	assert(reader != NULL);
	assert(reader->rollback.mostRecent.tag != NULL);
	DEBUG_PRINT(
		"  ACTION: %s; part=%s; pos=%d\n",
		pypTagIsClosing(reader->rollback.mostRecent.tag) ? "closing" : (reader->rollback.mostRecent.tag->children == NULL ? "complete" : "opening"),
		reader->rollback.mostRecent.tag->text,
		*reader->ptrCurrentBlockPosition
	);

	// Setup vars
	blockStart = reader->rollback.start.block;
	blockEnd = *reader->ptrCurrentBlock;
	positionStart = reader->rollback.start.blockPosition;
	positionEnd = *reader->ptrCurrentBlockPosition;

	// Reset rollback
	pypReadRollbackReset(reader);


	// Previous data
	pypReadProcessBlock(reader, reader->processingPosition, positionStart, blockStart);
	reader->processingPosition = positionEnd + 1; // Update position to the end of the tag


	// Check for a children group
	if (reader->rollback.mostRecent.tag->children != NULL) {
		assert(pypTagIsComplete(reader->rollback.mostRecent.tag));

		// New stack entry
		if (!pypTagStackPush(reader, reader->rollback.mostRecent.tag->children->firstChild)) return PYP_FALSE; // error

		// No error
		errorId = PYP_READER_ERROR_ID_NO_ERROR;

		// Check if it's a continuation
		if (pypTagIsContinuation(reader->rollback.mostRecent.tag)) {
			assert(reader->rollback.mostRecent.tag->processingInfo != NULL);

			if (reader->processingStack.tail->isContinuation) {
				if (
					reader->processingStack.tail->parent->processingInfo == reader->rollback.mostRecent.tag->processingInfo ||
					(reader->settings->flags & (PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_ERROR | PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_CONTINUE)) == PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_CONTINUE
				) {
					assert(reader->processingStack.tail->processingInfoCustom != NULL);

					// Pop
					pypProcessingStackTailUpdateEndPositionIncludingTag(reader);
					if (!pypProcessingStackPop(reader)) return PYP_FALSE; // error

					// Update current stack entry
					if (!pypProcessingStackTailUpdateOpening(reader)) return PYP_FALSE; // error

					// Okay (to skip over the rest of the stuff)
					return PYP_TRUE;
				}
				else {
					// Continuation tag mismatch
					pypProcessingStackTailUpdateEndPositionIncludingTag(reader);
					if (!pypProcessingStackPop(reader)) return PYP_FALSE; // error

					// Update error
					if ((reader->settings->flags & PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_ERROR) != 0) {
						errorId = PYP_READER_ERROR_ID_CONTINUATION_MISMATCHED_OPENING_TAG;
						reader->processingStack.tail->errorId = PYP_READER_ERROR_ID_CONTINUATION_MISMATCHED_CLOSING_TAG;
					}

					// Pop second time
					pypProcessingStackTailUpdateEndPositionIncludingTag(reader);
					if (!pypProcessingStackPop(reader)) return PYP_FALSE; // error
				}
			}
			else {
				// Update error
				if ((reader->settings->flags & PYP_READER_FLAG_ON_CONTINUATION_UNMATCHED_TAG_ERROR) != 0) {
					errorId = PYP_READER_ERROR_ID_CONTINUATION_UNMATCHED_OPENING_TAG;
				}
			}
		}

		if (reader->rollback.mostRecent.tag->processingInfo == NULL) {
			// Process the tag
			if (!pypReadProcessTag(reader, positionStart, blockStart, positionEnd, blockEnd)) return PYP_FALSE; // error
		}
		else {
			// Create a new data buffer
			dataBuffer = pypDataBufferCreate();
			if (dataBuffer == NULL) {
				// Error
				reader->status = PYP_READ_ERROR_MEMORY;
				return PYP_FALSE;
			}

			// Update stack
			if (!pypProcessingStackPush(reader, PYP_FALSE, reader->rollback.mostRecent.tag->processingInfo, dataBuffer)) {
				// Cleanup
				pypDataBufferDelete(dataBuffer);
				return PYP_FALSE;
			}
			reader->processingStack.tail->errorId = errorId;

			// Update line counters
			pypProcessingStackTailUpdateStartPositionIncludingTag(reader);
			pypProcessingStackTailUpdateEndPositionIncludingTag(reader);
		}
	}
	else {
		if (pypTagIsClosing(reader->rollback.mostRecent.tag)) {
			// Only modify processing stack if the tag stack entry matches
			if (reader->tagStack.tail == reader->processingStack.tail->tagStackEntry) {
				if (
					pypTagIsContinuation(reader->rollback.mostRecent.tag) &&
					(
						reader->processingStack.tail->errorId == PYP_READER_ERROR_ID_NO_ERROR || // there must be no error
						(reader->settings->flags & PYP_READER_FLAG_ON_CONTINUATION_ALLOW_LATE_ERROR_OUTPUT) != 0 // or late error output must be enabled
					)
				) {
					// Create a new data buffer
					dataBuffer = pypDataBufferCreate();
					if (dataBuffer == NULL) {
						// Error
						reader->status = PYP_READ_ERROR_MEMORY;
						return PYP_FALSE;
					}

					// Create a new processing info
					processingInfoNext = pypProcessingInfoCreate(reader->processingStack.tail->processingInfo->continuationModifier, NULL, NULL, NULL);
					if (processingInfoNext == NULL) {
						// Error
						reader->status = PYP_READ_ERROR_MEMORY;
						pypDataBufferDelete(dataBuffer);
						return PYP_FALSE;
					}

					// Push to the stack
					pypProcessingStackTailUpdateEndPositionExcludingTag(reader);
					if (!pypProcessingStackPush(reader, PYP_TRUE, processingInfoNext, dataBuffer)) {
						// Error
						pypProcessingInfoDelete(processingInfoNext);
						pypDataBufferDelete(dataBuffer);
						return PYP_FALSE;
					}
					reader->processingStack.tail->processingInfoCustom = processingInfoNext;

					// Update line counters
					pypProcessingStackTailUpdateStartPositionExcludingTag(reader);
					pypProcessingStackTailUpdateEndPositionExcludingTag(reader);
				}
				else {
					// Pop
					pypProcessingStackTailUpdateEndPositionExcludingTag(reader);
					if (!pypProcessingStackPop(reader)) return PYP_FALSE; // error
				}
			}
			else {
				// Process the tag
				if (!pypReadProcessTag(reader, positionStart, blockStart, positionEnd, blockEnd)) return PYP_FALSE; // error
			}

			// Pop from the stack
			pypTagStackPop(reader);
		}
		else {
			// It's an "escape" sequence; process the tag
			if (!pypReadProcessTag(reader, positionStart, blockStart, positionEnd, blockEnd)) return PYP_FALSE; // error
		}
	}

	// Okay
	return PYP_TRUE;
}

PypBool
pypReadRollback(PypReader* reader) {
	// Vars

	// Assertions
	assert(reader != NULL);
	assert(reader->rollback.active);
	DEBUG_PRINT(
		"  ROLLBACK: block_change=%s; position=%d->%d; arbitrary_chars=%d;\n",
		(*reader->ptrCurrentBlock == reader->rollback.mostRecent.block) ? "false" : "true",
		*reader->ptrCurrentBlockPosition,
		reader->rollback.mostRecent.blockPosition,
		reader->rollback.mostRecent.arbitraryChars
	);

	// Rollback
	*reader->ptrCurrentBlockPosition = reader->rollback.mostRecent.blockPosition;
	*reader->ptrCurrentBlock = reader->rollback.mostRecent.block;
	*reader->ptrArbitraryChars = reader->rollback.mostRecent.arbitraryChars;

	// Rollback continuation
	if ((*reader->ptrArbitraryChars) == 0) {
		if (reader->rollback.mostRecent.tag != NULL) {
			// Perform action
			if (!pypReadPerformAction(reader)) return PYP_FALSE; // error
		}
		else {
			// No completed tag
			pypReadRollbackReset(reader);
		}
	}
	else {
		// Arbitrary characters need to be found
		assert(reader->rollback.mostRecent.tag != NULL);
		reader->tagStack.tail->tag = reader->rollback.mostRecent.tag;
	}

	// Okay
	return PYP_TRUE;
}

PypBool
pypReadTagMatched(PypReader* reader, PypBool allowArbitraryChars) {
	// Assertions
	assert(reader != NULL);
	assert(reader->tagStack.tail->tag != NULL);

	// Check if it can be used as a completed tag
	if (pypTagIsComplete(reader->tagStack.tail->tag)) {
		// Setup rollback for completion
		reader->rollback.mostRecent.tag = reader->tagStack.tail->tag;
		reader->rollback.mostRecent.blockPosition = *reader->ptrCurrentBlockPosition;
		reader->rollback.mostRecent.block = *reader->ptrCurrentBlock;
		if (allowArbitraryChars) reader->rollback.mostRecent.arbitraryChars = reader->rollback.mostRecent.tag->arbitraryChars;
	}

	// Check if the tag has no chilren (i.e. it is complete match)
	if (reader->tagStack.tail->tag->firstChild == NULL) {
		if (allowArbitraryChars && reader->rollback.mostRecent.arbitraryChars > 0) {
			// Arbitrary characters must be read in
			*(reader->ptrArbitraryChars) = reader->rollback.mostRecent.arbitraryChars;
		}
		else {
			// Perform an action
			if (!pypReadPerformAction(reader)) return PYP_FALSE; // error
		}
	}
	else {
		if (allowArbitraryChars) {
			// Continue in case there's a longer match
			reader->tagStack.tail->tagListFirst = reader->tagStack.tail->tag->firstChild;
			reader->tagStack.tail->tag = NULL;
		}
		else {
			// This occured after an arbitrary character string; perform an action
			if (!pypReadPerformAction(reader)) return PYP_FALSE; // error
		}
	}

	// Okay
	return PYP_TRUE;
}



// Setup a reader object
void
pypReaderInit(PypReader* reader, FILE* inputStream, FILE* outputStream, FILE* errorStream, PypDataBuffer* dataBuffer, const PypProcessingInfo* processingInfo, const PypTagGroup* group, const PypReaderSettings* settings, PypReadBlock** ptrCurrentBlock, PypSize* ptrCurrentBlockPosition, PypSize* ptrArbitraryChars, void* data) {
	// Vars
	PypSize i;

	// Assertions
	assert(reader != NULL);
	assert(inputStream != NULL);
	assert(outputStream != NULL);
	assert(processingInfo != NULL);
	assert(settings != NULL);
	assert(group != NULL);
	assert(group->firstChild != NULL);
	assert(settings != NULL);
	assert(ptrCurrentBlock != NULL);
	assert(ptrCurrentBlockPosition != NULL);
	assert(ptrArbitraryChars != NULL);

	// Setup reader
	reader->status = PYP_READ_OKAY;

	reader->streamPosition.charPosition = 0;
	reader->streamPosition.lineNumber = 0;
	reader->streamPosition.linePosition = 0;
	reader->streamPosition.newlineCompletion = 0;

	reader->ptrCurrentBlock = ptrCurrentBlock;
	reader->ptrCurrentBlockPosition = ptrCurrentBlockPosition;
	reader->ptrArbitraryChars = ptrArbitraryChars;

	reader->processingPosition = 0;
	reader->outputStream = outputStream;
	reader->errorStream = errorStream;

	reader->data = data;
	reader->settings = settings;

	// Rollback members
	reader->rollback.active = PYP_FALSE;
	for (i = 0; i < 2; ++i) {
		reader->rollback.entries[i].streamPosition = reader->streamPosition;
		reader->rollback.entries[i].blockPosition = 0;
		reader->rollback.entries[i].arbitraryChars = 0;
		reader->rollback.entries[i].block = NULL;
		reader->rollback.entries[i].tag = NULL;
	}

	// Tag stack members
	reader->tagStack.head.parent = NULL;
	reader->tagStack.head.tag = NULL;
	reader->tagStack.head.tagListFirst = group->firstChild;
	reader->tagStack.tail = &reader->tagStack.head;

	// Processing stack members
	reader->processingStack.head.dataBuffer = dataBuffer;
	reader->processingStack.head.processingInfo = processingInfo;
	reader->processingStack.head.parent = NULL;
	reader->processingStack.head.isContinuation = PYP_FALSE;
	reader->processingStack.head.errorId = PYP_READER_ERROR_ID_NO_ERROR;
	reader->processingStack.head.tagStackEntry = reader->tagStack.tail;

	reader->processingStack.head.streamPositionFirst.start = reader->streamPosition;
	reader->processingStack.head.streamPositionFirst.end = reader->streamPosition;
	reader->processingStack.head.streamPositionFirst.nextSibling = NULL;
	reader->processingStack.head.streamPositionLast = &reader->processingStack.head.streamPositionFirst;

	reader->processingStack.tail = &reader->processingStack.head;
}

// Delete data related to a reader object, but don't delete the reader itself
void
pypReaderClean(PypReader* reader) {
	// Vars
	PypTagStackEntry* tsEntry;
	PypTagStackEntry* tsNext;
	PypProcessingStackEntry* psEntry;
	PypProcessingStackEntry* psNext;

	// Assertions
	assert(reader != NULL);

	// Delete tag stack
	for (tsEntry = reader->tagStack.tail; tsEntry != &reader->tagStack.head; tsEntry = tsNext) {
		assert(tsEntry != NULL);
		tsNext = tsEntry->parent;
		pypTagStackEntryDelete(tsEntry);
	}

	// Delete processing stack
	for (psEntry = reader->processingStack.tail; psEntry != &reader->processingStack.head; psEntry = psNext) {
		assert(psEntry != NULL);
		psNext = psEntry->parent;
		pypProcessingStackEntryDelete(psEntry);
	}

	// Delete other stuff (this probably actually isn't needed)
	pypProcessingStackEntryStreamPositionsDelete(&reader->processingStack.head);
}



// Read from a stream
PypReadStatus
pypReadFromStream(FILE* inputStream, FILE* outputStream, FILE* errorStream, PypDataBuffer* dataBuffer, const PypProcessingInfo* processingInfo, const PypTagGroup* group, const PypReaderSettings* settings, void* data) {
	// Vars
	PypSize arbitraryChars = 0;
	PypSize tagPos = 0;
	PypSize i = 0;
	PypChar c;
	PypReadStatus status;
	PypReadBlock* currentBlock;
	PypReadBlock* lastReadBlock;
	PypReader reader;

	// Assertions
	assert(inputStream != NULL);
	assert(outputStream != NULL);
	assert(processingInfo != NULL);
	assert(group != NULL);
	assert(settings != NULL);


	// Setup circular array; this is used if any rollback that started in a previous read-block
	currentBlock = pypReadBlockCircularListCreate(settings->readBlockCount, settings->readBlockSize);
	if (currentBlock == NULL) return PYP_READ_ERROR_MEMORY;
	lastReadBlock = currentBlock->previousSibling;

	// Reader
	pypReaderInit(&reader, inputStream, outputStream, errorStream, dataBuffer, processingInfo, group, settings, &currentBlock, &i, &arbitraryChars, data);


	// Read loop
	while (PYP_TRUE) {
		DEBUG_VAR(int readingNew = (currentBlock->previousSibling == lastReadBlock));

		// Read block
		if (currentBlock->previousSibling == lastReadBlock) {
			currentBlock->readLength = fread(currentBlock->buffer, sizeof(PypChar), currentBlock->bufferSize, inputStream);
			lastReadBlock = currentBlock;
		}
		DEBUG_PRINT(
			"READ_LOOP: rollback=%s; mode=%s; length=%d;\n",
			reader.rollback.active ? "active  " : "inactive",
			readingNew ? "new" : "pre",
			currentBlock->readLength
		);



		// Iterate
		while (i < currentBlock->readLength) {
			// Get char
			c = currentBlock->buffer[i];
			reader.mostRecentChar = c;

			// Search for match
			if (reader.tagStack.tail->tag == NULL) {
				assert(reader.tagStack.tail->tagListFirst != NULL);
				assert(reader.tagStack.tail->tagListFirst->text != NULL);
				reader.tagStack.tail->tag = reader.tagStack.tail->tagListFirst;

				while (PYP_TRUE) {
					// Match
					if (reader.tagStack.tail->tag->text[0] == c) {
						// Match
						tagPos = 1;
						if (!reader.rollback.active) {
							// Setup rollback if its not already active
							reader.rollback.active = PYP_TRUE;

							reader.rollback.start.blockPosition = i;
							reader.rollback.start.block = currentBlock;
							reader.rollback.start.arbitraryChars = 0;
							reader.rollback.start.tag = reader.tagStack.tail->tagListFirst;
							reader.rollback.start.streamPosition = reader.streamPosition;

							reader.rollback.mostRecent.blockPosition = i;
							reader.rollback.mostRecent.block = currentBlock;
							reader.rollback.mostRecent.arbitraryChars = 0;
							reader.rollback.mostRecent.tag = NULL;
							reader.rollback.start.streamPosition = reader.streamPosition;
						}
						if (tagPos >= reader.tagStack.tail->tag->textLength) {
							// This tag has been matched (so far)
							if (!pypReadTagMatched(&reader, PYP_TRUE)) goto cleanup; // error
						}

						// Match okay; continue to next character
						break;
					}

					// Next
					reader.tagStack.tail->tag = reader.tagStack.tail->tag->nextSibling;
					if (reader.tagStack.tail->tag == NULL) {
						// No matches found
						if (reader.rollback.active) {
							// Rollback action
							if (!pypReadRollback(&reader)) goto cleanup; // error
						}

						// Continue to next character
						break;
					}
				}
			}
			else {
				// Rollback at this point is active, since at least 1 char has been matched
				assert(reader.rollback.active);

				if (arbitraryChars > 0) {
					if (--arbitraryChars == 0) {
						// Tag has been matched (so far)
						if (!pypReadTagMatched(&reader, PYP_FALSE)) goto cleanup; // error

						assert(reader.tagStack.tail->tag == NULL);
						assert(arbitraryChars == 0);
					}
				}
				else if (reader.tagStack.tail->tag->text[tagPos] == c) {
					if (++tagPos >= reader.tagStack.tail->tag->textLength) {
						// Tag has been matched (so far)
						if (!pypReadTagMatched(&reader, PYP_TRUE)) goto cleanup; // error

						assert(reader.tagStack.tail->tag == NULL || arbitraryChars > 0);
					}
				}
				else {
					// Rollback action
					if (!pypReadRollback(&reader)) goto cleanup; // error

					assert(reader.tagStack.tail->tag == NULL || arbitraryChars > 0);
				}
			}

			// Position updating
			pypUpdateStreamPosition(&reader.streamPosition, c);

			// Next
			++i;
		}



		// Process block
		if (!reader.rollback.active) {
			// Previous data
			pypReadProcessBlock(&reader, reader.processingPosition, currentBlock->readLength, currentBlock);
			reader.processingPosition = 0;
		}



		// Done?
		if (currentBlock->readLength < currentBlock->bufferSize) {
			// Rollback if necessary
			if (reader.rollback.active) {
				// Roll back
				if (!pypReadRollback(&reader)) goto cleanup; // error
				++i; // Go to next position, so no infinite loop
				continue;
			}

			// Done
			break;
		}



		// Buffer swapping
		assert(currentBlock->nextSibling != NULL);
		if (currentBlock->nextSibling == reader.rollback.start.block) {
			// New buffer required
			DEBUG_PRINT("Adding new read block\n");
			if (!pypReadBlockExtendAfter(currentBlock, settings->readBlockSize)) {
				// Error
				reader.status = PYP_READ_ERROR_MEMORY;
				goto cleanup;
			}
		}
		// Swap
		currentBlock = currentBlock->nextSibling;


		// Reset position
		i = 0;
	}


	// Close any open processing stack entries
	while (reader.processingStack.tail != &reader.processingStack.head) {
		// Update error
		if ((reader.settings->flags & PYP_READER_FLAG_ON_UNCLOSED_TAG_ERROR) != 0) {
			reader.processingStack.tail->errorId = PYP_READER_ERROR_ID_UNCLOSED_TAG;
		}

		// Pop
		pypProcessingStackTailUpdateEndPositionIncludingTag(&reader);
		if (!pypProcessingStackPop(&reader)) goto cleanup; // error
	}


	// Cleanup
	cleanup:
	status = reader.status;
	pypReaderClean(&reader);
	pypReadBlockCircularListDelete(currentBlock);

	// Okay
	return status;
}



// Create reader settings
PypReaderSettings*
pypReaderSettingsCreate(PypReaderFlags flags, PypSize readBlockCount, PypSize readBlockSize) {
	PypReaderSettings* readSettings;
	size_t i;

	readSettings = memAlloc(PypReaderSettings);
	if (readSettings == NULL) return NULL; // error

	readSettings->flags = flags;
	readSettings->readBlockCount = readBlockCount;
	readSettings->readBlockSize = readBlockSize;

	for (i = 0; i < PYP_READER_ERROR_ID_COUNT; ++i) {
		readSettings->errorMessages[i] = NULL;
	}

	return readSettings;
}

// Delete reader settings
void
pypReaderSettingsDelete(PypReaderSettings* readSettings) {
	// Assertions
	assert(readSettings != NULL);

	// Delete
	memFree(readSettings);
}


