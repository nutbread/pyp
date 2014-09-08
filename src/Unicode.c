#include <assert.h>
#include <stdint.h>
#include "Unicode.h"
#include "Memory.h"



/*
	UTF-8 decoding implementation; http://bjoern.hoehrmann.de/utf-8/decoder/dfa/

	Copyright (c) 2008-2009 Bjoern Hoehrmann

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*/
#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8DecodeTables[] = {
	// The first part of the table maps bytes to character classes that to reduce the size of the transition table and create bitmasks.
	0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1 ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7 ,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7 ,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8 ,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2 ,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

	// The second part is a transition table that maps a combination of a state of the automaton and a character class to a state.
	0 ,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
	12,0 ,12,12,12,12,12,0 ,12,0 ,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
	12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
	12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
	12,36,12,12,12,12,12,12,12,12,12,12,
};


#ifdef _MSC_VER
static __inline void utf8Decode(uint32_t* state, uint32_t* codepoint, uint32_t byte);
#else
static inline void utf8Decode(uint32_t* state, uint32_t* codepoint, uint32_t byte);
#endif

void
utf8Decode(uint32_t* state, uint32_t* codepoint, uint32_t byte) {
	uint32_t type = utf8DecodeTables[byte];

	*codepoint = (*state != UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codepoint << 6) :
		(0xff >> type) & (byte);

	*state = utf8DecodeTables[256 + (*state) + type];
	//return *state;
}



UnicodeStatus
unicodeUTF8Encode(const unicode_char* input, char** output, size_t* outputLength, size_t* errorCount) {
	// Assertions
	assert(input != NULL);
	assert(output != NULL);
	assert(outputLength != NULL);
	assert(errorCount != NULL);

	// Return using length
	return unicodeUTF8EncodeLength(input, wcslen(input), output, outputLength, errorCount);
}

UnicodeStatus
unicodeUTF8EncodeLength(const unicode_char* input, size_t inputLength, char** output, size_t* outputLength, size_t* errorCount) {
	// Vars
	const unicode_char* inputEnd = &input[inputLength];
	const unicode_char* unchanged = input;
	const unicode_char* i;
	uint32_t uChar;
	unicode_char c2;
	size_t j;
	size_t bufferLength;
	char* o;

	// Assertions
	assert(input != NULL);
	assert(output != NULL);
	assert(outputLength != NULL);
	assert(errorCount != NULL);

	// Start counting bytes that need no change
	while (1) {
		if (unchanged >= inputEnd) {
			// Perform a simple completion
			*output = memAllocArray(char, inputLength + 1);
			if (*output == NULL) return UNICODE_ERROR_MEMORY; // error

			// Copy
			for (j = 0; j <= inputLength; ++j) {
				(*output)[j] = input[j];
			}

			// Final
			*outputLength = inputLength;
			*errorCount = 0;

			// Done
			return UNICODE_OKAY;
		}
		if (*unchanged >= 0x0080) break;

		// Next
		++unchanged;
	}

	// Remaining bytes
	bufferLength = (unchanged - input) + (inputEnd - unchanged) * 3 + 1;
	o = memAllocArray(char, bufferLength);
	if (o == NULL) return UNICODE_ERROR_MEMORY;

	*output = o;
	*errorCount = 0;

	// Copy unchanged
	for (i = input; i < unchanged; ) {
		assert(o < (*output) + bufferLength);
		*(o++) = *(i++);
	}

	// Get new
	i = unchanged;
	while (i < inputEnd) {
		uChar = *i;

		if (uChar >= 0x0080) {
			if (uChar >= 0x0800) {
				if (uChar >= 0xD800 && uChar < 0xE000) {
					if (uChar < 0xDC00) {
						// Surrogate start
						if (++i >= inputEnd) {
							// Error: incomplete surrogate
							++(*errorCount);
							break;
						}

						c2 = *i;
						if (c2 >= 0xDC00 || c2 < 0xE000) {
							// 4 chars
							uChar = 0x010000 + (((uChar & 0x03FF) << 10) | (c2 & 0x03FF));
							assert(uChar < 0x200000);

							assert(o + 3 < (*output) + bufferLength);
							*(o++) = 0xF0 | ((uChar >> 18));
							*(o++) = 0x80 | ((uChar >> 12) & 0x3F);
							*(o++) = 0x80 | ((uChar >> 6) & 0x3F);
							*(o++) = 0x80 | ((uChar) & 0x3F);
						}
						else {
							// Error: invalid surrogate tail
							++(*errorCount);
						}
					}
					else {
						// Error: missing surrogate head
						++(*errorCount);
					}
				}
				else {
					// 3 chars
					assert(o + 2 < (*output) + bufferLength);
					*(o++) = 0xE0 | ((uChar >> 12));
					*(o++) = 0x80 | ((uChar >> 6) & 0x3F);
					*(o++) = 0x80 | ((uChar) & 0x3F);
				}
			}
			else {
				// 2 chars
				assert(o + 1 < (*output) + bufferLength);
				*(o++) = 0xC0 | ((uChar >> 6));
				*(o++) = 0x80 | ((uChar) & 0x3F);
			}
		}
		else {
			// 1 char
			assert(o < (*output) + bufferLength);
			*(o++) = uChar;
		}

		// Next
		++i;
	}

	// Null terminate
	assert(o <= (*output) + bufferLength);
	*o = '\x00';

	// Vars
	*outputLength = o - *output;

	// Realloc
	if (*outputLength < bufferLength) {
		char* outputRealloced = memReallocArray(*output, char, (*outputLength) + 1);
		if (outputRealloced != NULL) *output = outputRealloced;
	}

	// Done
	return UNICODE_OKAY;
}

UnicodeStatus
unicodeUTF8Decode(const char* input, unicode_char** output, size_t* outputCharacterCount, size_t* outputBufferLength, size_t* errorCount) {
	// Assertions
	assert(input != NULL);
	assert(output != NULL);
	assert(outputCharacterCount != NULL);
	assert(outputBufferLength != NULL);
	assert(errorCount != NULL);

	// Get length
	return unicodeUTF8DecodeLength(input, strlen(input), output, outputBufferLength, outputCharacterCount, errorCount);
}

UnicodeStatus
unicodeUTF8DecodeLength(const char* input, size_t inputLength, unicode_char** output, size_t* outputCharacterCount, size_t* outputBufferLength, size_t* errorCount) {
	// Vars
	const char* inputEnd = &input[inputLength];
	const char* i = input;
	unicode_char* o;
	uint32_t codepoint;
	uint32_t state = UTF8_ACCEPT;
	uint32_t statePre = UTF8_ACCEPT;
	size_t charCount = 0;
	size_t surrogateCount = 0;

	// Assertions
	assert(input != NULL);
	assert(output != NULL);
	assert(outputCharacterCount != NULL);
	assert(outputBufferLength != NULL);
	assert(errorCount != NULL);

	// Create
	o = memAllocArray(unicode_char, inputLength + 1);
	if (o == NULL) return UNICODE_ERROR_MEMORY; // error

	*output = o;
	*errorCount = 0;

	while (i < inputEnd) {
		utf8Decode(&state, &codepoint, (unsigned char) (*i));
		switch (state) {
			case UTF8_ACCEPT:
			{
				// Completed
				++charCount;
				if (codepoint > 0xFFFF) {
					// Surrogate
					assert(o + 1 < (*output) + inputLength);
					++surrogateCount;
					*(o++) = (unicode_char) (0xD7C0 + (codepoint >> 10));
					*(o++) = (unicode_char) (0xDC00 + (codepoint & 0x3FF));
				}
				else {
					// Normal
					assert(o < (*output) + inputLength);
					*(o++) = (unicode_char) codepoint;
				}
			}
			break;
			case UTF8_REJECT:
			{
				// Error
				++errorCount;
				state = UTF8_ACCEPT;
				if (statePre != UTF8_ACCEPT) {
					statePre = UTF8_ACCEPT;
					continue; // skip next increment
				}
			}
			break;
		}

		// Next
		statePre = state;
		++i;
	}

	// Error char
	if (state != UTF8_ACCEPT) {
		// Incompletion character error
		++errorCount;
	}

	// Null terminate
	assert(o <= (*output) + inputLength);
	*o = '\x00';

	// Vars
	*outputBufferLength = charCount + surrogateCount + 1;
	*outputCharacterCount = charCount;

	// Realloc
	if (*outputBufferLength < inputLength + 1) {
		unicode_char* outputRealloced = memReallocArray(*output, unicode_char, *outputBufferLength);
		if (outputRealloced != NULL) *output = outputRealloced;
	}

	// Okay
	return UNICODE_OKAY;
}



size_t
getCharStringLength(const char* source) {
	// Assertions
	assert(source != NULL);

	// Length
	return strlen(source);
}

size_t
getUnicodeCharStringLength(const unicode_char* source) {
	// Assertions
	assert(source != NULL);

	// Length
	return wcslen(source);
}


