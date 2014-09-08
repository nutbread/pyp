#include <assert.h>
#include <stdio.h>
#include "PypDataBufferModifiers.h"



// Constants
static const char * const hexChars = "0123456789ABCDEF";



// Return a stringified representation of the input
PypReadStatus
pypDataBufferModifyToString(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data) {
	// Vars
	PypSize newTotalLength = 2;
	PypSize i;
	PypSize memcpyLength;
	int sequenceLength;
	char* inputBuffer;
	char* outputBuffer;
	char formatBuffer[4];
	char c;
	PypDataBuffer* output;
	PypDataBufferEntry* entry;
	PypDataBufferEntry* outputEntry;


	// Assertions
	assert(input != NULL);
	assert(outputDataBuffer != NULL);

	// Nullify output
	*outputDataBuffer = NULL;

	// Count new length
	for (entry = input->firstChild; entry != NULL; entry = entry->nextSibling) {
		for (i = 0; i < entry->bufferLength; ++i) {
			c = entry->buffer[i];

			if (c == '\\' || c == '"') {
				newTotalLength += 2; // Escape
			}
			else if (c < 0x20 || c >= 0x7f) {
				newTotalLength += 4; // \xHH format
			}
			else {
				newTotalLength += 1; // Normal
			}
		}
	}

	// Create new
	output = pypDataBufferCreate();
	if (output == NULL) return PYP_READ_ERROR_MEMORY;

	// Done?
	if (newTotalLength == 0) {
		// Done
		*outputDataBuffer = output;
		return PYP_READ_OKAY;
	}

	// Create entry
	outputEntry = pypDataBufferExtend(output, newTotalLength);
	if (outputEntry == NULL) {
		// Cleanup
		pypDataBufferDelete(output);
		return PYP_READ_ERROR_MEMORY;
	}

	// Copy data
	outputBuffer = outputEntry->buffer;
	*(outputBuffer++) = '"';


	for (entry = input->firstChild; entry != NULL; entry = entry->nextSibling) {
		inputBuffer = entry->buffer;
		memcpyLength = 0;

		for (i = entry->bufferLength; i > 0; --i) {
			// Get the char
			c = inputBuffer[memcpyLength];

			// Char test
			if (c == '\\' || c == '"') {
				// Escape
				formatBuffer[0] = '\\';
				formatBuffer[1] = c;
				sequenceLength = 2;
			}
			else if (c < 0x20 || c >= 0x7f) {
				// \xHH format
				formatBuffer[0] = '\\';
				formatBuffer[1] = 'x';
				formatBuffer[2] = hexChars[((unsigned int) c) / 16];
				formatBuffer[3] = hexChars[((unsigned int) c) % 16];
				sequenceLength = 4;
			}
			else {
				// It will be copied in one of the memcpy statements
				++memcpyLength;
				continue;
			}

			// Memory copy
			if (memcpyLength > 0) {
				memcpy(outputBuffer, inputBuffer, sizeof(char) * memcpyLength);
				outputBuffer += memcpyLength;
				inputBuffer += memcpyLength;
				memcpyLength = 0;
			}

			// Custom sequence
			memcpy(outputBuffer, formatBuffer, sizeof(char) * sequenceLength);
			outputBuffer += sequenceLength;
			inputBuffer += 1;
		}


		// Memory copy
		if (memcpyLength > 0) {
			memcpy(outputBuffer, inputBuffer, sizeof(char) * memcpyLength);
			outputBuffer += memcpyLength;
			inputBuffer += memcpyLength;
			memcpyLength = 0;
		}
	}

	*(outputBuffer++) = '"';

	// Done
	*outputDataBuffer = output;
	return PYP_READ_OKAY;
}



// Return a version with escaped HTML characters (&, <, >, ', ")
PypReadStatus
pypDataBufferModifyToEscapedHTML(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data) {
	// Vars
	PypSize newTotalLength = 0;
	PypSize i;
	PypSize memcpyLength;
	int sequenceLength;
	const char* sequenceBuffer;
	char* inputBuffer;
	char* outputBuffer;
	char c;
	PypDataBuffer* output;
	PypDataBufferEntry* entry;
	PypDataBufferEntry* outputEntry;

	// Assertions
	assert(input != NULL);
	assert(outputDataBuffer != NULL);

	// Nullify output
	*outputDataBuffer = NULL;

	// Count new length
	for (entry = input->firstChild; entry != NULL; entry = entry->nextSibling) {
		for (i = 0; i < entry->bufferLength; ++i) {
			c = entry->buffer[i];

			if (c == '\'' || c == '"') {
				newTotalLength += 6; // &apos; , &quot;
			}
			else if (c == '&') {
				newTotalLength += 5; // &amp;
			}
			else if (c == '<' || c == '>') {
				newTotalLength += 4; // &lt; , &gt;
			}
			else {
				newTotalLength += 1; // Normal
			}
		}
	}

	// Create new
	output = pypDataBufferCreate();
	if (output == NULL) return PYP_READ_ERROR_MEMORY;

	// Done?
	if (newTotalLength == 0) {
		// Done
		*outputDataBuffer = output;
		return PYP_READ_OKAY;
	}

	// Create entry
	outputEntry = pypDataBufferExtend(output, newTotalLength);
	if (outputEntry == NULL) {
		// Cleanup
		pypDataBufferDelete(output);
		return PYP_READ_ERROR_MEMORY;
	}

	// Copy data
	outputBuffer = outputEntry->buffer;
	for (entry = input->firstChild; entry != NULL; entry = entry->nextSibling) {
		inputBuffer = entry->buffer;
		memcpyLength = 0;

		for (i = entry->bufferLength; i > 0; --i) {
			// Get the char
			c = inputBuffer[memcpyLength];

			// Char test
			if (c == '<') {
				sequenceBuffer = "&lt;";
				sequenceLength = 4;
			}
			else if (c == '>') {
				sequenceBuffer = "&gt;";
				sequenceLength = 4;
			}
			else if (c == '\'') {
				sequenceBuffer = "&apos;";
				sequenceLength = 6;
			}
			else if (c == '"') {
				sequenceBuffer = "&quot;";
				sequenceLength = 6;
			}
			else if (c == '&') {
				sequenceBuffer = "&amp;";
				sequenceLength = 5;
			}
			else {
				// It will be copied in one of the memcpy statements
				++memcpyLength;
				continue;
			}

			// Memory copy
			if (memcpyLength > 0) {
				memcpy(outputBuffer, inputBuffer, sizeof(char) * memcpyLength);
				outputBuffer += memcpyLength;
				inputBuffer += memcpyLength;
				memcpyLength = 0;
			}

			// Custom sequence
			memcpy(outputBuffer, sequenceBuffer, sizeof(char) * sequenceLength);
			outputBuffer += sequenceLength;
			inputBuffer += 1;
		}


		// Memory copy
		if (memcpyLength > 0) {
			memcpy(outputBuffer, inputBuffer, sizeof(char) * memcpyLength);
			outputBuffer += memcpyLength;
			inputBuffer += memcpyLength;
			memcpyLength = 0;
		}
	}

	// Done
	*outputDataBuffer = output;
	return PYP_READ_OKAY;
}


