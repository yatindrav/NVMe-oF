/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#include "sp.h"
#include "trace.h"
#include "sp.tmh"

static PNVMEOF_MP_DRIVER_INFO psNvmeoFGlobalInfo = NULL;

ULONG
NVMeoFHwFindAdapter(__in    PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                    __in    PVOID HwContext,
                    __in    PVOID BusInfo,
                    __in    PCHAR ArgumentString,
                    __inout PPORT_CONFIGURATION_INFORMATION ConfigInfo,
                    __out   PBOOLEAN pbAgain)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(HwContext);
    UNREFERENCED_PARAMETER(BusInfo);
    UNREFERENCED_PARAMETER(ArgumentString);
    UNREFERENCED_PARAMETER(ConfigInfo);
    UNREFERENCED_PARAMETER(pbAgain);
    return (0);
}

BOOLEAN
NVMeoFHwInitialize(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt) 
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    return (TRUE);
}

VOID
NVMeoFHwReportAdapter(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    return;
}

VOID
NVMeoFHwReportLink(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    return ;
}

VOID
NVMeoFHwReportLog(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    return ;
}

VOID
NVMeoFHwFreeAdapterResources(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    return;
}

NTSTATUS
NVMeoFHwHandlePnP(__in PNVMEOF_HBA_EXT         psNVMeoFHbaExt,  // Adapter device-object extension from StorPort.
                  __in PSCSI_PNP_REQUEST_BLOCK psSrb)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psSrb);
    return (STATUS_SUCCESS);
}

NTSTATUS
NVMeoFHandleIOCTL(__in PNVMEOF_HBA_EXT     psNVMeoFHbaExt,  // Adapter device-object extension from StorPort.
                  __in PSCSI_REQUEST_BLOCK psSrb)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psSrb);
    return (STATUS_SUCCESS);
}

BOOLEAN
NVMeoFHwStartIo(__in PNVMEOF_HBA_EXT     psNVMeoFHbaExt,  // Adapter device-object extension from StorPort.
                __in PSCSI_REQUEST_BLOCK psSrb)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psSrb);
    return (STATUS_SUCCESS);
}

BOOLEAN
NVMeoFHwResetBus(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt,  // Adapter device-object extension from StorPort.
                 __in ULONG         ulBusResetParam)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(ulBusResetParam);
    return (TRUE);
}

SCSI_ADAPTER_CONTROL_STATUS
NVMeoFHwAdapterControl(__in PNVMEOF_HBA_EXT           psNVMeoFHbaExt,
                       __in SCSI_ADAPTER_CONTROL_TYPE ControlType,
                       __in PVOID                     pvParams)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(ControlType);
    UNREFERENCED_PARAMETER(pvParams);
    return (ScsiAdapterControlSuccess);
}

UCHAR
NVMeoFScsiExecuteMain(__in PNVMEOF_HBA_EXT      psNVMeoFHbaExt,
                      __in PSCSI_REQUEST_BLOCK  psSrb,
                      __in PNVMEOF_LU_EXTENSION psLUExt,
                      __in PUCHAR               pucStatus)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psSrb);
    UNREFERENCED_PARAMETER(psLUExt);
    UNREFERENCED_PARAMETER(pucStatus);
    return (0);
}

VOID
NVMeoFQueryRegParameters(__in PUNICODE_STRING puncRegPath,
                         __in PMP_REG_INFO psRegInfo)
{
    UNREFERENCED_PARAMETER(puncRegPath);
    UNREFERENCED_PARAMETER(psRegInfo);
}

UCHAR
NVMeoFGetDeviceType(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                    __in UCHAR ucPathId,
                    __in UCHAR ucTargetId,
                    __in UCHAR ucLun)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(ucPathId);
    UNREFERENCED_PARAMETER(ucTargetId);
    UNREFERENCED_PARAMETER(ucLun);
    return (UCHAR)0;
}

UCHAR
NVMeoFFindRemovedDevice(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                        __in PSCSI_REQUEST_BLOCK psSrb)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psSrb);
    return (UCHAR)0;
}

VOID
NVMeoFStopAdapter(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
}

VOID
NVMeoFTracingInit(__in PVOID pv1,
                  __in PVOID pv2)
{
    UNREFERENCED_PARAMETER(pv1);
    UNREFERENCED_PARAMETER(pv2);
}

VOID
NVMeoFTracingCleanup(__in PVOID pv)
{
    UNREFERENCED_PARAMETER(pv);
}

VOID
NVMeoFProcessServiceRequest(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                            __in PIRP psIrp)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psIrp);
}

VOID
NVMeoFCompleteServiceReq(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
}

VOID
NVMeoFInitializeWmiContext(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
}

BOOLEAN
NVMeoFHandleWmiSrb(__in    PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                   __inout PSCSI_WMI_REQUEST_BLOCK psWmiSrb)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psWmiSrb);
    return TRUE;
}

VOID
NVMeoFQueueServiceIrp(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                      __in PIRP            psIrp)
{
    UNREFERENCED_PARAMETER(psNVMeoFHbaExt);
    UNREFERENCED_PARAMETER(psIrp);
}

BOOLEAN
NVMeoFSetScsiSenseData(__inout PSCSI_REQUEST_BLOCK psSrb,
                       __in UCHAR ucScsiStatus,
                       __in UCHAR ucSenseKey,
                       __in UCHAR ucASC,
                       __in UCHAR ucASCQ)
{
    UNREFERENCED_PARAMETER(psSrb);
    UNREFERENCED_PARAMETER(ucScsiStatus);
    UNREFERENCED_PARAMETER(ucSenseKey);
    UNREFERENCED_PARAMETER(ucASC);
    UNREFERENCED_PARAMETER(ucASCQ);
    return FALSE;
}

#ifdef NVMEOF_DBG
VOID
NVMeoFPrintBuffer(__in PUCHAR buf,
    __in ULONG bufLen,
    __in ULONG printAlignment);
#else
#define NVMeoFPrintBuffer(x, y, z)
#endif

PNVMEOF_MP_DRIVER_INFO psNVMeoFGlobalInfo = NULL;

VOID
NVMeoFLogEvent(__in NTSTATUS ulLogLevel,
    __in __nullterminated PCHAR pcFormatString,
    ...);

typedef VOID(*fnNVMeoFDebugLog)(__in ULONG ulLogLevel,
    __in __nullterminated PCHAR pcFormatString,
    ...);

fnNVMeoFDebugLog
NVMeoFGetDebugLoggingFunction(VOID);

__declspec(dllexport)                                 // Ensure DriverEntry entry point visible to WinDbg even without a matching .pdb.            
NTSTATUS
DriverEntry(__in PVOID psDriverObject,
            __in PUNICODE_STRING psRegistryPath)
{
    NTSTATUS               status = STATUS_SUCCESS;
    HW_INITIALIZATION_DATA sNVMeoFHwInitData = { 0 };

    KdPrint(("%s:%d: Entering Driver Object=0x%p registry = %wZ!\n", __FUNCTION__, __LINE__, psDriverObject, psRegistryPath));
    psNVMeoFGlobalInfo = NVMeoFAllocateNpp(sizeof(NVMEOF_MP_DRIVER_INFO));
    if (!psNVMeoFGlobalInfo) {
        KdPrint(("%s:%d: Failed to allocate Root Driver Information!\n", __FUNCTION__, __LINE__));
        return (STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlZeroMemory(psNVMeoFGlobalInfo, sizeof(*psNVMeoFGlobalInfo));  // Set pMPDrvInfo's storage to a known state.

    psNVMeoFGlobalInfo->psDriverObj = psDriverObject;                 // Save pointer to driver object.

    KeInitializeSpinLock(&psNVMeoFGlobalInfo->MPIOExtLock);   //   "

    InitializeListHead(&psNVMeoFGlobalInfo->ListMPHBAObj);    // Initialize list head.
    InitializeListHead(&psNVMeoFGlobalInfo->ListMPIOExt);

    // Get registry parameters.
    NVMeoFQueryRegParameters(psRegistryPath, &psNVMeoFGlobalInfo->MPRegInfo);

    WPP_INIT_TRACING(psDriverObject, psRegistryPath);

    // Set up information for StorPortInitialize().

    RtlZeroMemory(&sNVMeoFHwInitData, sizeof(HW_INITIALIZATION_DATA));

    sNVMeoFHwInitData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);
    sNVMeoFHwInitData.HwInitialize = NVMeoFHwInitialize;       // Required.
    sNVMeoFHwInitData.HwStartIo = NVMeoFHwStartIo;          // Required.
    sNVMeoFHwInitData.HwFindAdapter = (PVOID)NVMeoFHwFindAdapter;      // Required.
    sNVMeoFHwInitData.HwResetBus = NVMeoFHwResetBus;         // Required.
    sNVMeoFHwInitData.HwAdapterControl = NVMeoFHwAdapterControl;   // Required.
    sNVMeoFHwInitData.HwFreeAdapterResources = NVMeoFHwFreeAdapterResources;
    sNVMeoFHwInitData.HwInitializeTracing = NVMeoFTracingInit;
    sNVMeoFHwInitData.HwCleanupTracing = NVMeoFTracingCleanup;
    sNVMeoFHwInitData.HwProcessServiceRequest = NVMeoFProcessServiceRequest;
    sNVMeoFHwInitData.MultipleRequestPerLu = TRUE;
    sNVMeoFHwInitData.AdapterInterfaceType = Internal;
    sNVMeoFHwInitData.FeatureSupport = STOR_FEATURE_VIRTUAL_MINIPORT | 
                                         STOR_FEATURE_FULL_PNP_DEVICE_CAPABILITIES  | 
                                         STOR_FEATURE_DEVICE_NAME_NO_SUFFIX  | 
                                         STOR_FEATURE_EXTRA_IO_INFORMATION |
                                         STOR_FEATURE_ADAPTER_CONTROL_PRE_FINDADAPTER |
                                         STOR_FEATURE_SET_ADAPTER_INTERFACE_TYPE | 
                                         STOR_FEATURE_NVME;
    sNVMeoFHwInitData.DeviceExtensionSize = sizeof(NVMEOF_HBA_EXT);
    sNVMeoFHwInitData.SpecificLuExtensionSize = sizeof(NVMEOF_LU_EXTENSION);
    sNVMeoFHwInitData.SrbExtensionSize = sizeof(NVMEOF_SRB_EXTENSION);

    status = StorPortInitialize(psDriverObject,                     // Tell StorPort we're here.
                                psRegistryPath,
                                (PHW_INITIALIZATION_DATA)&sNVMeoFHwInitData,     // Note: Have to override type!
                                NULL);
    if (!NT_SUCCESS(status)) {                     // Port driver said not OK?                                        
        NVMeoFFreeNpp(psNVMeoFGlobalInfo);
        KdPrint(("%s:%d:StorPortInitialize failed Exiting Status=0x%x!\n", __FUNCTION__, __LINE__, status));
        WPP_CLEANUP(psDriverObject);
    }

    NVMeoFLogEvent(NVMEF_INFORMATIONAL, "%s:%d: NVMeoF Driver Version %s", __FUNCTION__, __LINE__, NVMEOF_STORPORT_VERSION);
    NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: Exiting Status=0x%x!\n", __FUNCTION__, __LINE__, status);

    return status;
}