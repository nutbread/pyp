#ifndef __MEMORY_H
#define __MEMORY_H






#ifndef NDEBUG


// Debugging
#define memAlloc(type) \
	( (type*) memoryCustomMalloc_(sizeof(type)) )

#define memAllocArray(type, count) \
	( (type*) memoryCustomMalloc_(sizeof(type) * count) )

#define memRealloc(type, ptr) \
	( (type*) memoryCustomRealloc_(ptr, sizeof(type)) )

#define memReallocArray(ptr, type, count) \
	( (type*) memoryCustomRealloc_(ptr, sizeof(type) * count) )

#define memFree(x) \
	( memoryCustomFree_(x) )



void* memoryCustomMalloc_(size_t size);
void* memoryCustomRealloc_(void* ptr, size_t size);
void memoryCustomFree_(void* ptr);

#else

#include <stdlib.h>

// No debugging
#define memAlloc(type) \
	( (type*) malloc(sizeof(type)) )

#define memAllocArray(type, count) \
	( (type*) malloc(sizeof(type) * count) )

#define memRealloc(ptr, type) \
	( (type*) realloc(ptr, sizeof(type)) )

#define memReallocArray(ptr, type, count) \
	( (type*) realloc(ptr, sizeof(type) * count) )

#define memFree(x) \
	( free(x) )

#endif



void memoryStats();
void memoryCleanup();



#endif


