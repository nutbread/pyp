#include <assert.h>
#include "CommandLine.h"
#include "Memory.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Shellapi.h>
#endif



MAP_BODY_HELPER_HEADERS_STATIC(cmd_char*, CommandLineArgumentDescriptor*, commandLineArgumentNameMap, CommandLineArgumentNameMap);
MAP_FUNCTION_HEADERS_STATIC(cmd_char*, CommandLineArgumentDescriptor*, commandLineArgumentNameMap, CommandLineArgumentNameMap);
MAP_BODY(cmd_char*, CommandLineArgumentDescriptor*, commandLineArgumentNameMap, CommandLineArgumentNameMap)

MAP_BODY_HELPER_HEADERS_STATIC(char*, CommandLineArgumentValue*, commandLineArgumentValueMap, CommandLineArgumentValueMap);
MAP_FUNCTION_HEADERS_STATIC(char*, CommandLineArgumentValue*, commandLineArgumentValueMap, CommandLineArgumentValueMap);
MAP_BODY(char*, CommandLineArgumentValue*, commandLineArgumentValueMap, CommandLineArgumentValueMap)



// Headers
static void commandLineArgumentDescriptorDelete(CommandLineArgumentDescriptor* descriptor);
static CommandLineArgumentDescriptor* commandLineArgumentDescriptorCreate(char separator, const char* name, const char* longNames, const char* shortNames, const char* description, const char* paramName);
static void commandLineArgumentValueDelete(CommandLineArgumentValue* aValue);
static CommandLineStatusMessage* commandLineErrorListAdd(CommandLineStatus error);
static cmd_char* createCmdString(size_t lengthIncludingNull);
static cmd_char* createCmdStringFrom(const unicode_char* source, size_t lengthIncludingNull);



// Helper functions
cmd_char*
createCmdString(size_t lengthIncludingNull) {
	// Assertions
	assert(lengthIncludingNull > 0);

	// Create
	return memAllocArray(cmd_char, lengthIncludingNull);
}
cmd_char*
createCmdStringFromCString(const char* source, size_t lengthIncludingNull) {
	// Vars
	cmd_char* str;
	cmd_char* strPos;

	// Assertions
	assert(source != NULL);
	assert(lengthIncludingNull > 0);

	// Create
	str = memAllocArray(cmd_char, lengthIncludingNull);
	if (str == NULL) return NULL;
	strPos = str;

	// Copy
	while (--lengthIncludingNull > 0) {
		*(strPos++) = *(source++);
	}
	*strPos = '\x00'; // Null terminate

	// Done
	return str;
}
cmd_char*
createCmdStringFrom(const cmd_char* source, size_t lengthIncludingNull) {
	// Vars
	cmd_char* str;
	cmd_char* strPos;

	// Assertions
	assert(source != NULL);
	assert(lengthIncludingNull > 0);

	// Create
	str = memAllocArray(cmd_char, lengthIncludingNull);
	if (str == NULL) return NULL;
	strPos = str;

	// Copy
	while (--lengthIncludingNull > 0) {
		*(strPos++) = *(source++);
	}
	*strPos = '\x00'; // Null terminate

	// Done
	return str;
}



// Create an argument descriptor
CommandLineArgumentDescriptor*
commandLineArgumentDescriptorCreate(char separator, const char* name, const char* longNames, const char* shortNames, const char* description, const char* paramName) {
	// Vars
	CommandLineArgumentDescriptor* descriptor;
	CommandLineArgumentStringListEntry* entry;
	size_t i, j;

	// Assertions
	assert(name != NULL);
	assert(longNames != NULL || shortNames != NULL);
	assert(description != NULL);

	// Create new
	descriptor = memAlloc(CommandLineArgumentDescriptor);
	if (descriptor == NULL) return NULL; // error

	// Name
	i = strlen(name) + 1;
	descriptor->name = memAllocArray(char, i);
	if (descriptor->name == NULL) {
		// Cleanup
		memFree(descriptor);
		return NULL;
	}
	memcpy(descriptor->name, name, i);

	// Members
	descriptor->firstLongName = NULL;
	descriptor->lastLongName = &descriptor->firstLongName;
	descriptor->firstShortName = NULL;
	descriptor->lastShortName = &descriptor->firstShortName;
	descriptor->description = description;
	descriptor->paramName = paramName;
	descriptor->nextSibling = NULL;

	// Setup names
	if (shortNames != NULL) {
		for (i = 0; shortNames[i] != '\x00'; ++i) {
			// Create new entry
			entry = memAlloc(CommandLineArgumentStringListEntry);
			if (entry == NULL) goto cleanup;

			// Set its string
			entry->string = createCmdString(2);
			if (entry->string == NULL) {
				// Cleanup
				memFree(entry);
				goto cleanup;
			}

			entry->string[0] = shortNames[i];
			entry->string[1] = '\x00';
			entry->nextSibling = NULL;

			// Link
			*descriptor->lastShortName = entry;
			descriptor->lastShortName = &entry->nextSibling;
		}
	}
	if (longNames != NULL) {
		i = 0;
		j = 0;
		while (1) {
			// Find the end
			for (; longNames[i] != '\x00' && longNames[i] != separator; ++i);

			// No zero length arguments
			assert(i > j);

			// Create new entry
			entry = memAlloc(CommandLineArgumentStringListEntry);
			if (entry == NULL) goto cleanup;

			// Set its string
			entry->string = createCmdStringFromCString(&(longNames[j]), i - j + 1);
			if (entry->string == NULL) {
				// Cleanup
				memFree(entry);
				goto cleanup;
			}

			//memcpy(entry->string, &(longNames[j]), i - j);
			//entry->string[i - j] = '\x00';
			entry->nextSibling = NULL;

			// Link
			*descriptor->lastLongName = entry;
			descriptor->lastLongName = &entry->nextSibling;

			// Next
			if (longNames[i] == '\x00') break;
			j = ++i;
		}
	}

	// Okay
	return descriptor;


	// Error
	cleanup:
	commandLineArgumentDescriptorDelete(descriptor);
	return NULL;
}

// Delete an argument descriptor
void
commandLineArgumentDescriptorDelete(CommandLineArgumentDescriptor* descriptor) {
	// Vars
	CommandLineArgumentStringListEntry* entry;
	CommandLineArgumentStringListEntry* entryNext;

	// Assertions
	assert(descriptor != NULL);

	// Delete argument chains
	for (entry = descriptor->firstLongName; entry != NULL; entry = entryNext) {
		// Delete
		entryNext = entry->nextSibling;
		memFree(entry->string);
		memFree(entry);
	}

	for (entry = descriptor->firstShortName; entry != NULL; entry = entryNext) {
		// Delete
		entryNext = entry->nextSibling;
		memFree(entry->string);
		memFree(entry);
	}

	memFree(descriptor->name);

	// Delete
	memFree(descriptor);
}

void
commandLineArgumentValueDelete(CommandLineArgumentValue* aValue) {
	assert(aValue != NULL);

	// Delete
	if (aValue->value != NULL) memFree(aValue->value);
	memFree(aValue);
}



// Public functions
CommandLineDescriptor*
commandLineDescriptorCreate() {
	// Create
	CommandLineDescriptor* descriptor = memAlloc(CommandLineDescriptor);
	if (descriptor == NULL) return NULL; // error

	// Setup
	if (commandLineArgumentNameMapCreate(&descriptor->argumentsShort) == NULL) {
		// Cleanup
		memFree(descriptor);
		return NULL;
	}
	if (commandLineArgumentNameMapCreate(&descriptor->argumentsLong) == NULL) {
		// Cleanup
		commandLineArgumentNameMapClean(&descriptor->argumentsShort);
		memFree(descriptor);
		return NULL;
	}

	// Members
	descriptor->flags = COMMAND_LINE_FLAGS_NONE;
	descriptor->orderedArgumentFirst = NULL;
	descriptor->orderedArgumentLast = &descriptor->orderedArgumentFirst;
	descriptor->argumentFirst = NULL;
	descriptor->argumentLast = &descriptor->argumentFirst;

	// Done
	return descriptor;
}

void
commandLineDescriptorDelete(CommandLineDescriptor* descriptor) {
	// Vars
	CommandLineArgumentDescriptor* arg;
	CommandLineArgumentDescriptor* argNext;
	CommandLineArgumentOrderListEntry* ao;
	CommandLineArgumentOrderListEntry* aoNext;

	// Assertions
	assert(descriptor != NULL);

	// Clean maps
	commandLineArgumentNameMapClean(&descriptor->argumentsLong);
	commandLineArgumentNameMapClean(&descriptor->argumentsShort);

	// Delete arguments
	for (arg = descriptor->argumentFirst; arg != NULL; arg = argNext) {
		// Delete
		argNext = arg->nextSibling;
		commandLineArgumentDescriptorDelete(arg);
	}

	// Delete ordered arguments
	for (ao = descriptor->orderedArgumentFirst; ao != NULL; ao = aoNext) {
		aoNext = ao->nextSibling;
		memFree(ao);
	}

	// Delete
	memFree(descriptor);
}

CommandLineArgumentDescriptor*
commandLineDescriptorArgumentAdd(CommandLineDescriptor* descriptor, char separator, const char* name, const char* longNames, const char* shortNames, const char* description, const char* paramName) {
	// Vars
	CommandLineArgumentDescriptor* argDescriptor;
	CommandLineArgumentStringListEntry* argName;
	MapStatus mapStatus;

	// Assertions
	assert(descriptor != NULL);
	assert(name != NULL);
	assert(longNames != NULL || shortNames != NULL);
	assert(description != NULL);

	// Create the command
	argDescriptor = commandLineArgumentDescriptorCreate(separator, name, longNames, shortNames, description, paramName);
	if (argDescriptor == NULL) return NULL; // error

	// Add it to the descriptor
	*descriptor->argumentLast = argDescriptor;
	descriptor->argumentLast = &argDescriptor->nextSibling;

	// Add it to the maps
	for (argName = argDescriptor->firstLongName; argName != NULL; argName = argName->nextSibling) {
		mapStatus = commandLineArgumentNameMapAdd(&descriptor->argumentsLong, argName->string, argDescriptor);
		if (mapStatus != MAP_ADDED) goto cleanup;
	}
	for (argName = argDescriptor->firstShortName; argName != NULL; argName = argName->nextSibling) {
		mapStatus = commandLineArgumentNameMapAdd(&descriptor->argumentsShort, argName->string, argDescriptor);
		if (mapStatus != MAP_ADDED) goto cleanup;
	}

	// Okay
	return argDescriptor;


	// Error
	cleanup:
	commandLineArgumentDescriptorDelete(argDescriptor);
	return NULL;
}

CommandLineStatus
commandLineDescriptorOrderedArgumentAdd(CommandLineDescriptor* descriptor, CommandLineArgumentDescriptor* argument) {
	// Vars
	CommandLineArgumentOrderListEntry* aoEntry;

	// Assertions
	assert(descriptor != NULL);
	assert(argument != NULL);

	// Create
	aoEntry = memAlloc(CommandLineArgumentOrderListEntry);
	if (aoEntry == NULL) return COMMAND_LINE_ERROR_MEMORY; // error

	// Values
	aoEntry->argumentDescriptor = argument;
	aoEntry->nextSibling = NULL;

	// Add to the list
	*descriptor->orderedArgumentLast = aoEntry;
	descriptor->orderedArgumentLast = &aoEntry->nextSibling;

	// Okay
	return COMMAND_LINE_OKAY;
}

void
commandLineArgumentValuesDescriptorDelete(CommandLineArgumentValuesDescriptor* descriptor) {
	// Vars
	CommandLineStatusMessage* error;
	CommandLineStatusMessage* errorNext;

	// Assertions
	assert(descriptor != NULL);

	// Delete map
	commandLineArgumentValueMapClean(&descriptor->argumentValues);

	// Delete errors
	for (error = descriptor->errorFirst; error != NULL; error = errorNext) {
		// Delete
		errorNext = error->nextSibling;
		memFree(error);
	}

	// Delete
	memFree(descriptor);
}

CommandLineStatusMessage*
commandLineErrorListAdd(CommandLineStatus error) {
	// Vars
	CommandLineStatusMessage* msg;

	// Create
	msg = memAlloc(CommandLineStatusMessage);
	if (msg == NULL) return NULL; // error

	// Apply
	msg->error = error;
	msg->nextSibling = NULL;

	// Done
	return msg;
}

CommandLineArgumentValuesDescriptor*
commandLineParse(size_t argumentCount, cmd_char** argumentValues, const CommandLineDescriptor* descriptor) {
	// Vars
	size_t i = 0;
	size_t argLength = 0;
	const cmd_char* arg;
	cmd_char argShort[2] = { '\x00', '\x00' };
	const CommandLineArgumentOrderListEntry* orderedEntry;
	const CommandLineArgumentDescriptor* argDesc;
	CommandLineArgumentValuesDescriptor* valueDescriptor;
	CommandLineArgumentValue* argValue;
	CommandLineArgumentDescriptor* entry = NULL;
	CommandLineArgumentValue* valueEntry = NULL;
	CommandLineStatusMessage** errorNext;
	MapStatus mapStatus;

	// Assertions
	assert(argumentValues != NULL);
	assert(descriptor != NULL);

	// Create the value descriptor
	valueDescriptor = memAlloc(CommandLineArgumentValuesDescriptor);
	valueDescriptor->errorFirst = NULL;
	errorNext = &valueDescriptor->errorFirst;


	// Setup map
	if (commandLineArgumentValueMapCreate(&valueDescriptor->argumentValues) == NULL) {
		// Error
		memFree(valueDescriptor);
		return NULL;
	}
	for (argDesc = descriptor->argumentFirst; argDesc != NULL; argDesc = argDesc->nextSibling) {
		argValue = memAlloc(CommandLineArgumentValue);
		if (argValue == NULL) goto cleanup; // error
		argValue->descriptor = argDesc;
		argValue->defined = 0;
		argValue->value = NULL;

		mapStatus = commandLineArgumentValueMapAdd(&valueDescriptor->argumentValues, argDesc->name, argValue);
		if (mapStatus != MAP_ADDED) {
			// Error
			commandLineArgumentValueDelete(argValue);
			goto cleanup;
		}
	}

	// Loop
	orderedEntry = descriptor->orderedArgumentFirst;
	while (i < argumentCount) {
		arg = argumentValues[i];
		if (arg[0] == '-') {
			if (arg[1] == '-' && arg[2] != '\x00') {
				// Long param
				arg += 2;

				// Check
				mapStatus = commandLineArgumentNameMapFind(&descriptor->argumentsLong, arg, &entry);
				if (mapStatus == MAP_FOUND) {
					if (entry->paramName == NULL) {
						// Flag as defined
						mapStatus = commandLineArgumentValueMapFind(&valueDescriptor->argumentValues, entry->name, &valueEntry);
						assert(mapStatus == MAP_FOUND);
						valueEntry->defined = 1;
					}
					else {
						// Check for a value
						if (++i < argumentCount) {
							// Apply
							mapStatus = commandLineArgumentValueMapFind(&valueDescriptor->argumentValues, entry->name, &valueEntry);
							assert(mapStatus == MAP_FOUND);

							arg = argumentValues[i];
							argLength = getUnicodeCharStringLength(arg) + 1;

							// Copy value
							if (valueEntry->value != NULL) memFree(valueEntry->value);
							valueEntry->value = createCmdStringFrom(arg, argLength);
							if (valueEntry->value == NULL) goto cleanup; // error
							valueEntry->defined = 1;
						}
						else {
							// Error
							*errorNext = commandLineErrorListAdd(COMMAND_LINE_ERROR_MISSING_VALUE);
							if (*errorNext == NULL) goto cleanup;
							errorNext = &(*errorNext)->nextSibling;
						}
					}
				}
				else {
					// Error
					*errorNext = commandLineErrorListAdd(COMMAND_LINE_ERROR_INVALID_LONG_FLAG);
					if (*errorNext == NULL) goto cleanup;
					errorNext = &(*errorNext)->nextSibling;
				}

				// Next
				goto loopNext;
			}
			else if (arg[1] != '\x00') {
				// Short param(s)
				++arg;

				while (1) {
					// Set
					argShort[0] = *arg;

					// Check
					mapStatus = commandLineArgumentNameMapFind(&descriptor->argumentsShort, argShort, &entry);
					if (mapStatus == MAP_FOUND) {
						if (entry->paramName == NULL) {
							// Flag as defined
							mapStatus = commandLineArgumentValueMapFind(&valueDescriptor->argumentValues, entry->name, &valueEntry);
							assert(mapStatus == MAP_FOUND);
							valueEntry->defined = 1;
						}
						else {
							// Check for a value
							if (*(++arg) == '\x00') {
								if (++i < argumentCount) {
									// Update argument
									arg = argumentValues[i];
								}
								else {
									// Error
									*errorNext = commandLineErrorListAdd(COMMAND_LINE_ERROR_MISSING_VALUE);
									if (*errorNext == NULL) goto cleanup;
									errorNext = &(*errorNext)->nextSibling;
									break;
								}
							}

							// Apply
							mapStatus = commandLineArgumentValueMapFind(&valueDescriptor->argumentValues, entry->name, &valueEntry);
							assert(mapStatus == MAP_FOUND);

							argLength = getUnicodeCharStringLength(arg) + 1;

							// Copy value
							if (valueEntry->value != NULL) memFree(valueEntry->value);
							valueEntry->value = createCmdStringFrom(arg, argLength);
							if (valueEntry->value == NULL) goto cleanup; // error
							valueEntry->defined = 1;

							// Done
							break;
						}
					}
					else {
						// Error
						*errorNext = commandLineErrorListAdd(COMMAND_LINE_ERROR_INVALID_SHORT_FLAG);
						if (*errorNext == NULL) goto cleanup;
						errorNext = &(*errorNext)->nextSibling;
					}

					// End
					if (*(++arg) == '\x00') break;
				}

				// Next
				goto loopNext;
			}
		}

		// Not a param
		if (orderedEntry != NULL) {
			// Remove completed
			while (1) {
				// Check if it's completed
				mapStatus = commandLineArgumentValueMapFind(&valueDescriptor->argumentValues, orderedEntry->argumentDescriptor->name, &valueEntry);
				assert(mapStatus == MAP_FOUND);
				if (!valueEntry->defined) {
					argLength = getUnicodeCharStringLength(arg) + 1;

					// Set value
					if (valueEntry->value != NULL) memFree(valueEntry->value);
					valueEntry->value = createCmdStringFrom(arg, argLength);
					if (valueEntry->value == NULL) goto cleanup; // error
					valueEntry->defined = 1;

					// Advance ordered entry
					orderedEntry = orderedEntry->nextSibling;

					// Break inner, continue outer (skipping over error)
					goto loopNext;
				}

				// Next
				orderedEntry = orderedEntry->nextSibling;
				if (orderedEntry == NULL) break;
			}
		}

		// Error
		*errorNext = commandLineErrorListAdd(COMMAND_LINE_ERROR_INVALID_UNORDERED_ENTRY);
		if (*errorNext == NULL) goto cleanup;
		errorNext = &(*errorNext)->nextSibling;

		// Next; goto is used in place of continues because otherwise it becomes a mess trying to continue to the same place
		// Also because a break-inner, continue-outer form is used
		loopNext:
		++i;
	}

	// Done
	return valueDescriptor;

	// Cleanup
	cleanup:
	commandLineArgumentValuesDescriptorDelete(valueDescriptor);
	return NULL;
}

CommandLineArgumentValue*
commandLineArgumentValuesDescriptorGet(const CommandLineArgumentValuesDescriptor* descriptor, const char* name) {
	CommandLineArgumentValue* entry;

	assert(descriptor != NULL);
	assert(name != NULL);

	if (commandLineArgumentValueMapFind(&descriptor->argumentValues, name, &entry) == MAP_FOUND) return entry;
	return NULL;
}



// Command line getting
CommandLine*
commandLineGet(int argc, char** argv) {
	// Vars
	CommandLine* cl;
	int argCount;
	LPWSTR* argList;

	// Assertions
	assert(argv != NULL);

	cl = memAlloc(CommandLine);
	if (cl == NULL) return NULL; // error

	// Unicode
	argList = CommandLineToArgvW(GetCommandLineW(), &argCount);
	if (argList == NULL) {
		// Error
		memFree(cl);
		return NULL;
	}

	// Set
	cl->argumentCount = argCount;
	cl->argumentValues = argList;

	return cl;
}

void
commandLineDestroy(CommandLine* cl) {
	assert(cl != NULL);

	LocalFree(cl->argumentValues);

	memFree(cl);
}



// Mapping helper functions
MapHashValue commandLineArgumentNameMapKeyHashFunction(const cmd_char* key) {
	return mapHelperHashUnicode(key);
}
int commandLineArgumentNameMapKeyCompareFunction(const cmd_char* key1, const cmd_char* key2) {
	return mapHelperCompareUnicode(key1, key2);
}
int commandLineArgumentNameMapKeyCopyFunction(const cmd_char* key, cmd_char** output) {
	return mapHelperCopyUnicode(key, output);
}
void commandLineArgumentNameMapKeyDeleteFunction(cmd_char* key) {
	mapHelperDeleteUnicode(key);
}
void commandLineArgumentNameMapValueDeleteFunction(CommandLineArgumentDescriptor* value) {
	// Nothing
}

MapHashValue commandLineArgumentValueMapKeyHashFunction(const char* key) {
	return mapHelperHashString(key);
}
int commandLineArgumentValueMapKeyCompareFunction(const char* key1, const char* key2) {
	return mapHelperCompareString(key1, key2);
}
int commandLineArgumentValueMapKeyCopyFunction(const char* key, char** output) {
	return mapHelperCopyString(key, output);
}
void commandLineArgumentValueMapKeyDeleteFunction(char* key) {
	mapHelperDeleteString(key);
}
void commandLineArgumentValueMapValueDeleteFunction(CommandLineArgumentValue* value) {
	// Delete
	commandLineArgumentValueDelete(value);
}


