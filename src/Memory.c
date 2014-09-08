#include <stdlib.h>
#include "Memory.h"


#ifndef NDEBUG

#include <assert.h>
#include <stdint.h>
#include <stdio.h>



// Functions for mapping memory
struct MemoryMap_;
struct MemoryMapEntry_;
struct MemoryMapBucket_;

typedef uint32_t MemoryMapHashValue;

struct MemoryMapEntry_ {
	const void* key;
	size_t length;
	struct MemoryMapEntry_* nextSibling;
};

struct MemoryMapBucket_ {
	struct MemoryMapEntry_* firstChild;
	struct MemoryMapEntry_** ptrToNext;
};

typedef struct MemoryMap_ {
	size_t bucketCount;
	struct MemoryMapBucket_* buckets;
} MemoryMap;

typedef enum MemoryMapStatus_ {
	MEMORY_MAP_ERROR = 0x0,
	MEMORY_MAP_ADDED = 0x1,
	MEMORY_MAP_ALREADY_EXISTED = 0x2,
	MEMORY_MAP_FOUND = 0x3,
	MEMORY_MAP_NOT_FOUND = 0x4,
} MemoryMapStatus;

static MemoryMap* memoryMapCreate(size_t bucketCount);
static void memoryMapDelete(MemoryMap* map);
static MemoryMapStatus memoryMapAdd(MemoryMap* map, const void* key, size_t length);
static MemoryMapStatus memoryMapRemove(MemoryMap* map, const void* key);
static MemoryMapStatus memoryMapFind(MemoryMap* map, const void* key);

static MemoryMapHashValue memoryMapKeyHashFunction(const void* key);
static int memoryMapKeyCompareFunction(const void* key1, const void* key2);
static void memoryMapSetup();

static MemoryMap* globalMemoryMap = NULL;





// Create a new memoryMap with some custom functions
MemoryMap*
memoryMapCreate(size_t bucketCount) {
	// Vars
	size_t i;
	MemoryMap* memoryMap;

	// Create
	memoryMap = malloc(sizeof(MemoryMap));
	if (memoryMap == NULL) return NULL; // error

	// Setup
	memoryMap->bucketCount = bucketCount;
	memoryMap->buckets = malloc(sizeof(struct MemoryMapBucket_) * memoryMap->bucketCount);
	if (memoryMap->buckets == NULL) {
		// Cleanup
		free(memoryMap);
		return NULL;
	}

	// Setup buckets
	for (i = 0; i < memoryMap->bucketCount; ++i) {
		memoryMap->buckets[i].firstChild = NULL;
		memoryMap->buckets[i].ptrToNext = &memoryMap->buckets[i].firstChild;
	}

	// Done
	return memoryMap;
}

void
memoryMapDelete(MemoryMap* memoryMap) {
	// Vars
	struct MemoryMapEntry_* entry;
	struct MemoryMapEntry_* next;
	size_t i;

	// Assertions
	assert(memoryMap != NULL);

	// Delete bucket lists
	for (i = 0; i < memoryMap->bucketCount; ++i) {
		for (entry = memoryMap->buckets[i].firstChild; entry != NULL; entry = next) {
			// Delete
			next = entry->nextSibling;
			free(entry);
		}
	}

	// Delete memoryMap
	free(memoryMap->buckets);
	free(memoryMap);
}

MemoryMapStatus
memoryMapAdd(MemoryMap* memoryMap, const void* key, size_t length) {
	// Vars
	MemoryMapHashValue bucket;
	struct MemoryMapEntry_* entry;

	// Assertions
	assert(memoryMap != NULL);
	assert(key != NULL);
	assert(memoryMapFind(memoryMap, key) == MEMORY_MAP_NOT_FOUND); // Cannot already exist
	assert(memoryMapKeyCompareFunction(key, key));

	// Find hash/bucket
	bucket = memoryMapKeyHashFunction(key) % memoryMap->bucketCount;

	// Create new
	entry = malloc(sizeof(struct MemoryMapEntry_));
	if (entry == NULL) return MEMORY_MAP_ERROR; // error

	entry->key = key;
	entry->length = length;
	entry->nextSibling = NULL;

	// Add
	*(memoryMap->buckets[bucket].ptrToNext) = entry;
	memoryMap->buckets[bucket].ptrToNext = &entry->nextSibling;

	// Done
	return MEMORY_MAP_ADDED;
}

MemoryMapStatus
memoryMapRemove(MemoryMap* memoryMap, const void* key) {
	// Vars
	MemoryMapHashValue bucket;
	struct MemoryMapEntry_** ptrEntry;
	struct MemoryMapEntry_* deletionEntry;

	// Assertions
	assert(memoryMap != NULL);
	assert(key != NULL);
	assert(memoryMapKeyCompareFunction(key, key));

	// Find hash/bucket
	bucket = memoryMapKeyHashFunction(key) % memoryMap->bucketCount;

	// Find
	ptrEntry = &memoryMap->buckets[bucket].firstChild;

	while (*ptrEntry != NULL) {
		// Check
		if (memoryMapKeyCompareFunction((*ptrEntry)->key, key)) {
			// Get values
			deletionEntry = (*ptrEntry);

			// Change linking
			if (memoryMap->buckets[bucket].ptrToNext == &(*ptrEntry)->nextSibling) {
				memoryMap->buckets[bucket].ptrToNext = ptrEntry;
			}
			(*ptrEntry) = (*ptrEntry)->nextSibling;

			// Delete
			free(deletionEntry);

			// Return value
			return MEMORY_MAP_FOUND;
		}

		// Next
		ptrEntry = &(*ptrEntry)->nextSibling;
	}

	// Done
	return MEMORY_MAP_NOT_FOUND;
}

MemoryMapStatus
memoryMapFind(MemoryMap* memoryMap, const void* key) {
	// Vars
	MemoryMapHashValue bucket;
	struct MemoryMapEntry_* entry;

	// Assertions
	assert(memoryMap != NULL);
	assert(key != NULL);
	assert(memoryMapKeyCompareFunction(key, key));

	// Find hash/bucket
	bucket = memoryMapKeyHashFunction(key) % memoryMap->bucketCount;

	// Find
	entry = memoryMap->buckets[bucket].firstChild;

	while (entry != NULL) {
		// Check
		if (memoryMapKeyCompareFunction(entry->key, key)) {
			// Return value
			return MEMORY_MAP_FOUND;
		}

		// Next
		entry = entry->nextSibling;
	}

	// Done
	return MEMORY_MAP_NOT_FOUND;
}





MemoryMapHashValue
memoryMapKeyHashFunction(const void* key) {
	MemoryMapHashValue hash = ((size_t) key) + (((size_t) key) >> 3);

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

int
memoryMapKeyCompareFunction(const void* key1, const void* key2) {
	return ((size_t) key1) == ((size_t) key2);
}

void
memoryMapSetup() {
	if (globalMemoryMap == NULL) {
		globalMemoryMap = memoryMapCreate(256);
		assert(globalMemoryMap != NULL);
	}
}







// Memory counting
static size_t memAllocCount = 0;
static size_t memFreeCount = 0;
static signed long long pypUnmatchedMallocs = 0;



// Custom malloc function
void*
memoryCustomMalloc_(size_t size) {
	// Vars
	void* memory;

	// Setup
	memoryMapSetup();

	// Malloc
	memory = malloc(size);

	// Not null
	if (memory != NULL) {
		MemoryMapStatus status;

		// Increase counters
		++pypUnmatchedMallocs;
		++memAllocCount;

		// Check if it exists in the mapping
		status = memoryMapFind(globalMemoryMap, memory);
		if (status == MEMORY_MAP_FOUND) {
			int memory_location_not_freed = 0;
			assert(memory_location_not_freed);
		}

		// Add it to the map
		status = memoryMapAdd(globalMemoryMap, memory, size);
		assert(status == MEMORY_MAP_ADDED);
	}

	// Done
	return memory;
}

// Custom realloc function
void*
memoryCustomRealloc_(void* ptr, size_t size) {
	// Vars
	void* memory;

	// Assertions
	assert(ptr != NULL);
	assert(size > 0);

	memoryMapSetup();

	// Map
	memory = realloc(ptr, size);

	// Not null, update mapping
	if (memory != NULL && ptr != memory) {
		MemoryMapStatus status;

		// Check if it exists in the mapping
		status = memoryMapFind(globalMemoryMap, ptr);
		if (status == MEMORY_MAP_NOT_FOUND) {
			int memory_location_not_found = 0;
			assert(memory_location_not_found);
		}

		// Remove
		status = memoryMapRemove(globalMemoryMap, ptr);
		assert(status == MEMORY_MAP_FOUND);



		// Check if it exists in the mapping
		status = memoryMapFind(globalMemoryMap, memory);
		if (status == MEMORY_MAP_FOUND) {
			int memory_location_not_freed = 0;
			assert(memory_location_not_freed);
		}

		// Add it to the map
		status = memoryMapAdd(globalMemoryMap, memory, size);
		assert(status == MEMORY_MAP_ADDED);
	}

	// Done
	return memory;
}

// Custom free function
void
memoryCustomFree_(void* ptr) {
	memoryMapSetup();

	// Free
	free(ptr);

	// Not null
	if (ptr != NULL) {
		MemoryMapStatus status;

		// Update counters
		--pypUnmatchedMallocs;
		++memFreeCount;

		// Check if it exists in the mapping
		status = memoryMapFind(globalMemoryMap, ptr);
		if (status == MEMORY_MAP_NOT_FOUND) {
			int memory_location_not_found = 0;
			assert(memory_location_not_found);
		}

		// Remove
		status = memoryMapRemove(globalMemoryMap, ptr);
		assert(status == MEMORY_MAP_FOUND);
	}
}

// Dump memory statistics
void
memoryStats() {
	printf("Memory : [ malloc=%d , free=%d , unmatched=%d ]\n", (unsigned int) memAllocCount, (unsigned int) memFreeCount, (int) pypUnmatchedMallocs);
}

// Clean up memory
void
memoryCleanup() {
	if (globalMemoryMap != NULL) {
		memoryMapDelete(globalMemoryMap);
		globalMemoryMap = NULL;
	}

	memAllocCount = 0;
	memFreeCount = 0;
	pypUnmatchedMallocs = 0;
}

#else

// Empty functions
void
memoryStats() {
}

void
memoryCleanup() {
}

#endif




