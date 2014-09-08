#include <assert.h>
#include <Python.h>
#include <frameobject.h>
#if PY_MAJOR_VERSION < 3
#include <cStringIO.h>
#endif
#include "PypModule.h"
#include "PypReader.h"
#include "PypDataBuffer.h"
#include "Memory.h"
#include "Path.h"
#include "File.h"



// Structs
typedef struct PypModuleState_ {
	PyObject* error;
} PypModuleState;


// Module indo
PyDoc_STRVAR(pypModuleName, "pyp");
PyDoc_STRVAR(pypDocModule, "Python preprocessing module");
PyDoc_STRVAR(pypModuleExceptionName, "Error");
PyDoc_STRVAR(pypCompiledCodeFilenamePrefix, "pyp:");

// Module methods
PyDoc_STRVAR(pypDoc_include, "Include a file using the Python preprocessor");
static PyObject* pyp_include(PyObject* self, PyObject* args);

PyDoc_STRVAR(pypDoc_write, "Write to the output file stream");
static PyObject* pyp_write(PyObject* self, PyObject* args);

static PyMethodDef moduleMethods[] = {
    { "include", (PyCFunction) pyp_include , METH_VARARGS , pypDoc_include },
    { "write", (PyCFunction) pyp_write , METH_VARARGS , pypDoc_write },
	{ NULL } // sentinel
};

// Other
static PypDataBuffer* pypCurrentDataBuffer = NULL;
static PypModuleExecutionInfo* pypCurrentExecutionInfo = NULL;

// More methods
static void pypModuleExceptionHandlingDeinit(PypPythonState* pyState);
static PypModuleSetupStatus pypModuleExceptionHandlingInit(PypPythonState* pyState);
static PypBool pypPythonExceptionDisplay(PypDataBuffer* output, PypModuleExecutionInfo* executionInfo);

static PypBool pypCharIsWhitespaceNotNewline(PypChar c);
static PyObject* pypCompileCode(PypDataBuffer* output, PypModuleExecutionInfo* executionInfo, const PypStreamLocation* streamLocation, const char* sourceCode, PypBool isEval);
static PypReadStatus pypExecuteCode(PypDataBuffer* output, PypModuleExecutionInfo* executionInfo, PyObject* code, PypBool outputResult);

static PypBool pypStringObjectSetup(PyObject* object, const char* encoding, const char* encodingErrorMode, PyObject** newObject, char** buffer, Py_ssize_t* bufferLength);
static PypBool pypStringObjectExtendStream(FILE* stream, PyObject* object, const char* encoding, const char* encodingErrorMode);
static PypBool pypStringObjectExtendDataBuffer(PypDataBuffer* dataBuffer, PyObject* object, const char* encoding, const char* encodingErrorMode);

static void pypModuleIncludeFunctionsDeinit(PypPythonState* pyState);
static PypModuleSetupStatus pypModuleIncludeFunctionsInit(PypPythonState* pyState);
static PypBool pypPathAbsolute(PypPythonState* pyState, PyObject* object, unicode_char** buffer, size_t* bufferLength);
static PypBool pypPathCurrentDirectoryGet(PypPythonState* pyState, unicode_char** path, size_t* pathLength);
static PypBool pypPathCurrentDirectorySet(PypPythonState* pyState, const unicode_char* path);

static PypReadStatus pypDataBufferModifyExecute(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data, PypBool expression);



// Error state
#if PY_MAJOR_VERSION >= 3
#define GETSTATE(module) ((PypModuleState*) PyModule_GetState(module))
#else
static PypModuleState pypModuleState;
#define GETSTATE(module) (&pypModuleState)
#endif



// Python 3 specific things
#if PY_MAJOR_VERSION >= 3
static int pyp_traverse(PyObject* module, visitproc visit, void* arg);
static int pyp_clear(PyObject* module);

int
pyp_traverse(PyObject* module, visitproc visit, void* arg) {
	Py_VISIT(GETSTATE(module)->error);
	return 0;
}

int
pyp_clear(PyObject* module) {
	Py_CLEAR(GETSTATE(module)->error);
	return 0;
}

static struct PyModuleDef moduleDefinition = {
	PyModuleDef_HEAD_INIT,
	pypModuleName, // m_name
	pypDocModule, // m_doc
	sizeof(PypModuleState), // m_size
	moduleMethods, // m_methods
	NULL, // m_reload
	pyp_traverse, // m_traverse
	pyp_clear, // m_clear
	NULL // m_free
};
#endif



// Init function
#if PY_MAJOR_VERSION >= 3
#define INIT_ERROR return NULL
PyObject*
pypModuleInit(void)
#else
#define INIT_ERROR return
void
pypModuleInit(void)
#endif
{
	PyObject* module;
	PypModuleState* state;
	size_t moduleNameLength;
	size_t exceptionNameLength;
	char* exceptionName;

	// Create
	#if PY_MAJOR_VERSION >= 3
	module = PyModule_Create(&moduleDefinition);
	#else
	module = Py_InitModule3(pypModuleName, moduleMethods, pypDocModule);
	#endif
	if (module == NULL) INIT_ERROR; // error

	// Setup the exception name
	moduleNameLength = strlen(pypModuleName);
	exceptionNameLength = strlen(pypModuleExceptionName);
	exceptionName = memAllocArray(char, moduleNameLength + exceptionNameLength + 2);

	memcpy(exceptionName, pypModuleName, sizeof(char) * moduleNameLength);
	exceptionName[moduleNameLength] = '.';
	memcpy(&exceptionName[moduleNameLength + 1], pypModuleExceptionName, sizeof(char) * (exceptionNameLength + 1));

	// Get the state and give it an error
	state = GETSTATE(module);
	state->error = PyErr_NewExceptionWithDoc(exceptionName, NULL, NULL, NULL);
	memFree(exceptionName);

	if (state->error == NULL) {
		// Cleanup
		Py_DECREF(module);
		INIT_ERROR;
	}

	#if PY_MAJOR_VERSION < 3
	// Import cStringIO
	PycString_IMPORT;
	#endif

	// Done
	#if PY_MAJOR_VERSION >= 3
	return module;
	#endif
}



// Methods
PyObject*
pyp_include(PyObject* self, PyObject* args) {
	// Vars
	PyObject* object;
	unicode_char* filename;
	size_t filenameLength;
	FILE* inputStream;
	PypReadStatus rs = PYP_READ_OKAY;

	// Assertions
	assert(pypCurrentDataBuffer != NULL);
	assert(pypCurrentExecutionInfo != NULL);

	// Recursion start
	if (Py_EnterRecursiveCall(" in include")) {
		// Recursion limit
		Py_LeaveRecursiveCall();
		return NULL;
	}

	// Get the file name
	if (
		!PyArg_UnpackTuple(args, "include", 1, 1, &object) ||
		!pypPathAbsolute(pypCurrentExecutionInfo->pythonState, object, &filename, &filenameLength)
	) {
		// Error
		PyErr_BadArgument();
		Py_LeaveRecursiveCall();
		return NULL;
	}

	// Open file
	if (fileOpenUnicode(filename, "rb", &inputStream) == FILE_OPEN_OKAY) {
		// Setup execution info
		PypModuleExecutionInfo exeInfo;
		PypDataBuffer* outputDataBuffer = NULL;

		if (
			(outputDataBuffer = pypDataBufferCreate()) != NULL &&
			pypModuleExecutionInfoCreate(
				&exeInfo,
				pypCurrentExecutionInfo->readSettings,
				pypCurrentExecutionInfo->piMain,
				pypCurrentExecutionInfo->piCodeBlock,
				pypCurrentExecutionInfo->piCodeExpression,
				pypCurrentExecutionInfo->optimizedTags,
				inputStream,
				pypCurrentExecutionInfo->outputStream,
				pypCurrentExecutionInfo->errorStream,
				outputDataBuffer,
				filename,
				pypCurrentExecutionInfo->encoding,
				pypCurrentExecutionInfo->encodingErrorMode,
				pypCurrentExecutionInfo->pythonState
			) != NULL
		) {
			// If necessary: https://docs.python.org/2.7/c-api/reflection.html
			rs = pypIncludeFromExecutionInfo(&exeInfo);

			// Delete execution info
			pypModuleExecutionInfoClean(&exeInfo);

			// Output buffer to previous buffer
			assert(pypCurrentDataBuffer != outputDataBuffer);
			pypDataBufferExtendWithDataBufferAndDelete(pypCurrentDataBuffer, outputDataBuffer);
		}
		else if (outputDataBuffer != NULL) {
			// Delete
			pypDataBufferDelete(outputDataBuffer);
		}
	}
	else {
		// Exception
		PyErr_SetString(
			#if PY_MAJOR_VERSION >= 3
			PyExc_FileNotFoundError,
			#else
			PyExc_IOError,
			#endif
			"Error opening include file"
		);

		// Clean
		memFree(filename);

		// Recursion complete
		Py_LeaveRecursiveCall();
		return NULL;
	}

	// Clean
	memFree(filename);

	// Recursion complete
	Py_LeaveRecursiveCall();

	// Return
	if (rs != PYP_READ_OKAY) {
		// Display error
		switch (rs) {
			case PYP_READ_ERROR_MEMORY:
			{
				// Exception
				PyErr_SetString(
					PyExc_MemoryError,
					"Memory error"
				);
			}
			break;
			case PYP_READ_ERROR_OPEN:
			{
				// Exception
				PyErr_SetString(
					#if PY_MAJOR_VERSION >= 3
					PyExc_FileNotFoundError,
					#else
					PyExc_IOError,
					#endif
					"File open error"
				);
			}
			break;
			case PYP_READ_ERROR_READ:
			{
				// Exception
				PyErr_SetString(
					#if PY_MAJOR_VERSION >= 3
					PyExc_PermissionError,
					#else
					PyExc_IOError,
					#endif
					"Read error"
				);
			}
			break;
			case PYP_READ_ERROR_WRITE:
			{
				// Exception
				PyErr_SetString(
					#if PY_MAJOR_VERSION >= 3
					PyExc_PermissionError,
					#else
					PyExc_IOError,
					#endif
					"Write error"
				);
			}
			break;
			case PYP_READ_ERROR_DIRECTORY:
			{
				// Exception
				PyErr_SetString(
					#if PY_MAJOR_VERSION >= 3
					PyExc_FileNotFoundError,
					#else
					PyExc_IOError,
					#endif
					"Directory error"
				);
			}
			break;
			default:
			{
				// Exception
				PyErr_SetString(
					PyExc_Exception,
					"Error"
				);
			}
			break;
		}

		// Error
		return NULL;
	}

	// Done
	Py_RETURN_NONE;
}

PyObject*
pyp_write(PyObject* self, PyObject* args) {
	// Vars
	PyObject* object;

	// Assertions
	assert(pypCurrentDataBuffer != NULL);
	assert(pypCurrentExecutionInfo != NULL);

	// Get and modift
	if (
		!PyArg_UnpackTuple(args, "write", 1, 1, &object) ||
		!pypStringObjectExtendDataBuffer(pypCurrentDataBuffer, object, pypCurrentExecutionInfo->encoding, pypCurrentExecutionInfo->encodingErrorMode)
	) {
		// Error
		PyErr_BadArgument();
		return NULL;
	}

	// Done
	Py_RETURN_NONE;
}



// Visible methods
PypReadStatus
pypIncludeFromExecutionInfo(PypModuleExecutionInfo* executionInfo) {
	// Vars
	PypReadStatus readStatus;
	PypModuleExecutionInfo* previousExecutionInfo;
	unicode_char* applicationCwd = NULL;
	unicode_char* pythonCwd = NULL;
	cmd_char* newCwd = NULL;
	size_t applicationCwdLength;
	size_t pythonCwdLength;

	// Assertions
	assert(executionInfo != NULL);
	assert(executionInfo->pythonState != NULL);
	assert(executionInfo->pythonState->mainModule != NULL);
	assert(executionInfo->pythonState->pypModule != NULL);
	assert(executionInfo->pythonState->globalsDict != NULL);

	// Get the current directory
	previousExecutionInfo = pypCurrentExecutionInfo;
	if (
		pathGetCurrentWorkingDirectoryUnicode(&applicationCwd, &applicationCwdLength) != PATH_OKAY ||
		!pypPathCurrentDirectoryGet(executionInfo->pythonState, &pythonCwd, &pythonCwdLength) ||
		(newCwd = memAllocArray(cmd_char, executionInfo->inputFilenameStart)) == NULL
	) {
		// Error
		if (applicationCwd != NULL) memFree(applicationCwd);
		if (pythonCwd != NULL) memFree(pythonCwd);
		return PYP_READ_ERROR_DIRECTORY;
	}

	// Setup
	memcpy(newCwd, executionInfo->inputFilename, sizeof(cmd_char) * (executionInfo->inputFilenameStart - 1));
	newCwd[executionInfo->inputFilenameStart - 1] = '\x00';

	pypCurrentExecutionInfo = executionInfo;
	pathSetCurrentWorkingDirectoryUnicode(newCwd);
	pypPathCurrentDirectorySet(executionInfo->pythonState, newCwd);


	// Process
	readStatus = pypReadFromStream(executionInfo->inputStream, executionInfo->outputStream, executionInfo->errorStream, executionInfo->outputDataBuffer, executionInfo->piMain, executionInfo->optimizedTags, executionInfo->readSettings, executionInfo);


	// Revert current directory
	pypPathCurrentDirectorySet(executionInfo->pythonState, pythonCwd);
	pathSetCurrentWorkingDirectoryUnicode(applicationCwd);
	memFree(newCwd);
	memFree(pythonCwd);
	memFree(applicationCwd);

	// Revert
	pypCurrentExecutionInfo = previousExecutionInfo;

	// Don
	return readStatus;
}

PypModuleExecutionInfo*
pypModuleExecutionInfoCreate(PypModuleExecutionInfo* info, PypReaderSettings* readSettings, PypProcessingInfo* piMain, PypProcessingInfo* piCodeBlock, PypProcessingInfo* piCodeExpression, PypTagGroup* optimizedTags, FILE* inputStream, FILE* outputStream, FILE* errorStream, PypDataBuffer* outputDataBuffer, const cmd_char* inputFilename, const char* encoding, const char* encodingErrorMode, PypPythonState* pythonState) {
	// Vars
	PypBool created = PYP_FALSE;
	size_t i;

	// Assertions
	assert(readSettings != NULL);
	assert(piMain != NULL);
	assert(piCodeBlock != NULL || piCodeExpression != NULL);
	assert(optimizedTags != NULL);
	assert(inputStream != NULL);
	assert(outputStream != NULL);
	assert(errorStream != NULL);
	assert(inputFilename != NULL);
	assert(encoding != NULL);
	assert(encodingErrorMode != NULL);
	assert(pythonState != NULL);

	// Create
	if (info == NULL) {
		info = memAlloc(PypModuleExecutionInfo);
		if (info == NULL) return NULL;
		created = PYP_TRUE;
	}

	// Set
	info->readSettings = readSettings;

	info->piMain = piMain;
	info->piCodeBlock = piCodeBlock;
	info->piCodeExpression = piCodeExpression;

	info->optimizedTags = optimizedTags;

	info->inputStream = inputStream;
	info->outputStream = outputStream;
	info->errorStream = errorStream;
	info->outputDataBuffer = outputDataBuffer;

	// Copy file name
	if (pathAbsoluteUnicode(inputFilename, &info->inputFilename, &info->inputFilenameLength) != PATH_OKAY) goto cleanup; // error
	info->inputFilenameStart = 0;
	for (i = info->inputFilenameLength; i > 0; ) {
		--i;

		if (pathCharIsSeparatorUnicode(info->inputFilename[i])) {
			// Done
			info->inputFilenameStart = i + 1;
			break;
		}
	}

	info->encoding = encoding;
	info->encodingErrorMode = encodingErrorMode;

	info->pythonState = pythonState;

	// Done
	return info;

	// Cleanup
	cleanup:
	if (created) memFree(info);
	return NULL;
}

void
pypModuleExecutionInfoClean(PypModuleExecutionInfo* executionInfo) {
	assert(executionInfo != NULL);
	assert(executionInfo->inputFilename != NULL);

	memFree(executionInfo->inputFilename);
}

PypPythonState*
pypModulePythonSetup(const cmd_char* applicationPath) {
	// Vars
	PypPythonState* state;
	size_t applicationPathLength;
	size_t outputCharacterCount;
	size_t errorCount;

	// Assertions
	assert(applicationPath != NULL);

	// Create state
	state = memAlloc(PypPythonState);
	if (state == NULL) return NULL; // error

	// Vars
	state->mainModule = NULL;
	state->pypModule = NULL;
	state->globalsDict = NULL;
	state->localsDict = NULL;

	state->exceptionHandlerCompiledCode = NULL;
	state->exceptionHandlerGlobalsDict = NULL;

	state->includeFunctionOsModule = NULL;
	state->includeFunctionOsPathModule = NULL;

	// Setup paths
	state->applicationName = NULL;
	state->applicationNameUnicode = NULL;

	applicationPathLength = getUnicodeCharStringLength(applicationPath);
	state->applicationNameUnicode = memAllocArray(cmd_char, applicationPathLength + 1);
	if (state->applicationNameUnicode == NULL) {
		state->status = PYP_MODULE_SETUP_STATUS_ERROR_MEMORY;
		return state; // error
	}
	memcpy(state->applicationNameUnicode, applicationPath, sizeof(cmd_char) * (applicationPathLength + 1));

	if (unicodeUTF8EncodeLength(applicationPath, applicationPathLength, &state->applicationName, &outputCharacterCount,&errorCount) != UNICODE_OKAY) {
		state->status = PYP_MODULE_SETUP_STATUS_ERROR_MEMORY;
		return state; // error
	}

	// Add the module to be initialized
	if (PyImport_AppendInittab("pyp", pypModuleInit) != 0) {
		state->status = PYP_MODULE_SETUP_STATUS_ERROR_PYTHON;
		return state; // error
	}

	// Setup python
	#if PY_MAJOR_VERSION >= 3
	Py_SetProgramName(state->applicationNameUnicode);
	#else
	Py_SetProgramName(state->applicationName);
	#endif
	Py_Initialize();

	// Okay
	state->status = PYP_MODULE_SETUP_STATUS_OKAY;
	return state;
}

void
pypModulePythonFinalize(PypPythonState* pythonState) {
	assert(pythonState != NULL);

	// Finish
	Py_Finalize();

	// Delete paths
	if (pythonState->applicationName != NULL) memFree(pythonState->applicationName);
	if (pythonState->applicationNameUnicode != NULL) memFree(pythonState->applicationNameUnicode);
	memFree(pythonState);
}

PypModuleSetupStatus
pypModulePythonInit(PypModuleExecutionInfo* executionInfo) {
	// Vars
	PypPythonState* pyState;

	// Assertions
	assert(executionInfo != NULL);
	assert(executionInfo->pythonState != NULL);
	assert(executionInfo->pythonState->mainModule == NULL);
	assert(executionInfo->pythonState->pypModule == NULL);
	assert(executionInfo->pythonState->globalsDict == NULL);
	assert(executionInfo->pythonState->localsDict == NULL);

	// Module importing
	pyState = executionInfo->pythonState;
	if (
		(pyState->mainModule = PyImport_AddModule("__main__")) == NULL ||
		(pyState->pypModule = PyImport_ImportModule(pypModuleName)) == NULL ||
		(pyState->globalsDict = PyDict_Copy(PyModule_GetDict(pyState->mainModule))) == NULL ||
		PyDict_SetItemString(pyState->globalsDict, pypModuleName, pyState->pypModule) != 0
	) {
		// Error
		pypModulePythonDeinit(executionInfo);
		return PYP_MODULE_SETUP_STATUS_ERROR_PYTHON;
	}

	// Okay
	return PYP_MODULE_SETUP_STATUS_OKAY;
}

void
pypModulePythonDeinit(PypModuleExecutionInfo* executionInfo) {
	// Vars
	PypPythonState* pyState;

	// Assertions
	assert(executionInfo != NULL);
	assert(executionInfo->pythonState != NULL);

	// Clear
	pyState = executionInfo->pythonState;
	if (pyState->globalsDict != NULL) {
		Py_DECREF(pyState->globalsDict);
		pyState->globalsDict = NULL;
	}
	if (pyState->localsDict != NULL) {
		Py_DECREF(pyState->localsDict);
		pyState->localsDict = NULL;
	}
	if (pyState->pypModule != NULL) {
		Py_DECREF(pyState->pypModule);
		pyState->pypModule = NULL;
	}
	if (pyState->mainModule != NULL) {
		pyState->mainModule = NULL;
	}

	pypModuleExceptionHandlingDeinit(pyState);
	pypModuleIncludeFunctionsDeinit(pyState);
}



// Object output
PypBool
pypStringObjectSetup(PyObject* object, const char* encoding, const char* encodingErrorMode, PyObject** newObject, char** buffer, Py_ssize_t* bufferLength) {
	assert(object != NULL);
	assert(encoding != NULL);
	assert(encodingErrorMode != NULL);
	assert(newObject != NULL);
	assert(buffer != NULL);
	assert(bufferLength != NULL);

	#if PY_MAJOR_VERSION >= 3
	if (PyUnicode_Check(object)) {
		// Unicode
		return (
			(*newObject = PyObject_CallMethod(object, "encode", "ss", encoding, encodingErrorMode)) != NULL &&
			PyBytes_AsStringAndSize(*newObject, buffer, bufferLength) == 0
		);
	}
	else if (PyBytes_Check(object)) {
		// Standard string
		*newObject = NULL;
		return (
			PyBytes_AsStringAndSize(object, buffer, bufferLength) == 0
		);
	}
	#else
	if (PyUnicode_Check(object)) {
		// Unicode
		return (
			(*newObject = PyObject_CallMethod(object, "encode", "ss", encoding, encodingErrorMode)) != NULL &&
			PyString_AsStringAndSize(*newObject, buffer, bufferLength) == 0
		);
	}
	else if (PyString_Check(object)) {
		// Standard string
		*newObject = NULL;
		return (
			PyString_AsStringAndSize(object, buffer, bufferLength) == 0
		);
	}
	#endif

	// Error: not a string
	return PYP_FALSE;
}

PypBool
pypStringObjectExtendStream(FILE* stream, PyObject* object, const char* encoding, const char* encodingErrorMode) {
	// Vars
	PyObject* newObject;
	char* buffer;
	Py_ssize_t bufferLength;

	// Assertions
	assert(stream != NULL);
	assert(object != NULL);
	assert(encoding != NULL);
	assert(encodingErrorMode != NULL);

	if (pypStringObjectSetup(object, encoding, encodingErrorMode, &newObject, &buffer, &bufferLength)) {
		// Output
		fwrite(buffer, sizeof(char), bufferLength, stream);

		// Clear
		if (newObject != NULL) Py_DECREF(newObject);
		return PYP_TRUE;
	}

	// Error
	return PYP_FALSE;
}

PypBool
pypStringObjectExtendDataBuffer(PypDataBuffer* dataBuffer, PyObject* object, const char* encoding, const char* encodingErrorMode) {
	// Vars
	PyObject* newObject;
	char* buffer;
	Py_ssize_t bufferLength;

	// Assertions
	assert(dataBuffer != NULL);
	assert(object != NULL);
	assert(encoding != NULL);
	assert(encodingErrorMode != NULL);

	if (pypStringObjectSetup(object, encoding, encodingErrorMode, &newObject, &buffer, &bufferLength)) {
		// Output
		if (bufferLength > 0) {
			PypDataBufferEntry* entry = pypDataBufferExtendWithData(dataBuffer, buffer, bufferLength);
			if (entry == NULL) {
				// Error
				if (newObject != NULL) Py_DECREF(newObject);
				return PYP_FALSE;
			}
		}

		// Clear
		if (newObject != NULL) Py_DECREF(newObject);
		return PYP_TRUE;
	}

	// Error
	return PYP_FALSE;
}



// Python cwd interaction
PypModuleSetupStatus
pypModuleIncludeFunctionsInit(PypPythonState* pyState) {
	// Vars
	const char* const osModuleName = "os";

	// Assertions
	assert(pyState != NULL);
	assert(pyState->includeFunctionOsModule == NULL);
	assert(pyState->includeFunctionOsPathModule == NULL);

	// Module importing
	if (
		(pyState->includeFunctionOsModule = PyImport_ImportModule(osModuleName)) == NULL ||
		(pyState->includeFunctionOsPathModule = PyObject_GetAttrString(pyState->includeFunctionOsModule, "path")) == NULL
	) {
		// Error
		pypModuleIncludeFunctionsDeinit(pyState);
		return PYP_MODULE_SETUP_STATUS_ERROR_PYTHON;
	}

	// Okay
	return PYP_MODULE_SETUP_STATUS_OKAY;
}

void
pypModuleIncludeFunctionsDeinit(PypPythonState* pyState) {
	assert(pyState != NULL);

	// Clear
	pyState->includeFunctionOsPathModule = NULL;
	if (pyState->includeFunctionOsModule != NULL) {
		Py_DECREF(pyState->includeFunctionOsModule);
		pyState->includeFunctionOsModule = NULL;
	}
}

PypBool
pypPathAbsolute(PypPythonState* pyState, PyObject* pathObject, unicode_char** outputBuffer, size_t* outputBufferLength) {
	// Vars
	PyObject* absPath = NULL;
	PyObject* methodName = NULL;
	PyObject* newObject = NULL;
	#if PY_MAJOR_VERSION < 3
	PyObject* pathObjectNew = NULL;
	#endif
	char* buffer;
	Py_ssize_t bufferLength;
	size_t outputCharacterCount;
	size_t errorCount;

	// Assertions
	assert(pyState != NULL);
	assert(pathObject != NULL);
	assert(outputBuffer != NULL);
	assert(outputBufferLength != NULL);

	// Setup
	if (
		pyState->includeFunctionOsModule == NULL &&
		pypModuleIncludeFunctionsInit(pyState) != PYP_MODULE_SETUP_STATUS_OKAY
	) {
		// Error
		return PYP_FALSE;
	}

	// Convert to unicode if python 2
	#if PY_MAJOR_VERSION < 3
	if (!PyUnicode_Check(pathObject) && PyString_Check(pathObject)) {
		pathObjectNew = PyUnicode_FromObject(pathObject);
		if (pathObjectNew == NULL) return PYP_FALSE; // error
		pathObject = pathObjectNew;
	}
	#endif

	// os.path.abspath(p);
	if (
		(methodName = PyUnicode_FromString("abspath")) == NULL ||
		(absPath = PyObject_CallMethodObjArgs(pyState->includeFunctionOsPathModule, methodName, pathObject, NULL)) == NULL ||
		!pypStringObjectSetup(absPath, "utf-8", "strict", &newObject, &buffer, &bufferLength) ||
		unicodeUTF8DecodeLength(buffer, bufferLength, outputBuffer, &outputCharacterCount, outputBufferLength, &errorCount) != UNICODE_OKAY
	) {
		// Error
		goto cleanup;
	}

	// Done
	#if PY_MAJOR_VERSION < 3
	if (pathObjectNew != NULL) Py_DECREF(pathObjectNew);
	#endif
	Py_DECREF(methodName);
	Py_DECREF(absPath);
	if (newObject != NULL) Py_DECREF(newObject);
	return PYP_TRUE;


	// Cleanup
	cleanup:
	PyErr_Clear();
	#if PY_MAJOR_VERSION < 3
	if (pathObjectNew != NULL) Py_DECREF(pathObjectNew);
	#endif
	if (methodName != NULL) Py_DECREF(methodName);
	if (absPath != NULL) Py_DECREF(absPath);
	if (newObject != NULL) Py_DECREF(newObject);
	return PYP_FALSE;
}

PypBool
pypPathCurrentDirectoryGet(PypPythonState* pyState, unicode_char** path, size_t* pathLength) {
	// Vars
	PyObject* absPath = NULL;
	PyObject* newObject = NULL;
	char* buffer;
	Py_ssize_t bufferLength;
	size_t outputCharacterCount;
	size_t errorCount;

	// Assertions
	assert(pyState != NULL);
	assert(path != NULL);
	assert(pathLength != NULL);

	// Setup
	if (
		pyState->includeFunctionOsModule == NULL &&
		pypModuleIncludeFunctionsInit(pyState) != PYP_MODULE_SETUP_STATUS_OKAY
	) {
		// Error
		return PYP_FALSE;
	}

	// os.getcwd();
	if (
		#if PY_MAJOR_VERSION >= 3
		(absPath = PyObject_CallMethod(pyState->includeFunctionOsModule, "getcwd", NULL)) == NULL ||
		#else
		(absPath = PyObject_CallMethod(pyState->includeFunctionOsModule, "getcwdu", NULL)) == NULL ||
		#endif
		!pypStringObjectSetup(absPath, "utf-8", "strict", &newObject, &buffer, &bufferLength) ||
		unicodeUTF8DecodeLength(buffer, bufferLength, path, &outputCharacterCount, pathLength, &errorCount) != UNICODE_OKAY
	) {
		// Error
		goto cleanup;
	}

	// Done
	Py_DECREF(absPath);
	if (newObject != NULL) Py_DECREF(newObject);
	return PYP_TRUE;


	// Cleanup
	cleanup:
	PyErr_Clear();
	if (absPath != NULL) Py_DECREF(absPath);
	if (newObject != NULL) Py_DECREF(newObject);
	return PYP_FALSE;
}

PypBool
pypPathCurrentDirectorySet(PypPythonState* pyState, const unicode_char* path) {
	// Vars
	PyObject* result = NULL;
	PyObject* pathObject = NULL;

	// Assertions
	assert(pyState != NULL);
	assert(path != NULL);

	// Setup
	if (
		pyState->includeFunctionOsModule == NULL &&
		pypModuleIncludeFunctionsInit(pyState) != PYP_MODULE_SETUP_STATUS_OKAY
	) {
		// Error
		return PYP_FALSE;
	}

	// os.chdir(p);
	if (
		(pathObject = PyUnicode_FromWideChar(path, getUnicodeCharStringLength(path))) == NULL ||
		(result = PyObject_CallMethod(pyState->includeFunctionOsModule, "chdir", NULL)) == NULL
	) {
		// Error
		goto cleanup;
	}

	// Done
	Py_DECREF(result);
	Py_DECREF(pathObject);
	return PYP_TRUE;


	// Cleanup
	cleanup:
	PyErr_Clear();
	if (result != NULL) Py_DECREF(result);
	if (pathObject != NULL) Py_DECREF(pathObject);
	return PYP_FALSE;
}



// Exception handling
PypModuleSetupStatus
pypModuleExceptionHandlingInit(PypPythonState* pyState) {
	// Vars
	PyCompilerFlags compileFlags;
	PyObject* tracebackModule = NULL;
	#if PY_MAJOR_VERSION >= 3
	PyObject* ioModule = NULL;
	const char* const ioModuleName = "io";
	#endif
	const char* const tracebackModuleName = "traceback";
	const char* const sourceCode =
		"traceback.print_exception("
			"exception_type,"
			"exception_value,"
			"exception_traceback,"
			"file=exception_print_target"
		");\n";

	// Assertions
	assert(pyState != NULL);
	assert(pyState->exceptionHandlerCompiledCode == NULL);
	assert(pyState->exceptionHandlerGlobalsDict == NULL);

	// Setup
	compileFlags.cf_flags = 0;

	// Module importing
	if (
		(pyState->exceptionHandlerGlobalsDict = PyDict_New()) == NULL ||
		(tracebackModule = PyImport_ImportModule(tracebackModuleName)) == NULL ||
		PyDict_SetItemString(pyState->exceptionHandlerGlobalsDict, tracebackModuleName, tracebackModule) != 0 ||
		#if PY_MAJOR_VERSION >= 3
		(ioModule = PyImport_ImportModule(ioModuleName)) == NULL ||
		PyDict_SetItemString(pyState->exceptionHandlerGlobalsDict, ioModuleName, ioModule) != 0 ||
		#endif
		(pyState->exceptionHandlerCompiledCode = Py_CompileStringFlags(sourceCode, pypModuleName, Py_file_input, &compileFlags)) == NULL
	) {
		// Error
		if (tracebackModule != NULL) Py_DECREF(tracebackModule);
		#if PY_MAJOR_VERSION >= 3
		if (ioModule != NULL) Py_DECREF(ioModule);
		#endif
		pypModuleExceptionHandlingDeinit(pyState);
		return PYP_MODULE_SETUP_STATUS_ERROR_PYTHON;
	}

	// Okay
	#if PY_MAJOR_VERSION >= 3
	Py_DECREF(ioModule);
	#endif
	Py_DECREF(tracebackModule);
	return PYP_MODULE_SETUP_STATUS_OKAY;
}

void
pypModuleExceptionHandlingDeinit(PypPythonState* pyState) {
	assert(pyState != NULL);

	// Clear
	if (pyState->exceptionHandlerCompiledCode != NULL) {
		Py_DECREF(pyState->exceptionHandlerCompiledCode);
		pyState->exceptionHandlerCompiledCode = NULL;
	}
	if (pyState->exceptionHandlerGlobalsDict != NULL) {
		Py_DECREF(pyState->exceptionHandlerGlobalsDict);
		pyState->exceptionHandlerGlobalsDict = NULL;
	}
}

PypBool
pypPythonExceptionDisplay(PypDataBuffer* output, PypModuleExecutionInfo* executionInfo) {
	PypModuleSetupStatus setupStatus;
	PypPythonState* pyState = executionInfo->pythonState;
	PyObject* exception;
	PyObject* value;
	PyObject* traceback;
	PyObject* exceptionOutputStream = NULL;
	PyObject* tracebackLocalsDict = NULL;
	PyObject* exceptionValue = NULL;
	PyObject* returnObject = NULL;
	PypBool returnCode = PYP_FALSE;

	// Empty output
	if (executionInfo->errorStream == NULL) pypDataBufferEmpty(output);

	// Get exception
    PyErr_Fetch(&exception, &value, &traceback);
    if (exception == NULL) return PYP_FALSE; // No error
    PyErr_NormalizeException(&exception, &value, &traceback);
    if (exception == NULL) return PYP_FALSE; // No error

	// Setup exception handling
	if (pyState->exceptionHandlerCompiledCode == NULL) {
		setupStatus = pypModuleExceptionHandlingInit(pyState);
		if (setupStatus != PYP_MODULE_SETUP_STATUS_OKAY) goto cleanup; // error
	}

	// Vars
	#if PY_MAJOR_VERSION >= 3
	{
		PyObject* ioModule;

		ioModule = PyDict_GetItemString(pyState->exceptionHandlerGlobalsDict, "io");
		if (ioModule == NULL) goto cleanup; // error

		exceptionOutputStream = PyObject_CallMethod(ioModule, "StringIO", NULL);
		if (exceptionOutputStream == NULL) goto cleanup; // error
	}
	#else
	exceptionOutputStream = (PycStringIO->NewOutput)(512);
	#endif

	// Setup local vars
	if (traceback == NULL) {
		traceback = Py_None;
		Py_INCREF(traceback);
	}
	if (value == NULL) {
		value = Py_None;
		Py_INCREF(value);
	}
	if (
		(tracebackLocalsDict = PyDict_New()) == NULL ||
		PyDict_SetItemString(tracebackLocalsDict, "exception_type", exception) != 0 ||
		PyDict_SetItemString(tracebackLocalsDict, "exception_value", value) != 0 ||
		PyDict_SetItemString(tracebackLocalsDict, "exception_traceback", traceback) != 0 ||
		PyDict_SetItemString(tracebackLocalsDict, "exception_print_target", exceptionOutputStream) != 0
	) {
		// Error
		goto cleanup;
	}

	// Execute
	#if PY_MAJOR_VERSION >= 3
	returnObject = PyEval_EvalCodeEx(
		pyState->exceptionHandlerCompiledCode,
		pyState->exceptionHandlerGlobalsDict, tracebackLocalsDict,
		NULL, 0,
		NULL, 0,
		NULL, 0,
		NULL, NULL
	);
	#else
	returnObject = PyEval_EvalCodeEx(
		(PyCodeObject*) pyState->exceptionHandlerCompiledCode,
		pyState->exceptionHandlerGlobalsDict, tracebackLocalsDict,
		NULL, 0,
		NULL, 0,
		NULL, 0,
		NULL
	);
	#endif

	// Get the execption value
	if (returnObject != NULL) {
		#if PY_MAJOR_VERSION >= 3
		exceptionValue = PyObject_CallMethod(exceptionOutputStream, "getvalue", NULL);
		#else
		exceptionValue = (PycStringIO->cgetvalue)(exceptionOutputStream);
		#endif
		if (exceptionValue != NULL) {
			if (executionInfo->errorStream == NULL) {
				returnCode = pypStringObjectExtendDataBuffer(output, exceptionValue, executionInfo->encoding, executionInfo->encodingErrorMode);
			}
			else {
				returnCode = pypStringObjectExtendStream(executionInfo->errorStream, exceptionValue, executionInfo->encoding, executionInfo->encodingErrorMode);
			}
		}
	}

	// Clear error
	if (PyErr_Occurred() != NULL) PyErr_Clear(); // this shouldn't happen, but just in case

	// Clear
	cleanup:
	Py_DECREF(exception);
	if (value != NULL) Py_DECREF(value);
	if (traceback != NULL) Py_DECREF(traceback);
	if (tracebackLocalsDict != NULL) Py_DECREF(tracebackLocalsDict);
	if (exceptionOutputStream != NULL) Py_DECREF(exceptionOutputStream);
	if (exceptionValue != NULL) Py_DECREF(exceptionValue);
	if (returnObject != NULL) Py_DECREF(returnObject);
	return returnCode;
}



// Code compilation and execution
PypBool
pypCharIsWhitespaceNotNewline(PypChar c) {
	return (
		c == ' ' ||
		c == '\t' ||
		c == '\x0b' ||
		c == '\x0c'
	);
}

PyObject*
pypCompileCode(PypDataBuffer* output, PypModuleExecutionInfo* executionInfo, const PypStreamLocation* streamLocation, const char* sourceCode, PypBool isEval) {
	// Vars
	PyCompilerFlags compileFlags;
	PyObject* compiledCode;
	PypSize newFilenameLengthPrefix;
	size_t newFilenameLengthSuffix;
	size_t errorCount;
	char* suffixBuffer;
	char* fullBuffer;

	// Assertions
	assert(output != NULL);
	assert(executionInfo != NULL);
	assert(streamLocation != NULL);
	assert(sourceCode != NULL);
	assert(PyErr_Occurred() == NULL);

	// Create new
	compileFlags.cf_flags = 0;

	// Setup filename
	if (unicodeUTF8Encode(&executionInfo->inputFilename[executionInfo->inputFilenameStart], &suffixBuffer, &newFilenameLengthSuffix, &errorCount) != UNICODE_OKAY) return NULL; // error

	newFilenameLengthPrefix = strlen(pypCompiledCodeFilenamePrefix);
	fullBuffer = memAllocArray(char, newFilenameLengthPrefix + newFilenameLengthSuffix + 1);
	if (fullBuffer == NULL) {
		// Error
		memFree(suffixBuffer);
		return NULL;
	}

	memcpy(fullBuffer, pypCompiledCodeFilenamePrefix, sizeof(char) * newFilenameLengthPrefix);
	memcpy(&fullBuffer[newFilenameLengthPrefix], suffixBuffer, sizeof(char) * (newFilenameLengthSuffix + 1));
	memFree(suffixBuffer);

	// Compile code
	compiledCode = Py_CompileStringFlags(sourceCode, fullBuffer, (isEval ? Py_eval_input : Py_file_input), &compileFlags);

	// Free memory
	memFree(fullBuffer);

	// Check
	if (compiledCode == NULL) {
		// Should be a PyExc_SyntaxError
		if (PyErr_Occurred() != NULL) {
			// Display
			pypPythonExceptionDisplay(output, executionInfo);
		}

		// Error occured
		return NULL;
	}

	// Okay
	return compiledCode;
}

PypReadStatus
pypExecuteCode(PypDataBuffer* output, PypModuleExecutionInfo* executionInfo, PyObject* code, PypBool outputResult) {
	// Vars
	PyObject* returnObj;

	// Assertions
	assert(output != NULL);
	assert(executionInfo != NULL);
	assert(code != NULL);
	assert(PyErr_Occurred() == NULL);


	// Create new
	#if PY_MAJOR_VERSION >= 3
	returnObj = PyEval_EvalCodeEx(
		code,
		executionInfo->pythonState->globalsDict, executionInfo->pythonState->localsDict,
		NULL, 0,
		NULL, 0,
		NULL, 0,
		NULL, NULL
	);
	#else
	returnObj = PyEval_EvalCodeEx(
		(PyCodeObject*) code,
		executionInfo->pythonState->globalsDict, executionInfo->pythonState->localsDict,
		NULL, 0,
		NULL, 0,
		NULL, 0,
		NULL
	);
	#endif

	if (returnObj == NULL) {
		if (PyErr_Occurred() != NULL) {
			// Display
			pypPythonExceptionDisplay(output, executionInfo);
			return PYP_READ_ERROR_CODE_EXECUTION;
		}
	}
	else if (outputResult) {
		// Output?
		if (returnObj != Py_None) {
			// Output
			pypStringObjectExtendDataBuffer(output, returnObj, executionInfo->encoding, executionInfo->encodingErrorMode);
			// Errors not checked; if an error occurs, that's okay
		}
	}

	// Clean
	Py_DECREF(returnObj);

	// Okay
	return PYP_READ_OKAY;
}



// Python code execution
PypReadStatus
pypDataBufferModifyExecute(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data, PypBool expression) {
	// Vars
	const char* sourceBuffer;
	size_t sourceBufferOffset;
	PypDataBufferEntry* entryNew;
	PyObject* code;
	PypModuleExecutionInfo* executionInfo = (PypModuleExecutionInfo*) data;
	PypDataBuffer* pypPreviousDataBuffer = pypCurrentDataBuffer;
	PypReadStatus status;

	// Assertions
	assert(input != NULL);
	assert(outputDataBuffer != NULL);
	assert(streamLocation != NULL);
	assert(data != NULL);

	// Setup
	*outputDataBuffer = NULL;

	// Unify
	if (!pypDataBufferUnify(input, PYP_TRUE, &entryNew)) {
		// Error
		return PYP_READ_ERROR_MEMORY;
	}

	// Get the source buffer
	sourceBuffer = (entryNew == NULL) ? "" : entryNew->buffer;
	assert(sourceBuffer != NULL);

	// Cut off leading whitespace
	sourceBufferOffset = 0;
	while (*sourceBuffer != '\x00' && pypCharIsWhitespaceNotNewline(*sourceBuffer)) {
		++sourceBuffer;
		++sourceBufferOffset;
	}

	// Create new
	*outputDataBuffer = pypDataBufferCreate();
	if (*outputDataBuffer == NULL) {
		// Error
		status = PYP_READ_ERROR_MEMORY;
		goto cleanup;
	}
	pypCurrentDataBuffer = *outputDataBuffer;

	// Compile
	code = pypCompileCode(*outputDataBuffer, executionInfo, streamLocation, sourceBuffer, expression);
	if (code == NULL) {
		// Error
		status = PYP_READ_ERROR_CODE_EXECUTION;
		goto cleanup;
	}

	// Execute
	status = pypExecuteCode(*outputDataBuffer, executionInfo, code, expression); // error check

	// Clean code
	Py_DECREF(code);

	// Done
	cleanup:
	pypCurrentDataBuffer = pypPreviousDataBuffer;
	return status;
}

PypReadStatus
pypDataBufferModifyExecuteCode(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data) {
	// Execute
	return pypDataBufferModifyExecute(input, outputDataBuffer, streamLocation, data, PYP_FALSE);
}

PypReadStatus
pypDataBufferModifyExecuteExpression(PypDataBuffer* input, PypDataBuffer** outputDataBuffer, const PypStreamLocation* streamLocation, void* data) {
	// Execute
	return pypDataBufferModifyExecute(input, outputDataBuffer, streamLocation, data, PYP_TRUE);
}


