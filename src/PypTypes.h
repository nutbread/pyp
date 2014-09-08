#ifndef __PYP_TYPES_H
#define __PYP_TYPES_H



#include <string.h>

#define PYP_COMPILER_MICROSOFT	0
#define PYP_COMPILER_GCC		1

#ifdef _MSC_VER
#define PYP_COMPILER PYP_COMPILER_MICROSOFT
#else
#define PYP_COMPILER PYP_COMPILER_GCC
#endif


typedef enum PypBool_ {
	PYP_FALSE = 0x0,
	PYP_TRUE = 0x1,
} PypBool;

typedef char PypChar;

typedef size_t PypSize;



#endif


