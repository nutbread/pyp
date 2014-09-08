#ifndef __FILE_H
#define __FILE_H



#include <stdio.h>
#include "Unicode.h"



typedef enum FileOpenStatus_ {
	FILE_OPEN_OKAY = 0x0,
	FILE_OPEN_ERROR = 0x1,
} FileOpenStatus;


FileOpenStatus fileOpen(const char* filename, const char* mode, FILE** outputFile);
FileOpenStatus fileOpenUnicode(const unicode_char* filename, const char* mode, FILE** outputFile);
void fileClose(FILE* file);



#endif


