#pragma once

extern bool logWindowOpen;

#if ENABLE_MESSAGE_LOG

enum message_type
{
	message_type_normal,
	message_type_warning,
	message_type_error,

	message_type_count,
};

#define LOG_MESSAGE(message, ...) logMessageInternal(message_type_normal, __FILE__, __FUNCTION__, __LINE__, message, __VA_ARGS__)
#define LOG_WARNING(message, ...) logMessageInternal(message_type_warning, __FILE__, __FUNCTION__, __LINE__, message, __VA_ARGS__)
#define LOG_ERROR(message, ...) logMessageInternal(message_type_error, __FILE__, __FUNCTION__, __LINE__, message, __VA_ARGS__)

void logMessageInternal(message_type type, const char* file, const char* function, uint32 line, const char* format, ...);

void initializeMessageLog();
void updateMessageLog(float dt);


#else
#define LOG_MESSAGE(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)

#define initializeMessageLog(...)
#define updateMessageLog(...)

#endif
