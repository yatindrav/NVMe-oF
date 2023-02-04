/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once

#define NVMEOF_DEBUG_LOG_LEVEL_VESBOSE      0x1
#define NVMEOF_DEBUG_LOG_LEVEL_DEBUG        0x2
#define NVMEOF_DEBUG_LOG_LEVEL_INFORMATION  0x3
#define NVMEOF_DEBUG_LOG_LEVEL_WARNING      0x4
#define NVMEOF_DEBUG_LOG_LEVEL_ERROR        0x5


#define NVMEOF_EVENT_LOG_LEVEL_INFORMATION  0x1
#define NVMEOF_EVENT_LOG_LEVEL_WARNING      0x2
#define NVMEOF_EVENT_LOG_LEVEL_ERROR        0x3


typedef VOID(*fnNVMeoFDebugLog)(__in ULONG ulLogLevel,
                                __in __nullterminated PCHAR pcFormatString,
                                ...);
fnNVMeoFDebugLog NVMeoFGetDebugLoggingFunction(VOID);
VOID
NVMeoFLogEvent(__in NTSTATUS ulLogLevel,
               __in __nullterminated PCHAR pcFormatString,
               ...);

#ifdef NVMEOF_DEBUG
#define _NVMeoFDebugLog(ulLogLevel, pcFormatString,...)     \
{                                                          \
	fnNVMeoFDebugLog fn = NVMeoFGetDebugLoggingFunction(); \
	if (fn) {                                              \
		fn(ulLogLevel, pcFormatString, __VA_ARGS__);       \
	}                                                      \
};
#else
#define _NVMeoFDebugLog(ulLogLevel, pcFormatString,...)
#endif


#define NVMeoFDebugLog(ulLogLevel, pcFormatString,...)  _NVMeoFDebugLog(ulLogLevel, "%s:%d:" pcFormatString, __FUNCTION__, __LINE__, __VA_ARGS__);
