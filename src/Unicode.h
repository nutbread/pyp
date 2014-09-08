#ifndef __UNICODE_H
#define __UNICODE_H



#include <string.h>



typedef enum UnicodeStatus_ {
	UNICODE_OKAY = 0x0,
	UNICODE_ERROR_MEMORY = 0x1,
	UNICODE_ERROR_FORMAT = 0x2,
} UnicodeStatus;

typedef wchar_t unicode_char;

UnicodeStatus unicodeUTF8Encode(const unicode_char* input, char** output, size_t* outputLength, size_t* errorCount);
UnicodeStatus unicodeUTF8EncodeLength(const unicode_char* input, size_t inputLength, char** output, size_t* outputLength, size_t* errorCount);
UnicodeStatus unicodeUTF8Decode(const char* input, unicode_char** output, size_t* outputCharacterCount, size_t* outputBufferLength, size_t* errorCount);
UnicodeStatus unicodeUTF8DecodeLength(const char* input, size_t inputLength, unicode_char** output, size_t* outputCharacterCount, size_t* outputBufferLength, size_t* errorCount);

size_t getCharStringLength(const char* source);
size_t getUnicodeCharStringLength(const unicode_char* source);



#endif


