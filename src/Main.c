#include <stddef.h>
#include <stdio.h>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <Python.h>
#include "PypTags.h"
#include "Memory.h"
#include "PypReader.h"
#include "PypProcessing.h"
#include "PypDataBufferModifiers.h"
#include "CommandLine.h"
#include "PypModule.h"
#include "Path.h"
#include "File.h"
#include "../res/Resources.h"




// Prototypes
typedef struct ArgumentError_ {
	const char* message;
	struct ArgumentError_* nextSibling;
} ArgumentError;

int main(int argc, char** argv);
static int mainInner(int argc, cmd_char** argv, const CommandLineDescriptor* cld, const CommandLineArgumentValuesDescriptor* clvd);
static CommandLineDescriptor* commandLineSetup();
static PypTagGroup* tagsInit(const PypProcessingInfo* piCodeBlock, const PypProcessingInfo* piCodeExpression, int allowContinuation);
static void usage(const cmd_char* applicationName, const CommandLineDescriptor* cld, FILE* outputStream);
static int compareCmdStringToCharString(const cmd_char* cmdString, const char* charString);

static ArgumentError* errorListExtend(const char* message);
static void errorListDelete(ArgumentError* errorList);



// Outer main
int
main(int argc, char** argv) {
	// Vars
	CommandLine* cl = NULL;
	CommandLineDescriptor* cld = NULL;
	CommandLineArgumentValuesDescriptor* clvd = NULL;
	int returnValue = 0;

	// Create command line
	if (
		(cl = commandLineGet(argc, argv)) == NULL ||
		(cld = commandLineSetup()) == NULL ||
		(clvd = commandLineParse(cl->argumentCount - 1, &cl->argumentValues[1], cld)) == NULL
	) {
		// Delete
		if (cl != NULL) commandLineDestroy(cl);
		if (cld != NULL) commandLineDescriptorDelete(cld);

		// Error
		returnValue = -1;

		// Output
		fprintf(stderr, "Command line setup error\n");
	}
	else {
		if (clvd->errorFirst != NULL) {
			// Command line parsing errors
			const char* message = NULL;
			CommandLineStatusMessage* error = clvd->errorFirst;

			fprintf(stderr, (error->nextSibling == NULL) ? "Error parsing command line arguments:\n" : "Errors parsing command line arguments:\n");

			for (; error != NULL; error = error->nextSibling) {
				// Get type
				switch (error->error) {
					case COMMAND_LINE_ERROR_MEMORY:
						message = "Memory error";
					break;
					case COMMAND_LINE_ERROR_INVALID_LONG_FLAG:
						message = "Invalid long flag";
					break;
					case COMMAND_LINE_ERROR_INVALID_SHORT_FLAG:
						message = "Invalid short flag";
					break;
					case COMMAND_LINE_ERROR_MISSING_VALUE:
						message = "Value missing";
					break;
					case COMMAND_LINE_ERROR_INVALID_UNORDERED_ENTRY:
						message = "Extra argument";
					break;
					default:
						message = NULL;
					break;
				}

				// Output
				fprintf(stderr, "  %s\n", message);
			}

			// Error
			returnValue = -1;
		}
		else {
			CommandLineArgumentValue* v;

			// Version info
			if ((v = commandLineArgumentValuesDescriptorGet(clvd, "version")) != NULL && v->defined) {
				// Version info
				printf("Version %s\n", RES_PRODUCT_VERSION_STR);
			}
			else if ((v = commandLineArgumentValuesDescriptorGet(clvd, "help")) != NULL && v->defined) {
				// Help
				usage(cl->argumentValues[0], cld, stdout);
			}
			else if (cl->argumentCount <= 1) {
				// Invalid usage
				usage(cl->argumentValues[0], cld, stderr);
				returnValue = -1;
			}
			else {
				// Execute
				returnValue = mainInner(cl->argumentCount, cl->argumentValues, cld, clvd);
			}

			// Destroy command line
			commandLineArgumentValuesDescriptorDelete(clvd);
			commandLineDescriptorDelete(cld);
			commandLineDestroy(cl);
		}
	}

	// Debugging stats
	#ifndef NDEBUG
	memoryStats();
	memoryCleanup();
	printf("returnValue = %d;\n", returnValue);
	#endif

	// Done
	return returnValue;
}



// Main
int
mainInner(int argc, cmd_char** argv, const CommandLineDescriptor* cld, const CommandLineArgumentValuesDescriptor* clvd) {
	// Vars
	PypProcessingInfo* piMain = NULL;
	PypProcessingInfo* piCodeBlock = NULL;
	PypProcessingInfo* piCodeExpression = NULL;
	PypTagGroup* optimizedTags = NULL;
	PypPythonState* pythonState = NULL;
	PypReaderSettings* readSettings = NULL;
	FILE* inputStream = NULL;
	FILE* outputStream = NULL;
	cmd_char* inputFilename = NULL;
	cmd_char* outputFilename = NULL;
	int returnCode = 0;

	CommandLineArgumentValue* v;
	PypReaderFlags flags = PYP_READER_FLAG_ON_UNCLOSED_TAG_ERROR | PYP_READER_FLAG_ON_CONTINUATION_UNMATCHED_TAG_ERROR | PYP_READER_FLAG_ON_CONTINUATION_MISMATCHED_TAG_ERROR;
	PypSize readBlockCount = 2;
	PypSize readBlockSize = 10240;
	int allowContinuation = 1;
	FILE* errorStream = stderr;
	PypDataBufferModifier inlineErrorEscapeFunction = NULL;
	char* encodingDefault = "utf-8";
	char* encodingErrorModeDefault = "strict";
	char* encoding = encodingDefault;
	char* encodingErrorMode = encodingErrorModeDefault;
	ArgumentError* errorFirst = NULL;
	ArgumentError** errorNext = &errorFirst;

	// Read arguments
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "input")) != NULL && v->defined) {
		inputFilename = v->value;
		if (compareCmdStringToCharString(inputFilename, "-") == 0) {
			// Change to binary mode on Windows
			#ifdef WIN32
			setmode(fileno(stdin), O_BINARY);
			#endif

			inputStream = stdin;
		}
	}
	else {
		// Error
		*errorNext = errorListExtend("Missing input target");
		if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "output")) != NULL && v->defined) {
		outputFilename = v->value;
		if (compareCmdStringToCharString(outputFilename, "-") == 0) {
			// Change to binary mode on Windows
			#ifdef WIN32
			setmode(fileno(stdout), O_BINARY);
			#endif

			outputStream = stdout;
		}
	}
	else {
		// Error
		*errorNext = errorListExtend("Missing output target");
		if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
	}

	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "no-continuations")) != NULL && v->defined) {
		allowContinuation = 0;
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "read-block-size")) != NULL && v->defined) {
		char* value = NULL;
		size_t outputLength;
		size_t errorCount;

		if (unicodeUTF8Encode(v->value, &value, &outputLength, &errorCount) == UNICODE_OKAY) {
			char* valueEnd = value;
			long int numericValue;

			numericValue = strtol(value, &valueEnd, 10);
			if (valueEnd == value || *valueEnd != '\x00') {
				// Error
				*errorNext = errorListExtend("Invalid numeric format");
				if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
			}
			else if (numericValue <= 0) {
				// Error
				*errorNext = errorListExtend("Invalid numeric value");
				if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
			}
			else {
				// Apply value
				readBlockSize = numericValue;
				if (readBlockSize <= 0) {
					// Strange overflow error
					*errorNext = errorListExtend("Value overflow");
					if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
				}
			}

			// Clean
			memFree(value);
		}
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "read-block-count")) != NULL && v->defined) {
		char* value = NULL;
		size_t outputLength;
		size_t errorCount;

		if (unicodeUTF8Encode(v->value, &value, &outputLength, &errorCount) == UNICODE_OKAY) {
			char* valueEnd = value;
			long int numericValue;

			numericValue = strtol(value, &valueEnd, 10);
			if (valueEnd == value || *valueEnd != '\x00') {
				// Error
				*errorNext = errorListExtend("Invalid numeric format");
				if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
			}
			else if (numericValue <= 0) {
				// Error
				*errorNext = errorListExtend("Invalid numeric value");
				if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
			}
			else {
				// Apply value
				readBlockCount = numericValue;
				if (readBlockCount <= 0) {
					// Strange overflow error
					*errorNext = errorListExtend("Value overflow");
					if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
				}
			}

			// Clean
			memFree(value);
		}
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "inline-errors")) != NULL && v->defined) {
		errorStream = NULL;
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "inline-error-modifer")) != NULL && v->defined) {
		if (compareCmdStringToCharString(outputFilename, "html") == 0) {
			inlineErrorEscapeFunction = pypDataBufferModifyToEscapedHTML;
		}
		else if (compareCmdStringToCharString(outputFilename, "none") != 0) {
			// Invalid value
			*errorNext = errorListExtend("Invalid value");
			if (*errorNext != NULL) errorNext = &(*errorNext)->nextSibling;
		}
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "encoding")) != NULL && v->defined) {
		size_t outputLength;
		size_t errorCount;
		unicodeUTF8Encode(v->value, &encoding, &outputLength, &errorCount);
	}
	if ((v = commandLineArgumentValuesDescriptorGet(clvd, "encoding-errors")) != NULL && v->defined) {
		size_t outputLength;
		size_t errorCount;
		unicodeUTF8Encode(v->value, &encodingErrorMode, &outputLength, &errorCount);
	}



	// Errors
	if (errorFirst != NULL) {
		ArgumentError* error;

		// Error
		fprintf(stderr, (errorFirst->nextSibling == NULL) ? "Error processing command line arguments:\n" : "Errors processing command line arguments:\n");
		for (error = errorFirst; error != NULL; error = error->nextSibling) {
			fprintf(stderr, "  %s\n", error->message);
		}

		// Done
		returnCode = -1;
	}
	else if (
		(piMain = pypProcessingInfoCreate(NULL, NULL, inlineErrorEscapeFunction, NULL)) == NULL ||
		(piCodeBlock = pypProcessingInfoCreate(pypDataBufferModifyExecuteCode, NULL, NULL, pypDataBufferModifyToString)) == NULL ||
		(piCodeExpression = pypProcessingInfoCreate(pypDataBufferModifyExecuteExpression, NULL, NULL, pypDataBufferModifyToString)) == NULL ||
		(optimizedTags = tagsInit(piCodeBlock, piCodeExpression, allowContinuation)) == NULL ||
		(readSettings = pypReaderSettingsCreate(flags, readBlockCount, readBlockSize)) == NULL ||
		(pythonState = pypModulePythonSetup(argv[0])) == NULL ||
		pythonState->status != PYP_MODULE_SETUP_STATUS_OKAY
	) {
		// Error
		fprintf(stderr, "Processing setup error; likely ran out of memory\n");
		returnCode = -1;
	}
	else {
		if (
			(inputStream == NULL && fileOpenUnicode(inputFilename, "rb", &inputStream) != FILE_OPEN_OKAY) ||
			(outputStream == NULL && fileOpenUnicode(outputFilename, "wb", &outputStream) != FILE_OPEN_OKAY)
		) {
			if (inputStream == NULL) {
				// Error opening input
				fprintf(stderr, "Error opening input file\n");
			}
			else {
				// Error opening output
				fprintf(stderr, "Error opening output file\n");
			}

			// Error
			returnCode = -1;
		}
		else {
			PypModuleExecutionInfo exeInfo;

			// Set error messages
			readSettings->errorMessages[PYP_READER_ERROR_ID_UNCLOSED_TAG] = "Unclosed tag\n";
			readSettings->errorMessages[PYP_READER_ERROR_ID_CONTINUATION_UNMATCHED_OPENING_TAG] = "Invalid tag opening continuation\n";
			readSettings->errorMessages[PYP_READER_ERROR_ID_CONTINUATION_MISMATCHED_OPENING_TAG] = "Mismatched tag continuation opening\n";
			readSettings->errorMessages[PYP_READER_ERROR_ID_CONTINUATION_MISMATCHED_CLOSING_TAG] = "Mismatched tag continuation closing\n";

			// Execution setup
			if (pypModuleExecutionInfoCreate(
				&exeInfo,
				readSettings,
				piMain,
				piCodeBlock,
				piCodeExpression,
				optimizedTags,
				inputStream,
				outputStream,
				errorStream,
				NULL,
				inputFilename,
				encoding,
				encodingErrorMode,
				pythonState
			) == NULL) {
				// Error
				fprintf(stderr, "Execution setup error; likely ran out of memory\n");
				returnCode = -1;
			}
			else {
				PypReadStatus rs;

				// Setup pyp
				pypModulePythonInit(&exeInfo);

				// Execute
				rs = pypIncludeFromExecutionInfo(&exeInfo);
				if (rs != PYP_READ_OKAY) {
					const char* errorMessage = NULL;

					switch (rs) {
						case PYP_READ_ERROR_MEMORY:
							errorMessage = "Memory error";
						break;
						case PYP_READ_ERROR_OPEN:
							errorMessage = "File open error";
						break;
						case PYP_READ_ERROR_READ:
							errorMessage = "Read error";
						break;
						case PYP_READ_ERROR_WRITE:
							errorMessage = "Write error";
						break;
						case PYP_READ_ERROR_DIRECTORY:
							errorMessage = "Directory error";
						break;
						default:
							errorMessage = "Error";
						break;
					}

					fprintf(stderr, "An error occured during execution: %s\n", errorMessage);
					returnCode = 1;
				}

				// Deinit python
				pypModulePythonDeinit(&exeInfo);
				pypModuleExecutionInfoClean(&exeInfo);
			}
		}
	}

	// Clean
	if (encoding != encodingDefault) memFree(encoding);
	if (encodingErrorMode != encodingErrorModeDefault) memFree(encodingErrorMode);
	if (inputStream != NULL && inputStream != stdin) fclose(inputStream);
	if (outputStream != NULL && outputStream != stdout) fclose(outputStream);
	if (pythonState != NULL) pypModulePythonFinalize(pythonState);
	if (readSettings != NULL) pypReaderSettingsDelete(readSettings);
	if (optimizedTags != NULL) pypTagGroupDeleteTree(optimizedTags);
	if (piMain != NULL) pypProcessingInfoDelete(piMain);
	if (piCodeBlock != NULL) pypProcessingInfoDelete(piCodeBlock);
	if (piCodeExpression != NULL) pypProcessingInfoDelete(piCodeExpression);
	errorListDelete(errorFirst);

	// Done
	return returnCode;
}



// Create command line descriptor
CommandLineDescriptor*
commandLineSetup() {
	// Command line parsing
	CommandLineDescriptor* commandLineDescriptor;
	CommandLineArgumentDescriptor* argDescriptorInput = NULL;
	CommandLineArgumentDescriptor* argDescriptorOutput = NULL;
	const char commandLineNameSeparator = '|';

	// Create descriptor
	commandLineDescriptor = commandLineDescriptorCreate();
	if (commandLineDescriptor == NULL) return NULL; // error

	// Add arguments
	if (
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"version",
			"version",
			"v",
			"Show version info and exit",
			NULL
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"help",
			"help|usage",
			"h?",
			"Show usage info and exit",
			NULL
		) == NULL ||
		(argDescriptorInput = commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"input",
			"input",
			"i",
			"The path to the input file, or \"-\" for stdin",
			"path"
		)) == NULL ||
		(argDescriptorOutput = commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"output",
			"output",
			"o",
			"The path to the output file, or \"-\" for stdout",
			"path"
		)) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"read-block-size",
			"read-block-size",
			"b",
			"The size (in characters) of a file read block",
			"size"
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"read-block-count",
			"read-block-count",
			"c",
			"The initial number of blocks to cycle between; default is 2",
			"count"
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"no-continuations",
			"no-continuations",
			NULL,
			"Disable tag continuations",
			NULL
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"inline-errors",
			"inline-errors",
			NULL,
			"Errors will be output in the file rather than to stderr",
			NULL
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"inline-error-modifer",
			"inline-error-modifer",
			NULL,
			"How to modify inline error messages; available values are \"html\", \"none\"; default is \"none\"",
			"method"
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"encoding",
			"encoding",
			NULL,
			"Method of encoding unicode; available values are the same as python's .encode values; default is \"utf-8\"",
			"mode"
		) == NULL ||
		commandLineDescriptorArgumentAdd(commandLineDescriptor, commandLineNameSeparator,
			"encoding-errors",
			"encoding-errors",
			NULL,
			"Method dealing with encoding errors; available values are the same as python's .encode values; default is \"strict\"",
			"mode"
		) == NULL ||
		// Ordered arguments
		commandLineDescriptorOrderedArgumentAdd(
			commandLineDescriptor,
			argDescriptorInput
		) != COMMAND_LINE_OKAY ||
		commandLineDescriptorOrderedArgumentAdd(
			commandLineDescriptor,
			argDescriptorOutput
		) != COMMAND_LINE_OKAY
	) {
		// Cleanup
		commandLineDescriptorDelete(commandLineDescriptor);
		return NULL;
	}

	// Done
	return commandLineDescriptor;
}



// Setup python
PypTagGroup*
tagsInit(const PypProcessingInfo* piCodeBlock, const PypProcessingInfo* piCodeExpression, int allowContinuation) {
	// Vars
	PypTagGroup* tgLevel1 = NULL;
	PypTagGroup* tgLevel2 = NULL;
	PypTagGroup* tgCloser = NULL;
	PypTagGroup* tgEscapes = NULL;
	PypTagGroup* tgOptimized = NULL;
	PypTag* tag = NULL;

	// Assertions
	assert(piCodeBlock != NULL || piCodeExpression != NULL);


	// Create basic tag structure
	if ((tgLevel1 = pypTagGroupCreate()) == NULL) goto cleanup;
	if ((tgLevel2 = pypTagGroupCreate()) == NULL) goto cleanup;



	//{ Main tag <? ?>
	if (piCodeBlock != NULL) {
		if ((tgCloser = pypTagGroupCreate()) == NULL) goto cleanup;

		if ((tag = pypTagCreate("<?", 0, PYP_TAG_FLAGS_NONE, tgCloser, tgLevel2)) == NULL) goto cleanup;
		pypTagGroupAddTag(tgLevel1, tag);
		pypSetProcessingInfo(tag, piCodeBlock);

		if ((tag = pypTagCreate("?>", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
		pypTagGroupAddTag(tgCloser, tag);

		if (allowContinuation) {
			if ((tag = pypTagCreate("<?...", 0, PYP_TAG_FLAG_CONTINUATION, tgCloser, tgLevel2)) == NULL) goto cleanup;
			pypTagGroupAddTag(tgLevel1, tag);
			pypSetProcessingInfo(tag, piCodeBlock);

			if ((tag = pypTagCreate("...?>", 0, PYP_TAG_FLAG_CONTINUATION, NULL, NULL)) == NULL) goto cleanup;
			pypTagGroupAddTag(tgCloser, tag);
		}
	}
	//}

	//{ Main eval tag <?= ?>
	if (piCodeExpression != NULL) {
		if ((tgCloser = pypTagGroupCreate()) == NULL) goto cleanup;

		if ((tag = pypTagCreate("<?=", 0, PYP_TAG_FLAGS_NONE, tgCloser, tgLevel2)) == NULL) goto cleanup;
		pypTagGroupAddTag(tgLevel1, tag);
		pypSetProcessingInfo(tag, piCodeExpression);

		if ((tag = pypTagCreate("?>", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
		pypTagGroupAddTag(tgCloser, tag);

		if (allowContinuation) {
			if ((tag = pypTagCreate("<?=...", 0, PYP_TAG_FLAG_CONTINUATION, tgCloser, tgLevel2)) == NULL) goto cleanup;
			pypTagGroupAddTag(tgLevel1, tag);
			pypSetProcessingInfo(tag, piCodeExpression);

			if ((tag = pypTagCreate("...?>", 0, PYP_TAG_FLAG_CONTINUATION, NULL, NULL)) == NULL) goto cleanup;
			pypTagGroupAddTag(tgCloser, tag);
		}
	}
	//}

	//{ Single ' quoted string
	if ((tgCloser = pypTagGroupCreate()) == NULL) goto cleanup;
	if ((tgEscapes = pypTagGroupCreate()) == NULL) goto cleanup;

	if ((tag = pypTagCreate("\'", 0, PYP_TAG_FLAGS_NONE, tgCloser, tgEscapes)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgLevel2, tag);

	if ((tag = pypTagCreate("\'", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\r", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\n", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\r\n", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\\\r\n", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgEscapes, tag);

	if ((tag = pypTagCreate("\\", 1, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgEscapes, tag);
	//}

	//{ Single " quoted string
	if ((tgCloser = pypTagGroupCreate()) == NULL) goto cleanup;
	if ((tgEscapes = pypTagGroupCreate()) == NULL) goto cleanup;

	if ((tag = pypTagCreate("\"", 0, PYP_TAG_FLAGS_NONE, tgCloser, tgEscapes)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgLevel2, tag);

	if ((tag = pypTagCreate("\"", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\r", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\n", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\r\n", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\\\r\n", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgEscapes, tag);

	if ((tag = pypTagCreate("\\", 1, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgEscapes, tag);
	//}

	//{ Triple ' quoted string
	if ((tgCloser = pypTagGroupCreate()) == NULL) goto cleanup;
	if ((tgEscapes = pypTagGroupCreate()) == NULL) goto cleanup;

	if ((tag = pypTagCreate("\'\'\'", 0, PYP_TAG_FLAGS_NONE, tgCloser, tgEscapes)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgLevel2, tag);

	if ((tag = pypTagCreate("\'\'\'", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\\", 1, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgEscapes, tag);
	//}

	//{ Triple " quoted string
	if ((tgCloser = pypTagGroupCreate()) == NULL) goto cleanup;
	if ((tgEscapes = pypTagGroupCreate()) == NULL) goto cleanup;

	if ((tag = pypTagCreate("\"\"\"", 0, PYP_TAG_FLAGS_NONE, tgCloser, tgEscapes)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgLevel2, tag);

	if ((tag = pypTagCreate("\"\"\"", 0, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgCloser, tag);

	if ((tag = pypTagCreate("\\", 1, PYP_TAG_FLAGS_NONE, NULL, NULL)) == NULL) goto cleanup;
	pypTagGroupAddTag(tgEscapes, tag);
	//}




	// Optimize
	tgOptimized = pypTagGroupOptimize(tgLevel1);
	if (tgOptimized == NULL) goto cleanup;


	// Delete setup group tree
	pypTagGroupDeleteTree(tgLevel1);


	// Done
	return tgOptimized;


	// Depending on where the error occured, this might not free all the memory created
	// Though this is not really a problem, since the application will exit on error
	cleanup:
	if (tgLevel1 != NULL) pypTagGroupDeleteTree(tgLevel1);
	return NULL;
}



// Usage info
void
usage(const cmd_char* applicationName, const CommandLineDescriptor* cld, FILE* outputStream) {
	// Vars
	char* applicationNameEncoded = NULL;
	char* applicationNameEncodedFilename;
	char* tempStr;
	CommandLineArgumentDescriptor* argument;
	CommandLineArgumentStringListEntry* argName;
	size_t outputLength;
	size_t errorCount;
	size_t i;

	// Assertions
	assert(applicationName != NULL);
	assert(cld != NULL);
	assert(outputStream != NULL);

	// Encode
	if (unicodeUTF8Encode(applicationName, &applicationNameEncoded, &outputLength, &errorCount) != UNICODE_OKAY) return; // error

	// Find filename
	i = 0;
	applicationNameEncodedFilename = applicationNameEncoded;
	while (applicationNameEncoded[i] != '\x00') {
		if (pathCharIsSeparatorAnsi(applicationNameEncoded[i])) {
			applicationNameEncodedFilename = &applicationNameEncoded[++i];
		}
		else {
			++i;
		}
	}


	// Output usage info
	fprintf(outputStream,
		"Usage:\n"
		"    %s [arguments] input-filename output-filename\n\n\n"
		"Available flags:\n",
		applicationNameEncodedFilename
	);

	// Arguments
	for (argument = cld->argumentFirst; argument != NULL; argument = argument->nextSibling) {
		if ((argName = argument->firstLongName) != NULL) {
			for (; argName != NULL; argName = argName->nextSibling) {
				if (unicodeUTF8Encode(argName->string, &tempStr, &outputLength, &errorCount) != UNICODE_OKAY) continue; // error

				fprintf(outputStream, "  --%s", tempStr);
				if (argument->paramName != NULL) {
					fprintf(outputStream, " <%s>", argument->paramName);
				}
				fprintf(outputStream, "\n");

				memFree(tempStr);
			}
		}
		if ((argName = argument->firstShortName) != NULL) {
			for (; argName != NULL; argName = argName->nextSibling) {
				if (unicodeUTF8Encode(argName->string, &tempStr, &outputLength, &errorCount) != UNICODE_OKAY) continue; // error

				if (argName == argument->firstShortName) {
					fprintf(outputStream, "  -%s", tempStr);
				}
				else {
					fprintf(outputStream, ", -%s", tempStr);
				}
				if (argument->paramName != NULL) {
					fprintf(outputStream, " <%s>", argument->paramName);
				}

				memFree(tempStr);
			}
			fprintf(outputStream, "\n");
		}

		if (argument->description != NULL) {
			fprintf(outputStream, "    %s\n", argument->description);
		}

		fprintf(outputStream, "\n");
	}

	// Clear
	if (applicationNameEncoded != NULL) memFree(applicationNameEncoded);
}



// Compare strings
int
compareCmdStringToCharString(const cmd_char* cmdString, const char* charString) {
	cmd_char c;

	while (1) {
		c = (unsigned char) (*charString);
		if (*cmdString == c) {
			// Same
			if (c == '\x00') return 0; // Done
			++cmdString;
			++charString;
		}
		else {
			// Differ
			if (*cmdString < c) return -1;
			return 1;
		}
	}
}



// Error list
ArgumentError*
errorListExtend(const char* message) {
	// Vars
	ArgumentError* e;

	// Assertions
	assert(message != NULL);

	// Create
	e = memAlloc(ArgumentError);
	if (e == NULL) return NULL; // error

	// Members
	e->message = message;
	e->nextSibling = NULL;

	// Done
	return e;
}

void
errorListDelete(ArgumentError* errorList) {
	ArgumentError* next;

	for (; errorList != NULL; errorList = next) {
		// Delete
		next = errorList->nextSibling;
		memFree(errorList);
	}
}



// Pronounced as: p as in "pneumatic", y as in "vinyl", p as in "psychic"


