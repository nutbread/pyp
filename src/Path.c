#include <assert.h>
#include "Path.h"
#include "Memory.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <stdio.h>



// Headers
typedef struct PathComponent_ {
	size_t start;
	size_t length;
	int isDotDot;
	struct PathComponent_* parent;
} PathComponent;

#ifdef _WIN32
static int getDriveLabel(const char* path, PathComponent** pc);
static void normalizePathComponent(const char* path, size_t* start, size_t* length);
#endif

#ifdef _WIN32
const char pathSeparator = '\\';
#else
const char pathSeparator = '/';
#endif



// Static
#ifdef _WIN32
int
getDriveLabel(const char* path, PathComponent** pc) {
	// Vars
	PathComponent* pcNew;
	char c;

	// Assertions
	assert(path != NULL);
	assert(pc != NULL);
	assert(*pc == NULL);

	// Check if it matches regex /[a-zA-Z]:/
	if ((c = path[0] & 0xDF) >= 'A' && c <= 'Z' && path[1] == ':') {
		// Create
		pcNew = memAlloc(PathComponent);
		if (pcNew == NULL) return -1; // error
		pcNew->start = 0;
		pcNew->length = 2;
		pcNew->parent = NULL;
		pcNew->isDotDot = 0;
		*pc = pcNew;

		// Okay
		return 0;
	}

	// Not found
	return 1;
}

void
normalizePathComponent(const char* path, size_t* start, size_t* length) {
	// Vars
	size_t i;
	size_t i_start;
	size_t i_end;
	char c;

	// Assertions
	assert(path != NULL);
	assert(start != NULL);
	assert(length != NULL);
	assert(*start + *length > *start);

	// Truncate trailing dots and spaces
	i_start = *start;
	i_end = i_start + *length - 1;
	i = i_end;
	while (1) {
		c = path[i];
		if (!(c == ' ' || c == '.')) {
			// Complete
			*length -= i_end - i;
			break;
		}

		// Loop
		if (i <= i_start) {
			// Nullify length
			*length = 0;
			break;
		}
		--i;
	}
}
#endif



// External
PathStatus
pathNormalize(const char* path, char** normalizedPath, size_t* normalizedPathLength, size_t* filenameOffset) {
	// Vars
	size_t pathLength = 0;
	size_t pathPosition = 0;
	size_t compCount = 0;
	size_t compStart;
	size_t compLength;
	PathComponent* pcRecent = NULL;
	PathComponent* pc;
	int isRootRelative;
	int isDotDot = 0;
	char* newPath;
	#ifdef _WIN32
	PathComponent* pcDriveLabel = NULL;
	int isRootUNC = 0;
	#endif

	// Assertions
	assert(path != NULL);
	assert(normalizedPath != NULL);
	assert(normalizedPathLength != NULL);

	// Start
	if ((isRootRelative = pathCharIsSeparatorAnsi(path[0]))) {
		++pathPosition;

		#ifdef _WIN32
		if ((isRootUNC = pathCharIsSeparatorAnsi(path[1]))) {
			++pathPosition;
		}
		#endif
	}
	#ifdef _WIN32
	else {
		int state = getDriveLabel(path, &pcDriveLabel);
		if (state < 0) return PATH_ERROR_MEMORY; // Memory error
		if (state == 0) {
			assert(pcDriveLabel != NULL);
			pathPosition += pcDriveLabel->length;

			if ((isRootRelative = pathCharIsSeparatorAnsi(path[pathPosition]))) {
				++pathPosition;
			}
		}
	}
	#endif
	pathLength = pathPosition;

	// Find components
	if (path[pathPosition] != '\x00') {
		// Loop
		do {
			// Find path
			compStart = pathPosition;
			while (1) {
				// Next
				if (path[pathPosition] == '\x00' || pathCharIsSeparatorAnsi(path[pathPosition])) {
					// Done
					break;
				}

				// Increase
				++pathPosition;
			}

			// Modify
			if (pathPosition > compStart) {
				// Something was found
				if (pathPosition - compStart == 1) {
					if (path[compStart] == '.') {
						// Do nothing
						continue;
					}
				}
				else if (pathPosition - compStart == 2) {
					if (path[compStart] == '.' && path[compStart + 1] == '.') {
						// Prefix ..s are not removed
						if (pcRecent == NULL || pcRecent->isDotDot) {
							isDotDot = 1;
						}
						else {
							// Delete previous
							pc = pcRecent;
							pcRecent = pcRecent->parent;

							--compCount;
							pathLength -= pc->length;

							memFree(pc);

							continue;
						}
					}
				}

				compLength = pathPosition - compStart;
				#ifdef _WIN32
				if (!isRootUNC) {
					normalizePathComponent(path, &compStart, &compLength);
					if (compLength == 0) continue; // skip
				}
				#endif

				// Otherwise, add a path component
				pc = memAlloc(PathComponent);
				if (pc == NULL) goto cleanup; // error
				pc->start = compStart;
				pc->length = compLength;
				pc->parent = pcRecent;
				pc->isDotDot = isDotDot;

				pcRecent = pc;
				isDotDot = 0;

				++compCount;
				pathLength += pc->length;
			}
		}
		while (path[pathPosition++] != '\x00');
	}

	// Create string
	if (compCount > 0) pathLength += compCount - 1;

	newPath = memAllocArray(char, pathLength + 1);
	if (newPath == NULL) goto cleanup; // error
	*normalizedPathLength = pathLength;
	*filenameOffset = pathLength;

	// Write
	newPath[pathLength] = '\x00';
	if (pcRecent != NULL) {
		*filenameOffset -= pcRecent->length;
		while (1) {
			// Copy
			assert(pathLength >= pcRecent->length);
			pathLength -= pcRecent->length;
			memcpy(&newPath[pathLength], &path[pcRecent->start], pcRecent->length);

			// Delete and next
			pc = pcRecent->parent;
			memFree(pcRecent);
			if (pc == NULL) break;
			pcRecent = pc;

			// Separator
			assert(pathLength > 0);
			newPath[--pathLength] = pathSeparator;
		}
	}

	if (isRootRelative) {
		// Separator
		assert(pathLength > 0);
		newPath[--pathLength] = pathSeparator;

		#ifdef _WIN32
		// Copy drive label
		if (isRootUNC) {
			assert(pcDriveLabel == NULL);

			// Separator
			assert(pathLength > 0);
			newPath[--pathLength] = pathSeparator;
		}
		#endif
	}
	#ifdef _WIN32
	if (pcDriveLabel != NULL) {
		assert(pathLength == pcDriveLabel->length);
		memcpy(newPath, &path[pcDriveLabel->start], pcDriveLabel->length);
		memFree(pcDriveLabel);
	}
	#endif

	// Done
	*normalizedPath = newPath;
	return PATH_OKAY;

	// Cleanup
	cleanup:
	#ifdef _WIN32
	if (pcDriveLabel != NULL) {
		memFree(pcDriveLabel);
	}
	#endif
	for (; pcRecent != NULL; pcRecent = pc) {
		pc = pcRecent->parent;
		memFree(pcRecent);
	}
	return PATH_ERROR_MEMORY;
}

PathStatus
pathGetCurrentWorkingDirectoryAnsi(char** path, size_t* pathLength) {
#ifdef _WIN32
	DWORD space;
	char* pathNew;

	// Assertions
	assert(path != NULL);
	assert(pathLength != NULL);

	space = GetCurrentDirectoryA(0, NULL);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	pathNew = memAllocArray(char, space);
	if (pathNew == NULL) return PATH_ERROR_MEMORY; // error

	space = GetCurrentDirectoryA(space, pathNew);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	// Okay
	*path = pathNew;
	*pathLength = space;
	return PATH_OKAY;
#else
	// Not implemented
	return PATH_ERROR_NOT_IMPLEMENTED;
#endif
}

PathStatus
pathGetCurrentWorkingDirectoryUnicode(unicode_char** path, size_t* pathLength) {
#ifdef _WIN32
	DWORD space;
	unicode_char* pathNew;

	// Assertions
	assert(path != NULL);
	assert(pathLength != NULL);

	space = GetCurrentDirectoryW(0, NULL);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	pathNew = memAllocArray(unicode_char, space);
	if (pathNew == NULL) return PATH_ERROR_MEMORY; // error

	space = GetCurrentDirectoryW(space, pathNew);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	// Okay
	*path = pathNew;
	*pathLength = space;
	return PATH_OKAY;
#else
	// Not implemented
	return PATH_ERROR_NOT_IMPLEMENTED;
#endif
}

PathStatus
pathSetCurrentWorkingDirectoryAnsi(const char* path) {
	assert(path != NULL);

	if (SetCurrentDirectoryA(path) == 0) return PATH_ERROR_GENERIC;
	return PATH_OKAY;
}

PathStatus
pathSetCurrentWorkingDirectoryUnicode(const unicode_char* path) {
	assert(path != NULL);

	if (SetCurrentDirectoryW(path) == 0) return PATH_ERROR_GENERIC;
	return PATH_OKAY;
}

PathStatus
pathAbsoluteAnsi(const char* path, char** absolutePath, size_t* absolutePathLength) {
#ifdef _WIN32
	DWORD space;
	char* absPath;

	space = GetFullPathNameA(path, 0, NULL, NULL);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	absPath = memAllocArray(char, space);
	if (absPath == NULL) return PATH_ERROR_MEMORY; // error

	space = GetFullPathNameA(path, space, absPath, NULL);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	// Done
	*absolutePath = absPath;
	*absolutePathLength = space;
	return PATH_OKAY;
#else
	// Not implemented
	return PATH_ERROR_NOT_IMPLEMENTED;
#endif
}

PathStatus
pathAbsoluteUnicode(const unicode_char* path, unicode_char** absolutePath, size_t* absolutePathLength) {
#ifdef _WIN32
	DWORD space;
	unicode_char* absPath;

	space = GetFullPathNameW(path, 0, NULL, NULL);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	absPath = memAllocArray(unicode_char, space);
	if (absPath == NULL) return PATH_ERROR_MEMORY; // error

	space = GetFullPathNameW(path, space, absPath, NULL);
	if (space == 0) return PATH_ERROR_GENERIC; // error

	// Done
	*absolutePath = absPath;
	*absolutePathLength = space;
	return PATH_OKAY;
#else
	// Not implemented
	return PATH_ERROR_NOT_IMPLEMENTED;
#endif
}

int
pathCharIsSeparatorAnsi(char c) {
	return (
		#ifdef _WIN32
		c == '\\' ||
		#endif
		c == '/'
	);
}

int
pathCharIsSeparatorUnicode(unicode_char c) {
	return (
		#ifdef _WIN32
		c == '\\' ||
		#endif
		c == '/'
	);
}


