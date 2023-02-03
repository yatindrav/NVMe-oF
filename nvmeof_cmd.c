#include <wdm.h>
#include <ntstrsafe.h>
#include "nvme.h"
#include "nvmef.h"
#include "nvmeftcp.h"
#include "nvmefcmd.h"
#include "userctrl.h"
#include "errdbg.h"

#include "trace.h"
#include "nvmeof_cmd.tmh"

VOID
NVMeoFDisplayCQE(__in PCHAR pcFuncName,
	__in PNVME_COMPLETION_QUEUE_ENTRY psCQE)
{
	UNREFERENCED_PARAMETER(pcFuncName);
	UNREFERENCED_PARAMETER(psCQE);

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: %s CS Data %#llx Status %hx CMD ID %hx SQHEAD %hx SQID %hx!\n", __FUNCTION__, __LINE__,
		pcFuncName,
		psCQE->sCmdRespData.ullDDW,
		psCQE->sStatus.usStatus,
		psCQE->usCommandID,
		psCQE->usSQHead,
		psCQE->usSQId);

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: Detailed Status: 0x%hx SC Bits 0x%hx SCT 0x%hx DNR 0x%hx More 0x%hx PhaseTag 0x%hx!!!\n", __FUNCTION__, __LINE__,
		pcFuncName,
		psCQE->sStatus.usStatus,
		psCQE->sStatus.sStatusField.SC,
		psCQE->sStatus.sStatusField.SCT,
		psCQE->sStatus.sStatusField.DNR,
		psCQE->sStatus.sStatusField.More,
		psCQE->sStatus.sStatusField.PhaseTag);
}

#ifdef PDS_DBG
static
VOID
NVMeoFDisplayNVMeFabCmd(__in PCHAR pcFuncName,
	__in PNVMEOF_GENERIC_COMMAND psCmd)
{
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: %s Opcode %hx Flags %hx CmdID %hx CmdType %hx!\n", __FUNCTION__, __LINE__,
		pcFuncName,
		psCmd->ucOpCode,
		psCmd->ucResvd1,
		psCmd->usCommandId,
		psCmd->ucFabricCmdType);
	switch (psCmd->ucFabricCmdType) {
	case NVMEOF_TYPE_PROPERTY_GET:
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: %s PROP_GET Attrib %hx Offset %x!\n", __FUNCTION__, __LINE__,
			pcFuncName,
			((PNVMEOF_SET_PROPERTY_COMMAND)psCmd)->ucAttrib,
			((PNVMEOF_SET_PROPERTY_COMMAND)psCmd)->ulOffset);
		break;
	case NVMEOF_TYPE_PROPERTY_SET:
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: %s PROP_SET Attrib %hx Offset %x Value %llx!\n", __FUNCTION__, __LINE__,
			pcFuncName,
			((PNVMEOF_SET_PROPERTY_COMMAND)psCmd)->ucAttrib,
			((PNVMEOF_SET_PROPERTY_COMMAND)psCmd)->ulOffset,
			((PNVMEOF_SET_PROPERTY_COMMAND)psCmd)->ullValue);
		break;
	case NVMEOF_TYPE_CONNECT:
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: %s CONNECT: DataPtr %#llx RecFmt %hx QId %hx SQSize %hx CAttr %hx Resvd3 %hx KATO %lx!\n", __FUNCTION__, __LINE__,
			pcFuncName,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->sDataPtr.PRP.PRP1,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->usRecordFormat,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->usQId,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->usSQSize,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->ucCAttr,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->ucResvd3,
			((PFABRIC_CONTROLLER_TYPE_CONNECT_COMMAND)psCmd)->ulKATO);
		break;
	default:
		return;
	}
}

static
VOID
NVMeoFDisplayNVMeSetFeatureCmd(__in PCHAR pcFuncName,
	__in PNVME_FEATURES_COMMAND psCmd)
{
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: %s Opcode %hx Flags %hx CmdID %hx FeatID %hx!\n", __FUNCTION__, __LINE__,
		pcFuncName,
		psCmd->ucOpCode,
		psCmd->ucFlags,
		psCmd->usCommandId,
		psCmd->ulFeatureID);
}

VOID
NVMeoFDisplayNVMeCmd(__in PCHAR pcFuncName,
	__in PNVME_CMD psCmd)
{
	UNREFERENCED_PARAMETER(pcFuncName);
	switch (psCmd->sGeneric.sCDW0.ucOpCode) {
	case NVME_OVER_FABRICS_COMMAND:
		NVMeoFDisplayNVMeFabCmd(pcFuncName, (PNVMEOF_GENERIC_COMMAND)psCmd);
		break;
	case NVME_ADMIN_OPCODE_SET_FEATURES:
		NVMeoFDisplayNVMeSetFeatureCmd(pcFuncName, (PNVME_FEATURES_COMMAND)psCmd);
		break;
	default:
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: %s Opcode %hx Flags %hx CmdID %hx NSID %#lx DW2 %#lx DW3 %#lx Metadata %#llx DataPtr %#llX!\n", __FUNCTION__, __LINE__,
			pcFuncName,
			psCmd->sGeneric.sCDW0.ucOpCode,
			psCmd->sGeneric.sCDW0.sFlags.ucFlags,
			psCmd->sGeneric.sCDW0.usCommandID,
			psCmd->sGeneric.ulNSId,
			psCmd->sGeneric.sCDW2_3.ulaReserved[0],
			psCmd->sGeneric.sCDW2_3.ulaReserved[1],
			psCmd->sGeneric.sCDW4_5.ullMetaDataPointer,
			psCmd->sGeneric.sDataPtr.PRP.PRP1);

		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: %s DW10 %#lx DW11 %#lx DW12 %#lx DW13 %#lx DW14 %#lx DW15 %#lx!\n", __FUNCTION__, __LINE__,
			pcFuncName,
			psCmd->sGeneric.ulCDW10,
			psCmd->sGeneric.ulCDW11,
			psCmd->sGeneric.ulCDW12,
			psCmd->sGeneric.ulCDW13,
			psCmd->sGeneric.ulCDW14,
			psCmd->sGeneric.ulCDW15);
	}
}
#endif

static
NTSTATUS
NVMeoFGetGenericWinStatus(__in USHORT usStatus)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Entering!\n", __FUNCTION__, __LINE__);
	switch (usStatus) {
	case NVME_SC_GEN_SUCCESS:Status = STATUS_SUCCESS; break;
	case NVME_SC_GEN_INVALID_OPCODE:Status = STATUS_INVALID_PARAMETER; break;
	case NVME_SC_GEN_INVALID_FIELD:Status = STATUS_INVALID_FIELD_IN_PARAMETER_LIST; break;
	case NVME_SC_GEN_CMDID_CONFLICT:Status = STATUS_INVALID_PARAMETER; break;
	case NVME_SC_GEN_DATA_XFER_ERROR:Status = STATUS_IO_DEVICE_ERROR; break;
	case NVME_SC_GEN_POWER_LOSS_ABORT:Status = STATUS_DEVICE_POWERED_OFF; break;
	case NVME_SC_GEN_INTERNAL_ERROR:Status = STATUS_INTERNAL_ERROR; break;
	case NVME_SC_GEN_CMD_ABORT_REQESTED:Status = STATUS_TRANSACTION_ABORTED; break;
	case NVME_SC_GEN_SQ_DEL_ABORT:Status = STATUS_TRANSACTION_ABORTED; break;
	case NVME_SC_GEN_FUSED_FAIL_ABORT:Status = STATUS_TRANSACTION_ABORTED; break;
	case NVME_SC_GEN_FUSED_MISSING_ABORT:Status = STATUS_TRANSACTION_ABORTED; break;
	case NVME_SC_GEN_INVALID_NS_FORMAT:Status = STATUS_INVALID_IMAGE_FORMAT; break;
	case NVME_SC_GEN_CMD_SEQ_ERROR:Status = STATUS_REQUEST_OUT_OF_SEQUENCE; break;
	case NVME_SC_GEN_INVALID_SGL_SEG_DESC:Status = STATUS_TOO_MANY_SEGMENT_DESCRIPTORS; break;
	case NVME_SC_GEN_INVALID_SGL_DESC_COUNT:Status = STATUS_TOO_MANY_SEGMENT_DESCRIPTORS; break;
	case NVME_SC_GEN_INVALID_DATA_SGL_LENGTH:Status = STATUS_INVALID_BUFFER_SIZE; break;
	case NVME_SC_GEN_INVALID_METADATA_SGL_LENGTH:Status = STATUS_INVALID_BUFFER_SIZE; break;
	case NVME_SC_GEN_INVALID_SGL_DESC_TYPE:Status = STATUS_OBJECT_TYPE_MISMATCH;  break;
	case NVME_SC_GEN_INVALID_USE_CTRLR_MEM_BUFF:Status = STATUS_OBJECT_TYPE_MISMATCH; break;
	case NVME_SC_GEN_INVALID_PRP_OFFSET:Status = STATUS_INVALID_OFFSET_ALIGNMENT;  break;
	case NVME_SC_GEN_ATOMIC_WRITE_UNIT_EXCEED:Status = STATUS_INVALID_BUFFER_SIZE;  break;
	case NVME_SC_GEN_OPERATION_DENIED:Status = STATUS_INVALID_BUFFER_SIZE; break;
	case NVME_SC_GEN_INVALID_SGL_OFFSET:Status = STATUS_INVALID_PARAMETER;  break;
	case NVME_SC_GEN_INVALID_SGL_SUBTYPE:Status = STATUS_INVALID_PARAMETER;  break;
	case NVME_SC_GEN_HOST_ID_INCONSISTENT_FMT:Status = STATUS_INVALID_IMAGE_FORMAT; break;
	case NVME_SC_GEN_KEEP_ALIVE_TIMER_EXPIRED:Status = STATUS_IO_TIMEOUT; break;
	case NVME_SC_GEN_INVALID_KEEP_ALIVE_TIMEOUT:Status = STATUS_IO_TIMEOUT; break;
	case NVME_SC_GEN_CMD_ABORT_PREEMPT_ABORT:Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_GEN_SANITIZE_FAILED: Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_GEN_SANITIZE_IN_PROGRESS:Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_GEN_INVALID_SGL_DATA_BLK_GRAN:Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_GEN_UNSUPPORTED_CMD_CMB_QUEUE:Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_NS_WRITE_PROTECTED:Status = STATUS_MEDIA_WRITE_PROTECTED; break;
	case NVME_SC_CMD_INTERRUPTED: Status = STATUS_INTERRUPTED;  break;
	case NVME_SC_XIENT_XPORT_ERROR:Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_LBA_RANGE: Status = STATUS_INVALID_BLOCK_LENGTH; break;
	case NVME_SC_CAP_EXCEEDED: Status = STATUS_DISK_QUOTA_EXCEEDED; break;
	case NVME_SC_NS_NOT_READY:Status = STATUS_DEVICE_NOT_READY; break;
	case NVME_SC_RESERVATION_CONFLICT:Status = STATUS_UNSUCCESSFUL; break;
	case NVME_SC_FORMAT_IN_PROGRESS:Status = STATUS_UNSUCCESSFUL; break;
	default:Status = STATUS_UNSUCCESSFUL; break;
	}
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
	}
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
NTSTATUS
NVMeoFGetCmdSpecificWinStatus(USHORT usStatus)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Entering!\n", __FUNCTION__, __LINE__);
	switch (usStatus) {
	case NVME_SC_INVALID_CQ: break;
	case NVME_SC_INVALID_QID: break;
	case NVME_SC_INVALID_QUEUE_SIZE: break;
	case NVME_SC_ABORT_CMD_LIMIT_EXCEEDED: break;
	case NVME_SC_ABORT_CMD_MISSING: break;
	case NVME_SC_ASYNC_EVT_REQ_LIMIT_EXCEEDED: break;
	case NVME_SC_INVALID_FIRMWARE_SLOT: break;
	case NVME_SC_INVALID_FIRMWARE_IMAGE: break;
	case NVME_SC_INVALID_INTERRUPT_VECTOR: break;
	case NVME_SC_INVALID_LOG_PAGE: break;
	case NVME_SC_INVALID_FORMAT: break;
	case NVME_SC_FW_ACTIVATION_NEEDS_CONV_RESET: break;
	case NVME_SC_INVALID_QUEUE_DELETE: break;
	case NVME_SC_FEATURE_ID_NOT_SAVEABLE: break;
	case NVME_SC_FEATURE_NOT_CHANGEABLE: break;
	case NVME_SC_FEATURE_NOT_FOR_NAMESPACE: break;
	case NVME_SC_FW_ACTIV_NEEDS_NVM_SUBSYS_RESET: break;
	case NVME_SC_FW_ACTIVATION_NEEDS_CTRLR_RESET: break;
	case NVME_SC_FW_ACT_NEEDS_MAX_TIME_VIOLATION: break;
	case NVME_SC_FW_ACIVATION_PROHIBITED: break;
	case NVME_SC_OVERLAPPING_RANGE: break;
	case NVME_SC_NS_INSUFFICENT_CAP: break;
	case NVME_SC_NS_UNAVAILABLE_ID: break;
	case NVME_SC_NS_RESERVED_17: break;
	case NVME_SC_NS_ALREADY_ATTACHED: break;
	case NVME_SC_NS_IS_PRIVATE: break;
	case NVME_SC_NS_NOT_ATTACHED: break;
	case NVME_SC_NS_THIN_PROVISIONING_UNSUPPORTED: break;
	case NVME_SC_NS_CTRLR_INVALID_LIST: break;
	case NVME_SC_DEV_SELF_TEST_IN_PROGRESS: break;
	case NVME_SC_BOOT_PARTITION_WRITE_PROHIBITED: break;
	case NVME_SC_INVALID_CTRLR_IDENTIFIER: break;
	case NVME_SC_INVALID_SECONDARY_CTRLR_STATE: break;
	case NVME_SC_INVALID_NUM_CTRLR_RESOURCE: break;
	case NVME_SC_INVALID_RESOURCE_IDENTIFIER: break;
	case NVME_SC_PROHIBITED_SANITIZE_PMR_ENABLED: break;
	case NVME_SC_INVALID_ANA_GROUP_IDENTIFIER: break;
	case NVME_SC_FAILED_ANA_ATTACHED: break;
	case NVME_SC_CONNECT_INCOMPATIBLE_FORMAT:break;
	case NVME_SC_CONNECT_CTROLLER_BUSY:break;
	case NVME_SC_CONNECT_INVALID_PARAMS:break;
	case NVME_SC_CONNECT_RESTART_DISCOVERY:break;
	case NVME_SC_CONNECT_INVALID_HOST:break;
	case NVME_SC_RESTART_DISCOVERY:break;
	case NVME_SC_AUTH_REQUIRED:break;
	}
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
	}
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
NTSTATUS
NVMeoFGetMediaAndDataWinStatus(USHORT usStatus)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Entering!\n", __FUNCTION__, __LINE__);
	switch (usStatus) {
	case NVME_SC_WRITE_FAULT: break;
	case NVME_SC_UNRECOVERED_READ_ERROR: break;
	case NVME_SC_E2E_GUARD_CHECK_ERROR: break;
	case NVME_SC_E2E_APPN_TAG_CHECK_ERROR: break;
	case NVME_SC_E2E_REF_TAG_CHECK_ERROR: break;
	case NVME_SC_COMPARE_FAILED: break;
	case NVME_SC_ACCESS_DENIED: break;
	case NVME_SC_DEALLOCATED_UNWRITTEN_LBLK: break;
	}
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
	}
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
NTSTATUS
NVMeoFGetPathSpecificWinStatus(USHORT usStatus)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Entering!\n", __FUNCTION__, __LINE__);
	switch (usStatus) {
	case NVME_SC_INTERNAL_PATH_ERROR: break;
	case NVME_SC_ASYM_ACCESS_PERSISTENT_LOSS: break;
	case NVME_SC_ASYM_ACCESS_INACCESSIBLE: break;
	case NVME_SC_ASYM_ACCESS_TRANSITION: Status = STATUS_OBJECT_PATH_INVALID; break;
	case NVME_SC_CONTROLLER_PATH_ERROR: break;
	case NVME_SC_HOST_PATH_ERROR: break;
	case NVME_SC_CMD_ABORTED_BY_HOST: break;
	}
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d: Status is not Success 0x%X!\n", __FUNCTION__, __LINE__, Status);
	}
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return (Status);
}

NTSTATUS
NVMeoFGetWinError(__in PNVME_COMPLETION_QUEUE_ENTRY psCqe)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	switch ((psCqe->sStatus.sStatusField.SCT)) {
	case NVME_COMMAND_SCT_GENERIC:
		return NVMeoFGetGenericWinStatus((psCqe->sStatus.usStatus & 0x7FF));
	case NVME_COMMAND_SCT_CMD_SPCIFIC:
		return NVMeoFGetCmdSpecificWinStatus((psCqe->sStatus.usStatus & 0x7FF));
	case NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY:
		return NVMeoFGetMediaAndDataWinStatus((psCqe->sStatus.usStatus & 0x7FF));
	case NVME_COMMAND_SCT_PATH_RELATED:
		return NVMeoFGetPathSpecificWinStatus((psCqe->sStatus.usStatus & 0x7FF));
	case NVME_COMMAND_SCT_VENDOR_SPECIFIC:
		break;
	}
	return (Status);
}

static 
PNVMEOF_REQUEST_RESPONSE
NVMeoFAdminRequestResponseAllocate(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                   __in DWORD dwSendDatalLen,
                                   __in DWORD dwReceiveDatalLen)
{
	PNVMEOF_REQUEST_RESPONSE pvNvmeCtrlrCmd = NULL;
	NTSTATUS Status =
		psController->sFabricOps._FabricNVMeoFRequestsAllocate(psController,
                                                               dwSendDatalLen,
                                                               dwReceiveDatalLen,
                                                               &pvNvmeCtrlrCmd);
	if (NT_SUCCESS(Status)) {
		return pvNvmeCtrlrCmd;
	} else {
		return NULL;
	}
}

static
VOID
NVMeoFAdminRequestResponseFree(__in PNVMEOF_FABRIC_CONTROLLER psController,
                               __in PNVMEOF_REQUEST_RESPONSE  psReqResp)
{
	psController->sFabricOps._FabricNVMeoFRequestsFree(psController, psReqResp);
}

VOID
NVMeoFSetSGNull(__in PNVMEOF_FABRIC_CONTROLLER psFabController, __in PNVME_CMD psCmd)
{
	psFabController->sFabricOps._FabricSetSGFlag(psFabController, 0, psCmd, 0, 0, 0, NVMEOF_COMMAND_DESCRIPTORT_TYPE_NULL);
}

VOID
NVMeoFSetSGInline(__in PNVMEOF_FABRIC_CONTROLLER psFabController, __in PNVME_CMD psCmd, ULONG ulLen)
{
	psFabController->sFabricOps._FabricSetSGFlag(psFabController, 0, psCmd, 0, ulLen, 0, NVMEOF_COMMAND_DESCRIPTORT_TYPE_INLINE);
}

VOID
NVMeoFSetSG(__in PNVMEOF_FABRIC_CONTROLLER psFabController, __in PNVME_CMD psCmd, ULONGLONG ullAddr, ULONG ulLen)
{
	psFabController->sFabricOps._FabricSetSGFlag(psFabController, 0, psCmd, ullAddr, ulLen, 0, NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_SINGLE);
}

NTSTATUS
NVMeoFSetFeatures(__in PNVMEOF_FABRIC_CONTROLLER psFabController,
                  __in ULONG                     ulFeatureID,
                  __in ULONG                     ulDWD11,
                  __in_opt PVOID                 pvBuff,
                  __in_opt DWORD                 dwBufLen)
{
	NTSTATUS                 Status = STATUS_SUCCESS;
	PNVME_CMD                psNvmeCmd = NULL;
	PNVME_CMD_FLAGS          psFlags = NULL;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;

	psAdminReqResp = 
		NVMeoFAdminRequestResponseAllocate(psFabController,
                                           dwBufLen,
                                           0);
	if (!psAdminReqResp) {
		return STATUS_NO_MEMORY;
	}

	psNvmeCmd =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psFabController, psAdminReqResp);

	psNvmeCmd->sFeatures.ucOpCode = NVME_ADMIN_OPCODE_SET_FEATURES;
	psNvmeCmd->sFeatures.usCommandId = psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueGetCmdID(psFabController->sSession.psAdminQueue);
	psFlags = (PNVME_CMD_FLAGS)&psNvmeCmd->sFeatures.ucFlags;
	psFlags->sOptions.ucRprOrSglForXfer = NVME_CMD_SGL_METASEG;
	psNvmeCmd->sFeatures.ulFeatureID = ulFeatureID;
	psNvmeCmd->sFeatures.ulDWord11 = ulDWD11;
	if (dwBufLen) {
		PVOID pvDataBuffer =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommandData(psFabController, psAdminReqResp);
		RtlMoveMemory(pvDataBuffer, pvBuff, dwBufLen);
		NVMeoFSetSGInline(psFabController, psNvmeCmd, dwBufLen);
	}
	else {
		NVMeoFSetSGNull(psFabController, psNvmeCmd);
	}

	psAdminReqResp->ulFlags |= NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE_SYNC;

	Status =
		psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueSubmitRequest(&psFabController->sSession.psAdminQueue,
                                                                                    psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psFabController,
                                                                             psAdminReqResp);
		//NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (!psCqe->sStatus.sStatusField.SC && psCqe->sStatus.sStatusField.SCT) {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d: Return value = 0x%hx\n", __FUNCTION__, __LINE__,
				           psCqe->sStatus.usStatus);
			Status = NVMeoFGetWinError(psCqe);
		}
	}

	NVMeoFAdminRequestResponseFree(psFabController, psAdminReqResp);

	return Status;
}

NTSTATUS
NVMeoFGetLogPage(__in PNVMEOF_FABRIC_CONTROLLER psFabController,
                 __in UCHAR ucLID,
                 __in ULONG ulNSID,
                 __in UCHAR ucLSP,
                 __in PVOID pvDataBuffer,
                 __in ULONG ulDataToRead,
                 __in ULONGLONG ullOffset)
{
	NTSTATUS                 Status = STATUS_SUCCESS;
	PNVME_CMD                psNvmeCmd = NULL;
	ULONG                    ulTotalDWToReturn = (ulDataToRead >> SHIFT_2BIT) - 1;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;
	PNVME_CMD_FLAGS          psFlags = NULL;

	psAdminReqResp =
		NVMeoFAdminRequestResponseAllocate(psFabController,
			                               0,
			                               ulDataToRead);
	if (!psAdminReqResp) {
		return STATUS_NO_MEMORY;
	}

	psNvmeCmd =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psFabController, psAdminReqResp);

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d:Sending Get Log Page %u with length= %d\n", __FUNCTION__, __LINE__, ucLID, ulDataToRead);
	psNvmeCmd->sGetLogPage.OpCode = NVME_ADMIN_OPCODE_GET_LOG_PAGE;
	psNvmeCmd->sGetLogPage.ucLId = ucLID;
	psNvmeCmd->sGetLogPage.ulNSId = ulNSID;
	psNvmeCmd->sGetLogPage.ucLSp = ucLSP;
	psNvmeCmd->sGetLogPage.usNumDWUpper = (ulTotalDWToReturn >> SHIFT_16BIT) & WORD_MASK;
	psNvmeCmd->sGetLogPage.usNumDWLower = ulTotalDWToReturn & WORD_MASK;
	psNvmeCmd->sGetLogPage.ulLpOu = (ullOffset >> SHIFT_32BIT) & DOUBLE_WORD_MASK;
	psNvmeCmd->sGetLogPage.ulLpOl = ullOffset & DOUBLE_WORD_MASK;
	psNvmeCmd->sGetLogPage.usCommandId = psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueGetCmdID(psFabController->sSession.psAdminQueue);
	psFlags = (PNVME_CMD_FLAGS)&psNvmeCmd->sGetLogPage.ucFlags;
	psFlags->sOptions.ucRprOrSglForXfer = NVME_CMD_SGL_METABUF;

	psAdminReqResp->ulFlags |= NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE_SYNC;

	Status =
		psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueSubmitRequest(psFabController->sSession.psAdminQueue,
                                                                         psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psFabController,
				psAdminReqResp);
		NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (!psCqe->sStatus.sStatusField.SC && psCqe->sStatus.sStatusField.SCT) {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d: Return value = 0x%hx\n", __FUNCTION__, __LINE__,
				psCqe->sStatus.usStatus);
			Status = NVMeoFGetWinError(psCqe);
			if (NT_SUCCESS(Status)) {
				PVOID pvReceivedDataBuffer =
					psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetResponseData(psFabController, psAdminReqResp);
				RtlCopyMemory(pvDataBuffer, pvReceivedDataBuffer, ulDataToRead);
			}
		}
	}

	NVMeoFAdminRequestResponseFree(psFabController, psAdminReqResp);
	return Status;
}

static
NTSTATUS
NVMeoFIssueIndentifyCommand(__in PNVMEOF_FABRIC_CONTROLLER psFabController,
                            __in UINT                      uiCommand,
                            __in UINT                      uiNSId,
                            __inout PVOID                  pvDataOut,
                            __in ULONG                     ulDataLen)
{
	NTSTATUS                     Status = STATUS_SUCCESS;
	PNVME_CMD                    psNvmeCmd = NULL;
	PNVME_CMD_FLAGS              psFlags = NULL;
	PNVMEOF_REQUEST_RESPONSE     psAdminReqResp = NULL;

	sizeof(NVMEOF_FABRIC_CONTROLLER);

	switch (uiCommand) {
	case NVME_IDENTIFY_CNS_CTRL:
		if (ulDataLen < sizeof(NVME_IDENTIFY_CONTROLLER)) {
			return (STATUS_BUFFER_TOO_SMALL);
		}
		break;
	case NVME_IDENTIFY_CNS_NS:
		if (ulDataLen < sizeof(NVME_IDENTYFY_NAMESPACE)) {
			return (STATUS_BUFFER_TOO_SMALL);
		}
		break;
	case NVME_IDENTIFY_CNS_NS_ACTIVE_LIST:
		if (ulDataLen < sizeof(ULONG)) {
			return (STATUS_BUFFER_TOO_SMALL);
		}
		break;
	default:
		return STATUS_NOT_SUPPORTED;
	}

	psAdminReqResp =
		NVMeoFAdminRequestResponseAllocate(psFabController,
                                           0,
                                           ulDataLen);
	if (!psAdminReqResp) {
		return STATUS_NO_MEMORY;
	}

	psNvmeCmd =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psFabController, psAdminReqResp);

	psNvmeCmd->sIdentify.ucOpCode = NVME_ADMIN_OPCODE_IDENTIFY;
	switch (uiCommand) {
	case NVME_IDENTIFY_CNS_CTRL:
		psNvmeCmd->sIdentify.ucCNS = NVME_IDENTIFY_CNS_CTRL;
		break;
	case NVME_IDENTIFY_CNS_NS:
		psNvmeCmd->sIdentify.ucCNS = NVME_IDENTIFY_CNS_NS;
		psNvmeCmd->sIdentify.ulNSId = uiNSId;
		break;
	case NVME_IDENTIFY_CNS_NS_ACTIVE_LIST:
		psNvmeCmd->sIdentify.ucCNS = NVME_IDENTIFY_CNS_NS_ACTIVE_LIST;
		break;
	default:
		psFabController->sFabricOps._FabricNVMeoFRequestsFree(psFabController, psAdminReqResp);
		return STATUS_NOT_SUPPORTED;
	}

	psNvmeCmd->sIdentify.usCommandId = psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueGetCmdID(psFabController->sSession.psAdminQueue);
	psFlags = (PNVME_CMD_FLAGS)&psNvmeCmd->sIdentify.ucFlags;
	psFlags->sOptions.ucRprOrSglForXfer = NVME_CMD_SGL_METABUF;


	psAdminReqResp->ulFlags |= NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE_SYNC_RECEIVE_DATA;

	Status =
		psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueSubmitRequest(psFabController->sSession.psAdminQueue,
			                                                                        psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psFabController,
				psAdminReqResp);
		PVOID pvReceivedDataBuffer =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetResponseData(psFabController, psAdminReqResp);
		NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (NVME_IDENTIFY_CNS_CTRL == uiCommand) {
			*(PNVME_IDENTIFY_CONTROLLER)pvDataOut = *(PNVME_IDENTIFY_CONTROLLER)pvReceivedDataBuffer;
		}
		else if (NVME_IDENTIFY_CNS_NS == uiCommand) {
			*(PNVME_IDENTYFY_NAMESPACE)pvDataOut = *(PNVME_IDENTYFY_NAMESPACE)pvReceivedDataBuffer;
		}
		else if (NVME_IDENTIFY_CNS_NS_ACTIVE_LIST == uiCommand) {
			*(PULONG)pvDataOut = *(PULONG)pvReceivedDataBuffer;
		}
	}

	NVMeoFAdminRequestResponseFree(psFabController, psAdminReqResp);

	return Status;
}

NTSTATUS
NVMeoFRegisterRead64(__in PNVMEOF_FABRIC_CONTROLLER psFabController,
                     __in ULONG                     ulOffset,
                     __inout PULONGLONG             pullDataRead)
{
	return psFabController->sFabricOps._FabricRegisterRead64(psFabController, ulOffset, pullDataRead);
}

NTSTATUS
NVMeoFRegisterRead32(__in    PNVMEOF_FABRIC_CONTROLLER psFabController,
                     __in    ULONG ulOffset,
                     __inout PULONG  pulDataWrite)
{
	return psFabController->sFabricOps._FabricRegisterRead32(psFabController, ulOffset, pulDataWrite);
}

NTSTATUS
NVMeoFRegisterWrite32(__in   PNVMEOF_FABRIC_CONTROLLER psFabController,
                      __in    ULONG ulOffset,
                      __in ULONG  ulDataWrite)
{
	return psFabController->sFabricOps._FabricRegisterWrite32(psFabController, ulOffset, ulDataWrite);
}


NTSTATUS
NVMeoFIndentifyController(__in PNVMEOF_FABRIC_CONTROLLER psFabController)
{
	NTSTATUS Status = STATUS_SUCCESS;

	Status =NVMeoFRegisterRead32(psFabController,
                                 NVME_PROPERTY_OFFSET_VS,
                                 &psFabController->ulVendorSpecific);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d:Reading VS failed with Status (0x%x)\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d:Reading VS failed with Status (0x%x)\n", __FUNCTION__, __LINE__, Status);
		return (STATUS_UNSUCCESSFUL);
	}

	Status = NVMeoFRegisterRead64(psFabController,
			                      NVME_PROPERTY_OFFSET_CAP,
			                      &psFabController->ullControllerCapability);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d:Reading CAP failed wit Status (0x%x)\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d:Reading CAP failed with Status (0x%x)\n", __FUNCTION__, __LINE__, Status);
		return (STATUS_UNSUCCESSFUL);
	}

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d:Controller Capabilities 0x%llx.\n", __FUNCTION__, __LINE__, 
		           psFabController->sCommonControllerInfo.ullControllerCapability);

	Status =
		NVMeoFIssueIndentifyCommand(psFabController,
                                    NVME_IDENTIFY_CNS_CTRL,
                                    0,
                                    &psFabController->sNVMoFControllerId,
                                    sizeof(psFabController->sNVMoFControllerId));
	if (NT_SUCCESS(Status)) {
		if (psFabController->ControllerType == FABRIC_CONTROLLER_TYPE_CONNECT) {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Connect Controller %hu\n", __FUNCTION__, __LINE__,
				psFabController->sCommonControllerInfo.usControllerID);
			psFabController->usControllerID = psFabController->sNVMoFControllerId.usCtrlrID;
		}
	}

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Identify Controller Request returns status = 0x%x\n", __FUNCTION__, __LINE__, Status);

	return Status;
}

NTSTATUS
NVMeoFIndentifyGetActiveNSList(__in PNVMEOF_FABRIC_CONTROLLER_CONNECT psFabController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	Status =
		NVMeoFIssueIndentifyCommand((PNVMEOF_FABRIC_CONTROLLER)psFabController,
                                    NVME_IDENTIFY_CNS_NS_ACTIVE_LIST,
                                    0,
                                    &psFabController->ulaValidNameSpaces[0],
                                    sizeof(ULONG));
	if (NT_SUCCESS(Status)) {
		psFabController->ulCurrentNamespaceID = psFabController->ulaValidNameSpaces[0];
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Namespace ID = %u\n", __FUNCTION__, __LINE__, psFabController->psConnectControllerCtx->ulCurrentNamespaceID);
	}
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:NVMeoFSyncIssueIndentifyCommand returns status = 0x%x\n", __FUNCTION__, __LINE__, Status);
	return Status;
}

NTSTATUS
NVMeoFIndentifyNamespace(__in PNVMEOF_FABRIC_CONTROLLER_CONNECT psFabController, __in ULONG nsID)
{
	NTSTATUS Status = STATUS_SUCCESS;
	Status =
		NVMeoFIssueIndentifyCommand((PNVMEOF_FABRIC_CONTROLLER)psFabController,
                                    NVME_IDENTIFY_CNS_NS,
                                    nsID,
                                    &psFabController->sNVMEoFNameSpaceId,
                                    sizeof(NVME_IDENTYFY_NAMESPACE));
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "NVMeoFSyncIssueIndentifyCommand returns status = 0x%x\n", Status);
	return Status;
}

NTSTATUS
NVMeoFWaitForControllerReady(__in PNVMEOF_FABRIC_CONTROLLER psFabController,
                             __in BOOLEAN bEnabled)
{
	ULONG ulControllerStatus = 0;
	ULONG ulEnabledBit = bEnabled ? NVME_CSTS_RDY : 0;
	NTSTATUS Status = STATUS_SUCCESS;
	KEVENT sEvent = { 0 };
	LARGE_INTEGER sliWaitTime = { 0 };
	ULONG Counter = 3;
	sliWaitTime.QuadPart = Int32x32To64(NVMEOF_GET_CAP_READY_TIMEOUT(psFabController->ullControllerCapability) * 500, -10);

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	KeInitializeEvent(&sEvent, NotificationEvent, FALSE);

	while (Counter) {
		Status = NVMeoFRegisterRead32(psFabController, NVME_PROPERTY_OFFSET_CSTS, &ulControllerStatus);
		if (NT_ERROR(Status)) {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d:Failed to Read controller DWORD at 0x%x the data 0x%x with Status 0x%x\n", __FUNCTION__, __LINE__,
				           NVME_PROPERTY_OFFSET_CSTS,
				           ulControllerStatus,
				           Status);
			NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d:Failed to Read controller DWORD at 0x%x the data 0x%x with Status 0x%x\n", __FUNCTION__, __LINE__,
				           NVME_PROPERTY_OFFSET_CSTS, 
                           ulControllerStatus, 
                           Status);
			return (Status);
		}
		else {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Data at 0x%x is (0x%x)\n", __FUNCTION__, __LINE__,
				NVME_PROPERTY_OFFSET_CSTS,
				ulControllerStatus);
			if ((ulControllerStatus & NVME_CSTS_RDY) == ulEnabledBit) {
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
				return (STATUS_SUCCESS);
			}
			else {
				Status = KeWaitForSingleObject(&sEvent, Executive, KernelMode, FALSE, &sliWaitTime);
				if (NT_ERROR(Status)) {
					NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d:Failed to wait with 0x%x\n", __FUNCTION__, __LINE__, Status);
					NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d:Failed to wait with 0x%x\n", __FUNCTION__, __LINE__, Status);
					return (STATUS_UNSUCCESSFUL);
				}
			}
		}
		Counter--;
	}

	if (!Counter) {
		return (STATUS_NO_SUCH_DEVICE);
	}
	else {
		return (STATUS_SUCCESS);
	}
}

NTSTATUS
NVMeoFEnableController(__in PNVMEOF_FABRIC_CONTROLLER psFabController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UINT32 uiPageSizeShift = PAGE_SHIFT;
	UINT32 uiControllerConfig = 0;

	uiControllerConfig = NVME_CC_CSS_NVM;
	uiControllerConfig |= (uiPageSizeShift - PAGE_SHIFT) << NVME_CC_MPS_SHIFT;
	uiControllerConfig |= NVME_CC_AMS_RR | NVME_CC_SHN_NONE;
	uiControllerConfig |= NVME_CC_IOSQES | NVME_CC_IOCQES;
	uiControllerConfig |= NVME_CC_ENABLE;

	Status = NVMeoFRegisterWrite32(psFabController, NVME_PROPERTY_OFFSET_CC, uiControllerConfig);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Failed to write DWORD at 0x%x the data 0x%x with Status 0x%x!\n", __FUNCTION__, __LINE__,
			NVME_PROPERTY_OFFSET_CC, uiControllerConfig, Status);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d:Failed to write DWORD at 0x%x the data 0x%x with Status 0x%x!\n", __FUNCTION__, __LINE__,
			NVME_PROPERTY_OFFSET_CC, uiControllerConfig, Status);
	}
	else {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d:Wrote controller DWORD at 0x%x the data 0x%x!\n", __FUNCTION__, __LINE__,
			NVME_PROPERTY_OFFSET_CC, uiControllerConfig);
	}

	Status = NVMeoFWaitForControllerReady(psFabController, TRUE);
	if (NT_ERROR(Status)) {
		return Status;
	}
	return  (STATUS_SUCCESS);
}

NTSTATUS
NVMeoFAsyncEventRequest(__in PNVMEOF_FABRIC_CONTROLLER psFabController)
{
	NTSTATUS  Status = STATUS_SUCCESS;
	PNVME_CMD psNvmeCmd = NULL;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;

	psAdminReqResp = 
		NVMeoFAdminRequestResponseAllocate(psFabController,
		                                   0,
		                                   0);
	if (!psAdminReqResp) {
		return STATUS_NO_MEMORY;
	}

	psNvmeCmd = psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psFabController, psAdminReqResp);

	psNvmeCmd->sGeneric.sCDW0.ucOpCode = NVME_ADMIN_OPCODE_ASYNC_EVENT;
	psNvmeCmd->sGeneric.sCDW0.usCommandID = NVME_ASYNC_EVENT_COMMAND_ID;
	psNvmeCmd->sGeneric.sCDW0.sFlags.sOptions.ucRprOrSglForXfer |= NVME_CMD_SGL_METABUF;

	NVMeoFSetSGNull(psFabController, psNvmeCmd);

	psAdminReqResp->ulFlags |= NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE_SYNC;

	Status =
		psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueSubmitRequest(psFabController->sSession.psAdminQueue, psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Sent %#x\n", __FUNCTION__, __LINE__, Status);
	}

	NVMeoFAdminRequestResponseFree(psFabController, psAdminReqResp);

	return Status;
}

static
VOID
NVMeoFPrintANADescriptors(PNVME_ANA_ACCESS_LOG_PAGE psNVMeNSANALogPage)
{
	PNVME_ANA_GROUP_DESCRIPTOR psANADesc = (PNVME_ANA_GROUP_DESCRIPTOR)psNVMeNSANALogPage->ucaANAGroupDescriptor;
	ULONG ulNSIdCount = 0;
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d:ANA Descriptor ChangeCount =%llx and Group desc Count = %hu\n", __FUNCTION__, __LINE__,
                   psNVMeNSANALogPage->usANAGroupDescCount,
                   psNVMeNSANALogPage->ullChageCount);

	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "%s:%d:Get ANA Descriptor: Group ID = %#x ChangeCount = %#llx, NSIDs=%d, NS State=%hx ", __FUNCTION__, __LINE__,
		psANADesc->ulGroupID,
		psANADesc->ullChangeCount,
		psANADesc->ulNSIds,
		psANADesc->ucState);
	while (ulNSIdCount < psANADesc->ulNSIds) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "NSID:%lu\n", psANADesc->ulaNSIds[ulNSIdCount]);
		ulNSIdCount++;
	}
	NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_INFORMATION, "\n");
}

NTSTATUS
NVMeoFGetANADescriptorLogPage(__in PNVMEOF_FABRIC_CONTROLLER_CONNECT psFabController)
{
	NTSTATUS                  Status = STATUS_SUCCESS;
	PVOID                     pvNSANADataBuffer = NULL;
	ULONG                     ulANADataLen = sizeof(NVME_ANA_ACCESS_LOG_PAGE) +
		(sizeof(NVME_ANA_GROUP_DESCRIPTOR) * psFabController->sNVMoFControllerId.ulNumANAGrpId) +
		psFabController->sNVMoFControllerId.ulMaxNumAllowedNS * sizeof(ULONG);

	if (ulANADataLen < NVMEOF_MAX_ADMIN_LOG_PAGE_DATA_LENGTH) {
		ulANADataLen = NVMEOF_MAX_ADMIN_LOG_PAGE_DATA_LENGTH;
	}

	pvNSANADataBuffer = NVMeoFCmdAllocateNpp(ulANADataLen);
	if (!pvNSANADataBuffer) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = NVMeoFGetLogPage((PNVMEOF_FABRIC_CONTROLLER)psFabController,
		                      NVME_LOG_ANA,
		                      0,
		                      0,
		                      pvNSANADataBuffer,
		                      ulANADataLen,
		                      0);
	if (!NT_SUCCESS(Status)) {
		NVMeoFCmdFreeNpp(pvNSANADataBuffer);
		return Status;
	}

	if (psFabController->psNVMeoFNSANALogPage) {
		RtlCopyMemory(psFabController->psNVMeoFNSANALogPage, pvNSANADataBuffer, ulANADataLen);
		NVMeoFCmdFreeNpp(pvNSANADataBuffer);
	} else {
		psFabController->psNVMeoFNSANALogPage = pvNSANADataBuffer;
	}

	NVMeoFPrintANADescriptors(psFabController->psNVMeoFNSANALogPage);

	return STATUS_SUCCESS;
}

NTSTATUS
NVMeoFAsyncIssueKeepAlive(__in PNVMEOF_FABRIC_CONTROLLER psFabController)
{
	NTSTATUS  Status = STATUS_SUCCESS;
	PNVME_CMD psNvmeCmd = NULL;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;

	if ((psFabController->ulControllerState == NVMEOF_CONTROLLER_STATE_DISCONNECTING) || 
		(psFabController->ulControllerState == NVMEOF_CONTROLLER_STATE_DISCONNECTED)) {
		NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_ERROR, "%s:%d: Disconnect is issued!\n", __FUNCTION__, __LINE__);
		NVMeoFLogEvent(NVMEOF_EVENT_LOG_LEVEL_ERROR, "%s:%d: Disconnect is issued!\n", __FUNCTION__, __LINE__);
		return STATUS_CONNECTION_DISCONNECTED;
	}

	psAdminReqResp = NVMeoFAdminRequestResponseAllocate(psFabController,
		                                                0,
		                                                0);
	if (!psAdminReqResp) {
		return STATUS_NO_MEMORY;
	}

	psNvmeCmd =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psFabController, psAdminReqResp);

	psNvmeCmd->sKeepAlive.ucOpCode = NVME_ADMIN_OPCODE_KEEP_ALIVE;
	psNvmeCmd->sKeepAlive.usCommandId = psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueGetCmdID(psFabController->sSession.psAdminQueue);
	psNvmeCmd->sKeepAlive.sFlags.sOptions.ucRprOrSglForXfer |= NVME_CMD_SGL_METABUF;

	NVMeoFSetSGNull(psFabController, psNvmeCmd);

	psAdminReqResp->ulFlags |= NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE_SYNC;

	Status =
		psFabController->sSession.psAdminQueue->sQueueOps._NVMeoFQueueSubmitRequest(psFabController->sSession.psAdminQueue,
			                                                                        psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psFabController,
				                                                             psAdminReqResp);
		//NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (psCqe->sStatus.sStatusField.SC) {
			Status = NVMeoFGetWinError(psCqe);
		}
	}

	NVMeoFAdminRequestResponseFree(psFabController, psAdminReqResp);

	return (Status);
}

static
NTSTATUS
PdsConvertStringToInteger(__in PCHAR string,
                          __in ULONG Base,
                          __in PULONG retInt)
{
	NTSTATUS Status = STATUS_SUCCESS;
	ANSI_STRING sAnsiStr = { 0 };
	UNICODE_STRING sUnicodeStr = { 0 };

	RtlInitAnsiString(&sAnsiStr, string);

	Status = RtlAnsiStringToUnicodeString(&sUnicodeStr, &sAnsiStr, TRUE);
	if (NT_SUCCESS(Status)) {
		Status = RtlUnicodeStringToInteger(&sUnicodeStr, Base, retInt);
		RtlFreeUnicodeString(&sUnicodeStr);
	}

	return Status;
}

NTSTATUS
NVMeoFGetDiscoveryLogPage(__in PNVMEOF_FABRIC_CONTROLLER psFabController,
                          __in PVOID pvUserDataBuffer,
                          __in ULONG ulDataToRead)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_DISCOVERY_RESP_PAGE_HEADER psDiscRespData;
	PVOID pvDiscDataBuffer = NULL;

	pvDiscDataBuffer = NVMeoFCmdAllocateNpp(ulDataToRead);
	if (!pvDiscDataBuffer) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(pvDiscDataBuffer, ulDataToRead);

	Status = NVMeoFGetLogPage(psFabController,
		                      NVME_LOG_DISC,
		                      0,
		                      0,
		                      pvDiscDataBuffer,
		                      ulDataToRead,
		                      0);
	if (NT_ERROR(Status)) {
		return Status;
	}

	psDiscRespData = pvDiscDataBuffer;
	if (psDiscRespData->ullRecordCount) {
		PNVMEOF_USER_DISCOVERY_RESPONSE psUserDiscData = pvUserDataBuffer;
		if (ulDataToRead <= NVMEOF_NUM_LOG_PAGE_ENTRIES_READ_BYTES) {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Data len = %d Genrec %llx NumRec %llx\n", __FUNCTION__, __LINE__,
				ulDataToRead, psDiscRespData->ullGenerationCounter, psDiscRespData->ullRecordCount);
			psUserDiscData->NumEntries = psDiscRespData->ullRecordCount;
			Status = STATUS_SUCCESS;
		} else {
			NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "%s:%d:Data len = %d Genrec %llx NumRec %llx\n", __FUNCTION__, __LINE__,
		                   ulDataToRead, psDiscRespData->ullGenerationCounter, psDiscRespData->ullRecordCount);
			PNVMEOF_USER_DISCOVERY_RESPONSE_ENTRY psUserDiscEnt = psUserDiscData->Entries;
			PNVMEOF_DISCOVERY_RESP_PAGE_ENTRY  psDiscPgEnt = psDiscRespData->sDiscoveryLogEntries;
			psUserDiscData->NumEntries = psDiscRespData->ullRecordCount;
			for (ULONG ulRecCnt = 0; ulRecCnt < (ULONG)psDiscRespData->ullRecordCount; ulRecCnt++, psUserDiscEnt++, psDiscPgEnt++) {
				ULONG port = 0;
				PCHAR pcTermenatingchar = NULL;
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "Port ID:%d\n", psDiscPgEnt->usPortID);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "Address family:%d\n", psDiscPgEnt->ucAddressFamily);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "nqn type:%d\n", psDiscPgEnt->ucSubsystemType);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "transport:%d\n", psDiscPgEnt->ucXportType);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "traddr:%s\n", psDiscPgEnt->caXportAddress);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "Port:%s\n", psDiscPgEnt->sXportSpecificSubtype);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "NQN:%s\n", psDiscPgEnt->caNVMSubSystemNQN);
				Status = PdsConvertStringToInteger(psDiscPgEnt->caXportServiceIdentifier, 10, &port);
				NVMeoFDebugLog(NVMEOF_DEBUG_LOG_LEVEL_DEBUG, "Port:%d\n", (USHORT)port);
				psUserDiscEnt->portid = psDiscPgEnt->usPortID;
				psUserDiscEnt->addrfamily = psDiscPgEnt->ucAddressFamily;
				psUserDiscEnt->nqntype = psDiscPgEnt->ucSubsystemType;
				psUserDiscEnt->transport = psDiscPgEnt->ucXportType;
				Status = RtlIpv4StringToAddressA(psDiscPgEnt->caXportAddress, TRUE, &pcTermenatingchar, &psUserDiscEnt->traddr.sin_addr);
				psUserDiscEnt->traddr.sin_port = (USHORT)port;
				memcpy(psUserDiscEnt->subnqn, psDiscPgEnt->caNVMSubSystemNQN, NVMF_NQN_FIELD_LEN);
			}
			Status = STATUS_SUCCESS;
		}
	}

	NVMeoFCmdFreeNpp(pvDiscDataBuffer);

	return Status;
}
