#include <assert.h>
#include "Map.h"
#include "Memory.h"



// Helper functions
MapHashValue
mapHelperHashString(const char* key) {
	// Vars
	MapHashValue hash = 0;

	// Assertions
	assert(key != NULL);

	// Hash
	while (*key != '\x00') {
		hash += *key;
		hash += (hash << 10);
		hash ^= (hash >> 6);

		// Next
		++key;
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	// Done
	return hash;
}

int
mapHelperCompareString(const char* key1, const char* key2) {
	// String compare
	return (strcmp(key1, key2) == 0);
}

int
mapHelperCopyString(const char* key, char** output) {
	// Vars
	size_t length;

	// Assertions
	assert(key != NULL);
	assert(output != NULL);

	// Get length
	length = strlen(key) + 1;

	// Create
	*output = memAllocArray(char, length);
	if (*output == NULL) return -1; // error

	// Copy
	memcpy(*output, key, sizeof(char) * length);

	// Done
	return 0;
}

void
mapHelperDeleteString(char* key) {
	// Assertions
	assert(key != NULL);

	// Delete
	memFree(key);
}




MapHashValue
mapHelperHashUnicode(const unicode_char* key) {
	// Vars
	MapHashValue hash = 0;

	// Assertions
	assert(key != NULL);

	// Hash
	while (*key != '\x00') {
		hash += *key;
		hash += (hash << 10);
		hash ^= (hash >> 6);

		// Next
		++key;
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	// Done
	return hash;
}

int
mapHelperCompareUnicode(const unicode_char* key1, const unicode_char* key2) {
	// String compare
	return (wcscmp(key1, key2) == 0);
}

int
mapHelperCopyUnicode(const unicode_char* key, unicode_char** output) {
	// Vars
	size_t length;

	// Assertions
	assert(key != NULL);
	assert(output != NULL);

	// Get length
	length = wcslen(key) + 1;

	// Create
	*output = memAllocArray(unicode_char, length);
	if (*output == NULL) return -1; // error

	// Copy
	memcpy(*output, key, sizeof(unicode_char) * length);

	// Done
	return 0;
}

void
mapHelperDeleteUnicode(unicode_char* key) {
	// Assertions
	assert(key != NULL);

	// Delete
	memFree(key);
}


