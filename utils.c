/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#include <wdm.h>
#include "sp.h"
#include "errdbg.h"

__forceinline
void NVMeoFAsciiToWChar(__inout PWCHAR pwcDst,
                        __in    PCHAR  pcSrc,
                        __in    ULONG ulStrLen)
{
	for (ULONG ulLen = 0; ((ulLen < ulStrLen) && *pcSrc); ulLen++) {
		*pwcDst++ = (wchar_t)(*pcSrc++);
	}
}

__forceinline
NTSTATUS
NVMeoFLogSystemEvent(__in  PDRIVER_OBJECT  psDriverObject,
                     __in  NTSTATUS        SpecificIOStatus,
                     __in  PCHAR           pcStringToLog)
{
	static ULONG ulSequenceNumber = 0;
	ULONG ulStrLen = (ULONG)strlen(pcStringToLog) + 1;
	ULONG ulMaxWCharAvailbleInMaxLogEntry = (ERROR_LOG_MAXIMUM_SIZE - FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData)) / sizeof(WCHAR);
	ULONG ulNumLogPacket = (ulStrLen / (ulMaxWCharAvailbleInMaxLogEntry - 1)) + 1;
	ULONG ulLocalSeqNum = InterlockedAdd((volatile LONG*)&ulSequenceNumber, ulNumLogPacket);
	ULONG ulCurrSeqNum = ulLocalSeqNum - ulNumLogPacket;
	for (; ulCurrSeqNum < ulLocalSeqNum; ulCurrSeqNum++) {
		PIO_ERROR_LOG_PACKET psErrEntry = NULL;
		UCHAR                ucLogPacketSz = (((ulStrLen * sizeof(WCHAR)) + FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData)) < ERROR_LOG_MAXIMUM_SIZE) ?
			(UCHAR)((ulStrLen * sizeof(WCHAR)) + FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData)) : (UCHAR)ERROR_LOG_MAXIMUM_SIZE;
		psErrEntry = IoAllocateErrorLogEntry(psDriverObject, ucLogPacketSz);
		if (psErrEntry) {
			ULONG ulStrSzToCopy = (ucLogPacketSz == ERROR_LOG_MAXIMUM_SIZE) ?
				((ERROR_LOG_MAXIMUM_SIZE - FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData)) / sizeof(WCHAR) - 1) : ulStrLen;
			RtlZeroMemory(psErrEntry, ucLogPacketSz);
			psErrEntry->NumberOfStrings = 1;
			psErrEntry->ErrorCode = SpecificIOStatus;
			psErrEntry->SequenceNumber = ulCurrSeqNum;
			psErrEntry->StringOffset = FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData);
			NVMeoFAsciiToWChar((WCHAR*)psErrEntry->DumpData, pcStringToLog, ulStrSzToCopy);
			IoWriteErrorLogEntry(psErrEntry);
			pcStringToLog += ulStrSzToCopy;
			ulStrLen -= ulStrSzToCopy;
		}
		else {
			KdPrint(("%s:%d:Failed to allocate error log entry (len=%d).", __FUNCTION__, __LINE__, ucLogPacketSz));
			KdPrint(("%s\n", pcStringToLog));
		}
	}

	return (STATUS_SUCCESS);
}

BOOLEAN
NVMeoFCanPrintDebugLog(__in ULONG ulLogLevel)
{
	if (psNVMeoFGlobalInfo && (ulLogLevel < psNVMeoFGlobalInfo->MPRegInfo.ulDebugLevel)) {
		return TRUE;
	}
	return FALSE;
}

VOID
NVMeoFPrintDebugLog(__in ULONG ulLogLevel,
                    __in __nullterminated PCHAR pcFormatString,
                    ...)
{
	if (psNVMeoFGlobalInfo && (ulLogLevel < psNVMeoFGlobalInfo->MPRegInfo.ulDebugLevel)) {
#if USE_ALLOCATED_DEBUG_PRINT_BUFFER
		PCHAR pcPrintBuffer = NVMeoFBusAllocateNpp(sizeof(CHAR) * DEBUG_STRING_BUFFER_SIZE_256B);
#else
		CHAR caBuff[NVMEOF_UTILS_BUFFER_SIZE_512B] = { 0 };
		PCHAR pcPrintBuffer = caBuff;
#endif
		if (pcPrintBuffer) {
			va_list variableArgumentList = NULL;
			va_start(variableArgumentList, pcFormatString);
			RtlZeroMemory(pcPrintBuffer, (sizeof(CHAR) * NVMEOF_UTILS_BUFFER_SIZE_512B));
			_vsnprintf_s(pcPrintBuffer,
				(NVMEOF_UTILS_BUFFER_SIZE_512B - 1),
				(NVMEOF_UTILS_BUFFER_SIZE_512B - 1),
				pcFormatString,
				variableArgumentList);
			va_end(variableArgumentList);
			DbgPrint("%s", pcPrintBuffer);
#if USE_ALLOCATED_DEBUG_PRINT_BUFFER
			NVMeoFBusFreeNpp(pcPrintBuffer);
#endif
		}
	}
}

VOID
NVMeoFLogEvent(__in NTSTATUS ulLogLevel,
               __in __nullterminated PCHAR pcFormatString,
               ...)
{
	PCHAR    pcPrintBuffer = NULL;
	va_list variableArgumentList = NULL;
	pcPrintBuffer = NVMeoFAllocateNpp(sizeof(CHAR) * NVMEOF_UTILS_BUFFER_SIZE_2KB);
	if (!pcPrintBuffer) {
		return;
	}

	va_start(variableArgumentList, pcFormatString);
	_vsnprintf(pcPrintBuffer,
               ((NVMEOF_UTILS_BUFFER_SIZE_2KB * sizeof(pcPrintBuffer[0])) - 1),
               pcFormatString,
               variableArgumentList);
	va_end(variableArgumentList);
	if (psNVMeoFGlobalInfo) {
		NVMeoFLogSystemEvent(psNVMeoFGlobalInfo->psDriverObj,
			                 ulLogLevel,
			                 pcPrintBuffer);
	}
	NVMeoFFreeNpp(pcPrintBuffer);
}

fnNVMeoFDebugLog
NVMeoFGetDebugLoggingFunction(VOID)
{
	fnNVMeoFDebugLog fn = NULL;
#ifdef NVMEOF_DEBUG
	fn = NVMeoFPrintDebugLog;
#endif
	return (fn);
}

static
void
NVMeoFUtilsPrintBuffer(__in PUCHAR buff,
                       __in ULONG bufSize,
                       __in ULONG printCnt)
{
	UNREFERENCED_PARAMETER(buff);
	ULONG cnt = 0;
	for (; cnt < bufSize; cnt += printCnt) {
		NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x\n",
			buff[cnt], buff[cnt + 1], buff[cnt + 2], buff[cnt + 3], buff[cnt + 4], buff[cnt + 5], buff[cnt + 6], buff[cnt + 7],
			buff[cnt + 8], buff[cnt + 9], buff[cnt + 10], buff[cnt + 11], buff[cnt + 12], buff[cnt + 13], buff[cnt + 14], buff[cnt + 15]);
		if ((cnt + (printCnt << 1)) > bufSize) {
			cnt += printCnt;
			for (; cnt < bufSize; cnt++)
				NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%.02x ", buff[cnt]);
		}
	}
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "\n");
}

static
VOID
NVMeoFRdmaSockPrintBufferMdl(__in PMDL  psBufferMdl)
{
	PUCHAR pucBuffer = NULL;
	ULONG  ulBuffLen = 0;
	while (psBufferMdl) {
		ulBuffLen = MmGetMdlByteCount(psBufferMdl);
		pucBuffer = MmGetSystemAddressForMdlSafe(psBufferMdl, NormalPagePriority | MdlMappingNoExecute);
		NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Buffer Lenght %u\n", __FUNCTION__, __LINE__, ulBuffLen);
		if (pucBuffer) {
			NVMeoFUtilsPrintBuffer(pucBuffer, ulBuffLen, 16);
			psBufferMdl = psBufferMdl->Next;
		}
		else {
			break;
		}
	}
}

NTSTATUS
GetBestInterfaceEx(PSOCKADDR psDestSockAddr, PNET_IFINDEX pdwBestIfIndex);

static
NTSTATUS
NVMeoFUtilsGetInterfaceIndexFromSourceIPAddress(__in PSOCKADDR_INET psLocalAddress,
                                                __inout PIF_INDEX pulIfIndex)
{
	PMIB_IPPATH_TABLE psIPPathTable = NULL;
	PMIB_IPPATH_ROW psMibIPEntry = NULL;
	ULONG ulCounter = 0;

	if (!psLocalAddress->Ipv4.sin_addr.S_un.S_addr) {
		return (STATUS_INVALID_PARAMETER);
	}

	ULONG ulNetHelperReturn =
		GetIpPathTable(psLocalAddress->si_family,
			&psIPPathTable);
	while ((ulNetHelperReturn == STATUS_SUCCESS) && (psIPPathTable->NumEntries > ulCounter)) {
		psMibIPEntry = &psIPPathTable->Table[ulCounter];
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:IP Address (0x%X) of I/f %lu comparing to src (0x%X) !\n", __FUNCTION__, __LINE__,
			psMibIPEntry->Source.Ipv4.sin_addr.S_un.S_addr,
			psMibIPEntry->InterfaceIndex,
			psLocalAddress->Ipv4.sin_addr.S_un.S_addr);
		if (psMibIPEntry->Source.Ipv4.sin_addr.S_un.S_addr == psLocalAddress->Ipv4.sin_addr.S_un.S_addr) {
			*pulIfIndex = psMibIPEntry->InterfaceIndex;
			FreeMibTable(psMibIPEntry);
			return (STATUS_SUCCESS);
		}
	}
	if (psMibIPEntry) {
		FreeMibTable(psMibIPEntry);
	}
	return (STATUS_INVALID_PARAMETER);
}

NTSTATUS
NVMeoFUtilsGetIfIndexAndLocalIpAddress(__in    PSOCKADDR_INET      psDestAddr,
                                       __inout PSOCKADDR_INET      psLocalAddress,
                                       __inout PMIB_IPFORWARD_ROW2 psIpRoute,
                                       __inout PNET_IFINDEX        pdwIfIndex)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NET_IFINDEX dwIfIndex = 0;

	if (!psDestAddr || !psLocalAddress || !pdwIfIndex) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Either Destinatio, Local or IF idx address NULL %p %p %p!\n", __FUNCTION__, __LINE__,
			psDestAddr,
			psLocalAddress,
			pdwIfIndex);
		return (STATUS_INVALID_PARAMETER);
	}

	if (!psLocalAddress->Ipv4.sin_addr.S_un.S_addr) {
		Status =
			NVMeoFUtilsGetInterfaceIndexFromSourceIPAddress(psLocalAddress,
				&dwIfIndex);
		if (Status == STATUS_INVALID_PARAMETER) {
			Status = GetBestInterfaceEx((PSOCKADDR)psDestAddr, &dwIfIndex);
		}
	} else {
		Status = GetBestInterfaceEx((PSOCKADDR)psDestAddr, &dwIfIndex);
	}

	if (Status != STATUS_SUCCESS) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to get I/f index for target with Status 0x%X!\n", __FUNCTION__, __LINE__,
			Status);
		return (STATUS_NETWORK_ACCESS_DENIED);
	}

	*pdwIfIndex = dwIfIndex;

	Status = 
		GetBestRoute2(NULL,
		              dwIfIndex,
		              psLocalAddress,
		              psDestAddr,
		              0,
		              psIpRoute,
		              psLocalAddress);
	if (Status != STATUS_SUCCESS) {
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Failed to get IP Path entry Address 0x%x with status 0x%x!\n", __FUNCTION__, __LINE__,
			psLocalAddress->Ipv4.sin_addr.S_un.S_addr, Status);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to get the Best interface for IP Address 0x%x status 0x%x!\n", __FUNCTION__, __LINE__,
			psLocalAddress->Ipv4.sin_addr.S_un.S_addr, Status);
		*pdwIfIndex = 0;
		Status = STATUS_NETWORK_ACCESS_DENIED;
	} else {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Best interface for IP Address 0x%x is %x status 0x%x!\n", __FUNCTION__, __LINE__,
			psLocalAddress->Ipv4.sin_addr.S_un.S_addr, *pdwIfIndex, Status);
		Status = STATUS_SUCCESS;
	}

	return Status;
}

VOID
NVMeoFUtilsPrintHexBuffer(ULONG  ulLoglevel,
                          PUCHAR pucBuffer,
                          ULONG  ulBufSize,
                          ULONG  ulPrintCntPerLine)
{
	if (NVMeoFCanPrintDebugLog(ulLoglevel)) {
		ULONG ulBufferCharCount = 0;
		PCHAR pcFormatString = NULL;
		PCHAR pcTmepStr = NULL;
		ULONG ulCharPerLn = 0;
#define SINGLE_CHAR_FORMAT_STRING "%.02hx "
#define NVMEOF_RECEIVE_BUFFER_SIZE_512B 512
		pcFormatString = NVMeoFRdmaAllocateNpp(NVMEOF_RECEIVE_BUFFER_SIZE_512B);
		if (!pcFormatString) {
			return;
		}

		for (; ulBufferCharCount < (ulBufSize - (ulBufSize % ulPrintCntPerLine));
			ulBufferCharCount += ulPrintCntPerLine) {
			for (pcTmepStr = pcFormatString, ulCharPerLn = 0; ulCharPerLn < ulPrintCntPerLine; ulCharPerLn++, pucBuffer++) {
				sprintf(pcTmepStr, SINGLE_CHAR_FORMAT_STRING, *pucBuffer);
				pcTmepStr += strlen(pcTmepStr);
			}
			NVMeoFDebugLog(ulLoglevel, "%s\n", pcFormatString);
			RtlZeroMemory(pcFormatString, NVMEOF_RECEIVE_BUFFER_SIZE_512B);
		}
		RtlZeroMemory(pcFormatString, NVMEOF_RECEIVE_BUFFER_SIZE_512B);
		if (ulBufSize % ulPrintCntPerLine) {
			for (pcTmepStr = pcFormatString, ulCharPerLn = 0; ulCharPerLn < (ulBufSize % ulPrintCntPerLine); ulCharPerLn++, pucBuffer++) {
				sprintf(pcTmepStr, SINGLE_CHAR_FORMAT_STRING, *pucBuffer);
				pcTmepStr += strlen(pcTmepStr);
			}
			NVMeoFDebugLog(ulLoglevel, "%s\n", pcFormatString);
		}

		NVMeoFRdmaFreeNpp(pcFormatString);
	}
}

