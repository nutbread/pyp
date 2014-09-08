#include <assert.h>
#include <share.h>
#include "File.h"
#include "Unicode.h"
#include "PypTypes.h"



FileOpenStatus
fileOpen(const char* filename, const char* mode, FILE** outputFile) {
	// Assertions
	assert(filename != NULL);
	assert(mode != NULL);
	assert(outputFile != NULL);

	#if PYP_COMPILER == PYP_COMPILER_MICROSOFT
	// Open
	*outputFile = _fsopen(filename, mode, _SH_DENYWR);
	#else
	// Open
	*outputFile = fopen(filename, mode);
	#endif

	// Error
	if (*outputFile == NULL) return FILE_OPEN_ERROR;

	// Okay
	return FILE_OPEN_OKAY;
}

FileOpenStatus
fileOpenUnicode(const unicode_char* filename, const char* mode, FILE** outputFile) {
	// Vars
	unicode_char uMode[4];
	unicode_char* uModePos = uMode;

	// Assertions
	assert(filename != NULL);
	assert(mode != NULL);
	assert(outputFile != NULL);
	assert(strlen(mode) < 4);

	// Copy mode
	while (1) {
		// Copy
		*uModePos = *mode;

		// Next
		if (*mode == '\x00') break;
		++mode;
		++uModePos;
	}

	#if PYP_COMPILER == PYP_COMPILER_MICROSOFT
	// Open
	*outputFile = _wfsopen(filename, uMode, _SH_DENYWR);
	#else
	// Open
	*outputFile = _wfopen(filename, uMode);
	#endif

	// Error
	if (*outputFile == NULL) return FILE_OPEN_ERROR;

	// Okay
	return FILE_OPEN_OKAY;
}

void
fileClose(FILE* file) {
	// Assertions
	assert(file != NULL);

	// Close
	fclose(file);
}



