/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once
#include <ntstrsafe.h>
#include "nvme.h"
#include "nvmef.h"
#include "nvmeftcp.h"

#define NVMEOF_MAX_ADMIN_LOG_PAGE_DATA_LENGTH 4096

VOID
NVMeoFSetSGNull(__in PNVMEOF_FABRIC_CONTROLLER psController,
                __in PNVME_CMD psCmd);

NTSTATUS
NVMeoFSetFeatures(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr,
                  __in ULONG              ulFeatureID,
                  __in ULONG              ulDWD11,
                  __in_opt PVOID          pvBuff,
                  __in_opt DWORD          dwBufLen);

NTSTATUS
NVMeoFGetLogPage(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr,
                 __in UCHAR              ucLID,
                 __in ULONG              ulNSID,
                 __in UCHAR              ucLSP,
                 __in PVOID              pvDataBuffer,
                 __in ULONG              ulDataToRead,
                 __in ULONGLONG          ullOffset);

NTSTATUS
NVMeoFIndentifyController(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr);

NTSTATUS
NVMeoFIndentifyGetActiveNSList(__in PNVMEOF_FABRIC_CONTROLLER_CONNECT psFabCtrlr);

NTSTATUS
NVMeoFIndentifyNamespace(__in PNVMEOF_FABRIC_CONTROLLER_CONNECT psFabCtrlr,
                         __in ULONG nsID);

NTSTATUS
NVMeoFEnableController(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr);

NTSTATUS
NVMeoFAsyncEventRequest(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr);

NTSTATUS
NVMeoFGetANADescriptorLogPage(__in PNVMEOF_FABRIC_CONTROLLER_CONNECT psFabCtrlr);

NTSTATUS
NVMeoFGetWinError(__in PNVME_COMPLETION_QUEUE_ENTRY psNVMECQe);

VOID
NVMeoFDisplayCQE(__in PCHAR pcFuncName,
                 __in PNVME_COMPLETION_QUEUE_ENTRY psCQE);

#ifdef _DBG
VOID
NVMeoFDisplayNVMeCmd(__in PCHAR pcFuncName,
                     __in PNVME_CMD psCmd);

#else
#define NVMeoFDisplayNVMeCmd(X, Y)
#endif


NTSTATUS
NVMeoFRegisterRead64(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr,
                     __in ULONG ulOffset,
                     __inout PULONGLONG pllDataRead);
NTSTATUS
NVMeoFRegisterRead32(__in    PNVMEOF_FABRIC_CONTROLLER psFabCtrlr,
                     __in    ULONG ulOffset,
                     __inout PULONG plDataRead);
NTSTATUS
NVMeoFRegisterWrite32(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr,
                      __in ULONG ulOffset,
                      __in ULONG ulDataWrite);

NTSTATUS
NVMeoFGetDiscoveryLogPage(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr,
                          __in PVOID pvUserDataBuffer,
                          __in ULONG ulDataToRead);

NTSTATUS
NVMeoFAsyncIssueKeepAlive(__in PNVMEOF_FABRIC_CONTROLLER psFabCtrlr);

#define NVMEOF_CMD_MEMORY_TAG 'ndmC'

FORCEINLINE
PVOID
NVMeoFCmdAllocateNpp(_In_ SIZE_T szNumBytes)
{
    POOL_EXTENDED_PARAMETER sParams = { 0 };
    sParams.Type = PoolExtendedParameterPriority;
    sParams.Priority = NormalPoolPriority;
    return ExAllocatePool3(POOL_FLAG_NON_PAGED, szNumBytes, NVMEOF_CMD_MEMORY_TAG, &sParams, 1);
}

FORCEINLINE
VOID
NVMeoFCmdFreeNpp(_In_ PVOID pvEntry)
{
    POOL_EXTENDED_PARAMETER sParams = { 0 };
    sParams.Type = PoolExtendedParameterPriority;
    sParams.Priority = NormalPoolPriority;
    ExFreePool2(pvEntry, NVMEOF_CMD_MEMORY_TAG, &sParams, 1);
}
