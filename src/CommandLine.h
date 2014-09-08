#ifndef __COMMAND_LINE_H
#define __COMMAND_LINE_H



#include <stdint.h>
#include "Map.h"
#include "Unicode.h"
#include "CommandLineChar.h"



typedef enum CommandLineStatus_ {
	COMMAND_LINE_OKAY = 0x0,
	COMMAND_LINE_ERROR_MEMORY = 0x1,
	COMMAND_LINE_ERROR_INVALID_LONG_FLAG = 0x2,
	COMMAND_LINE_ERROR_INVALID_SHORT_FLAG = 0x3,
	COMMAND_LINE_ERROR_MISSING_VALUE = 0x4,
	COMMAND_LINE_ERROR_INVALID_UNORDERED_ENTRY = 0x5,
} CommandLineStatus;

enum {
	COMMAND_LINE_FLAGS_NONE = 0x0,
	COMMAND_LINE_FLAGS_STOP_AFTER_ORDERED_ARGUMENTS = 0x1,
};
typedef uint32_t CommandLineFlags;



MAP_DATA_HEADER(CommandLineArgumentNameMap);
MAP_DATA_HEADER(CommandLineArgumentValueMap);



struct CommandLineDescriptor_;
struct CommandLineArgumentDescriptor_;
struct CommandLineArgumentStringListEntry_;
struct CommandLineArgumentOrderListEntry_;

typedef struct CommandLineDescriptor_ {
	CommandLineFlags flags;
	struct CommandLineArgumentDescriptor_* argumentFirst;
	struct CommandLineArgumentDescriptor_** argumentLast;
	struct CommandLineArgumentOrderListEntry_* orderedArgumentFirst;
	struct CommandLineArgumentOrderListEntry_** orderedArgumentLast;
	CommandLineArgumentNameMap argumentsShort;
	CommandLineArgumentNameMap argumentsLong;
} CommandLineDescriptor;

typedef struct CommandLineArgumentDescriptor_ {
	char* name;
	struct CommandLineArgumentStringListEntry_* firstLongName;
	struct CommandLineArgumentStringListEntry_** lastLongName;
	struct CommandLineArgumentStringListEntry_* firstShortName;
	struct CommandLineArgumentStringListEntry_** lastShortName;
	const char* description;
	const char* paramName;
	struct CommandLineArgumentDescriptor_* nextSibling;
} CommandLineArgumentDescriptor;

typedef struct CommandLineArgumentOrderListEntry_ {
	const struct CommandLineArgumentDescriptor_* argumentDescriptor;
	struct CommandLineArgumentOrderListEntry_* nextSibling;
} CommandLineArgumentOrderListEntry;

typedef struct CommandLineArgumentStringListEntry_ {
	cmd_char* string;
	struct CommandLineArgumentStringListEntry_* nextSibling;
} CommandLineArgumentStringListEntry;

typedef struct CommandLineStatusMessage_ {
	CommandLineStatus error;
	struct CommandLineStatusMessage_* nextSibling;
} CommandLineStatusMessage;

typedef struct CommandLineArgumentValuesDescriptor_ {
	CommandLineArgumentValueMap argumentValues;
	CommandLineStatusMessage* errorFirst;
} CommandLineArgumentValuesDescriptor;

typedef struct CommandLineArgumentValue_ {
	int defined;
	const struct CommandLineArgumentDescriptor_* descriptor;
	cmd_char* value;
} CommandLineArgumentValue;

typedef struct CommandLine_ {
	int argumentCount;
	cmd_char** argumentValues;
} CommandLine;



CommandLineDescriptor* commandLineDescriptorCreate();
void commandLineDescriptorDelete(CommandLineDescriptor* descriptor);
CommandLineArgumentDescriptor* commandLineDescriptorArgumentAdd(CommandLineDescriptor* descriptor, char separator, const char* name, const char* longNames, const char* shortNames, const char* description, const char* paramName);
CommandLineStatus commandLineDescriptorOrderedArgumentAdd(CommandLineDescriptor* descriptor, CommandLineArgumentDescriptor* argument);
CommandLineArgumentValuesDescriptor* commandLineParse(size_t argumentCount, cmd_char** argumentValues, const CommandLineDescriptor* descriptor);
void commandLineArgumentValuesDescriptorDelete(CommandLineArgumentValuesDescriptor* descriptor);
CommandLineArgumentValue* commandLineArgumentValuesDescriptorGet(const CommandLineArgumentValuesDescriptor* descriptor, const char* name);



CommandLine* commandLineGet(int argc, char** argv);
void commandLineDestroy(CommandLine* cl);



#endif


