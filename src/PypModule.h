#ifndef __PYP_CLASS_H
#define __PYP_CLASS_H



#include <Python.h>
#include <stdio.h>
#include "PypTags.h"
#include "PypDataBuffer.h"
#include "PypProcessing.h"
#include "PypReader.h"
#include "Unicode.h"
#include "CommandLineChar.h"


struct PypPythonState_;

typedef struct PypModuleExecutionInfo_ {
	PypReaderSettings* readSettings;

	PypProcessingInfo* piMain;
	PypProcessingInfo* piCodeBlock;
	PypProcessingInfo* piCodeExpression;

	PypTagGroup* optimizedTags;

	FILE* inputStream;
	FILE* outputStream;
	FILE* errorStream;
	PypDataBuffer* outputDataBuffer;

	cmd_char* inputFilename;
	PypSize inputFilenameLength;
	PypSize inputFilenameStart;

	const char* encoding;
	const char* encodingErrorMode;

	struct PypPythonState_* pythonState;
} PypModuleExecutionInfo;

typedef enum PypModuleSetupStatus_ {
	PYP_MODULE_SETUP_STATUS_OKAY = 0x0,
	PYP_MODULE_SETUP_STATUS_ERROR_MEMORY = 0x1,
	PYP_MODULE_SETUP_STATUS_ERROR_PYTHON = 0x2,
} PypModuleSetupStatus;

typedef struct PypPythonState_ {
	char* applicationName;
	unicode_char* applicationNameUnicode;
	PypModuleSetupStatus status;

	PyObject* mainModule;
	PyObject* pypModule;
	PyObject* globalsDict;
	PyObject* localsDict;

	PyObject* exceptionHandlerCompiledCode;
	PyObject* exceptionHandlerGlobalsDict;

	PyObject* includeFunctionOsModule;
	PyObject* includeFunctionOsPathModule;
} PypPythonState;



#if PY_MAJOR_VERSION >= 3
#define pypModuleInit PyInit_pyp
PyObject* pypModuleInit(void);
#else
#define pypModuleInit initpyp
void pypModuleInit(void);
#endif



// Non-module methods
PypPythonState* pypModulePythonSetup(const cmd_char* applicationPath);
void pypModulePythonFinalize(PypPythonState* pythonState);
PypModuleSetupStatus pypModulePythonInit(PypModuleExecutionInfo* pypState);
void pypModulePythonDeinit(PypModuleExecutionInfo* pypState);

PypReadStatus pypIncludeFromExecutionInfo(PypModuleExecutionInfo* executionInfo);

PypModuleExecutionInfo* pypModuleExecutionInfoCreate(
	PypModuleExecutionInfo* info,
	PypReaderSettings* readSettings,
	PypProcessingInfo* piMain,
	PypProcessingInfo* piCodeBlock,
	PypProcessingInfo* piCodeExpression,
	PypTagGroup* optimizedTags,
	FILE* inputStream,
	FILE* outputStream,
	FILE* errorStream,
	PypDataBuffer* outputDataBuffer,
	const cmd_char* inputFilename,
	const char* encoding,
	const char* encodingErrorMode,
	PypPythonState* pythonState
);
void pypModuleExecutionInfoClean(PypModuleExecutionInfo* executionInfo);

PypReadStatus pypDataBufferModifyExecuteCode(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data);
PypReadStatus pypDataBufferModifyExecuteExpression(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data);



#endif


