#ifndef __PATH_H
#define __PATH_H



#include <string.h>
#include "Unicode.h"



typedef enum PathStatus_ {
	PATH_OKAY = 0x0,
	PATH_ERROR_GENERIC = 0x1,
	PATH_ERROR_MEMORY = 0x2,
	PATH_ERROR_NOT_IMPLEMENTED = 0x3,
} PathStatus;



PathStatus pathNormalize(const char* path, char** normalizedPath, size_t* normalizedPathLength, size_t* filenameOffset);
PathStatus pathGetCurrentWorkingDirectoryAnsi(char** path, size_t* pathLength);
PathStatus pathGetCurrentWorkingDirectoryUnicode(unicode_char** path, size_t* pathLength);
PathStatus pathSetCurrentWorkingDirectoryAnsi(const char* path);
PathStatus pathSetCurrentWorkingDirectoryUnicode(const unicode_char* path);
PathStatus pathAbsoluteAnsi(const char* path, char** absolutePath, size_t* absolutePathLength);
PathStatus pathAbsoluteUnicode(const unicode_char* path, unicode_char** absolutePath, size_t* absolutePathLength);
int pathCharIsSeparatorAnsi(char c);
int pathCharIsSeparatorUnicode(unicode_char c);



#endif


