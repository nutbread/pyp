#include <assert.h>
#include "PypDataBuffer.h"
#include "Memory.h"



// Create a new buffer
PypDataBuffer*
pypDataBufferCreate() {
	// Create
	PypDataBuffer* buffer = memAlloc(PypDataBuffer);
	if (buffer == NULL) return NULL; // error

	// Setup
	buffer->totalSize = 0;
	buffer->firstChild = NULL;
	buffer->lastChild = &buffer->firstChild;

	// Done
	return buffer;
}

// Delete a data buffer
void
pypDataBufferDelete(PypDataBuffer* dataBuffer) {
	// Vars
	PypDataBufferEntry* entry;
	PypDataBufferEntry* next;

	// Assertions
	assert(dataBuffer != NULL);

	// Delete loop
	for (entry = dataBuffer->firstChild; entry != NULL; entry = next) {
		// Delete
		next = entry->nextSibling;
		memFree(entry->buffer);
		memFree(entry);
	}

	memFree(dataBuffer);
}

// Empty all data from a data buffer
void
pypDataBufferEmpty(PypDataBuffer* dataBuffer) {
	// Vars
	PypDataBufferEntry* entry;
	PypDataBufferEntry* next;

	// Assertions
	assert(dataBuffer != NULL);

	// Delete loop
	for (entry = dataBuffer->firstChild; entry != NULL; entry = next) {
		// Delete
		next = entry->nextSibling;
		memFree(entry->buffer);
		memFree(entry);
	}

	// Zero data
	dataBuffer->totalSize = 0;
	dataBuffer->firstChild = NULL;
	dataBuffer->lastChild = &dataBuffer->firstChild;
}

// Extend it without copying any data
PypDataBufferEntry*
pypDataBufferExtend(PypDataBuffer* dataBuffer, PypSize dataLength) {
	// Vars
	PypDataBufferEntry* entry;

	// Assertions
	assert(dataBuffer != NULL);
	assert(dataLength > 0);

	// Create
	entry = memAlloc(PypDataBufferEntry);
	if (entry == NULL) return NULL; // error

	// Setup
	#ifndef NDEBUG
	++dataLength; // Debug only; null terminates without interfering with anything else
	#endif
	entry->buffer = memAllocArray(PypChar, dataLength);
	if (entry->buffer == NULL) {
		// Cleanup
		memFree(entry);
		return NULL;
	}
	#ifndef NDEBUG
	entry->buffer[--dataLength] = '\x00'; // Debug only; null terminates without interfering with anything else
	#endif

	entry->bufferLength = dataLength;
	dataBuffer->totalSize += dataLength;
	entry->nextSibling = NULL;

	// Link
	*dataBuffer->lastChild = entry;
	dataBuffer->lastChild = &entry->nextSibling;

	// Done
	return entry;
}

// Extend it with copying data
PypDataBufferEntry*
pypDataBufferExtendWithData(PypDataBuffer* dataBuffer, const PypChar* data, PypSize dataLength) {
	// Vars
	PypDataBufferEntry* entry;

	// Assertions
	assert(dataBuffer != NULL);
	assert(data != NULL);
	assert(dataLength > 0);

	// Create
	entry = pypDataBufferExtend(dataBuffer, dataLength);
	if (entry == NULL) return NULL;

	// Copy
	memcpy(entry->buffer, data, sizeof(PypChar) * dataLength);

	// Done
	return entry;
}

// Extend it with copying a string
PypDataBufferEntry*
pypDataBufferExtendWithString(PypDataBuffer* dataBuffer, const PypChar* data) {
	// Assertions
	assert(dataBuffer != NULL);
	assert(data != NULL);
	assert(strlen(data) > 0);

	return pypDataBufferExtendWithData(dataBuffer, data, strlen(data));
}

// Extend it with another instance
void
pypDataBufferExtendWithDataBufferAndDelete(PypDataBuffer* dataBuffer, PypDataBuffer* other) {
	// Assertions
	assert(dataBuffer != NULL);
	assert(other != NULL);

	if (other->firstChild != NULL) {
		// Must be something to copy
		assert(other->lastChild != &other->firstChild);

		// Copy data over
		dataBuffer->totalSize += other->totalSize;
		*(dataBuffer->lastChild) = other->firstChild;
		dataBuffer->lastChild = other->lastChild;
	}

	// Delete other
	memFree(other);
}

// Unify
PypBool
pypDataBufferUnify(PypDataBuffer* dataBuffer, PypBool nullTerminate, PypDataBufferEntry** ptrNewEntry) {
	// Vars
	PypChar* stringPos;
	PypDataBufferEntry* entry;
	PypDataBufferEntry* next;
	PypDataBufferEntry* entryNew;

	// Assertions
	assert(dataBuffer != NULL);
	assert(nullTerminate == 0 || nullTerminate == 1);

	// Early exit if nothing needs to be done
	*ptrNewEntry = NULL;
	if (dataBuffer->firstChild == NULL) return PYP_TRUE;
	if (dataBuffer->firstChild->nextSibling == NULL) {
		*ptrNewEntry = dataBuffer->firstChild;
		return PYP_TRUE;
	}

	// Create
	entryNew = memAlloc(PypDataBufferEntry);
	if (entryNew == NULL) return PYP_FALSE; // error

	entryNew->bufferLength = dataBuffer->totalSize;
	entryNew->nextSibling = NULL;

	// Create
	entryNew->buffer = memAllocArray(PypChar, entryNew->bufferLength + nullTerminate);
	if (entryNew->buffer == NULL) {
		memFree(entryNew);
		return PYP_FALSE; // error
	}
	stringPos = entryNew->buffer;

	// Copy
	for (entry = dataBuffer->firstChild; entry != NULL; entry = entry->nextSibling) {
		memcpy(stringPos, entry->buffer, sizeof(PypChar) * entry->bufferLength);
		stringPos += entry->bufferLength;
	}

	// Null terminate
	if (nullTerminate) *stringPos = '\x00';

	// Delete loop
	for (entry = dataBuffer->firstChild; entry != NULL; entry = next) {
		// Delete
		next = entry->nextSibling;
		memFree(entry->buffer);
		memFree(entry);
	}

	// Update lists
	dataBuffer->firstChild = entryNew;
	dataBuffer->lastChild = &entryNew->nextSibling;

	// Done
	*ptrNewEntry = entryNew;
	return PYP_TRUE;
}


