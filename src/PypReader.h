#ifndef __PYP_READER_H
#define __PYP_READER_H



#include <stdint.h>
#include "PypTypes.h"


enum {
	PYP_READER_ERROR_ID_NO_ERROR = 0x0,
	PYP_READER_ERROR_ID_UNCLOSED_TAG = 0x1,
	PYP_READER_ERROR_ID_CONTINUATION_UNMATCHED_OPENING_TAG = 0x2,
	PYP_READER_ERROR_ID_CONTINUATION_MISMATCHED_OPENING_TAG = 0x3,
	PYP_READER_ERROR_ID_CONTINUATION_MISMATCHED_CLOSING_TAG = 0x4,
	PYP_READER_ERROR_ID_COUNT = 0x5,
};
enum {
	PYP_READER_FLAGS_NONE = 0x0,
	PYP_READER_FLAG_ON_UNCLOSED_TAG_ERROR = 0x1,
	PYP_READER_FLAG_ON_CONTINUATION_UNMATCHED_TAG_ERROR = 0x2,
	PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_ERROR = 0x4,
	PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_CONTINUE = 0x8,
	PYP_READER_FLAG_ON_CONTINUATION_ALLOW_LATE_ERROR_OUTPUT = 0x10,
	PYP_READER_FLAG_TREAT_SYNTAX_ERRORS_AS_SUCCESS = 0x20,
};
/*
	PYP_READER_FLAG_ON_UNCLOSED_TAG_ERROR
		Display an error when an unclosed tag is encountered
		Example that would raise an error:
			text1<? code1

	PYP_READER_FLAG_ON_CONTINUATION_UNMATCHED_TAG_ERROR
		Display an error when an invalid opening continuation is found
		Example that would raise an error:
			text1<?... code1 ?>

	PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_ERROR
		Display an error when an opening continuation does not match the previous continuation
		Example that would raise an error:
			text1<? code1 ...?>text2<?=... code2 ?>text3

	PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_CONTINUE
		Allows a mismatched continuation to continue the original tag, rather than stopping and re-opening
		Example:
			text1<? code ...?>text2<?=... code ?>text3
		If the flag is inabled, the code is processed as:
			text1<? code ...?>text2<?... code ?>text3
		If the flag is NOT inabled, the code is processed as:
			text1<? code ...?>text2<?... ?><?= code ?>text3

	PYP_READER_FLAG_ON_CONTINUATION_ALLOW_LATE_ERROR_OUTPUT
		Allows tags to fully evaluate before erroring
		Example:
			text1<?... code1 ...?>text2<? code2 ?>text3<?... code3 ?>text4
		If the flag is inabled, the code is processed as:
			text1<?... code1 ...?>text2(evaluated_code2_results)text3<?... code3 ?>text4
		If the flag is NOT inabled, the code is processed as: (note that 2 errors would be thrown here)
			text1<?... code1 ?>text2<? code2 ?>text3<?... code3 ?>text4
*/



typedef enum PypReadStatus_ {
	PYP_READ_OKAY = 0x0,
	PYP_READ_ERROR = 0x1,
	PYP_READ_ERROR_MEMORY = 0x2,
	PYP_READ_ERROR_OPEN = 0x3,
	PYP_READ_ERROR_READ = 0x4,
	PYP_READ_ERROR_WRITE = 0x5,
	PYP_READ_ERROR_CODE_EXECUTION = 0x6,
	PYP_READ_ERROR_DIRECTORY = 0x7,
} PypReadStatus;



struct PypTagGroup_;
struct PypProcessingInfo_;
struct PypDataBuffer_;
typedef uint32_t PypReaderFlags;



typedef struct PypReaderSettings_ {
	PypReaderFlags flags;
	PypSize readBlockCount;
	PypSize readBlockSize;
	const PypChar* errorMessages[PYP_READER_ERROR_ID_COUNT];
} PypReaderSettings;

typedef struct PypStreamPosition_ {
	PypSize charPosition;
	PypSize lineNumber;
	PypSize linePosition;
	PypSize newlineCompletion;
} PypStreamPosition;

typedef struct PypStreamLocation_ {
	union {
		struct {
			PypStreamPosition start;
			PypStreamPosition end;
		};
		PypStreamPosition positions[2];
	};
	struct PypStreamLocation_* nextSibling;
} PypStreamLocation;



PypReadStatus pypReadFromStream(FILE* inputStream, FILE* outputStream, FILE* errorStream, struct PypDataBuffer_* dataBuffer, const struct PypProcessingInfo_* processingInfo, const struct PypTagGroup_* group, const PypReaderSettings* settings, void* data);

PypReaderSettings* pypReaderSettingsCreate(PypReaderFlags flags, PypSize readBlockCount, PypSize readBlockSize);
void pypReaderSettingsDelete(PypReaderSettings* readSettings);



#endif


