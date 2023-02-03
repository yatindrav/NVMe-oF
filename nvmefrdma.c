#include "nvme.h"
#include "nvmef.h"
#include "xport_wrq.h"
#include "fabric_api.h"
#include "nvmefrdma.h"
#include "errdbg.h"
#include "utils.h"

#include "trace.h"
#include "nvmeof_cmd.tmh"
/****************************** Module Header Starts *********************************************************************************
* Module Name:  NVMeoRDMA.c
* Project:      Pavilion Virtual Bus Driver part of NVMeOF Solution
* Writer:       Yatindra Vaishanv
* Description:  This module takes care of following NVMe over TCP Protocol operations:
*               1. Creating Admin queue for discovery log page inquery.
*               2. Creating Admin queue and IO or submission queues for IO with the target.
*               3. Managing the Admin and IO queues
*               4. Allocating the Protocol specific data and information
* Copyright (c) 2019 - 2020 Paviion Data Systems Inc.
*
*
****************************** Module Header Ends***********************************************************************************/

#include "trace.h"
//#include "NVMeofRDMA.tmh"


//size_t wcstombs(char* dest, const wchar_t* src, size_t max);
static PNVMEOF_RDMA_GLOBAL gpsRdmaControllerList = NULL;


static
USHORT
NVMeoFRdmaGetAdminCommandID(__in PNVMEOF_FABRIC_CONTROLLER psController);

static
PNVMEOF_QUEUE
NVMeoFRdmaGetIOQFromQId(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId);

static
NTSTATUS
NVMeoFRdmaNvmeofXportSetSG(__in PNVMEOF_FABRIC_CONTROLLER       psController,
                           __in ULONG                           ulQId,
                           __inout PNVME_CMD                    psNvmeCmd,
                           __in ULONGLONG                       ullAddress,
                           __in ULONG                           Length,
                           __in UINT32                          uiRemoteMemoryToken,
                           __in NVMEOF_COMMAND_DESCRIPTORT_TYPE ulCmdDesc);


static
NTSTATUS
NVMeoFRdmaCreateAsyncSocket(__in PNVMEOF_FABRIC_CONTROLLER psController,
                            __in PSOCKADDR_INET            psDestAddr,
                            __in PSOCKADDR_INET            psLocalAddress,
                            __in PPDS_NDK_CALLBACKS        psDispatcher,
                            __inout PNDK_SOCKET*           ppsConnectedSocket)
{
	NTSTATUS Status = STATUS_SUCCESS;

	Status = 
		NVMeoFUtilsGetIfIndexAndLocalIpAddress(psDestAddr,
                                               psLocalAddress,
                                               &psController->sIpRoute,
                                               &psController->ulInterfaceIndex);

	Status = NdkCreateRdmaSocket(psLocalAddress,
                                 psDestAddr,
                                 psController->ulInterfaceIndex,
                                 0,
                                 psDispatcher,
                                 ppsConnectedSocket);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to get create RDMA socket with status 0x%X!\n", __FUNCTION__, __LINE__,
			Status);
		return (Status);
	}

	return (Status);
}

static
NTSTATUS
NVMeoFRdmaCreateAndConnectQP(__in PNVMEOF_FABRIC_CONTROLLER psController,
                             __in PNDK_QP_CBS               psSQDispatcher,
                             __in PNVMEOF_RDMA_PRIVATE_DATA psQueueInfo,
                             __out PNDK_QUEUE_PAIR_BUNDLE   psConnectedQP)
{
	NTSTATUS Status = STATUS_SUCCESS;
	RtlZeroMemory(psConnectedQP, sizeof(*psConnectedQP));
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:In %#p %#p %#p %#p!\n", __FUNCTION__, __LINE__,
                   psController,
                   psSQDispatcher,
                   psQueueInfo,
                   psConnectedQP);

	Status = 
		NdkCreateRdmaQPair((PNDK_SOCKET)psController->sSession.pvSessionFabricCtx,
                           psSQDispatcher,
                           psQueueInfo->usHrqsize,
                           psQueueInfo->usHsqsize,
                           psConnectedQP);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to create RDMA QP with status 0x%X!\n", __FUNCTION__, __LINE__,
			Status);
		return (Status);
	}

	Status = 
		NdkConnectRdmaQPair((PNDK_SOCKET)psController->sSession.pvSessionFabricCtx,
                            psConnectedQP,
                            0,
                            psQueueInfo,
                            sizeof(*psQueueInfo),
                            FALSE);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to connect RDMA QP with status 0x%X!\n", __FUNCTION__, __LINE__,
			Status);
		return (Status);
	}

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out 0x%X!\n", __FUNCTION__, __LINE__, Status);
	return (Status);
}

static
VOID
NVMeoFRdmaInitializeAdminQPMrReqCompletion(_In_ PNVMEOF_RDMA_WORK_REQUEST psCreateMr,
                                           _In_ NTSTATUS Status)
{
	psCreateMr->Status = psCreateMr->sCompletionCtx.Status = Status;
	KeSetEvent(&psCreateMr->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
}

VOID
NVMeoFRdmaInitializeIOQPMrReqCompletion(_In_ PNVMEOF_RDMA_WORK_REQUEST psCreateMr,
                                        _In_ NTSTATUS Status)
{
	psCreateMr->Status = psCreateMr->sCompletionCtx.Status = Status;
	KeSetEvent(&psCreateMr->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
}

static
PNVMEOF_RDMA_WORK_REQUEST
NVMeoFRdmaGetFreeSendWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	                             __in ULONG              ulQId)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;
	ULONG ulMaxWorkReq = 0, ulReqCount = 0;
	PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulQId);

	psRdmaIoReq = psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList;
	ulMaxWorkReq = psSQ->ulQueueDepth;

	while (ulReqCount < ulMaxWorkReq) {
		if (_InterlockedCompareExchange(&psRdmaIoReq->lReqState,
			                            NVMEOF_WORK_REQUEST_STATE_ALLOCATED,
			                            NVMEOF_WORK_REQUEST_STATE_FREE) == NVMEOF_WORK_REQUEST_STATE_FREE) {
			return psRdmaIoReq;
		}
		ulReqCount++;
		psRdmaIoReq++;
	}

	return NULL;
}

static
PNVMEOF_RDMA_WORK_REQUEST
NVMeoFRdmaAllocateAndInitializeSendWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	                                           __in ULONG              ulQId,
	                                           __in PNVME_CMD          psNVMeCmd,
	                                           __in ULONG              ulCmdLen,
	                                           __in PVOID              pvDataBuffer,
	                                           __in ULONG              ulDataLen)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq =
		NVMeoFRdmaGetFreeSendWorkRequest(psController, ulQId);
	if (psRdmaIoReq) {
		if (ulQId == NVME_ADMIN_QUEUE_ID) {
			psRdmaIoReq->psDataBufferMdlMap->pvBuffer = pvDataBuffer;
			psRdmaIoReq->psDataBufferMdlMap->ulBufferLength = ulDataLen;
			psRdmaIoReq->psDataBufferMdlMap->psBufferMdl = NULL;
			psRdmaIoReq->psBufferMdlMap->pvBuffer = psNVMeCmd;
			psRdmaIoReq->psBufferMdlMap->ulBufferLength = ulCmdLen;
			psRdmaIoReq->psBufferMdlMap->psBufferMdl = NULL;
		} else {
			psRdmaIoReq->psDataBufferMdlMap->pvBuffer = pvDataBuffer;
			psRdmaIoReq->psDataBufferMdlMap->ulBufferLength = ulDataLen;
			psRdmaIoReq->psDataBufferMdlMap->psBufferMdl = NULL;
			*(PNVME_CMD)psRdmaIoReq->psBufferMdlMap->pvBuffer = *(PNVME_CMD)psNVMeCmd;
		}
	}

	return (psRdmaIoReq);
}

static
PNVMEOF_RDMA_WORK_REQUEST
NVMeoFRdmaGetFreeReceiveWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                    __in ULONG              ulQId)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;
	ULONG ulWorkReqCount = 0;

	PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulQId);
	psRdmaIoReq = psSQ->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList;
	ulWorkReqCount = psSQ->ulQueueDepth;

	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Found Send Q %p WorkReqCount %u!\n", __FUNCTION__, __LINE__,
		psRdmaIoReq,
		ulWorkReqCount);

	while (ulWorkReqCount) {
		if (_InterlockedCompareExchange(&psRdmaIoReq[(ulWorkReqCount - 1)].lReqState,
			                            PDS_NVMEOF_RESPONSE_STATE_SUBMITTED,
			                            PDS_NVMEOF_RESPONSE_STATE_FREE) == PDS_NVMEOF_RESPONSE_STATE_FREE) {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Found Send WorkReq %p!\n", __FUNCTION__, __LINE__,
				&psRdmaIoReq[(ulWorkReqCount - 1)]);
			return &psRdmaIoReq[(ulWorkReqCount - 1)];
		}
		ulWorkReqCount--;
	}

	return NULL;
}

static
PNVMEOF_RDMA_WORK_REQUEST
NVMeoFRdmaAdminFindSendWorkRequest(_In_ PNVMEOF_QUEUE psAdminQ,
                                   _In_ USHORT usCmdId)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = psAdminQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList;
	while (psRdmaIoReq < (psAdminQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList + psAdminQ->ulWorkRequestCount)) {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Current Req %p Command ID %hx Request State %d!\n", __FUNCTION__, __LINE__,
		               psRdmaIoReq,
		               psRdmaIoReq->usCommandID,
		               psRdmaIoReq->lReqState);
		if (psRdmaIoReq->usCommandID == usCmdId) {
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Found Send Req Out!\n", __FUNCTION__, __LINE__);
			return psRdmaIoReq;
		}
		psRdmaIoReq++;
	}
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to find Sent Req for CmdID %hu!\n", __FUNCTION__, __LINE__, usCmdId);
	return NULL;
}

static
NTSTATUS
NVMeoFRdmaSubmitReceiveWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                   __in ULONG ulQId,
                                   __in PNVMEOF_RDMA_WORK_REQUEST psWorkReq);
#ifdef NVMEOF_DBG
VOID
NVMeoFUtilsPrintHexBuffer(ULONG  ulLoglevel,
                          PUCHAR pucBuffer,
                          ULONG  ulBufSize,
                          ULONG  ulPrintCntPerLine);
#else
#define NVMeoFRdmaPrintHexBuffer(ulLoglevel, pucBuffer, ulBufSize, ulPrintCntPerLine)
#endif

static
NTSTATUS
NVMeoFRdmaAdminHandleRcqRcvReq(_In_ PNVMEOF_QUEUE psAdminQ,
                               _In_ PNDK_WORK_REQUEST psNdkWkReq,
                               _In_ NTSTATUS Status,
                               _In_ ULONG ulByteXferred)
{
	PNVMEOF_RDMA_WORK_REQUEST psRcvWorkReq = NULL;
	BOOLEAN                   bResubmitReceive = FALSE;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In AdminQ 0x%p WrkReq 0x%p Status 0x%x Bytes Xferred %u WkReq type %u!\n",
		           __FUNCTION__,
		           __LINE__,
		           psAdminQ,
		           psNdkWkReq,
		           Status,
		           ulByteXferred,
		           psNdkWkReq->ulType);

	if (psNdkWkReq->ulType == NDK_WREQ_SQ_RECEIVE) {
		PNVMEOF_RDMA_WORK_REQUEST psSndWkReq = NULL;
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Handling Receive Status 0x%x Xferred 0x%x!\n",
			           __FUNCTION__,
			           __LINE__,
			           Status,
			           ulByteXferred);
		psRcvWorkReq = psNdkWkReq->pvCallbackCtx;
		psRcvWorkReq->Status = Status;
		psRcvWorkReq->ulByteXferred = ulByteXferred;
		if (NT_SUCCESS(Status)) {
			if (ulByteXferred >= sizeof(NVME_COMPLETION_QUEUE_ENTRY)) {
				NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_VERBOSE,
					psRcvWorkReq->psBufferMdlMap->pvBuffer,
					ulByteXferred,
					PDS_PRINT_HEX_PER_LINE_8_BYTES);
				psSndWkReq =
					NVMeoFRdmaAdminFindSendWorkRequest(psAdminQ,
						((PNVME_COMPLETION_QUEUE_ENTRY)psRcvWorkReq->psBufferMdlMap->pvBuffer)->usCommandID);
				if (psSndWkReq) {
					if (psSndWkReq->fNVMeReqCompletion) {
						psSndWkReq->fNVMeReqCompletion(psSndWkReq, psRcvWorkReq, ulByteXferred, Status);
					}
					bResubmitReceive = TRUE;
					Status = STATUS_SUCCESS;
				} else {
					NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received Completion for Invalid Command(0x%hu) Received Bytes %u!\n",
						__FUNCTION__,
						__LINE__,
						((PNVME_COMPLETION_QUEUE_ENTRY)psRcvWorkReq->psBufferMdlMap->pvBuffer)->usCommandID,
						ulByteXferred);
					NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_VERBOSE,
						psRcvWorkReq->psBufferMdlMap->pvBuffer,
						ulByteXferred,
						PDS_PRINT_HEX_PER_LINE_8_BYTES);
					bResubmitReceive = TRUE;
					Status = STATUS_INVALID_PARAMETER;
				}
			} else {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Invalid size (%u) of buffer Received!\n",
					__FUNCTION__,
					__LINE__,
					ulByteXferred);
				bResubmitReceive = TRUE;
				Status = STATUS_INVALID_PARAMETER;
			}
		} else {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Receive Completed with Error (0x%X)!\n",
				__FUNCTION__,
				__LINE__,
				Status);
			bResubmitReceive = FALSE;
		}
	} else {
		ASSERT(0);
	}

	if (bResubmitReceive) {
		Status =
			NVMeoFRdmaSubmitReceiveWorkRequest(psAdminQ->psParentController,
				                               NVME_ADMIN_QUEUE_ID,
				                               psRcvWorkReq);
		if (NT_SUCCESS(Status)) {
			_InterlockedIncrement(&psAdminQ->psFabricCtx->psRdmaCtx->lReceiveWorkRequestOutstanding);
			_InterlockedIncrement64(&psAdminQ->psFabricCtx->psRdmaCtx->llReceiveWorkRequestSubmitted);
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Resubmitted Receive for Admin Q req(%d)!\n",
				__FUNCTION__,
				__LINE__,
				psAdminQ->lReceiveWorkReqSubmitted);
		} else {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Not Resubmitting Receive for Admin Q req(%d)!\n",
				__FUNCTION__,
				__LINE__,
				psAdminQ->lReceiveWorkReqSubmitted);
		}
	} else {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Not Resubmitting Receive for Admin Q req(%d)!\n",
		               __FUNCTION__,
		               __LINE__,
		               psAdminQ->lReceiveWorkReqSubmitted);
	}

	return (Status);
}

static
VOID
NVMeoFRdmaAdminRcqCallback(_In_ PNVMEOF_QUEUE psAdminQ,
                           _In_ NTSTATUS CqStatus)
{
	ULONG ulResultsCount = 0;
	PNDK_WORK_REQUEST psNdkWkReq = NULL;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	if (NT_ERROR(CqStatus)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Handle this case!!!!!\n", __FUNCTION__, __LINE__);
	}
	psNdkWkReq = psAdminQ->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq;
	psNdkWkReq->ulType = NDK_WREQ_SQ_GET_RECEIVE_RESULTS;
	psNdkWkReq->sGetRcqResultReq.pvNdkRcqResultExx = psAdminQ->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults;
	psNdkWkReq->sGetRcqResultReq.usMaxNumReceiveResults = psAdminQ->psFabricCtx->psRdmaCtx->lReceiveResultsCount;
	psNdkWkReq->sGetRcqResultReq.usReceiveResultEx = FALSE;
	while (1) {
		ulResultsCount =
			NdkSubmitRequest(psAdminQ->psParentController->sSession.pvSessionFabricCtx,
                             &psAdminQ->psFabricCtx->psRdmaCtx->psNdkQP,
                             psNdkWkReq);
		if (!ulResultsCount) {
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:No results returned!!!!!\n", __FUNCTION__, __LINE__);
			break;
		}
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Results returned %u!!!!!\n", __FUNCTION__, __LINE__, ulResultsCount);
		PNDK_RESULT psResult = NULL;
		for (ULONG i = 0; i < ulResultsCount; i++) {
			_InterlockedDecrement(&psAdminQ->psFabricCtx->psRdmaCtx->lReceiveWorkRequestOutstanding);
			_InterlockedIncrement64(&psAdminQ->psFabricCtx->psRdmaCtx->llReceiveWorkRequestCompleted);
			if (psNdkWkReq->sGetRcqResultReq.usReceiveResultEx) {
				PNDK_RESULT_EX  psResultEx = &((PNDK_RESULT_EX)psAdminQ->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults)[i];
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: Resultx QPCtx 0x%llx ReqCtx 0x%llx Xferred %u Status 0x%x type %u type specific %llx!\n", __FUNCTION__, __LINE__,
					           psResultEx->QPContext,
					           psResultEx->RequestContext,
					           psResultEx->BytesTransferred,
					           psResultEx->Status,
					           psResultEx->Type,
					           psResultEx->TypeSpecificCompletionOutput);
				psResult = (PNDK_RESULT)psResultEx;
			} else {
				psResult = &((PNDK_RESULT)psAdminQ->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults)[i];
			}
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Req status %#x!!!!!\n", __FUNCTION__, __LINE__, psResult->Status);
			if (psResult->Status != STATUS_CANCELLED) {
				NTSTATUS Status = STATUS_SUCCESS;
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Issuing return work request handling for type %u!!!!!\n",
					           __FUNCTION__,
					           __LINE__,
					           ((PNDK_WORK_REQUEST)psResult->RequestContext)->ulType);
				Status = 
					NVMeoFRdmaAdminHandleRcqRcvReq(psAdminQ,
					                               psResult->RequestContext,
					                               psResult->Status,
					                               psResult->BytesTransferred);
			}
			else {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Receive Request Cancelled Disconnect Status %lu!\n", __FUNCTION__, __LINE__,
					psAdminQ->psParentController->ulDisconnectFlag);
			}
		}
		ulResultsCount = 0;
	}

	if (psAdminQ->psFabricCtx->psRdmaCtx->lReceiveWorkRequestOutstanding) {
		NdkArmCompletionQueue(psAdminQ->psParentController->sSession.pvSessionFabricCtx,
                              psAdminQ->psFabricCtx->psRdmaCtx->psNdkQP->psNdkRcq,
                              NDK_CQ_NOTIFY_ANY);
	}
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

static
NTSTATUS
NVMeoFRdmaAdminHandleSqReturnWorkRequest(_In_ PNVMEOF_QUEUE psAdminQ,
                                         _In_ PNDK_WORK_REQUEST psNdkWkReq,
                                         _In_ NTSTATUS Status)
{
	PNVMEOF_RDMA_WORK_REQUEST psWorkReq = NULL;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In AdminQ 0x%p WrkReq 0x%p Status 0x%x WkReq type %s!\n",
		           __FUNCTION__,
		           __LINE__,
		           psAdminQ,
		           psNdkWkReq,
		           Status,
		           pcaWorkRequestString[psNdkWkReq->ulType]);
	switch (psNdkWkReq->ulType) {
	case NDK_WREQ_SQ_SEND:
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Handling Send Status 0x%x!\n",
			__FUNCTION__,
			__LINE__,
			Status);
		psWorkReq = psNdkWkReq->pvCallbackCtx;
		if (psWorkReq->fSendReqCompletion) {
			psWorkReq->fSendReqCompletion(psWorkReq, Status);
		}
		break;
	case NDK_WREQ_SQ_FAST_REGISTER:
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Handling Fast Register Status 0x%x!\n",
                       __FUNCTION__,
                       __LINE__,
                       Status);
		psWorkReq = psNdkWkReq->pvCallbackCtx;
		if (psWorkReq->ulType == PDS_NVMEOF_REQUEST_TYPE_DATA) {
			KeSetEvent(&psWorkReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
		} else if (psWorkReq->ulType == PDS_NVMEOF_REQUEST_TYPE_RECEIVE) {
			_InterlockedDecrement64(&psAdminQ->l64FRMRCurrentCount);
			KeSetEvent(&psWorkReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
		}
		break;
	case NDK_WREQ_SQ_INVALIDATE:
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Handling Invalidate Status 0x%x!\n",
                       __FUNCTION__,
                       __LINE__,
                       Status);
		psWorkReq = psNdkWkReq->pvCallbackCtx;
		break;
	case NDK_WREQ_SQ_SEND_INVALIDATE:
	case NDK_WREQ_SQ_READ:
	case NDK_WREQ_SQ_WRITE:
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Unsupported Request (%s) Completion Status 0x%x!\n",
                       __FUNCTION__,
                       __LINE__,
                       pcaWorkRequestString[psNdkWkReq->ulType],
                       Status);
		ASSERT(0);
		break;
	default:
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Handling Invalidate Request (%u)!\n",
                       __FUNCTION__,
                       __LINE__,
                       psNdkWkReq->ulType);
		break;
	}
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaAdminScqCallback(_In_ PNVMEOF_QUEUE psAdminQ,
                           _In_ NTSTATUS CqStatus)
{
	ULONG ulResultsCount = 0;
	PNDK_WORK_REQUEST psNdkWkReq = NULL;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	if (NT_ERROR(CqStatus)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Handle this case!!!!!\n", __FUNCTION__, __LINE__);
	}

	psNdkWkReq = psAdminQ->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq;

	RtlZeroMemory(psNdkWkReq, sizeof(*psNdkWkReq));
	psNdkWkReq->ulType = NDK_WREQ_SQ_GET_SEND_RESULTS;
	psNdkWkReq->sGetRcqResultReq.pvNdkRcqResultExx = psAdminQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults;
	psNdkWkReq->sGetRcqResultReq.usMaxNumReceiveResults = psAdminQ->psFabricCtx->psRdmaCtx->lSubmissionResultsCount;
	psNdkWkReq->sGetRcqResultReq.usReceiveResultEx = FALSE;

	while (1) {
		ulResultsCount =
			NdkSubmitRequest(psAdminQ->psParentController->sSession.pvSessionFabricCtx,
				             psAdminQ->psFabricCtx->psRdmaCtx->psNdkQP,
                             psNdkWkReq);
		if (ulResultsCount) {
			for (ULONG i = 0; i < ulResultsCount; i++) {
				PNDK_RESULT psResult = &psAdminQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults[i];
				NTSTATUS Status = STATUS_SUCCESS;
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: Resultx QPCtx 0x%llx ReqCtx 0x%llx Xferred %u Status 0x%x!\n",
					           __FUNCTION__,
					           __LINE__,
					           psResult->QPContext,
					           psResult->RequestContext,
					           psResult->BytesTransferred,
					           psResult->Status);
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Issuing return work request handling for type %u!!!!!\n",
                               __FUNCTION__,
                               __LINE__,
                               ((PNDK_WORK_REQUEST)psResult->RequestContext)->ulType);
				Status = 
					NVMeoFRdmaAdminHandleSqReturnWorkRequest(psAdminQ,
                                                             psResult->RequestContext,
                                                             psResult->Status);
			}
			ulResultsCount = 0;
		}
		else {
			break;
		}
	}

	if (psAdminQ->psFabricCtx->psRdmaCtx->lSendWorkRequestOutstanding) {
		PdsNdkArmCompletionQueue(psAdminQ->psParentController->sSession.pvSessionFabricCtx,
                                 psAdminQ->psFabricCtx->psRdmaCtx->psNdkQP->psNdkScq,
                                 NDK_CQ_NOTIFY_ANY);
	}
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

VOID
NVMeoFRdmaAdminDisconnectQPEvent(_In_ PNVMEOF_QUEUE psAdminQueue)
{
	PNVMEOF_FABRIC_CONTROLLER psController = psAdminQueue->psFabricCtx->psRdmaCtx->psParentController;
	_InterlockedExchange((volatile LONG*)&psAdminQueue->psFabricCtx->psRdmaCtx->lFlags, ADMIN_QUEUE_DISCONNECTED);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Trying to see if disconnect WI can be scheduled 0x%x Admin Q!\n", __FUNCTION__, __LINE__,
		psController->ulDisconnectFlag);
	if (_InterlockedCompareExchange((volatile LONG*)&psController->ulDisconnectFlag, CONTROLLER_ADMIN_DISCONNECTED_EVENT, 0) == 0) {
		if (psController->sChildNotifier.fnChangeNotify) {
			psController->sChildNotifier.fnChangeNotify(psController->sChildNotifier.pvChildContext, STATUS_PORT_DISCONNECTED);
		}

		if (psController->sOsManager.psDisconnectWorkItem && psController->sOsManager.psDisconnectWIRoutine) {
			NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Scheduling disconnect WI!\n", __FUNCTION__, __LINE__);
			IoQueueWorkItem(psController->sOsManager.psDisconnectWorkItem,
				            psController->sOsManager.psDisconnectWIRoutine,
				            CriticalWorkQueue,
				            psController);
		}
	}

	NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Admin Disconnect for Controller %s 0x%x!\n", __FUNCTION__, __LINE__,
		psController->caSubsystemNQN, psController->sTargetAddress.Ipv4.sin_addr.S_un.S_addr);
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Admin Disconnect for Controller %s 0x%x!\n", __FUNCTION__, __LINE__,
		psController->caSubsystemNQN, psController->sTargetAddress.Ipv4.sin_addr.S_un.S_addr);
	return;
}

VOID
NVMeoFRdmaAdminConnectQPEvent(_In_ PNVMEOF_QUEUE psAdminQ,
                              _In_ PNDK_CONNECTOR psNdkConnector)
{
	UNREFERENCED_PARAMETER(psAdminQ);
	UNREFERENCED_PARAMETER(psNdkConnector);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

VOID
NVMeoFRdmaAdminQPRequestCompletion(_In_ PNVMEOF_QUEUE psAdminQ,
	_In_ NTSTATUS Status)
{
	UNREFERENCED_PARAMETER(psAdminQ);
	UNREFERENCED_PARAMETER(Status);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

FORCEINLINE
USHORT
NVMeoFRdmaAdminGetResponseCommandID(__in PUCHAR pucArrivedDataBuffer)
{
	PNVME_COMPLETION_QUEUE_ENTRY psRdmaNVMeCQE = (PNVME_COMPLETION_QUEUE_ENTRY)pucArrivedDataBuffer;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return  (psRdmaNVMeCQE->usCommandID);
}

static
VOID
NVMeoFRdmaHandleAsyncEvents(__in PNVMEOF_QUEUE                psAdminQ,
                            __in PNVME_COMPLETION_QUEUE_ENTRY psCQE)
{
	ULONG ulAERNoticeType = (psCQE->sCmdRespData.sCmdSpecData.ulDW0 & 0xff00) >> 8;
	BOOLEAN bFireEventHandler = TRUE;
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: AER Notice Type %#x!\n", __FUNCTION__, __LINE__, ulAERNoticeType);
	NVMeoFLogEvent(NVME_OF_INFORMATIONAL, "%s:%d: AER Notice Type %#x!\n", __FUNCTION__, __LINE__, ulAERNoticeType);
	if (!psAdminQ->psParentController->sOsManager.hEventHandlerTimer) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: AER Notice Type %#x While No Event handler!\n", __FUNCTION__, __LINE__, ulAERNoticeType);
		NVMeoFLogEvent(NVME_OF_INFORMATIONAL, "%s:%d: AER Notice Type %#x. Not ready to handle, Ignoring.!\n", __FUNCTION__, __LINE__, ulAERNoticeType);
		return;
	}

	switch (ulAERNoticeType) {
	case NVMEOF_AER_NOTIFY_NS_CHANGED:
		_interlockedbittestandset((volatile LONG*)&psAdminQ->psParentController->ulAsyncEventsArrived, PDS_NVME_AEN_BIT_POS_NSATTR);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received Event for NameSpace Attr changed!\n", __FUNCTION__, __LINE__);
		NVMeoFLogEvent(NVME_OF_INFORMATIONAL, "%s:%d:Received Event for NameSpace Attr changed!\n", __FUNCTION__, __LINE__);
		break;
	case NVMEOF_AER_NOTIFY_FW_ACT_STARTING:
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received Event for FW Activation!\n", __FUNCTION__, __LINE__);
		NVMeoFLogEvent(NVME_OF_INFORMATIONAL, "%s:%d:Received Event for FW Activation!\n", __FUNCTION__, __LINE__);
		_interlockedbittestandset((volatile LONG*)&psAdminQ->psParentController->ulAsyncEventsArrived, PDS_NVME_AEN_BIT_POS_FW_ACT);
		break;
	case NVMEOF_AER_NOTIFY_ANA:
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received Event for ANA!\n", __FUNCTION__, __LINE__);
		NVMeoFLogEvent(NVME_OF_INFORMATIONAL, "%s:%d:Received Event for ANA!\n", __FUNCTION__, __LINE__);
		_interlockedbittestandset((volatile LONG*)&psAdminQ->psParentController->ulAsyncEventsArrived, PDS_NVME_AEN_BIT_POS_ANA_CHANGE);
		break;
	case NVMEOF_AER_NOTIFY_DISC_CHANGED:
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received Event for DISCOVERY Change!\n", __FUNCTION__, __LINE__);
		NVMeoFLogEvent(NVME_OF_INFORMATIONAL, "%s:%d:Received Event for DISCOVERY Change!\n", __FUNCTION__, __LINE__);
		_interlockedbittestandset((volatile LONG*)&psAdminQ->psParentController->ulAsyncEventsArrived, PDS_NVME_AEN_BIT_POS_DISC_CHANGE);
		break;
	default:
		bFireEventHandler = FALSE;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Async Event result %#x!\n", __FUNCTION__, __LINE__, psCQE->sCmdRespData.sCmdSpecData.ulDW0);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d: Unsupported Async Event %#x!\n", __FUNCTION__, __LINE__, psCQE->sCmdRespData.sCmdSpecData.ulDW0);
		break;
	}

	if (bFireEventHandler) {
		if (psAdminQ->psParentController->sOsManager.hEventHandlerTimer) {
			WdfTimerStart(psAdminQ->psParentController->sOsManager.hEventHandlerTimer, PDS_CHANGE_ASYNC_REQUEST_TIMER_DUE_TIME);
		}
	}
}

static
NTSTATUS
NVMeoFRdmaHandleCurrentRecivedAdminResponse(__in    PNVMEOF_QUEUE psAdminQ,
	__in    PUCHAR       pucArrivedDataBuffer,
	__inout SIZE_T       szlDataRead)
{
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: RDMA Response!!!\n", __FUNCTION__, __LINE__);
	NTSTATUS Status = STATUS_INVALID_MESSAGE;
	ULONG ulAERType = 0;
	PNVME_COMPLETION_QUEUE_ENTRY psRdmaPdu = (PNVME_COMPLETION_QUEUE_ENTRY)pucArrivedDataBuffer;
	UNREFERENCED_PARAMETER(szlDataRead);

	//NVMeoFDisplayCQE(__FUNCTION__, psRdmaPdu);
	ulAERType = psRdmaPdu->sCmdRespData.sCmdSpecData.ulDW0 & 0x07;
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: AER Type %#x!\n", __FUNCTION__, __LINE__, ulAERType);
	switch (ulAERType) {
	case NVME_AER_NOTICE_EVENT:
		NVMeoFRdmaHandleAsyncEvents(psAdminQ, psRdmaPdu);
		Status = STATUS_SUCCESS;
		break;
	case NVME_AER_NOTICE_ERROR:
	case NVME_AER_NOTICE_SMART:
	case NVME_AER_NOTICE_CSS:
	case NVME_AER_NOTICE_VS:
		//		ctrl->aen_result = result;
	default:
		break;
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaAdminXportConnect(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS    Status = STATUS_SUCCESS;
	PNDK_QP_CBS psAdminQPCbs = &psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psSQCbs;
	PNVMEOF_RDMA_PRIVATE_DATA psNvmeRdmaAdminPrivData = &psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psQueuePvtData;
	psAdminQPCbs->fnConnectEventHandler = NVMeoFRdmaAdminConnectQPEvent;
	psAdminQPCbs->fnQPDisconnectEventHandler = NVMeoFRdmaAdminDisconnectQPEvent;
	psAdminQPCbs->fnQPReqCompletion = NVMeoFRdmaAdminQPRequestCompletion;
	psAdminQPCbs->fnReceiveCQNotificationEventHandler = NVMeoFRdmaAdminRcqCallback;
	psAdminQPCbs->fnSendCQNotificationEventHandler = NVMeoFRdmaAdminScqCallback;
	psAdminQPCbs->fnSrqNotificationEventHandler = NULL;
	psAdminQPCbs->pvConnectEventCtx = psAdminQPCbs->pvQPDisconnectEventCtx = 
	psAdminQPCbs->pvQPReqCompletionCtx = psAdminQPCbs->pvReceiveCQNotificationCtx = 
	psAdminQPCbs->pvSendCQNotificationCtx = psController->sSession.psAdminQueue;
	psAdminQPCbs->pvSRQNotificationCtx = NULL;

	RtlZeroMemory(psNvmeRdmaAdminPrivData, sizeof(*psNvmeRdmaAdminPrivData));
	psNvmeRdmaAdminPrivData->usHrqsize = NVME_ADMIN_QUEUE_REQUEST_COUNT;
	psNvmeRdmaAdminPrivData->usHsqsize = (NVME_ADMIN_QUEUE_REQUEST_COUNT - 1);
	psNvmeRdmaAdminPrivData->usQId = NVME_ADMIN_QUEUE_ID;
	psNvmeRdmaAdminPrivData->usRecfmt = NVMEOF_RDMA_CM_FMT_1_0;
	Status = NVMeoFRdmaCreateAndConnectQP(psController,
		                                  psAdminQPCbs,
		                                  psNvmeRdmaAdminPrivData,
		                                  &psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psNdkQP);
	return  (Status);
}

FORCEINLINE
VOID
NVMeoFRdmaGetMdlInfo(__in PMDL psMdl,
                     __in PVOID pvBaseVA,
                     __inout PULONG pulLamAllocationSize)
{
#if 0
	for (psMdlInfo->ulTotalMdlCount = 0, psMdlInfo->ulMdlTotalByteCount = 0, psMdlInfo->ulTotalPageCount = 0; psMdl; psMdl = psMdl->Next) {
		ULONG ulLamPageCount;
		ulLamPageCount = PDS_GET_MDL_PAGE_COUNT(psMdl);
		psMdlInfo->ulTotalPageCount += ulLamPageCount;
		psMdlInfo->ulCurrentLamCbSize += PDS_GET_LAM_BUFFERSIZE(ulLamPageCount);
		psMdlInfo->ulTotalAdapterPageArraySize = PDS_GET_ADAPTER_PAGE_ARRAY_SIZE(ulLamPageCount);
		psMdlInfo->ulMdlTotalByteCount += MmGetMdlByteCount(psMdl);
		psMdlInfo->ulTotalMdlCount++;
	}
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Total MDLs %u Bytes %u Page %u LAM buffer size %u\n", __FUNCTION__, __LINE__,
		psMdlInfo->ulTotalMdlCount,
		psMdlInfo->ulMdlTotalByteCount,
		psMdlInfo->ulTotalPageCount,
		psMdlInfo->ulCurrentLamCbSize);
#else
	ULONG ulMdlCount = 0;
	//psMdlInfo->ulTotalPageCount = PDS_GET_MDL_PAGE_COUNT(psMdl, pvBaseVA);
	//psMdlInfo->ulCurrentLamCbSize = PDS_GET_LAM_BUFFERSIZE(psMdlInfo->ulTotalPageCount);
	//psMdlInfo->ulTotalAdapterPageArraySize = PDS_GET_ADAPTER_PAGE_ARRAY_SIZE(psMdlInfo->ulTotalPageCount);
	//psMdlInfo->ulMdlTotalByteCount = MmGetMdlByteCount(psMdl);
	//psMdlInfo->ulTotalMdlCount++;
	*pulLamAllocationSize = PDS_GET_LAM_BUFFERSIZE(PDS_GET_MDL_PAGE_COUNT(psMdl, pvBaseVA));
	for (; psMdl; ulMdlCount++, psMdl = psMdl->Next);

	if (ulMdlCount > 1) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:# MDLs %u Bytes %u Page %u LAM buffer size %u\n", __FUNCTION__, __LINE__,
			ulMdlCount,
			MmGetMdlByteCount(psMdl),
			(MmGetMdlByteCount(psMdl) >> PAGE_SHIFT),
			*pulLamAllocationSize);
		NT_ASSERT(0);
	}


	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: Required LAM buffer size %u\n", __FUNCTION__, __LINE__, *pulLamAllocationSize);

#endif
	return;
}

static
ULONG
NVMeoFRdmaPopulateSglFromLam(__in PNDK_LOGICAL_ADDRESS_MAPPING psLam,
                             __in PNDK_SGE psNdkSgl,
                             __in UINT32 uiMemoryToken,
                             __in ULONG ulPageOffset,
                             __in ULONG ulLength)
{
	ULONG ulAdapterPageCount = 0;
	PNDK_LOGICAL_ADDRESS psNdkLm = NULL;
	ULONG ulSgeCount = 0;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	if (!psLam->AdapterPageArray || !psLam->AdapterContext) {
		return 0;
	}

	for (ulAdapterPageCount = 0, psNdkLm = psLam->AdapterPageArray;
		(ulAdapterPageCount < psLam->AdapterPageCount);
		psNdkLm++, psNdkSgl++, ulSgeCount++, ulAdapterPageCount++) {
		psNdkSgl->MemoryRegionToken = uiMemoryToken;
		psNdkSgl->LogicalAddress.QuadPart = psNdkLm->QuadPart + ulPageOffset;
		psNdkSgl->Length = min3(PAGE_SIZE, (PAGE_SIZE - ulPageOffset), ulLength);
		ulPageOffset = 0;
		ulLength -= psNdkSgl->Length;
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return ulSgeCount;
}

static
PNDK_WORK_REQUEST
NVMeoFRdmaAllocateNdkWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromNPagedLookasideList(gpsRdmaControllerList->psNdkWorkRequestLAList);
}

static
VOID
NVMeoFRdmaFreeNdkWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToNPagedLookasideList(gpsRdmaControllerList->psNdkWorkRequestLAList, pvNode);
}

static
PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL
NVMeoFRdmaAllocateBufferMdlMap(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromLookasideListEx(gpsRdmaControllerList->psBufferMdlMapLAList);
}

static
VOID
NVMeoFRdmaFreeBufferMdlMap(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToLookasideListEx(gpsRdmaControllerList->psBufferMdlMapLAList, pvNode);
}

static
PNVMEOF_REQUEST_RESPONSE
NVMeoFRdmaAllocateAdminRequest(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromLookasideListEx(gpsRdmaControllerList->psRequestLAList);
}

static
VOID
NVMeoFRdmaFreeAdminRequest(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToLookasideListEx(gpsRdmaControllerList->psRequestLAList, pvNode);
}

static
PNDK_LOGICAL_ADDRESS_MAPPING
NVMeoFRdmaAllocateLAM(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromNPagedLookasideList(gpsRdmaControllerList->psLAMLAList);
}

static
VOID
NVMeoFRdmaFreeLAM(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToNPagedLookasideList(gpsRdmaControllerList->psLAMLAList, pvNode);
}

static
PNDK_LOGICAL_ADDRESS_MAPPING
NVMeoFRdmaAllocateDataLAM(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromLookasideListEx(gpsRdmaControllerList->psDataLAMLAList);
}

static
VOID
NVMeoFRdmaFreeDataLAM(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToLookasideListEx(gpsRdmaControllerList->psDataLAMLAList, pvNode);
}


static
PNVMEOF_RDMA_SGL_LAM_MAPPING
NVMeoFRdmaAllocateLamSglMap(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromNPagedLookasideList(gpsRdmaControllerList->psLamSglLAList);
}

static
VOID
NVMeoFRdmaFreeLamSglMap(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToNPagedLookasideList(gpsRdmaControllerList->psLamSglLAList, pvNode);
}

static
PNDK_SGE
NVMeoFRdmaAllocateSGL(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromNPagedLookasideList(gpsRdmaControllerList->psSGLLAList);
}

static
VOID
NVMeoFRdmaFreeSGL(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToNPagedLookasideList(gpsRdmaControllerList->psSGLLAList, pvNode);
}

static
PNVME_CMD
NVMeoFRdmaAllocateNVMeCmd(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromLookasideListEx(gpsRdmaControllerList->psNVMeCmdLAList);
}

static
VOID
NVMeoFRdmaFreeNVMeCmd(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToLookasideListEx(gpsRdmaControllerList->psNVMeCmdLAList, pvNode);
}
static
PNVME_COMPLETION_QUEUE_ENTRY
NVMeoFRdmaAllocateNVMeCqe(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	return
		ExAllocateFromLookasideListEx(gpsRdmaControllerList->psNVMeCqeLAList);
}

static
VOID
NVMeoFRdmaFreeNVMeCqe(__in PNVMEOF_FABRIC_CONTROLLER psController, PVOID pvNode)
{
	ExFreeToLookasideListEx(gpsRdmaControllerList->psNVMeCqeLAList, pvNode);
}

static
VOID
NVMeoFRdmaDeregisterMrCompletion(PNVMEOF_WORK_REQUEST_CONTEXT psContext, NTSTATUS Status)
{
	psContext->Status = Status;
	KeSetEvent(&psContext->sEvent, IO_NO_INCREMENT, FALSE);
}

static
VOID
NVMeoFRdmaCloseMrCompletion(_In_ PNVMEOF_WORK_REQUEST_CONTEXT psContext)
{
	KeSetEvent(&psContext->sEvent, IO_NO_INCREMENT, FALSE);
}

static
NTSTATUS
NVMeoFRdmaReleaseLamSglMap(__in    PNVMEOF_FABRIC_CONTROLLER    psController,
                           __in    ULONG                        ulQId,
                           __inout PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl)
{
	UNREFERENCED_PARAMETER(ulQId);
	PNDK_WORK_REQUEST psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psNdkWorkReq) {
		return (STATUS_NO_MEMORY);
	}

	if (psLamSgl->usInitialized) {
		RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));

		psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_RELEASE_LAM;
		psNdkWorkReq->sBuildLamReq.psLam = psLamSgl->psLaml;
		PdsNdkReleaseLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);

		NVMeoFRdmaFreeNpp(psLamSgl->psLaml);
		psLamSgl->psLaml = NULL;
		psLamSgl->usInitialized = 0;
	}

	if (ulQId == NVME_ADMIN_QUEUE_ID) {
		NVMEOF_WORK_REQUEST_CONTEXT sCtx = { 0 };
		KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);
		if (psLamSgl->psNdkMr) {
			NTSTATUS Status = STATUS_SUCCESS;
			Status =
				psLamSgl->psNdkMr->Dispatch->NdkDeregisterMr(psLamSgl->psNdkMr,
					NVMeoFRdmaDeregisterMrCompletion,
					&sCtx);
			if (Status == STATUS_PENDING) {
				KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, NULL);
			}
			KeResetEvent(&sCtx.sEvent);
			psNdkWorkReq->ulType = NDK_WREQ_PD_CLOSE_MR;
			psNdkWorkReq->sCloseFRMRReq.psNdkMr = psLamSgl->psNdkMr;
			psNdkWorkReq->sCloseFRMRReq.fnCloseCompletion = NVMeoFRdmaCloseMrCompletion;
			psNdkWorkReq->sCloseFRMRReq.pvContext = &sCtx;
			Status =
				NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, 
                                 NULL,
					             psNdkWorkReq);
			if (NT_PENDING(Status)) {
				KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, NULL);
			}
		}
		psLamSgl->psNdkMr = NULL;
		psLamSgl->usInitialized = FALSE;
	}

	NVMeoFRdmaFreeNdkWorkRequest(psController, psNdkWorkReq);
	psLamSgl->pvBaseVA = NULL;
	psLamSgl->psMdl = NULL;
	psLamSgl->usSgeCount = 0;
	psLamSgl->uiRemoteMemoryTokenKey = 0;
	psLamSgl->ulCurrentLamCbSize = 0;
	return STATUS_SUCCESS;
}

VOID
NVMeoFRdmaInitMrCompletion(_In_ PNVMEOF_RDMA_WORK_REQUEST psCreateMr,
                           _In_ NTSTATUS Status)
{
	psCreateMr->Status = Status;
	KeSetEvent(&psCreateMr->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
}

static
VOID
NVMeoFRdmaCreateMrCompletion(_In_ PNVMEOF_RDMA_WORK_REQUEST psCreateMr,
                             _In_ NTSTATUS Status,
                             _In_ PNDK_OBJECT_HEADER pNdkObject)
{
	psCreateMr->Status = Status;
	if (NT_SUCCESS(Status)) {
		psCreateMr->psNdkObject = pNdkObject;
	}
	KeSetEvent(&psCreateMr->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
}

static
NTSTATUS
NVMeoFRdmaCreateAndInitializeMr(__in PNVMEOF_FABRIC_CONTROLLER       psController,
                                __inout PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl)
{
	PNVMEOF_RDMA_WORK_REQUEST psCreateMr = NULL;
	NTSTATUS                       Status = STATUS_SUCCESS;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	psCreateMr = NVMeoFRdmaAllocateNpp(sizeof(*psCreateMr));
	if (!psCreateMr) {
		return (STATUS_NO_MEMORY);
	}

	RtlZeroMemory(psCreateMr, sizeof(*psCreateMr));

	psCreateMr->psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psCreateMr->psNdkWorkReq) {
		NVMeoFRdmaFreeNpp(psCreateMr);
		return (STATUS_NO_MEMORY);
	}

	psCreateMr->ulType = NVMEOF_REQUEST_TYPE_DATA;
	psCreateMr->psNdkWorkReq->ulType = NDK_WREQ_PD_CREATE_MR;
	psCreateMr->pvParentQP = psController->sSession.psAdminQueue;
	psCreateMr->psNdkWorkReq->sCreateFRMRReq.fnCreateMrCompletionCb = NVMeoFRdmaCreateMrCompletion;
	psCreateMr->psNdkWorkReq->sCreateFRMRReq.pvCreateMrCompletionCtx = psCreateMr;
	KeInitializeEvent(&psCreateMr->sCompletionCtx.sEvent, NotificationEvent, FALSE);
	Status = NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, NULL, psCreateMr->psNdkWorkReq);
	if (NT_PENDING(Status)) {
		KeWaitForSingleObject(&psCreateMr->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, NULL);
		Status = psCreateMr->Status;
		if (NT_SUCCESS(Status)) {
			psLamSgl->psNdkMr = (PNDK_MR)psCreateMr->psNdkObject;
		}
	} else if (NT_SUCCESS(Status)) {
		psLamSgl->psNdkMr = psCreateMr->psNdkWorkReq->sCreateFRMRReq.psNdkMr;
	}

	//NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:MR Create Status %x!\n", __FUNCTION__, __LINE__, Status);
	if (NT_ERROR(Status)) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, psCreateMr->psNdkWorkReq);
		NVMeoFRdmaFreeNpp(psCreateMr);
		return Status;
	}

	psCreateMr->psNdkWorkReq->ulType = NDK_WREQ_PD_INITIALIZE_MR;
	psCreateMr->psNdkWorkReq->sInitFRMRReq.psNdkMr = psLamSgl->psNdkMr;
	psCreateMr->psNdkWorkReq->sInitFRMRReq.ulInitAdapterPageCount = NVMEOF_RDMA_MAX_SEGMENTS;
	psCreateMr->psNdkWorkReq->sInitFRMRReq.bInitAllowRemoteAccess = TRUE;
	psCreateMr->psNdkWorkReq->fnCallBack = NVMeoFRdmaInitMrCompletion;
	psCreateMr->psNdkWorkReq->pvCallbackCtx = psCreateMr;
	KeResetEvent(&psCreateMr->sCompletionCtx.sEvent);
	Status = NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, 
		                      NULL, 
                              psCreateMr->psNdkWorkReq);
	if (NT_PENDING(Status)) {
		KeWaitForSingleObject(&psCreateMr->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, NULL);
		Status = psCreateMr->Status;
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:MR Init Status %x!\n", __FUNCTION__, __LINE__, Status);
	if (NT_ERROR(Status)) {
		psCreateMr->psNdkWorkReq->ulType = NDK_WREQ_PD_CLOSE_MR;
		psCreateMr->psNdkWorkReq->sCloseFRMRReq.psNdkMr = psLamSgl->psNdkMr;
		psCreateMr->psNdkWorkReq->sCloseFRMRReq.fnCloseCompletion = NULL;
		psCreateMr->psNdkWorkReq->sCloseFRMRReq.pvContext = NULL;
		PdsNdkCloseMemoryRegionAsync(psController->sSession.pvSessionFabricCtx, psCreateMr->psNdkWorkReq);
		NVMeoFRdmaFreeNdkWorkRequest(psController, psCreateMr->psNdkWorkReq);
		NVMeoFRdmaFreeNpp(psCreateMr);
		return (Status);
	}

	psCreateMr->psNdkWorkReq->ulType = NDK_WREQ_PD_GET_REMOTE_TOKEN_FROM_MR;
	psCreateMr->psNdkWorkReq->sGetMRTokenReq.psNdkMr = psLamSgl->psNdkMr;
	psLamSgl->uiRemoteMemoryTokenKey =
		NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, 
                         NULL,
                         psCreateMr->psNdkWorkReq);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:MR %#p Remote Memory Token %x!\n", __FUNCTION__, __LINE__,
		psLamSgl->psNdkMr,
		psLamSgl->uiRemoteMemoryTokenKey);

	NVMeoFRdmaFreeNdkWorkRequest(psController, psCreateMr->psNdkWorkReq);
	NVMeoFRdmaFreeNpp(psCreateMr);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
NTSTATUS
NVMeoFRdmaFastRegisterMr(__in PNVMEOF_FABRIC_CONTROLLER                 psController,
                         __in ULONG                              ulQId,
                         __in BOOLEAN                            bData,
                         __inout PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl)
{
	PNVMEOF_RDMA_WORK_REQUEST psCreateMr = NULL;
	NTSTATUS                  Status = STATUS_SUCCESS;
	PNDK_QUEUE_PAIR_BUNDLE    psPdsNdkQP = NULL;
	PNVMEOF_QUEUE             psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulQId);

	if (!psSQ) {
		return (STATUS_INVALID_PARAMETER);
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	psCreateMr = NVMeoFRdmaAllocateNpp(sizeof(*psCreateMr));
	if (!psCreateMr) {
		return (STATUS_NO_MEMORY);
	}

	RtlZeroMemory(psCreateMr, sizeof(*psCreateMr));

	psCreateMr->psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psCreateMr->psNdkWorkReq) {
		NVMeoFRdmaFreeNpp(psCreateMr);
		return (STATUS_NO_MEMORY);
	}

	psPdsNdkQP = psSQ->psFabricCtx->psRdmaCtx->psNdkQP;

	if (psPdsNdkQP) {
		if (bData) {
			psCreateMr->ulType = NVMEOF_REQUEST_TYPE_DATA;
		} else {
			psCreateMr->ulType = NVMEOF_REQUEST_TYPE_RECEIVE;
		}

		psCreateMr->psNdkWorkReq->ulType = NDK_WREQ_SQ_FAST_REGISTER;
		psCreateMr->psNdkWorkReq->pvCallbackCtx = psCreateMr;
		psCreateMr->psNdkWorkReq->sFRMRReq.psNdkMr = psLamSgl->psNdkMr;
		psCreateMr->psNdkWorkReq->sFRMRReq.ulAdapterPageCount = psLamSgl->psLaml->AdapterPageCount;
		psCreateMr->psNdkWorkReq->sFRMRReq.psAdapterPageArray = psLamSgl->psLaml->AdapterPageArray;
		psCreateMr->psNdkWorkReq->sFRMRReq.szLength = MmGetMdlByteCount(psLamSgl->psMdl);
		psCreateMr->psNdkWorkReq->sFRMRReq.pvBaseVirtualAddress = (PVOID)((ULONG_PTR)MmGetMdlVirtualAddress(psLamSgl->psMdl) ^ ((ULONG_PTR)psCreateMr & 0xFFFFFFFFFFFFF000));
		psCreateMr->psNdkWorkReq->ulFlags = (NDK_OP_FLAG_ALLOW_REMOTE_READ | NDK_OP_FLAG_ALLOW_REMOTE_WRITE);
		KeInitializeEvent(&psCreateMr->sCompletionCtx.sEvent, NotificationEvent, FALSE);
		Status = NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, psPdsNdkQP, psCreateMr->psNdkWorkReq);
		KeWaitForSingleObject(&psCreateMr->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, NULL);
	}
	else {
		Status = STATUS_INVALID_PARAMETER;
	} 

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:MR Fast reg status %x!\n", __FUNCTION__, __LINE__, Status);

	if (NT_SUCCESS(Status)) {
		psLamSgl->pvBaseVA = psCreateMr->psNdkWorkReq->sFRMRReq.pvBaseVirtualAddress;
	}

	NVMeoFRdmaFreeNdkWorkRequest(psController, psCreateMr->psNdkWorkReq);
	NVMeoFRdmaFreeNpp(psCreateMr);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
VOID
NVMeoFRdmaUpdateMemorytokenInSgl(__in    PNVMEOF_FABRIC_CONTROLLER              psController,
	__in    UINT32                          uiMemoryToken,
	__inout PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl)
{
	ULONG ulCount = 0;
	UNREFERENCED_PARAMETER(psController);
	while (ulCount < psLamSgl->usSgeCount) {
		psLamSgl->psSgl[ulCount].MemoryRegionToken = uiMemoryToken;
		ulCount++;
	}
}

static
NTSTATUS
NVMeoFRdmaCreateLamAndSglForMdl(__in    PNVMEOF_FABRIC_CONTROLLER              psController,
	__in    ULONG                           ulQId,
	__in    PMDL                            psMdl,
	__in    BOOLEAN                         bDataMdl,
	__inout PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDK_LOGICAL_ADDRESS_MAPPING psLam = NULL;
	PNDK_WORK_REQUEST psNdkWorkReq = NULL;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	if (!psMdl) {
		return (STATUS_INVALID_PARAMETER);
	}

	psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psNdkWorkReq) {
		return (STATUS_NO_MEMORY);
	}

	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));

	NVMeoFRdmaGetMdlInfo(psMdl,
		MmGetMdlVirtualAddress(psMdl),
		(PULONG)&psLamSgl->ulCurrentLamCbSize);
	if (!psLamSgl->psLaml) {
		psLam = NVMeoFRdmaAllocateNpp(psLamSgl->ulCurrentLamCbSize);
		if (!psLam) {
			NVMeoFRdmaFreeNdkWorkRequest(psController, psNdkWorkReq);
			return (STATUS_NO_MEMORY);
		}
		psLamSgl->psMdl = psMdl;
		psLamSgl->psLaml = psLam;
	}
	else {
		psLam = psLamSgl->psLaml;
	}

	RtlZeroMemory(psLam, psLamSgl->ulCurrentLamCbSize);

	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_BUILD_LAM;
	psNdkWorkReq->sBuildLamReq.psMdl = psMdl;
	psNdkWorkReq->sBuildLamReq.psLam = psLam;
	psNdkWorkReq->sBuildLamReq.ulLamCbSize = psLamSgl->ulCurrentLamCbSize;
	psNdkWorkReq->sBuildLamReq.szMdlBytesToMap = MmGetMdlByteCount(psMdl);
	psNdkWorkReq->fnCallBack = NULL;
	psNdkWorkReq->pvCallbackCtx = NULL;
	Status = PdsNdkBuildLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:BuildLam Failed With Status 0x%x!\n", __FUNCTION__, __LINE__, Status);
		return Status;
	}

	if (!bDataMdl) {
		psLamSgl->usSgeCount =
			(USHORT)NVMeoFRdmaPopulateSglFromLam(psLam,
				&psLamSgl->psSgl[psLamSgl->usSgeCount],
				psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->sQP.uiPrivilagedMemoryToken,
				psNdkWorkReq->sBuildLamReq.ulFboLAM,
				MmGetMdlByteCount(psMdl));
	}

	if (NT_SUCCESS(Status) && (bDataMdl)) {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Fast Registering LAM 0x%p!\n", __FUNCTION__, __LINE__, psLam);
		if (ulQId == NVME_ADMIN_QUEUE_ID) {
			Status =
				NVMeoFRdmaCreateAndInitializeMr(psController,
					psLamSgl);
			if (NT_SUCCESS(Status)) {
				Status =
					NVMeoFRdmaFastRegisterMr(psController,
						ulQId,
						bDataMdl,
						psLamSgl);
			}
			if (NT_SUCCESS(Status)) {
				NVMeoFRdmaUpdateMemorytokenInSgl(psController,
					psLamSgl->uiRemoteMemoryTokenKey,
					psLamSgl);
			}
		}
		else {
			Status =
				NVMeoFRdmaFastRegisterMr(psController,
					ulQId,
					bDataMdl,
					psLamSgl);
		}
	}
	else {
		//As privilage token for all the QP are same so Pulling it from Admin.
		NVMeoFRdmaUpdateMemorytokenInSgl(psController,
			psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->sQP.uiPrivilagedMemoryToken,
			psLamSgl);
	}

	if (NT_SUCCESS(Status)) {
		psLamSgl->usInitialized = TRUE;
	}

	NVMeoFRdmaFreeNdkWorkRequest(psController, psNdkWorkReq);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return Status;
}

static
NTSTATUS
NVMeoFRdmaSubmitReceiveWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                   __in ULONG ulQId,
                                   __in PNVMEOF_RDMA_WORK_REQUEST psWorkReq)
{
	PNDK_QUEUE_PAIR_BUNDLE psPdsNdkQP = NULL;
	PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulQId);
	if (!psSQ) {
		return STATUS_INVALID_PARAMETER;
	}

	psPdsNdkQP = &psSQ->psFabricCtx->psRdmaCtx->psNdkQP;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Submitting Receive Work Request 0x%p\n", __FUNCTION__, __LINE__,
		           psWorkReq->psNdkWorkReq);

	return
		NdkSubmitRequest(psController->sSession.pvSessionFabricCtx,
			             psPdsNdkQP,
			             psWorkReq->psNdkWorkReq);
}

static
NTSTATUS
NVMeoFRdmaAdminSubmitReceiveRequests(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVMEOF_RDMA_WORK_REQUEST psReceiveWorkReq =
		psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList;
	ULONG ulRcvCnt = 0;
	NTSTATUS Status = 0;
	for (; ulRcvCnt < psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestCount; psReceiveWorkReq++, ulRcvCnt++) {
		Status =
			NVMeoFRdmaSubmitReceiveWorkRequest(psController,
				NVME_ADMIN_QUEUE_ID,
				psReceiveWorkReq);
		if (NT_ERROR(Status)) {
			break;
		} else {
			_InterlockedIncrement64(&psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->llReceiveWorkRequestSubmitted);
			_InterlockedIncrement(&psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestOutstanding);
		}
	}

	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to Submit Receive Work Request 0x%u with Status 0x%x\n", __FUNCTION__, __LINE__,
			ulRcvCnt,
			Status);
	} else {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Successfully to Submitted all Receive Work Request!\n", __FUNCTION__, __LINE__);
	}

	return (Status);
}

static
VOID
NVMeoFRdmaAdminFreeReceiveResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVMEOF_QUEUE psAdminQueue = psController->sSession.psAdminQueue;
	PNVMEOF_RDMA_WORK_REQUEST psReceiveWorkReq = psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList;
	ULONG ulRcvCnt = 0;
	for (; (ulRcvCnt < psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestCount) && psReceiveWorkReq; psReceiveWorkReq++, ulRcvCnt++) {
		if (psReceiveWorkReq->psBufferMdlMap) {
			if (psReceiveWorkReq->psBufferMdlMap->psLamSglMap) {
				if (psReceiveWorkReq->psBufferMdlMap->psLamSglMap) {
					NVMeoFRdmaReleaseLamSglMap(psController, NVME_ADMIN_QUEUE_ID, psReceiveWorkReq->psBufferMdlMap->psLamSglMap);
				}

				if (psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psLaml) {
					NVMeoFRdmaFreeNpp(psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psLaml);
					psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psLaml = NULL;
				}
				if (psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psSgl) {
					NVMeoFRdmaFreeSGL(psController, psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psSgl);
					psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psSgl = NULL;
				}
				NVMeoFRdmaFreeLamSglMap(psController, psReceiveWorkReq->psBufferMdlMap->psLamSglMap);
				psReceiveWorkReq->psBufferMdlMap->psLamSglMap = NULL;
			}

			if (psReceiveWorkReq->psNdkWorkReq) {
				NVMeoFRdmaFreeNdkWorkRequest(psController, psReceiveWorkReq->psNdkWorkReq);
				psReceiveWorkReq->psNdkWorkReq = NULL;
			}

			if (psReceiveWorkReq->psBufferMdlMap->psBufferMdl) {
				IoFreeMdl(psReceiveWorkReq->psBufferMdlMap->psBufferMdl);
				psReceiveWorkReq->psBufferMdlMap->psBufferMdl = NULL;
			}

			if (psReceiveWorkReq->psBufferMdlMap->pvBuffer) {
				NVMeoFRdmaFreeNpp(psReceiveWorkReq->psBufferMdlMap->pvBuffer);
				psReceiveWorkReq->psBufferMdlMap->pvBuffer = NULL;
			}

			NVMeoFRdmaFreeBufferMdlMap(psController, psReceiveWorkReq->psBufferMdlMap);
			psReceiveWorkReq->psBufferMdlMap = NULL;
		}
	}

	if (psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults) {
		NVMeoFRdmaFreeNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults);
		psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults = NULL;
	}

	if (psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList) {
		NVMeoFRdmaFreeNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList);
		psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList = NULL;
	}

	if (psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq) {
		NVMeoFRdmaFreeNdkWorkRequest(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq);
		psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq = NULL;
	}
}

static
NTSTATUS
NVMeoFRdmaPrepareReceiveWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId,
	__in PNVMEOF_RDMA_WORK_REQUEST psWorkReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDK_WORK_REQUEST psNdkWorkReq = psWorkReq->psNdkWorkReq;

	RtlZeroMemory(psWorkReq->psBufferMdlMap->psLamSglMap, sizeof(PNVMEOF_RDMA_SGL_LAM_MAPPING));

	Status =
		NVMeoFRdmaCreateLamAndSglForMdl(psController,
			ulQId,
			psWorkReq->psBufferMdlMap->psBufferMdl,
			FALSE,
			psWorkReq->psBufferMdlMap->psLamSglMap);
	if (NT_ERROR(Status)) {
		return (Status);
	}

	psNdkWorkReq->ulType = NDK_WREQ_SQ_RECEIVE;
	psNdkWorkReq->fnCallBack = NULL;
	psNdkWorkReq->pvCallbackCtx = psWorkReq;
	psNdkWorkReq->ulFlags = 0;
	psNdkWorkReq->sReceiveReq.ulBytesToReceive = psWorkReq->psBufferMdlMap->ulBufferLength;
	psNdkWorkReq->sReceiveReq.psNdkSgl = psWorkReq->psBufferMdlMap->psLamSglMap->psSgl;
	psNdkWorkReq->sReceiveReq.usSgeCount = (USHORT)psWorkReq->psBufferMdlMap->psLamSglMap->usSgeCount;
	return STATUS_SUCCESS;
}

static
PMDL
NVMeoFRdmaAllocateMdlForBufferFromNP(__in PVOID pvBuffer,
	__in ULONG ulLength)
{
	PMDL psBufferMdl =
		IoAllocateMdl(pvBuffer,
			ulLength,
			FALSE,
			FALSE,
			NULL);
	if (!psBufferMdl) {
		return NULL;
	}

	MmBuildMdlForNonPagedPool(psBufferMdl);

	KeFlushIoBuffers(psBufferMdl, TRUE, TRUE);
	return psBufferMdl;
}

static
NTSTATUS
NVMeoFRdmaAdminAllocateReceiveResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVMEOF_RDMA_WORK_REQUEST psReceiveWorkReq = NULL;
	PNVMEOF_QUEUE psAdminQueue = psController->sSession.psAdminQueue;
	ULONG ulRcvCnt = 0;
	NTSTATUS Status = 0;

	psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestCount = NVMEOF_RDMA_RECEIVE_REQUEST;

	psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults =
		NVMeoFRdmaAllocateNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveResultsCount * sizeof(NDK_RESULT_EX));
	if (!psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults) {
		return (STATUS_NO_MEMORY);
	}

	psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList =
		NVMeoFRdmaAllocateNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestCount * sizeof(NVMEOF_RDMA_WORK_REQUEST));
	if (!psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList) {
		NVMeoFRdmaFreeNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults);
		return STATUS_NO_MEMORY;
	}

	psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psGetReceiveResultsNDKWrq) {
		NVMeoFRdmaFreeNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList);
		NVMeoFRdmaFreeNpp(psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestResults);
		return STATUS_NO_MEMORY;
	}

	psReceiveWorkReq = psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->psReceiveWorkRequestList;

	for (; ulRcvCnt < psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestCount;
		psReceiveWorkReq++, ulRcvCnt++) {
		psReceiveWorkReq->psBufferMdlMap =
			NVMeoFRdmaAllocateBufferMdlMap(psController);
		if (!psReceiveWorkReq->psBufferMdlMap) {
			break;
		}

		psReceiveWorkReq->psBufferMdlMap->ulBufferLength = NVMEOF_RDMA_RECEIVE_BUFFER_SIZE_128B;
		psReceiveWorkReq->psBufferMdlMap->pvBuffer = NVMeoFRdmaAllocateNpp(psReceiveWorkReq->psBufferMdlMap->ulBufferLength);
		if (!psReceiveWorkReq->psBufferMdlMap->pvBuffer) {
			Status = STATUS_NO_MEMORY;
			break;
		}

		psReceiveWorkReq->psNdkWorkReq =
			NVMeoFRdmaAllocateNdkWorkRequest(psController);
		if (!psReceiveWorkReq->psNdkWorkReq) {
			break;
		}

		psReceiveWorkReq->psBufferMdlMap->psBufferMdl =
			NVMeoFRdmaAllocateMdlForBufferFromNP(psReceiveWorkReq->psBufferMdlMap->pvBuffer,
				psReceiveWorkReq->psBufferMdlMap->ulBufferLength);
		if (!psReceiveWorkReq->psBufferMdlMap->psBufferMdl) {
			Status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		psReceiveWorkReq->psBufferMdlMap->psLamSglMap = NVMeoFRdmaAllocateLamSglMap(psController);
		if (!psReceiveWorkReq->psBufferMdlMap->psLamSglMap) {
			break;
		}
		RtlZeroMemory(psReceiveWorkReq->psBufferMdlMap->psLamSglMap, sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING));

		psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psSgl = NVMeoFRdmaAllocateSGL(psController);
		if (!psReceiveWorkReq->psBufferMdlMap->psLamSglMap) {
			break;
		}

		psReceiveWorkReq->psBufferMdlMap->psLamSglMap->psLaml = NULL;
		psReceiveWorkReq->pvParentQP = psAdminQueue;
		KeInitializeEvent(&psReceiveWorkReq->sCompletionCtx.sEvent, NotificationEvent, FALSE);
		psReceiveWorkReq->sCompletionCtx.Status = STATUS_SUCCESS;
		psReceiveWorkReq->ulType = PDS_NVMEOF_REQUEST_TYPE_RECEIVE;

		Status =
			NVMeoFRdmaPrepareReceiveWorkRequest(psController,
				                                NVME_ADMIN_QUEUE_ID,
				                                psReceiveWorkReq);
		if (NT_ERROR(Status)) {
			break;
		}
	}

	if (ulRcvCnt < psAdminQueue->psFabricCtx->psRdmaCtx->psFabricCtx->psRdmaCtx->lReceiveWorkRequestCount) {
		NVMeoFRdmaAdminFreeReceiveResources(psController);
		return (Status);
	}

	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaAdminFreeCommand(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psBufferMdlMap)
{
	if (psBufferMdlMap->psLamSglMap) {
		if (psBufferMdlMap->psLamSglMap) {
			NVMeoFRdmaReleaseLamSglMap(psController, NVME_ADMIN_QUEUE_ID, psBufferMdlMap->psLamSglMap);
		}

		if (psBufferMdlMap->psLamSglMap->psLaml) {
			NVMeoFRdmaFreeNpp(psBufferMdlMap->psLamSglMap->psLaml);
			psBufferMdlMap->psLamSglMap->psLaml = NULL;
		}
		if (psBufferMdlMap->psLamSglMap->psSgl) {
			NVMeoFRdmaFreeSGL(psController, psBufferMdlMap->psLamSglMap->psSgl);
			psBufferMdlMap->psLamSglMap->psSgl = NULL;
		}
		NVMeoFRdmaFreeLamSglMap(psController, psBufferMdlMap->psLamSglMap);
		psBufferMdlMap->psLamSglMap = NULL;
	}
}

static
VOID
NVMeoFRdmaAdminFreeData(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psDataBufferMdlMap)
{
	if (psDataBufferMdlMap->psLamSglMap) {
		if (psDataBufferMdlMap->psLamSglMap) {
			NVMeoFRdmaReleaseLamSglMap(psController, NVME_ADMIN_QUEUE_ID, psDataBufferMdlMap->psLamSglMap);
		}
		if (psDataBufferMdlMap->psLamSglMap->psLaml) {
			NVMeoFRdmaFreeNpp(psDataBufferMdlMap->psLamSglMap->psLaml);
			psDataBufferMdlMap->psLamSglMap->psLaml = NULL;
		}
		if (psDataBufferMdlMap->psLamSglMap->psSgl) {
			NVMeoFRdmaFreeSGL(psController, psDataBufferMdlMap->psLamSglMap->psSgl);
			psDataBufferMdlMap->psLamSglMap->psSgl = NULL;
		}
		NVMeoFRdmaFreeLamSglMap(psController, psDataBufferMdlMap->psLamSglMap);
		psDataBufferMdlMap->psLamSglMap = NULL;
	}
}

static
VOID
NVMeoFRdmaAdminFreeOneSubmmissionRequests(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PNVMEOF_RDMA_WORK_REQUEST psSubmissionWorkReq)
{
	if (psSubmissionWorkReq->psBufferMdlMap) {
		NVMeoFRdmaAdminFreeCommand(psController,
			psSubmissionWorkReq->psBufferMdlMap);
		NVMeoFRdmaFreeBufferMdlMap(psController, psSubmissionWorkReq->psBufferMdlMap);
		psSubmissionWorkReq->psBufferMdlMap = NULL;
	}

	if (psSubmissionWorkReq->psDataBufferMdlMap) {
		NVMeoFRdmaAdminFreeData(psController,
			psSubmissionWorkReq->psDataBufferMdlMap);
		NVMeoFRdmaFreeBufferMdlMap(psController, psSubmissionWorkReq->psDataBufferMdlMap);
		psSubmissionWorkReq->psDataBufferMdlMap = NULL;
	}

	if (psSubmissionWorkReq->psCqe) {
		NVMeoFRdmaFreeNVMeCqe(psController, psSubmissionWorkReq->psCqe);
		psSubmissionWorkReq->psCqe = NULL;
	}

	if (psSubmissionWorkReq->psNdkWorkReq) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, psSubmissionWorkReq->psNdkWorkReq);
		psSubmissionWorkReq->psNdkWorkReq = NULL;
	}

}

static
VOID
NVMeoFRdmaAdminFreeSubmissionResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVMEOF_RDMA_WORK_REQUEST psSubmissionWorkReq = NULL;
	ULONG ulScqCnt = 0;
	psSubmissionWorkReq = psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList;
	for (; (ulScqCnt < psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSubmissionWorkRequestCount) && psSubmissionWorkReq;
		psSubmissionWorkReq++, ulScqCnt++) {
		NVMeoFRdmaAdminFreeOneSubmmissionRequests(psController,
			psSubmissionWorkReq);
	}

	if (psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList) {
		NVMeoFRdmaFreeNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList);
		psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList = NULL;
	}

	if (psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq);
		psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq = NULL;
	}

	if (psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults) {
		NVMeoFRdmaFreeNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults);
		psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults = NULL;
	}
}

static
NTSTATUS
NVMeoFRdmaAdminAllocateSubmissionResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVMEOF_RDMA_WORK_REQUEST psSubmissionWorkReq = NULL;
	ULONG ulRcvCnt = 0;
	NTSTATUS Status = 0;

	psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSubmissionWorkRequestCount = NVME_ADMIN_QUEUE_REQUEST_COUNT;

	psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList =
		NVMeoFRdmaAllocateNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSubmissionWorkRequestCount * sizeof(NVMEOF_RDMA_WORK_REQUEST));
	if (!psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList) {
		return STATUS_NO_MEMORY;
	}

	psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults =
		NVMeoFRdmaAllocateNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSubmissionResultsCount * sizeof(NDK_RESULT_EX));
	if (!psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults) {
		NVMeoFRdmaFreeNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList);
		return STATUS_NO_MEMORY;
	}

	psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq =
		NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq) {
		NVMeoFRdmaFreeNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults);
		NVMeoFRdmaFreeNpp(psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList);
		return STATUS_NO_MEMORY;
	}

	psSubmissionWorkReq = psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList;

	for (; ulRcvCnt < psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSubmissionWorkRequestCount;
		psSubmissionWorkReq++, ulRcvCnt++) {
		psSubmissionWorkReq->psBufferMdlMap =
			NVMeoFRdmaAllocateBufferMdlMap(psController);
		if (!psSubmissionWorkReq->psBufferMdlMap) {
			break;
		}

		psSubmissionWorkReq->psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
		if (!psSubmissionWorkReq->psNdkWorkReq) {
			break;
		}

		psSubmissionWorkReq->psBufferMdlMap->psLamSglMap = NVMeoFRdmaAllocateLamSglMap(psController);
		if (!psSubmissionWorkReq->psBufferMdlMap->psLamSglMap) {
			break;
		}
		RtlZeroMemory(psSubmissionWorkReq->psBufferMdlMap->psLamSglMap, sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING));

		psSubmissionWorkReq->psBufferMdlMap->psLamSglMap->psSgl = NVMeoFRdmaAllocateSGL(psController);
		if (!psSubmissionWorkReq->psBufferMdlMap->psLamSglMap) {
			break;
		}

		psSubmissionWorkReq->psBufferMdlMap->psLamSglMap->psLaml = NULL;

		psSubmissionWorkReq->psDataBufferMdlMap =
			NVMeoFRdmaAllocateBufferMdlMap(psController);
		if (!psSubmissionWorkReq->psDataBufferMdlMap) {
			break;
		}

		psSubmissionWorkReq->psDataBufferMdlMap->psLamSglMap = NVMeoFRdmaAllocateLamSglMap(psController);
		if (!psSubmissionWorkReq->psDataBufferMdlMap->psLamSglMap) {
			break;
		}
		RtlZeroMemory(psSubmissionWorkReq->psDataBufferMdlMap->psLamSglMap, sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING));

		psSubmissionWorkReq->psDataBufferMdlMap->psLamSglMap->psSgl = NVMeoFRdmaAllocateSGL(psController);
		if (!psSubmissionWorkReq->psDataBufferMdlMap->psLamSglMap->psSgl) {
			break;
		}

		psSubmissionWorkReq->psCqe = NVMeoFRdmaAllocateNVMeCqe(psController);
		if (!psSubmissionWorkReq->psCqe) {
			break;
		}

		psSubmissionWorkReq->psDataBufferMdlMap->psLamSglMap->psLaml = NULL;

		psSubmissionWorkReq->pvParentQP = psController->sSession.psAdminQueue;
		KeInitializeEvent(&psSubmissionWorkReq->sCompletionCtx.sEvent, NotificationEvent, FALSE);
		psSubmissionWorkReq->ulType = PDS_NVMEOF_REQUEST_TYPE_SEND;
		psSubmissionWorkReq->usCommandID = 0xFFFF;
		psSubmissionWorkReq->pvParentQP = psController->sSession.psAdminQueue;
		_InterlockedExchange(&psSubmissionWorkReq->lReqState, NVMEOF_WORK_REQUEST_STATE_FREE);
	}

	if (ulRcvCnt < psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSubmissionResultsCount) {
		NVMeoFRdmaAdminFreeSubmissionResources(psController);
		return (Status);
	}

	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaFreeLookasideLists(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	if (gpsRdmaControllerList->psNdkWorkRequestLAList) {
		ExDeleteNPagedLookasideList(gpsRdmaControllerList->);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		gpsRdmaControllerList->psNdkWorkRequestLAList = NULL;
	}

	if (gpsRdmaControllerList->psBufferMdlMapLAList) {
		ExDeleteLookasideListEx(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		gpsRdmaControllerList->psBufferMdlMapLAList = NULL;
	}

	if (gpsRdmaControllerList->psLAMLAList) {
		ExDeleteNPagedLookasideList(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		gpsRdmaControllerList->psLAMLAList = NULL;
	}

	if (gpsRdmaControllerList->psLamSglLAList) {
		ExDeleteNPagedLookasideList(gpsRdmaControllerList->psLamSglLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLamSglLAList);
		gpsRdmaControllerList->psLamSglLAList = NULL;
	}

	if (gpsRdmaControllerList->psSGLLAList) {
		ExDeleteNPagedLookasideList(gpsRdmaControllerList->psSGLLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psSGLLAList);
		gpsRdmaControllerList->psSGLLAList = NULL;
	}

	if (gpsRdmaControllerList->psRequestLAList) {
		ExDeleteNPagedLookasideList(gpsRdmaControllerList->psRequestLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psRequestLAList);
		gpsRdmaControllerList->psRequestLAList = NULL;
	}

	if (gpsRdmaControllerList->psNVMeCmdLAList) {
		ExDeleteLookasideListEx(gpsRdmaControllerList->psNVMeCmdLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCmdLAList);
		gpsRdmaControllerList->psNVMeCmdLAList = NULL;
	}

	if (gpsRdmaControllerList->psNVMeCqeLAList) {
		ExDeleteLookasideListEx(gpsRdmaControllerList->psNVMeCqeLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCqeLAList);
		gpsRdmaControllerList->psNVMeCqeLAList = NULL;
	}

	if (gpsRdmaControllerList->psDataLAMLAList) {
		ExDeleteLookasideListEx(gpsRdmaControllerList->psDataLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psDataLAMLAList);
		gpsRdmaControllerList->psDataLAMLAList = NULL;
	}
}

static
NTSTATUS
NVMeoFRdmaAllocateLookasideLists(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	gpsRdmaControllerList->psNdkWorkRequestLAList = NVMeoFRdmaAllocateNpp(sizeof(NPAGED_LOOKASIDE_LIST));
	if (!gpsRdmaControllerList->psNdkWorkRequestLAList) {
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psBufferMdlMapLAList = NVMeoFRdmaAllocateNpp(sizeof(LOOKASIDE_LIST_EX));
	if (!gpsRdmaControllerList->psBufferMdlMapLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psLAMLAList = NVMeoFRdmaAllocateNpp(sizeof(NPAGED_LOOKASIDE_LIST));
	if (!gpsRdmaControllerList->psLAMLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psLamSglLAList = NVMeoFRdmaAllocateNpp(sizeof(NPAGED_LOOKASIDE_LIST));
	if (!gpsRdmaControllerList->psLamSglLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psSGLLAList = NVMeoFRdmaAllocateNpp(sizeof(NPAGED_LOOKASIDE_LIST));
	if (!gpsRdmaControllerList->psSGLLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLamSglLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psNVMeCmdLAList = NVMeoFRdmaAllocateNpp(sizeof(LOOKASIDE_LIST_EX));
	if (!gpsRdmaControllerList->psNVMeCmdLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psSGLLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLamSglLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psNVMeCqeLAList = NVMeoFRdmaAllocateNpp(sizeof(LOOKASIDE_LIST_EX));
	if (!gpsRdmaControllerList->psNVMeCqeLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCmdLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psSGLLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLamSglLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psRequestLAList = NVMeoFRdmaAllocateNpp(sizeof(NPAGED_LOOKASIDE_LIST));
	if (!gpsRdmaControllerList->psRequestLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCqeLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCmdLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psSGLLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLamSglLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	gpsRdmaControllerList->psDataLAMLAList = NVMeoFRdmaAllocateNpp(sizeof(LOOKASIDE_LIST_EX));
	if (!gpsRdmaControllerList->psDataLAMLAList) {
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psRequestLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCqeLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNVMeCmdLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psSGLLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLamSglLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psLAMLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psBufferMdlMapLAList);
		NVMeoFRdmaFreeNpp(gpsRdmaControllerList->psNdkWorkRequestLAList);
		return (STATUS_NO_MEMORY);
	}

	ExInitializeNPagedLookasideList(gpsRdmaControllerList->psNdkWorkRequestLAList,
		NULL,
		NULL,
		POOL_NX_ALLOCATION,
		sizeof(NDK_WORK_REQUEST),
		'bmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeLookasideListEx(gpsRdmaControllerList->psBufferMdlMapLAList,
		NULL,
		NULL,
		NonPagedPoolNxCacheAligned,
		0,
		sizeof(PDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL),
		'cmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeNPagedLookasideList(gpsRdmaControllerList->psLAMLAList,
		NULL,
		NULL,
		POOL_NX_ALLOCATION,
		SEND_RECEIVE_LAM_MAX_SIZE,
		'dmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeNPagedLookasideList(gpsRdmaControllerList->psLamSglLAList,
		NULL,
		NULL,
		POOL_NX_ALLOCATION,
		sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING),
		'emdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeNPagedLookasideList(gpsRdmaControllerList->psSGLLAList,
		NULL,
		NULL,
		POOL_NX_ALLOCATION,
		MAX_DATA_SGE_BUFFER_SIZE,
		'fmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeNPagedLookasideList(gpsRdmaControllerList->psRequestLAList,
		NULL,
		NULL,
		POOL_NX_ALLOCATION,
		sizeof(NVMEOF_REQUEST_RESPONSE),
		'gmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeLookasideListEx(gpsRdmaControllerList->psNVMeCqeLAList,
		NULL,
		NULL,
		NonPagedPoolNx,
		0,
		sizeof(NVME_COMPLETION_QUEUE_ENTRY),
		'hmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeLookasideListEx(gpsRdmaControllerList->psNVMeCmdLAList,
		NULL,
		NULL,
		NonPagedPoolNx,
		0,
		sizeof(NVME_CMD),
		'imdR',//NVMEF_RDMA_MEMORY_TAG,
		0);

	ExInitializeLookasideListEx(gpsRdmaControllerList->psDataLAMLAList,
		NULL,
		NULL,
		NonPagedPoolNx,
		0,
		DATA_LAM_MAX_SIZE,
		'jmdR',//NVMEF_RDMA_MEMORY_TAG,
		0);
	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaAdminFreeResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	KEVENT sEvent = { 0 };

	KeInitializeEvent(&sEvent, NotificationEvent, FALSE);
	while (psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lReceiveWorkRequestOutstanding ||
		psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lReceiveWorkRequestOutstanding ||
		psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lFRMRReqOutstanding) {
		LARGE_INTEGER liWaitTo = { 0 };
		liWaitTo.QuadPart = WDF_REL_TIMEOUT_IN_MS(1000LL);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Outstanding Req Rcv %d Snd %d FRMR %d!\n", __FUNCTION__, __LINE__,
			psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lReceiveWorkReqSubmitted,
			psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lSendWorkReqSubmitted,
			psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->lFRMRReqSubmitted);
		KeWaitForSingleObject(&sEvent, Executive, KernelMode, FALSE, &liWaitTo);
	}

	NVMeoFRdmaAdminFreeSubmissionResources(psController);
	NVMeoFRdmaAdminFreeReceiveResources(psController);
	NVMeoFRdmaAdminFreeLookasideLists(psController);
}

static
NTSTATUS
NVMeoFRdmaAdminAllocateResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;

	Status = NVMeoFRdmaAdminAllocateLookasideLists(psController);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Lookaside list allocation failed Status (0x%x)!\n",
			__FUNCTION__,
			__LINE__,
			Status);
		return Status;
	}
	//Need to allocate the Submission Queue Resources as Submission call back gets 
	//invoked before even submitting receive to receive data.
	Status = NVMeoFRdmaAdminAllocateSubmissionResources(psController);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Submission resources allocation failed Status (0x%x)!\n",
			__FUNCTION__,
			__LINE__,
			Status);
		return Status;
	}

	Status = NVMeoFRdmaAdminAllocateReceiveResources(psController);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Receive resources allocation failed Status (0x%x)!\n",
			__FUNCTION__,
			__LINE__,
			Status);
		return Status;
	}

	return  (Status);
}

static
NTSTATUS
NVMeoFRdmaAdminQChangeDisconnectAndReceiveEventState(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in BOOLEAN bEnable)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(bEnable);

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:%s Disconnect and Receive Event on Admin Queue!\n", __FUNCTION__, __LINE__, bEnable ? "Enabled" : "Disabled");
	return Status;
}

static
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NVMeoFRdmaXportCreateCompletion(_In_opt_ PVOID pvContext,
	_In_ NTSTATUS Status,
	_In_ PNDK_OBJECT_HEADER psNdkObject)
{
	UNREFERENCED_PARAMETER(pvContext);
	UNREFERENCED_PARAMETER(Status);
	UNREFERENCED_PARAMETER(psNdkObject);
}

static
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NVMeoFRdmaXportCloseCompletion(_In_opt_ PVOID pvContext)
{
	UNREFERENCED_PARAMETER(pvContext);
}

static
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NVMeoFRdmaXportRequestCompletion(_In_opt_ PVOID pvContext,
	_In_ NTSTATUS Status)
{
	UNREFERENCED_PARAMETER(pvContext);
	UNREFERENCED_PARAMETER(Status);
}

static
NTSTATUS
NVMeoFRdmaXportConnectControllerAdminQueue(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PDS_NDK_CALLBACKS sRDMASockedCB = { 0 };
	sRDMASockedCB.fnCloseCompletionCb = NVMeoFRdmaXportCloseCompletion;
	sRDMASockedCB.fnCreateCompletionCb = NVMeoFRdmaXportCreateCompletion;
	sRDMASockedCB.fnRequestCompletionCb = NVMeoFRdmaXportRequestCompletion;
	sRDMASockedCB.pvCloseCompletionCtx = psController;
	sRDMASockedCB.pvCreateCompletionCtx = psController;
	sRDMASockedCB.pvRequestCompletionCtx = psController;

	Status = NVMeoFRdmaCreateAsyncSocket(psController,
		                                 &psController->sTargetAddress,
		                                 &psController->sHostXportAddress,
		                                 &sRDMASockedCB,
		                                 (PNDK_SOCKET*)&psController->sSession.pvSessionFabricCtx);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NVMeoFRdmaAdminXportConnect() failed with status 0x%x\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:NVMeoFRdmaAdminXportConnect() failed with status 0x%x\n", __FUNCTION__, __LINE__, Status);
		return Status;
	}

	Status = NVMeoFRdmaAdminXportConnect(psController);
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NVMeoFRdmaAdminXportConnect() failed with status 0x%x\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:NVMeoFRdmaAdminXportConnect() failed with status 0x%x\n", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

	Status = NVMeoFRdmaAdminAllocateResources(psController);
	if (NT_ERROR(Status)) {

	}

	Status = NVMeoFRdmaAdminSubmitReceiveRequests(psController);
	if (NT_ERROR(Status)) {
	}

	return (Status);
}

static
NTSTATUS
NVMeoFRdmaCreateCommandLamAndSglForMdl(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId,
	__in PNVMEOF_RDMA_WORK_REQUEST psRdmaWkReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UINT32 uiMemoryToken = 0;
	psRdmaWkReq->psBufferMdlMap->psBufferMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(psRdmaWkReq->psBufferMdlMap->pvBuffer,
			psRdmaWkReq->psBufferMdlMap->ulBufferLength);
	if (!psRdmaWkReq->psBufferMdlMap->psBufferMdl) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (ulQId == NVME_ADMIN_QUEUE_ID) {
		uiMemoryToken = psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->sQP.uiPrivilagedMemoryToken;
	} else {
		PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulQId);
		uiMemoryToken = psSQ->psFabricCtx->psRdmaCtx->psNdkQP->uiPrivilagedMemoryToken;
	}

	Status =
		NVMeoFRdmaCreateLamAndSglForMdl(psController,
			ulQId,
			psRdmaWkReq->psBufferMdlMap->psBufferMdl,
			FALSE,
			psRdmaWkReq->psBufferMdlMap->psLamSglMap);
	if (NT_ERROR(Status)) {
		IoFreeMdl(psRdmaWkReq->psBufferMdlMap->psBufferMdl);
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaCreateDataLamForMdl(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId,
	__in PNVMEOF_RDMA_WORK_REQUEST psRdmaWkReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	BOOLEAN  bAllocatedMdl = FALSE;
	if (!psRdmaWkReq->psDataBufferMdlMap->psBufferMdl) {
		psRdmaWkReq->psDataBufferMdlMap->psBufferMdl =
			NVMeoFRdmaAllocateMdlForBufferFromNP(psRdmaWkReq->psDataBufferMdlMap->pvBuffer,
				psRdmaWkReq->psDataBufferMdlMap->ulBufferLength);
		if (!psRdmaWkReq->psDataBufferMdlMap->psBufferMdl) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		bAllocatedMdl = TRUE;
	}

	Status =
		NVMeoFRdmaCreateLamAndSglForMdl(psController,
			ulQId,
			psRdmaWkReq->psDataBufferMdlMap->psBufferMdl,
			TRUE,
			psRdmaWkReq->psDataBufferMdlMap->psLamSglMap);
	if (NT_ERROR(Status)) {
		if (bAllocatedMdl) {
			IoFreeMdl(psRdmaWkReq->psDataBufferMdlMap->psBufferMdl);
			psRdmaWkReq->psDataBufferMdlMap->psBufferMdl = NULL;
		}
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaPrepareSendWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId,
	__in PNVMEOF_RDMA_WORK_REQUEST psWorkReq)
{
	NTSTATUS Status = STATUS_SUCCESS;

	if (psWorkReq->psDataBufferMdlMap->pvBuffer) {
		Status =
			NVMeoFRdmaCreateDataLamForMdl(psController,
				ulQId,
				psWorkReq);
		if (NT_ERROR(Status)) {
			return (Status);
		}
		else {
			KeFlushIoBuffers(psWorkReq->psDataBufferMdlMap->psBufferMdl, TRUE, TRUE);
		}

		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Data buffer info addr 0x%p base VA 0x%llx len %u RemoteToken %lu\n", __FUNCTION__, __LINE__,
			psWorkReq->psBufferMdlMap->pvBuffer,
			psWorkReq->psDataBufferMdlMap->psLamSglMap->pvBaseVA,
			psWorkReq->psDataBufferMdlMap->ulBufferLength,
			psWorkReq->psDataBufferMdlMap->psLamSglMap->uiRemoteMemoryTokenKey);

		NVMeoFRdmaNvmeofXportSetSG(psController,
			ulQId,
			psWorkReq->psBufferMdlMap->pvBuffer,
			(ULONGLONG)psWorkReq->psDataBufferMdlMap->psLamSglMap->pvBaseVA,
			psWorkReq->psDataBufferMdlMap->ulBufferLength,
			psWorkReq->psDataBufferMdlMap->psLamSglMap->uiRemoteMemoryTokenKey,
			NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_SINGLE);
	}

	if (ulQId == NVME_ADMIN_QUEUE_ID) {
		Status =
			NVMeoFRdmaCreateCommandLamAndSglForMdl(psController, ulQId, psWorkReq);
		if (NT_ERROR(Status)) {
			return (Status);
		}
	}
	else {
		KeFlushIoBuffers(psWorkReq->psBufferMdlMap->psBufferMdl, TRUE, TRUE);
		NVMeoFDisplayNVMeCmd(__FUNCTION__,
			psWorkReq->psBufferMdlMap->pvBuffer);
	}

	psWorkReq->psNdkWorkReq->ulType = NDK_WREQ_SQ_SEND;
	psWorkReq->psNdkWorkReq->sSendReq.psNdkSgl = psWorkReq->psBufferMdlMap->psLamSglMap->psSgl;
	psWorkReq->psNdkWorkReq->sSendReq.usSgeCount = (USHORT)psWorkReq->psBufferMdlMap->psLamSglMap->usSgeCount;
	psWorkReq->psNdkWorkReq->fnCallBack = NULL;
	psWorkReq->psNdkWorkReq->pvCallbackCtx = psWorkReq;
	psWorkReq->psNdkWorkReq->ulFlags = 0;
	psWorkReq->psNdkWorkReq->sSendReq.uiRemoteMemoryRegionToken = 0;

	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaReleaseSendWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId,
	__in PNVMEOF_RDMA_WORK_REQUEST psWorkReq)
{
	psWorkReq->usCommandID = 0xFFFF;
	psWorkReq->fSendReqCompletion = NULL;
	psWorkReq->fNVMeReqCompletion = NULL;
	psWorkReq->pvParentReqCtx = NULL;
	psWorkReq->sCompletionCtx.Status = STATUS_SUCCESS;
	KeResetEvent(&psWorkReq->sCompletionCtx.sEvent);
	psWorkReq->Status = STATUS_SUCCESS;
	psWorkReq->ulReqFlag = 0;
	psWorkReq->ulByteXferred = 0;
	if (psWorkReq->psBufferMdlMap->psBufferMdl) {
		NVMeoFRdmaReleaseLamSglMap(psController,
			ulQId,
			psWorkReq->psBufferMdlMap->psLamSglMap);
		IoFreeMdl(psWorkReq->psBufferMdlMap->psBufferMdl);
		psWorkReq->psBufferMdlMap->psBufferMdl = NULL;
		psWorkReq->psBufferMdlMap->pvBuffer = NULL;
		psWorkReq->psBufferMdlMap->ulBufferLength = 0;
	}
	if (psWorkReq->psDataBufferMdlMap->psBufferMdl) {
		NVMeoFRdmaReleaseLamSglMap(psController,
			ulQId,
			psWorkReq->psDataBufferMdlMap->psLamSglMap);
		IoFreeMdl(psWorkReq->psDataBufferMdlMap->psBufferMdl);
		psWorkReq->psDataBufferMdlMap->psBufferMdl = NULL;
		psWorkReq->psDataBufferMdlMap->pvBuffer = NULL;
		psWorkReq->psDataBufferMdlMap->ulBufferLength = 0;
	}

	_InterlockedExchange(&psWorkReq->lReqState, PDS_NVMEOF_RESPONSE_STATE_FREE);
	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaSubmitSendWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in ULONG ulQId,
	__in PNVMEOF_RDMA_WORK_REQUEST psWorkReq)
{
	PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulQId);
	PNDK_QUEUE_PAIR_BUNDLE psPdsNdkQP = NULL;
    NT_ASSERT(psSQ);
    psPdsNdkQP = psSQ->psFabricCtx->psRdmaCtx->psNdkQP;
	return 	NdkSubmitRequest(psController->sSession.pvSessionFabricCtx,
		                     psPdsNdkQP,
		                     psWorkReq->psNdkWorkReq);
}

static
VOID
NVMeoFRdmaSubmissionWorkReqNVMeCompletion(__in PNVMEOF_RDMA_WORK_REQUEST psSqWReq,
                                          __in PNVMEOF_RDMA_WORK_REQUEST psRqWReq,
                                          __in ULONG                          ulBufferLen,
                                          __in NTSTATUS                       Status)
{
	PNVMEOF_REQUEST_RESPONSE psReq = psSqWReq->pvParentReqCtx;
	if (ulBufferLen == sizeof(NVME_COMPLETION_QUEUE_ENTRY)) {
		*(PNVME_COMPLETION_QUEUE_ENTRY)psReq->sRcvBuffer.sReqRespBuffer.pvBuffer = *(PNVME_COMPLETION_QUEUE_ENTRY)psRqWReq->psBufferMdlMap->pvBuffer;
		//NVMeoFDisplayCQE(__FUNCTION__, psRqWReq->psBufferMdlMap->pvBuffer);
		psReq->sRcvBuffer.sReqRespBuffer.ulBufferLen = psSqWReq->ulByteXferred = sizeof(NVME_COMPLETION_QUEUE_ENTRY);
		psSqWReq->Status = Status;
	} else {
		NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d:Got buffer of length 0x%x bytes\n", __FUNCTION__, __LINE__,
			           ulBufferLen);
		psSqWReq->Status = STATUS_INSUFFICIENT_RESOURCES;
	}
	KeSetEvent(&psSqWReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
}

static
VOID
NVMeoFRdmaSubmissionWorkReqSendOnlyNVMeCompletion(__in PNVMEOF_RDMA_WORK_REQUEST psSqWReq,
	__in NTSTATUS Status)
{
	psSqWReq->Status = Status;
	KeSetEvent(&psSqWReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
}

static
NTSTATUS
NVMeoFRdmaAdminSubmitRequestSyncAsync(__in PNVMEOF_FABRIC_CONTROLLER  psController,
	__in USHORT              usCommandID,
	__in PNVMEOF_REQUEST_RESPONSE psAdminReqResp)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_RDMA_WORK_REQUEST psSndWkReq = NULL;
	LARGE_INTEGER liCommandTimeOut = { 0 };

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Trying to send Command of Length 0x%x bytes\n", __FUNCTION__, __LINE__,
		psAdminReqResp->sSndBuffer.ulBufferLen);
	psSndWkReq =
		NVMeoFRdmaAllocateAndInitializeSendWorkRequest(psController,
			                                           NVME_ADMIN_QUEUE_ID,
			                                           psAdminReqResp->sSndBuffer.sReqRespBuffer.pvBuffer,
			                                           psAdminReqResp->sSndBuffer.sReqRespBuffer.ulBufferLen,
			                                           (psAdminReqResp->ulFlags == PDS_NVMEOF_ADMIN_REQUEST_TYPE_SYNC_SEND_DATA) ?
			                                           psAdminReqResp->sSndBuffer.sDataBuffer.pvBuffer :
			                                           psAdminReqResp->sRcvBuffer.sDataBuffer.pvBuffer,
			                                           (psAdminReqResp->ulFlags == PDS_NVMEOF_ADMIN_REQUEST_TYPE_SYNC_SEND_DATA) ?
			                                           psAdminReqResp->sSndBuffer.sDataBuffer.ulBufferLen:
			                                           psAdminReqResp->sRcvBuffer.sDataBuffer.ulBufferLen);
	if (!psSndWkReq) {
		NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d:Failed to allocate the Send Work Request!\n", __FUNCTION__, __LINE__);
		return (STATUS_INSUFFICIENT_RESOURCES);
	}

	KeResetEvent(&psSndWkReq->sCompletionCtx.sEvent);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Cmd Buff Len 0x%x Data Buff Len 0x%x bytes\n", __FUNCTION__, __LINE__,
		           psAdminReqResp->sSndBuffer.ulBufferLen,
		           psAdminReqResp->sSndBuffer.ulDataLen);
	psSndWkReq->pvParentReqCtx = psAdminReqResp;
	psSndWkReq->usCommandID = usCommandID;
	switch (psAdminReqResp->ulFlags) {
	case PDS_NVMEOF_ADMIN_REQUEST_TYPE_SEND_ONLY:
		psSndWkReq->fSendReqCompletion = NVMeoFRdmaSubmissionWorkReqSendOnlyNVMeCompletion;
		psSndWkReq->fNVMeReqCompletion = NULL;
		break;
	case PDS_NVMEOF_ADMIN_REQUEST_TYPE_SYNC:
	case PDS_NVMEOF_ADMIN_REQUEST_TYPE_SYNC_RECEIVE_DATA:
	case PDS_NVMEOF_ADMIN_REQUEST_TYPE_SYNC_SEND_DATA:
		psSndWkReq->fSendReqCompletion = NULL;
		psSndWkReq->fNVMeReqCompletion = NVMeoFRdmaSubmissionWorkReqNVMeCompletion;
		break;
	case PDS_NVMEOF_ADMIN_REQUEST_TYPE_ASYNC:
		break;
	default:
		ASSERT(0);
	}
	Status = NVMeoFRdmaPrepareSendWorkRequest(psController,
		NVME_ADMIN_QUEUE_ID,
		psSndWkReq);
	if (NT_ERROR(Status)) {
		NVMeoFRdmaReleaseSendWorkRequest(psController, NVME_ADMIN_QUEUE_ID, psSndWkReq);
		return Status;
	}

	_mm_mfence();

	KeResetEvent(&psSndWkReq->sCompletionCtx.sEvent);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Submiting Send Work Request 0x%llx!\n",
		__FUNCTION__,
		__LINE__,
		psSndWkReq->psNdkWorkReq);
	Status = NVMeoFRdmaSubmitSendWorkRequest(psController,
		NVME_ADMIN_QUEUE_ID,
		psSndWkReq);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Going to wait to complete Send Work Request Submit Status 0x%x!\n",
		__FUNCTION__,
		__LINE__,
		Status);
	//liCommandTimeOut.QuadPart = -PDS_MILLISECONDS_TO_100NANOSECONDS(PDS_SECONDS_TO_MILLISECONDS(PDS_NVME_ADMIN_COMMAND_TIMEOUT_SEC));
	liCommandTimeOut.QuadPart = WDF_REL_TIMEOUT_IN_MS(NVMEOF_ADMIN_COMMAND_TIMEOUT_SEC * 1000ULL);
	Status = KeWaitForSingleObject(&psSndWkReq->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, &liCommandTimeOut);
	if (Status == STATUS_TIMEOUT) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Wait complete with command timeout 0x%x issuing cleanup!\n",
			__FUNCTION__,
			__LINE__,
			Status);
		Status = STATUS_IO_OPERATION_TIMEOUT;
	}
	else {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:wait complete for Send Work Request Command Id %hu with Status 0x%x!\n",
			__FUNCTION__,
			__LINE__,
			((PNVME_COMPLETION_QUEUE_ENTRY)psAdminReqResp->sRcvBuffer.pvBuffer)->usCommandID,
			Status);
		if (NT_ERROR(psSndWkReq->Status)) {
			Status = psSndWkReq->Status;
		}
	}

	NVMeoFRdmaReleaseSendWorkRequest(psController, NVME_ADMIN_QUEUE_ID, psSndWkReq);
	return (Status);
}

static
NTSTATUS
NVMeoFRdmaAllocateCmd(__in    PNVMEOF_FABRIC_CONTROLLER psController,
	__in    ULONG  addnlLen,
	__in    USHORT usQId,
	__inout PVOID* pvCmd,
	__inout ULONG* pulCmdLen)
{
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(usQId);

	NTSTATUS Status = STATUS_SUCCESS;
	*pvCmd = (PNVME_CMD)NVMeoFRdmaAllocateNpp((sizeof(NVME_CMD) + addnlLen));
	if (!*pvCmd) {
		*pvCmd = NULL;
		*pulCmdLen = 0;
		return (STATUS_INSUFFICIENT_RESOURCES);
	}

	memset(*pvCmd, 0, sizeof(NVME_CMD) + addnlLen);
	*pulCmdLen = (sizeof(NVME_CMD) + addnlLen);
	return (Status);
}

static
PNVME_CMD
NVMeoFRdmaGetNvmeCmd(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PVOID Cmd)
{
	UNREFERENCED_PARAMETER(psController);
	return (PNVME_CMD)Cmd;
}

static
NTSTATUS
NVMeoFRdmaAllocateResponse(__in    PNVMEOF_FABRIC_CONTROLLER psController,
	__in    ULONG  ulAddnlLen,
	__in    USHORT usQId,
	__inout PVOID* pvResp,
	__inout ULONG* pulRespLen)
{
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(usQId);
	NTSTATUS Status = STATUS_SUCCESS;
	PNVME_CMD pvNVMePdu = (PNVME_CMD)NVMeoFRdmaAllocateNpp((sizeof(*pvNVMePdu) + ulAddnlLen));
	if (pvNVMePdu) {
		RtlZeroMemory(pvNVMePdu, sizeof(*pvNVMePdu));
		*pvResp = pvNVMePdu;
		*pulRespLen = (sizeof(*pvNVMePdu) + ulAddnlLen);
	}
	else {
		*pvResp = NULL;
		*pulRespLen = 0;
		Status = STATUS_INSUFFICIENT_RESOURCES;
	}
	return (Status);
}

static
NTSTATUS
NVMeoFRdmaFreePDU(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in USHORT usQId,
	__in PVOID  pvPdu)
{
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(usQId);
	NVMeoFRdmaFreeNpp(pvPdu);
	return STATUS_SUCCESS;
}

static
PNVME_COMPLETION_QUEUE_ENTRY
NVMeoFRdmaGetNvmeCQE(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PVOID pvResp)
{
	UNREFERENCED_PARAMETER(psController);
	return (PNVME_COMPLETION_QUEUE_ENTRY)pvResp;
}

static
NTSTATUS
NVMeoFRdmaGetCQEOrData(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PVOID pvResp,
	__in PVOID* pvDataOrCQE,
	__in ULONG* ulDataLen,
	__in PUCHAR bIsCQE)
{
	UNREFERENCED_PARAMETER(psController);
	if (!pvDataOrCQE || !bIsCQE || !ulDataLen)
		return STATUS_INVALID_PARAMETER;
	*pvDataOrCQE = pvResp;
	*bIsCQE = TRUE;
	return (STATUS_SUCCESS);
}

static
VOID
NVMeoFRdmaInitializeAdminPduHeader(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PVOID pvRdmaPdu,
	__in ULONG ulRdmaPduLen)
{
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(pvRdmaPdu);
	UNREFERENCED_PARAMETER(ulRdmaPduLen);
}

static
USHORT
NVMeoFRdmaGetAdminCommandID(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	ULONG cmdID = _InterlockedIncrement((PLONG)&psController->sSession.psAdminQueue->lNextCmdID) - 1;
	if ((cmdID & (MAX_CMD_ID - 1)) == NVME_ASYNC_EVENT_COMMAND_ID) {  //Skip NVME_ASYNC_EVENT_COMMAND_ID as it is already sent to target for response.
		cmdID = _InterlockedIncrement((PLONG)&psController->sSession.psAdminQueue->lNextCmdID) - 1;
	}
	return (USHORT)(cmdID & (MAX_CMD_ID - 1));
}

static
NTSTATUS
NVMeoFRdmaNvmeofFlushAllQueue(__in PNVMEOF_FABRIC_CONTROLLER psController,
                              __in NTSTATUS Status)
{
	USHORT         ulCount = 1;

	for (; (ulCount < (psController->sSession.lSQQueueCount + 1)); ulCount++) {
		PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulCount);
		if (psSQ) {
			if (psSQ->SQOps._SQFlushAllIOReq) {
				psSQ->SQOps._SQFlushAllIOReq(psController,
					psSQ,
					Status);
			}
		}
	}

	return STATUS_SUCCESS;
}

FORCEINLINE
VOID
NVMeoFRdmaNvmeofAssignUnaligned24(const ULONG ulVal, PUCHAR pucDest)
{
	*pucDest++ = (UCHAR)ulVal;
	*pucDest++ = (UCHAR)(ulVal >> 8);
	*pucDest++ = (UCHAR)(ulVal >> 16);
}

static
NTSTATUS
NVMeoFRdmaNvmeofXportSetSG(__in PNVMEOF_FABRIC_CONTROLLER                 psController,
	__in ULONG                              ulQId,
	__inout PNVME_CMD                       psNvmeCmd,
	__in ULONGLONG                          ullAddress,
	__in ULONG                              Length,
	__in UINT32                             uiRemoteMemoryToken,
	__in NVMEOF_COMMAND_DESCRIPTORT_TYPE ulCmdDesc)
{
	UNREFERENCED_PARAMETER(ulQId);
	UNREFERENCED_PARAMETER(psController);

	switch (ulCmdDesc) {
	    case NVMEOF_COMMAND_DESCRIPTORT_TYPE_NULL:
	    {
	    	PNVME_SGL_DATA_BLOCK_DESCRIPTOR psDesc = &psNvmeCmd->sGeneric.sDataPtr.DataBlockDiscriptor;
	    	psDesc->Address = ullAddress;
	    	psDesc->Length = Length;
	    	psDesc->SGLType = (NVME_KEYED_SGL_DESCRIPTOR_DATA << 4);
	    	break;
	    }
	    case NVMEOF_COMMAND_DESCRIPTORT_TYPE_INLINE:
	    {
	    	PNVME_SGL_DATA_BLOCK_DESCRIPTOR psDesc = &psNvmeCmd->sGeneric.sDataPtr.DataBlockDiscriptor;
	    	psDesc->Address = ullAddress;
	    	psDesc->Length = Length;
	    	psDesc->SGLType = (NVME_SGL_DESCRIPTOR_DATA << 4) | NVME_SGL_FMT_OFFSET;
	    	break;
	    }
	    case NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_SINGLE:
	    {
	    	PNVME_KEYED_SGL_DATA_BLOCK_DESCRIPTOR psDesc = &psNvmeCmd->sGeneric.sDataPtr.KeyedSGLDataBlkDiscriptor;
	    	psDesc->Address = ullAddress;
	    	NVMeoFRdmaNvmeofAssignUnaligned24(Length, psDesc->ucaLength);
	    	*(PUINT32)psDesc->Key = uiRemoteMemoryToken;
	    	psDesc->SGLType = (NVME_KEYED_SGL_DESCRIPTOR_DATA << 4);
	    	break;
	    }
	    case NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_MULTIPLE:
	    {
	    	PNVME_KEYED_SGL_DATA_BLOCK_DESCRIPTOR psDesc = &psNvmeCmd->sGeneric.sDataPtr.KeyedSGLDataBlkDiscriptor;
	    	psDesc->Address = ullAddress;
	    	NVMeoFRdmaNvmeofAssignUnaligned24(Length, psDesc->ucaLength);
	    	*(PUINT32)psDesc->Key = uiRemoteMemoryToken;
	    	psDesc->SGLType = (NVME_KEYED_SGL_DESCRIPTOR_DATA << 4) | NVME_SGL_FMT_INVALIDATE;
	    	break;
	    }
	}
	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaInitXportPipe(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVMEOF_QUEUE psAdminQueue = &psController->sSession.sAdminQueue;

	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminSubmitRequestSyncAsync = NVMeoFRdmaAdminSubmitRequestSyncAsync;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminAllocateRequest = NVMeoFRdmaAllocateCmd;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminGetNvmeRequest = NVMeoFRdmaGetNvmeCmd;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminAllocateResponse = NVMeoFRdmaAllocateResponse;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminFreeRequest = NVMeoFRdmaFreePDU;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminFreeResponse = NVMeoFRdmaFreePDU;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminGetNvmeCQE = NVMeoFRdmaGetNvmeCQE;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminInitializeXportHeader = NVMeoFRdmaInitializeAdminPduHeader;
	psAdminQueue->psFabricCtx->psRdmaCtx->sAdminOps._AdminQChangeEventsState = NULL;

	psAdminQueue->psParentController = psController;

	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaPrepareNVMeConnectCommand(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                    __in PNVME_CMD                 psNVMeConnectCmd,
                                    __in USHORT                    usQId,
                                    __in USHORT                    usSQSize,
                                    __in USHORT                    usCommandID,
                                    __in ULONG                     ulDataSz)
{
	PNVME_CMD_FLAGS psFlags = (PNVME_CMD_FLAGS)&psNVMeConnectCmd->sConnect.ucResvd1;
	psFlags->sOptions.ucRprOrSglForXfer = NVME_CMD_SGL_METABUF;
	psNVMeConnectCmd->sConnect.ucOpCode = NVME_OVER_FABRICS_COMMAND;
	psNVMeConnectCmd->sConnect.ucFabricCmdType = NVMEOF_TYPE_CONNECT;
	psNVMeConnectCmd->sConnect.usRecordFormat = 0;
	psNVMeConnectCmd->sConnect.ucCAttr = 0;
	psNVMeConnectCmd->sConnect.usCommandId = usCommandID;
	psNVMeConnectCmd->sConnect.usQId = usQId;
	psNVMeConnectCmd->sConnect.usSQSize = usSQSize;
	psNVMeConnectCmd->sConnect.ulKATO = (psController->ulKATO) ? PDS_SEC_TO_MS(psController->ulKATO) : 0;
	NVMeoFRdmaNvmeofXportSetSG(psController,
		                       usQId,
		                       psNVMeConnectCmd,
		                       0,
		                       ulDataSz,
		                       0,
		                       NVMEOF_COMMAND_DESCRIPTORT_TYPE_INLINE);
}

static
NTSTATUS
NVMeoFRdmaPrepareNVMeConnectCommandData(__in PNVMEOF_FABRIC_CONTROLLER  psController,
	__in PNVMOF_CONNECT_DATA psConnReqData,
	__in USHORT              usQId)
{
	NTSTATUS Status = STATUS_SUCCESS;
	psConnReqData->sHostId = psController->sHostID;
	psConnReqData->usCtrlId = (usQId == NVME_ADMIN_QUEUE_ID) ?
		NVMEOF_DISCOVERY_CONTROLLER_ID : psController->usControllerID;


	Status = RtlStringCchCopyA(psConnReqData->caSubsysNQN, sizeof(psConnReqData->caSubsysNQN), psController->caSubsystemNQN);
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:RtlStringCchCopyA Failed with Status = 0x%x\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:RtlStringCchCopyA Failed with Status = 0x%x\n", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

	Status = RtlStringCchCopyA(psConnReqData->caHostNQN, sizeof(psConnReqData->caHostNQN), psController->caHostNQN);
	if (!NT_SUCCESS(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:RtlStringCchCopyA Failed with Status = 0x%x\n", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:RtlStringCchCopyA Failed with Status = 0x%x\n", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

}

static
PNVME_CMD
NVMeoFRdmaAllocateConnectRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                 __in USHORT                    usQId,
                                 __in USHORT                    usSQSize,
                                 __in USHORT                    usCommandID,
                                 __out PULONG                   pulCmdSz)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMOF_CONNECT_DATA psConnReqData = NULL;
	ULONG ulPduHdrSz = psController->ulAlignment ?
		ALIGN_DOWN_BY(sizeof(NVME_CMD), psController->ulAlignment) :
		sizeof(NVME_CMD);
	ULONG ulDataSz = psController->ulAlignment ?
		ALIGN_DOWN_BY(sizeof(NVMOF_CONNECT_DATA), psController->ulAlignment) :
		sizeof(NVMOF_CONNECT_DATA);
	ULONG ulSzToAllocate = ulPduHdrSz + ulDataSz;
	PNVME_CMD psConnReq = NULL;

	*pulCmdSz = 0;
	psConnReq = (PNVME_CMD)NVMeoFRdmaAllocateNpp(ulSzToAllocate);
	if (!psConnReq) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:Failed to allocate NVMeoF Connect Request!", __FUNCTION__);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:Failed to allocate NVMeoF Connect Request!", __FUNCTION__);
		return (NULL);
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Sizeof NVMeoRDMA_PDU:0x%x\n", __FUNCTION__, __LINE__, ulPduHdrSz);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Sizeof Connect NVMeoRDMA_PDU:0x%x\n", __FUNCTION__, __LINE__, ulDataSz);
	RtlZeroMemory(psConnReq, ulSzToAllocate);

	//Setting up NVME Connect parameter
	psConnReqData = (PNVMOF_CONNECT_DATA)(psConnReq + 1);
	Status =
		NVMeoFRdmaPrepareNVMeConnectCommandData(psController,
			                                    psConnReqData,
			                                    usQId);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Error in preparing NVMeoF Connect Request Data %#x!", __FUNCTION__, __LINE__, Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Error in preparing NVMeoF Connect Request Data %#x!", __FUNCTION__, __LINE__, Status);
		return (NULL);
	}

	//Setting up NVME Connect command parameter
	NVMeoFRdmaPrepareNVMeConnectCommand(psController,
		psConnReq,
		usQId,
		usSQSize,
		usCommandID,
		ulDataSz);

	*pulCmdSz = ulSzToAllocate;

	return (psConnReq);
}

static
PNVME_CMD
NVMeoFRdmaAllocateAdminDisconnectRequest(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PNVME_CMD psDisconnectReq = NVMeoFRdmaAllocateNpp(sizeof(NVME_CMD));
	if (!psDisconnectReq)
		return NULL;

	UNREFERENCED_PARAMETER(psController);
	RtlZeroMemory(psDisconnectReq, sizeof(*psDisconnectReq));
	psDisconnectReq->sDisconnect.ucOpCode = NVME_OVER_FABRICS_COMMAND;
	psDisconnectReq->sDisconnect.ucFabricCmdType = NVMEOF_TYPE_DISCONNECT;
	psDisconnectReq->sDisconnect.usCommandId = NVMeoFRdmaGetAdminCommandID(psController);
	psDisconnectReq->sDisconnect.usRecordFormat = 0;
	return (PNVME_CMD)psDisconnectReq;
}

static
USHORT
NVMeoFRdmaGetDataFromResp(__in PNVMEOF_FABRIC_CONTROLLER psController,
                          __in PNVME_CMD psNvmePdu,
                          __in PVOID* pvDataPtr)
{
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(pvDataPtr);
	UNREFERENCED_PARAMETER(psNvmePdu);
	return (USHORT)0;
}

static
UCHAR
NVMeoFRdmaIsRespData(__in PNVMEOF_FABRIC_CONTROLLER psController,
                     __in PNVME_CMD psRdmaPdu)
{
	UNREFERENCED_PARAMETER(psController);
	UNREFERENCED_PARAMETER(psRdmaPdu);
	return (TRUE);
}

static
NTSTATUS
NVMeoFRdmaSetupAdminConnection(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;
	ULONG ulReqSz = 0;
	PNVMOF_CONNECT_DATA psConnectionRequestData = NULL;
	PNVME_CMD psConnectionRequest = NULL;
	ULONG ulDataSz = psController->ulAlignment ?
		ALIGN_DOWN_BY(sizeof(NVMOF_CONNECT_DATA), psController->ulAlignment) :
		sizeof(NVMOF_CONNECT_DATA);
	USHORT usCommandID = NVMeoFRdmaGetAdminCommandID(psController);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	Status =
		psController->sFabricOps._FabricNVMeoFRequestsAllocate(psController,
			NVME_ADMIN_QUEUE_ID,
			&psAdminReqResp,
			ulDataSz,
			0);
	if (NT_ERROR(Status)) {
		return STATUS_NO_MEMORY;
	}

	psConnectionRequestData =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommandData(psController, psAdminReqResp);
	psConnectionRequest =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psController, psAdminReqResp);

	Status =
		NVMeoFRdmaPrepareNVMeConnectCommandData(psController,
                                                psConnectionRequestData,
                                                NVME_ADMIN_QUEUE_ID);
	NVMeoFRdmaPrepareNVMeConnectCommand(psController,
		psConnectionRequest,
		NVME_ADMIN_QUEUE_ID,
		(NVME_ADMIN_QUEUE_REQUEST_COUNT - 1),
		usCommandID,
		ulDataSz);

	psAdminReqResp->ulFlags |= PDS_NVMEOF_ADMIN_REQUEST_TYPE_SYNC;

	Status = NVMeoFRdmaAdminSubmitRequestSyncAsync(psController, usCommandID, psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe = psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psController, psAdminReqResp);;
		//NVMeoFDisplayCQE(__FUNCTION__, &psAdminReqResp->sCqe);
		if (!psCqe->sStatus.sStatusField.SC && psCqe->sStatus.sStatusField.SCT) {
			psController->usControllerID = psCqe->sCmdRespData.sCmdSpecData.ulDW0 & 0xFFFF;
		}
	}

	psController->sFabricOps._FabricNVMeoFRequestsFree(psController, psAdminReqResp);

	return (Status);
}

static
FORCEINLINE
PNVMEOF_QUEUE
NVMeoFRdmaGetIOQFromQId(__in PNVMEOF_FABRIC_CONTROLLER psController,
                        __in ULONG ulQId)
{
	if (ulQId == 0) {
		return psController->sSession.psAdminQueue;
	} else {
		PNVMEOF_FABRIC_CONTROLLER_CONNECT psConnectController = psController;
		if (ulQId > (ULONG)(psConnectController->sSession.lSQQueueCount)) {
			return NULL;
		} else {
			USHORT usNumaNode = (USHORT)((ulQId-1) / psConnectController->sSession.usIOQPerNumaNode);
			USHORT usIOQNumaIndex = (USHORT)((ulQId-1) % psConnectController->sSession.usIOQPerNumaNode);
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Found Send Q NUMA Node %hu index %hu QId %u Queue idx %hu!\n", __FUNCTION__, __LINE__,
                           usNumaNode,
                           usIOQNumaIndex,
                           ulQId,
                           psController->sSession.psNUMACtx[usNumaNode]->sSQ[usIOQNumaIndex].usQId);
			return &psController->sSession.psNUMACtx[usNumaNode]->sSQ[usIOQNumaIndex];
		}
	}
}

static
PNVMEOF_RDMA_WORK_REQUEST
NVMeoFRdmaIOQGetRequestByCmdId(__in PNVMEOF_QUEUE psSQ,
                               __in USHORT usCmdID,
                               __in BOOLEAN bRemove)
{
	UNREFERENCED_PARAMETER(bRemove);
#ifdef NVMEOF_RDMA_USE_HASH_FOR_IO
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = psSQ->ppvIoReqTable[usCmdID];
	if (psRdmaIoReq) {
		NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Found request for cmd %hu!\n", __FUNCTION__, __LINE__, usCmdID);
		if (psRdmaIoReq->usCommandID == usCmdID) {
			if (bRemove) {
				psRdmaIoReq = _InterlockedExchangePointer((volatile PVOID*)&psSQ->ppvIoReqTable[usCmdID], (PVOID)NULL);
			}
			return psRdmaIoReq;
		}
		else {
			ASSERT(0);
		}
	}
	else if (!psRdmaIoReq) {
		ASSERT(0);
	}
#else
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;
	for (psRdmaIoReq = psSQ->psSubmissionWorkReq; psRdmaIoReq < (psSQ->psSubmissionWorkReq + psSQ->ulQDepth); psRdmaIoReq++) {
		if (psRdmaIoReq->usCommandID == usCmdID) {
			NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Found request for cmd %hu!\n", __FUNCTION__, __LINE__, usCmdID);
			return psRdmaIoReq;
		}
	}
#endif
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: IO Request Not Found cmd %hu!\n", __FUNCTION__, __LINE__, usCmdID);
	NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d: IO Request Not Found cmd %hu!\n", __FUNCTION__, __LINE__, usCmdID);
	return NULL;
}

static
PNVMEOF_QUEUE
NVMeoFRdmaGetNextNumaIOQueue(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	if (psController->sSession.psNUMACtx) {
		USHORT usNumaNode = KeGetCurrentNodeNumber();
		PNVMEOF_QUEUE psSQ = NULL;
		USHORT  usNumaQId = 0;
		usNumaQId = (USHORT)((_InterlockedIncrement64(&psController->sSession.psNUMACtx[usNumaNode]->lCurrentOutstandingRequest) - 1) &
			(USHORT)(psController->sSession.usIOQPerNumaNode - 1));
		psSQ = psController->sSession.psNUMACtx[usNumaNode]->psSQ;
		if (psSQ) {
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:NUMA NODE %hu NUMA QID 0x%hu With QID %hu!\n", __FUNCTION__, __LINE__, usNumaNode, usNumaQId, psSQ->usQId);
			return (&psSQ[usNumaQId]);
		} else {
			return (NULL);
		}
	}

	return NULL;
}

static
VOID
NVMeoFRdmaIOQReleaseOneSubmissionRequestCommandBuffer(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                                      __in PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest)
{
	psSubmissionRequest->psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_RELEASE_LAM;
	psSubmissionRequest->psNdkWorkReq->sReleaseLamReq.psLam = psSubmissionRequest->psBufferMdlMap->psLamSglMap->psLaml;
	PdsNdkReleaseLam(psController->sSession.pvSessionFabricCtx, psSubmissionRequest->psNdkWorkReq);

	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);

	if (psSubmissionRequest->psBufferMdlMap->psBufferMdl) {
		IoFreeMdl(psSubmissionRequest->psBufferMdlMap->psBufferMdl);
		psSubmissionRequest->psBufferMdlMap->psBufferMdl = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psBufferMdlMap->pvBuffer) {
		NVMeoFRdmaFreeNVMeCmd(psController, psSubmissionRequest->psBufferMdlMap->pvBuffer);
		psSubmissionRequest->psBufferMdlMap->pvBuffer = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psBufferMdlMap->psLamSglMap->psSgl) {
		NVMeoFRdmaFreeSGL(psController, psSubmissionRequest->psBufferMdlMap->psLamSglMap->psSgl);
		psSubmissionRequest->psBufferMdlMap->psLamSglMap->psSgl = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psBufferMdlMap->psLamSglMap->psLaml) {
		NVMeoFRdmaFreeLAM(psController, psSubmissionRequest->psBufferMdlMap->psLamSglMap->psLaml);
		psController, psSubmissionRequest->psBufferMdlMap->psLamSglMap->psLaml = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psBufferMdlMap->psLamSglMap) {
		NVMeoFRdmaFreeLamSglMap(psController, psSubmissionRequest->psBufferMdlMap->psLamSglMap);
		psSubmissionRequest->psBufferMdlMap->psLamSglMap = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}
}

static
VOID
NVMeoFRdmaIOQReleaseOneSubmissionRequestDataBuffer(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest)
{
	PNDK_MR psNdkMr = psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psNdkMr;
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	KeResetEvent(&psSubmissionRequest->sCompletionCtx.sEvent);
	Status =
		psNdkMr->Dispatch->NdkDeregisterMr(psNdkMr,
			NVMeoFRdmaDeregisterMrCompletion,
			&psSubmissionRequest->sCompletionCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&psSubmissionRequest->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, NULL);
	}
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	KeResetEvent(&psSubmissionRequest->sCompletionCtx.sEvent);
	psSubmissionRequest->psNdkWorkReq->ulType = NDK_WREQ_PD_CLOSE_MR;
	psSubmissionRequest->psNdkWorkReq->sCloseFRMRReq.psNdkMr = psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psNdkMr;
	psSubmissionRequest->psNdkWorkReq->sCloseFRMRReq.fnCloseCompletion = NVMeoFRdmaCloseMrCompletion;
	psSubmissionRequest->psNdkWorkReq->sCloseFRMRReq.pvContext = &psSubmissionRequest->sCompletionCtx;
	PdsNdkCloseMemoryRegionAsync(psController->sSession.pvSessionFabricCtx, psSubmissionRequest->psNdkWorkReq);
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	KeWaitForSingleObject(&psSubmissionRequest->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, NULL);
	if (psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psSgl) {
		NVMeoFRdmaFreeSGL(psController, psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psSgl);
		psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psSgl = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psLaml) {
		NVMeoFRdmaFreeDataLAM(psController, psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psLaml);
		psSubmissionRequest->psDataBufferMdlMap->psLamSglMap->psLaml = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psDataBufferMdlMap->psLamSglMap) {
		NVMeoFRdmaFreeLamSglMap(psController, psSubmissionRequest->psDataBufferMdlMap->psLamSglMap);
		psSubmissionRequest->psDataBufferMdlMap->psLamSglMap = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
}

static
VOID
NVMeoFRdmaIOQFreeOneSubmissionRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest)
{
	if (psSubmissionRequest->psBufferMdlMap) {
		NVMeoFRdmaIOQReleaseOneSubmissionRequestCommandBuffer(psController, psSubmissionRequest);
		NVMeoFRdmaFreeBufferMdlMap(psController, psSubmissionRequest->psBufferMdlMap);
		psSubmissionRequest->psBufferMdlMap = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psDataBufferMdlMap) {
		NVMeoFRdmaIOQReleaseOneSubmissionRequestDataBuffer(psController, psSubmissionRequest);
		NVMeoFRdmaFreeBufferMdlMap(psController, psSubmissionRequest->psDataBufferMdlMap);
		psSubmissionRequest->psDataBufferMdlMap = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psNdkWorkReq) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, psSubmissionRequest->psNdkWorkReq);
		psSubmissionRequest->psNdkWorkReq = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}

	if (psSubmissionRequest->psCqe) {
		NVMeoFRdmaFreeNVMeCqe(psController, psSubmissionRequest->psCqe);
		psSubmissionRequest->psNdkWorkReq = NULL;
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: here!\n", __FUNCTION__, __LINE__);
	}
}

static
NTSTATUS
NVMeoFRdmaSetupIOCmdRespLamAndSgl(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                  __in PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PMDL psMdl = psSubmissionRequest->psBufferMdlMap->psBufferMdl;
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl = psSubmissionRequest->psBufferMdlMap->psLamSglMap;
	PNDK_WORK_REQUEST psNdkWorkReq = psSubmissionRequest->psNdkWorkReq;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));

	NVMeoFRdmaGetMdlInfo(psMdl, MmGetMdlVirtualAddress(psMdl), (PULONG)&psLamSgl->ulCurrentLamCbSize);
	psLamSgl->psMdl = psMdl;

	psNdkWorkReq->sBuildLamReq.psMdl = psMdl;
	psNdkWorkReq->sBuildLamReq.psLam = psLamSgl->psLaml;
	psNdkWorkReq->sBuildLamReq.ulLamCbSize = psLamSgl->ulCurrentLamCbSize;
	psNdkWorkReq->sBuildLamReq.szMdlBytesToMap = MmGetMdlByteCount(psMdl);
	psNdkWorkReq->fnCallBack = NULL;
	psNdkWorkReq->pvCallbackCtx = NULL;
	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_BUILD_LAM;
	Status = PdsNdkBuildLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:BuildLam Failed With Status 0x%x!\n", __FUNCTION__, __LINE__, Status);
		return Status;
	}

	psLamSgl->usSgeCount =
		(USHORT)NVMeoFRdmaPopulateSglFromLam(psLamSgl->psLaml,
			                                 psLamSgl->psSgl,
			                                 psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->psNdkQP->uiPrivilagedMemoryToken,
			                                 psNdkWorkReq->sBuildLamReq.ulFboLAM,
			                                 MmGetMdlByteCount(psMdl));
	psLamSgl->usInitialized = TRUE;
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return Status;
}

static
VOID
NVMeoFRdmaReleaseIOCmdRespLamAndSgl(__in    PNVMEOF_FABRIC_CONTROLLER              psController,
	__inout __in PNVMEOF_RDMA_WORK_REQUEST psReceiveReq)
{
	PNDK_WORK_REQUEST psNdkWorkReq = psReceiveReq->psNdkWorkReq;
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));
	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_RELEASE_LAM;
	psNdkWorkReq->sReleaseLamReq.psLam = psReceiveReq->psBufferMdlMap->psLamSglMap->psLaml;
	PdsNdkReleaseLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

static
NTSTATUS
NVMeoFRdmaIOQSetupOneSubmissionRequestCommandBuffer(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                                    __in  PNVMEOF_QUEUE psSQ,
                                                    __in PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest)
{
	UNREFERENCED_PARAMETER(psSQ);
	PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psBufferMdlMap = psSubmissionRequest->psBufferMdlMap;

	psBufferMdlMap->pvBuffer = NVMeoFRdmaAllocateNVMeCmd(psController);
	if (!psBufferMdlMap->pvBuffer) {
		return STATUS_NO_MEMORY;
	}

	psBufferMdlMap->ulBufferLength = sizeof(NVME_CMD);

	RtlZeroMemory(psBufferMdlMap->pvBuffer, sizeof(NVME_CMD));

	psBufferMdlMap->psBufferMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(psBufferMdlMap->pvBuffer,
			sizeof(NVME_CMD));
	if (!psBufferMdlMap->psBufferMdl) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	psBufferMdlMap->psLamSglMap = NVMeoFRdmaAllocateLamSglMap(psController);
	if (!psBufferMdlMap->psLamSglMap) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(psBufferMdlMap->psLamSglMap, sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING));

	psBufferMdlMap->psLamSglMap->psSgl = NVMeoFRdmaAllocateSGL(psController);
	if (!psBufferMdlMap->psLamSglMap) {
		return STATUS_NO_MEMORY;
	}

	psBufferMdlMap->psLamSglMap->psLaml = NVMeoFRdmaAllocateLAM(psController);
	if (!psBufferMdlMap->psLamSglMap->psLaml) {
		return STATUS_NO_MEMORY;
	}


	return
		NVMeoFRdmaSetupIOCmdRespLamAndSgl(psController, psSubmissionRequest);
}

static
NTSTATUS
NVMeoFRdmaIOQSetupOneSubmissionRequestDataBuffer(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psDataBufferFromRequest)
{
	psDataBufferFromRequest->psLamSglMap = NVMeoFRdmaAllocateLamSglMap(psController);
	if (!psDataBufferFromRequest->psLamSglMap) {
		return STATUS_NO_MEMORY;
	}
	RtlZeroMemory(psDataBufferFromRequest->psLamSglMap, sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING));

	psDataBufferFromRequest->psLamSglMap->psSgl = NVMeoFRdmaAllocateSGL(psController);
	if (!psDataBufferFromRequest->psLamSglMap->psSgl) {
		return STATUS_NO_MEMORY;
	}

	psDataBufferFromRequest->psLamSglMap->psLaml = NVMeoFRdmaAllocateDataLAM(psController);
	if (!psDataBufferFromRequest->psLamSglMap->psLaml) {
		return STATUS_NO_MEMORY;
	}

	psDataBufferFromRequest->ulBufferFlag = 0;

	return
		NVMeoFRdmaCreateAndInitializeMr(psController,
                                        psDataBufferFromRequest->psLamSglMap);
}

static
NTSTATUS
NVMeoFRdmaIOQSetupOneSubmissionRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                       __in  PNVMEOF_QUEUE psSQ,
                                       __in PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest)
{
	NTSTATUS Status = STATUS_SUCCESS;
	psSubmissionRequest->psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psSubmissionRequest->psNdkWorkReq) {
		return STATUS_NO_MEMORY;
	}

	psSubmissionRequest->psBufferMdlMap = NVMeoFRdmaAllocateBufferMdlMap(psController);
	if (!psSubmissionRequest->psBufferMdlMap) {
		return STATUS_NO_MEMORY;
	}

	Status =
		NVMeoFRdmaIOQSetupOneSubmissionRequestCommandBuffer(psController,
                                                            psSQ,
                                                            psSubmissionRequest);
	if (NT_ERROR(Status)) {
		NVMeoFRdmaFreeBufferMdlMap(psController, psSubmissionRequest->psBufferMdlMap);
		return (Status);
	}

	psSubmissionRequest->psDataBufferMdlMap = NVMeoFRdmaAllocateBufferMdlMap(psController);
	if (!psSubmissionRequest->psDataBufferMdlMap) {
		return STATUS_NO_MEMORY;
	}

	Status =
		NVMeoFRdmaIOQSetupOneSubmissionRequestDataBuffer(psController,
			psSubmissionRequest->psDataBufferMdlMap);
	if (NT_ERROR(Status)) {
		return STATUS_NO_MEMORY;
	}

	psSubmissionRequest->psCqe = NVMeoFRdmaAllocateNVMeCqe(psController);
	if (!psSubmissionRequest->psNdkWorkReq) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(psSubmissionRequest->psNdkWorkReq, sizeof(*psSubmissionRequest->psNdkWorkReq));

	return (Status);
}

static
VOID
NVMeoFRdmaIOQFreeSubmissionResources(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in  PNVMEOF_QUEUE psSQ)
{
	PNVMEOF_RDMA_WORK_REQUEST psSubmissionRequest = NULL;

	psSubmissionRequest = psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList;
	for (ULONG ulScqCnt = 0; ulScqCnt < psSQ->psFabricCtx->psRdmaCtx->ulSubmissionWorkRequestCount;
		psSubmissionRequest++, ulScqCnt++) {
		NVMeoFRdmaIOQFreeOneSubmissionRequest(psController, psSubmissionRequest);
	}

	if (psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList) {
		NVMeoFRdmaFreeNpp(psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList);
		psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestList = NULL;
	}

	if (psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults) {
		NVMeoFRdmaFreeNpp(psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults);
		psSQ->psFabricCtx->psRdmaCtx->psSubmissionWorkRequestResults = NULL;
	}

	if (psSQ->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, psSQ->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq);
		psSQ->psFabricCtx->psRdmaCtx->psGetSubmissionResultsNDKWrq = NULL;
	}
}

static
NTSTATUS
NVMeoFRdmaIOQAllocateSubmissionResources(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                         __in  PNVMEOF_QUEUE psSQ)
{
	PNVMEOF_RDMA_WORK_REQUEST psSubmissionWorkReq = NULL;
	ULONG ulScqCnt = 0;
	NTSTATUS Status = 0;

	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionWorkRequestCount = 
		NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestResults = 
		      psSQ->ulQueueDepth;

	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList =
		NVMeoFRdmaAllocateNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionWorkRequestCount * 
			                       sizeof(NVMEOF_RDMA_WORK_REQUEST));
	if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList, 
		          NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionWorkRequestCount * 
		              sizeof(NVMEOF_RDMA_WORK_REQUEST));

	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestResults =
		NVMeoFRdmaAllocateNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionResultsCount * 
			                  sizeof(NDK_RESULT_EX));
	if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestResults) {
		NVMeoFRdmaFreeNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList);
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestResults, 
		          NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionResultsCount *
		              sizeof(NDK_RESULT_EX));

	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetSubmissionResultsNDKWrq = 
		          NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetSubmissionResultsNDKWrq) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetSubmissionResultsNDKWrq, sizeof(NDK_WORK_REQUEST));

	for (psSubmissionWorkReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList;
		(ulScqCnt < NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList);
		psSubmissionWorkReq++, ulScqCnt++) {
		Status = NVMeoFRdmaIOQSetupOneSubmissionRequest(psController, psSQ, psSubmissionWorkReq);
		if (NT_ERROR(Status)) {
			break;
		}

		KeInitializeEvent(&psSubmissionWorkReq->sCompletionCtx.sEvent, NotificationEvent, FALSE);
		psSubmissionWorkReq->ulType = NVMEOF_WORK_REQUEST_TYPE_IO;
		psSubmissionWorkReq->usCommandID = 0xFFFF;
		psSubmissionWorkReq->pvParentQP = psSQ;
		_InterlockedExchange(&psSubmissionWorkReq->lReqState, NVMEOF_WORK_REQUEST_STATE_FREE);
	}

	if (ulScqCnt < NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionWorkRequestCount) {
		NVMeoFRdmaIOQFreeSubmissionResources(psController, psSQ);
		return (Status);
	} else {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Submission Request Allocated %hu allocation for QID %hu!\n", __FUNCTION__, __LINE__,
		               ulScqCnt,
		               psSQ->usQId);
	}

	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaIOQReleaseOneReceiveRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                      __in PNVMEOF_QUEUE psSQ,
                                      __in PNVMEOF_RDMA_WORK_REQUEST psReceiveReq)
{
	UNREFERENCED_PARAMETER(psSQ);
	PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psReceiveBuffer = psReceiveReq->psBufferMdlMap;
	if (psReceiveBuffer) {
		if (psReceiveBuffer->psLamSglMap) {
			if (psReceiveBuffer->psLamSglMap->usInitialized) {
				NVMeoFRdmaReleaseIOCmdRespLamAndSgl(psController, psReceiveReq);
			}

			if (psReceiveBuffer->psLamSglMap->psLaml) {
				NVMeoFRdmaFreeLAM(psController, psReceiveBuffer->psLamSglMap->psLaml);
				psReceiveBuffer->psLamSglMap->psLaml = NULL;
			}

			if (psReceiveBuffer->psLamSglMap->psSgl) {
				NVMeoFRdmaFreeSGL(psController, psReceiveBuffer->psLamSglMap->psSgl);
				psReceiveBuffer->psLamSglMap->psSgl = NULL;
			}

			if (psReceiveBuffer->psLamSglMap) {
				NVMeoFRdmaFreeLamSglMap(psController, psReceiveBuffer->psLamSglMap);
				psReceiveBuffer->psLamSglMap = NULL;
			}
		}

		if (psReceiveBuffer->psBufferMdl) {
			IoFreeMdl(psReceiveBuffer->psBufferMdl);
			psReceiveBuffer->psBufferMdl = NULL;
		}

		if (psReceiveBuffer->pvBuffer) {
			NVMeoFRdmaFreeNVMeCqe(psController, psReceiveBuffer->pvBuffer);
			psReceiveBuffer->pvBuffer = NULL;
		}

		NVMeoFRdmaFreeBufferMdlMap(psController, psReceiveBuffer);
		psReceiveReq->psBufferMdlMap = NULL;
	}

	if (psReceiveReq->psNdkWorkReq) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, psReceiveReq->psNdkWorkReq);
		psReceiveReq->psNdkWorkReq = NULL;
	}
}

static
NTSTATUS
NVMeoFRdmaIOQSetupOneReceiveRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                    __in PNVMEOF_QUEUE psSQ,
                                    __in PNVMEOF_RDMA_WORK_REQUEST psReceiveRequest)
{
	UNREFERENCED_PARAMETER(psSQ);
	PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psReceiveBuffer = psReceiveRequest->psBufferMdlMap;
	psReceiveBuffer->pvBuffer = NVMeoFRdmaAllocateNVMeCqe(psController);
	if (!psReceiveBuffer->pvBuffer) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(psReceiveBuffer->pvBuffer, sizeof(NVME_COMPLETION_QUEUE_ENTRY));
	psReceiveBuffer->ulBufferLength = sizeof(NVME_COMPLETION_QUEUE_ENTRY);
	psReceiveBuffer->psBufferMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(psReceiveBuffer->pvBuffer,
			psReceiveBuffer->ulBufferLength);
	if (!psReceiveBuffer->psBufferMdl) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	psReceiveBuffer->psLamSglMap = NVMeoFRdmaAllocateLamSglMap(psController);
	if (!psReceiveBuffer->psLamSglMap) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(psReceiveBuffer->psLamSglMap, sizeof(NVMEOF_RDMA_SGL_LAM_MAPPING));

	psReceiveBuffer->psLamSglMap->psSgl = NVMeoFRdmaAllocateSGL(psController);
	if (!psReceiveBuffer->psLamSglMap->psSgl) {
		return STATUS_NO_MEMORY;
	}

	psReceiveBuffer->psLamSglMap->psLaml = NVMeoFRdmaAllocateLAM(psController);
	if (!psReceiveBuffer->psLamSglMap->psLaml) {
		return STATUS_NO_MEMORY;
	}

	return
		NVMeoFRdmaSetupIOCmdRespLamAndSgl(psController, psReceiveRequest);
}

static
NTSTATUS
NVMeoFRdmaIOQSetupReceiveRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                 __in PNVMEOF_QUEUE psSQ,
                                 __in PNVMEOF_RDMA_WORK_REQUEST psReceiveRequest)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDK_WORK_REQUEST psNdkWorkReq = NULL;
	psReceiveRequest->psBufferMdlMap = NVMeoFRdmaAllocateBufferMdlMap(psController);
	if (!psReceiveRequest->psBufferMdlMap) {
		return STATUS_NO_MEMORY;
	}

	psReceiveRequest->psNdkWorkReq = NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!psReceiveRequest->psNdkWorkReq) {
		return (STATUS_NO_MEMORY);
	}

	Status =
		NVMeoFRdmaIOQSetupOneReceiveRequest(psController,
                                            psSQ,
                                            psReceiveRequest);
	if (NT_ERROR(Status)) {
		return (Status);
	}

	psNdkWorkReq = psReceiveRequest->psNdkWorkReq;
	psNdkWorkReq->ulType = NDK_WREQ_SQ_RECEIVE;
	psNdkWorkReq->fnCallBack = NULL;
	psNdkWorkReq->pvCallbackCtx = psReceiveRequest;
	psNdkWorkReq->ulFlags = 0;
	psNdkWorkReq->sReceiveReq.psNdkSgl = psReceiveRequest->psBufferMdlMap->psLamSglMap->psSgl;
	psNdkWorkReq->sReceiveReq.usSgeCount = (USHORT)psReceiveRequest->psBufferMdlMap->psLamSglMap->usSgeCount;

	return (Status);
}

static
VOID
NVMeoFRdmaIOQFreeReceiveResources(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                  __in PNVMEOF_QUEUE psSQ)
{
	PNVMEOF_RDMA_WORK_REQUEST psReceiveReq = NULL;

	if (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults) {
		NVMeoFRdmaFreeNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults);
		NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults = NULL;
	}

	if (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq) {
		NVMeoFRdmaFreeNdkWorkRequest(psController, NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq);
		NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq = NULL;
	}

	psReceiveReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList;
	for (ULONG ulRcqCnt = 0; ulRcqCnt < NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveWorkRequestCount;
		psReceiveReq++, ulRcqCnt++) {
		NVMeoFRdmaIOQReleaseOneReceiveRequest(psController,
                                              psSQ,
                                              psReceiveReq);
	}

	if (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList) {
		NVMeoFRdmaFreeNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList);
		NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList = NULL;
	}

}

static
NTSTATUS
NVMeoFRdmaIOQAllocateReceiveResources(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                      __in PNVMEOF_QUEUE psSQ)
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG ulRcqCnt = 0;
	PNVMEOF_RDMA_WORK_REQUEST psReceiveWorkReq = NULL;
	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList = 
		NVMeoFRdmaAllocateNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveWorkRequestCount * 
			                   sizeof(NVMEOF_RDMA_WORK_REQUEST));
	if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList) {
		return STATUS_NO_MEMORY;
	}

	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults =
		NVMeoFRdmaAllocateNpp(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveWorkRequestCount * 
                               sizeof(NDK_RESULT_EX));
	if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults, 
		         (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveWorkRequestCount *
					 sizeof(NDK_RESULT_EX)));

	NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq = 
		NVMeoFRdmaAllocateNdkWorkRequest(psController);
	if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq, 
		          sizeof(*NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq));

	psReceiveWorkReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList;
	for (ulRcqCnt = 0; ulRcqCnt < NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveWorkRequestCount;
		psReceiveWorkReq++, ulRcqCnt++) {
		Status = NVMeoFRdmaIOQSetupReceiveRequest(psController, psSQ, psReceiveWorkReq);
		if (NT_ERROR(Status)) {
			break;
		}

		KeInitializeEvent(&psReceiveWorkReq->sCompletionCtx.sEvent, NotificationEvent, FALSE);
		psReceiveWorkReq->ulType = NVMEOF_WORK_REQUEST_TYPE_IO;
		psReceiveWorkReq->usCommandID = 0xFFFF;
		psReceiveWorkReq->pvParentQP = psSQ;
		_InterlockedExchange(&psReceiveWorkReq->lReqState, NVMEOF_WORK_REQUEST_STATE_FREE);
	}

	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Receive Request Allocated %hu allocation for QID %hu!\n", __FUNCTION__, __LINE__,
                   ulRcqCnt,
                   psSQ->usQId);

	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaIOQSubmitReceiveRequests(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                   __in PNVMEOF_QUEUE psSQ)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_RDMA_WORK_REQUEST psReceiveWorkReq = NULL;
	ULONG ulRcqCnt = 0;

	psReceiveWorkReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestList;
	for (ulRcqCnt = 0, NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llSubmissionWorkRequestSubmitted = 0; 
		      ulRcqCnt < NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveWorkRequestCount;
		      psReceiveWorkReq++, ulRcqCnt++) {
		Status = 
			NVMeoFRdmaSubmitReceiveWorkRequest(psController,
			                                   psSQ->usQId,
			                                   psReceiveWorkReq);
		if (NT_ERROR(Status)) {
			break;
		} else {
			_InterlockedIncrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding);
			_InterlockedIncrement64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llSubmissionWorkRequestSubmitted);
		}
	}

	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to submit Receive Request %u for QID %hu with Status %x!\n", __FUNCTION__, __LINE__,
                       ulRcqCnt,
                       psSQ->usQId,
                       Status);
	} else {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Submitted all %u Receive Request for Q %u!\n", __FUNCTION__, __LINE__,
                       ulRcqCnt,
                       psSQ->usQId);
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaAllocateAndInitializeRdmaIoReq(__in PNVMEOF_QUEUE psSQ)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: ulQDepth 0x%X, hash bucket size 0x%X\n", __FUNCTION__, __LINE__,
		psSQ->ulQDepth,
		psSQ->ulIOHashBucketSz);

#ifdef PDS_RDMA_USE_HASH_FOR_IO
	psSQ->ulIOHashBucketSz = PDS_HASH_BUCKET_SZ;
	psSQ->ppvIoReqTable = NVMeoFRdmaAllocateNpp(sizeof(PNVMEOF_RDMA_WORK_REQUEST) * psSQ->ulIOHashBucketSz);
	if (!psSQ->ppvIoReqTable) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		RtlZeroMemory(psSQ->ppvIoReqTable, (sizeof(PNVMEOF_RDMA_WORK_REQUEST) * psSQ->ulIOHashBucketSz));
	}
#endif

	Status = NVMeoFRdmaIOQAllocateSubmissionResources(psSQ->psParentController, psSQ);
	if (NT_ERROR(Status)) {
		NVMeoFRdmaIOQFreeSubmissionResources(psSQ->psParentController, psSQ);
		NVMeoFRdmaFreeNpp(psSQ->ppvIoReqTable);
		return Status;
	}

	Status = NVMeoFRdmaIOQAllocateReceiveResources(psSQ->psParentController, psSQ);
	if (NT_ERROR(Status)) {
		NVMeoFRdmaIOQFreeSubmissionResources(psSQ->psParentController, psSQ);
		NVMeoFRdmaIOQFreeReceiveResources(psSQ->psParentController, psSQ);
		NVMeoFRdmaFreeNpp(psSQ->ppvIoReqTable);
		return Status;
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaIOQueuesDisconnect(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS       Status = STATUS_SUCCESS;
	ULONG          ulCount = 1;
	PNVMEOF_QUEUE         psSQ = NULL;
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Controller id 0x%X\n", __FUNCTION__, __LINE__, psController->usControllerID);
	for (; (ulCount < (psController->sSession.lSQQueueCount+ 1)); ulCount++) {
		psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulCount);
		if (psSQ) {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Disconnecting IOQP %hu\n", __FUNCTION__, __LINE__, psSQ->usQId);
			if (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP && NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP->psNdkQp) {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Closing IO Queue %hu.\n", __FUNCTION__, __LINE__, psSQ->usQId);
				NdkDisconnectSyncAsyncRdmaSocketQP(psController->sSession.pvSessionFabricCtx, NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP);
			}
		}
	}

	return Status;
}

static
VOID
NVMeoFRdmaIOQWaitForSubmittedReqToComplete(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                           __in PNVMEOF_QUEUE psSQ)
{
	KEVENT sEvent = { 0 };
	UNREFERENCED_PARAMETER(psController);

	KeInitializeEvent(&sEvent, NotificationEvent, FALSE);
	while (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding ||
		   NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lSubmissionWorkRequestOutstanding ||
		   NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lFRMRReqOutstanding) {
		LARGE_INTEGER liWaitTo = { 0 };
		liWaitTo.QuadPart = WDF_REL_TIMEOUT_IN_MS(1000LL);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Outstanding Req Rcv %d Snd %d FRMR %d!\n", __FUNCTION__, __LINE__,
			NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding,
			NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lSubmissionWorkRequestOutstanding,
			NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lFRMRReqOutstanding);
		KeWaitForSingleObject(&sEvent, Executive, KernelMode, FALSE, &liWaitTo);
	}
}

static
NTSTATUS
NVMeoFRdmaIOQReleaseResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS       Status = STATUS_SUCCESS;
	ULONG          ulCount = psController->sSession.lSQQueueCount;
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Controller id 0x%X\n", __FUNCTION__, __LINE__, psController->usControllerID);
	for (ulCount = 1; ulCount < ((ULONG)psController->sSession.lSQQueueCount + 1); ulCount++) {
		PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, ulCount);
		if (psSQ) {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Releasing resources for IOQP %hu\n", __FUNCTION__, __LINE__, psSQ->usQId);
			NVMeoFRdmaIOQWaitForSubmittedReqToComplete(psController, psSQ);
			NVMeoFRdmaIOQFreeSubmissionResources(psSQ->psParentController, psSQ);
			NVMeoFRdmaIOQFreeReceiveResources(psSQ->psParentController, psSQ);
			if (psSQ->ppvIoReqTable) {
				NVMeoFRdmaFreeNpp(psSQ->ppvIoReqTable);
				psSQ->ppvIoReqTable = NULL;
			}
		}
	}

	psController->sSession.lSQQueueCount = 0;

	for (ulCount = 0; (ulCount < psController->sSession.usNumaNodeCount); ulCount++) {
		if (psController->sSession.psNumaSQPair[ulCount].psSQ) {
			NVMeoFRdmaFreeNpp(psController->sSession.psNumaSQPair[ulCount].psSQ);
			psController->sSession.psNumaSQPair[ulCount].psSQ = NULL;
		}
	}


	if (psController->sSession.psNumaSQPair) {
		NVMeoFRdmaFreeNpp(psController->sSession.psNumaSQPair);
		psController->sSession.psNumaSQPair = NULL;
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaIOQDisconnectAndReleaseResources(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NVMeoFRdmaIOQueuesDisconnect(psController);
	NVMeoFRdmaIOQReleaseResources(psController);
	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaNVMeIoResponseCompletion(__in PNVMEOF_QUEUE                         psSQ,
	__in PNVMEOF_RDMA_WORK_REQUEST psRcqRdmaIoRequest,
	__in PNVMEOF_RDMA_WORK_REQUEST psScqRequest,
	__in NTSTATUS                       Status);

static
NTSTATUS
NVMeoFRdmaIOQHandleRcqReq(_In_ PNVMEOF_QUEUE  psSQ,
                          _In_ BOOLEAN bResultEx,
                          _In_ PVOID   pvResult)
{
	PNVMEOF_RDMA_WORK_REQUEST psRcqRdmaIoRequest = NULL;
	PNVMEOF_RDMA_WORK_REQUEST psScqRdmaIoRequest = NULL;
	BOOLEAN                   bResubmitReceive = FALSE;
	NTSTATUS                  Status = ((PNDK_RESULT)pvResult)->Status;
	ULONG                     ulByteXferred = ((PNDK_RESULT)pvResult)->BytesTransferred;
	UINT32                    uiRMToken = 0;

	if (((PNDK_WORK_REQUEST)((PNDK_RESULT)pvResult)->RequestContext)->ulType != NDK_WREQ_SQ_RECEIVE) {
		ASSERT(0);
	}

	if (bResultEx) {
		if (((PNDK_RESULT_EX)pvResult)->Type == NdkOperationTypeReceiveAndInvalidate) {
			uiRMToken = (UINT32)((PNDK_RESULT_EX)pvResult)->TypeSpecificCompletionOutput;
		}
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In IOQ 0x%p QID %hx WrkReq 0x%p Status 0x%x Bytes Xferred %u WkReq type %s!\n",
                   __FUNCTION__,
                   __LINE__,
                   psSQ,
                   psSQ->usQId,
                   ((PNDK_RESULT)pvResult)->RequestContext,
                   Status,
                   ulByteXferred,
                   pcaWorkRequestString[((PNDK_WORK_REQUEST)((PNDK_RESULT)pvResult)->RequestContext)->ulType]);

	psRcqRdmaIoRequest = ((PNDK_WORK_REQUEST)((PNDK_RESULT)pvResult)->RequestContext)->pvCallbackCtx;
	psRcqRdmaIoRequest->Status = Status;
	psRcqRdmaIoRequest->ulByteXferred = ulByteXferred;
	if (NT_SUCCESS(Status)) {
		if (ulByteXferred >= sizeof(NVME_COMPLETION_QUEUE_ENTRY)) {
			NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_VERBOSE,
				                     psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer,
				                     ulByteXferred,
				                     PDS_PRINT_HEX_PER_LINE_8_BYTES);
			psScqRdmaIoRequest =
                NVMeoFRdmaIOQGetRequestByCmdId(psSQ,
                                               ((PNVME_COMPLETION_QUEUE_ENTRY)psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer)->usCommandID,
                                               TRUE);
			if (psScqRdmaIoRequest) {
				_InterlockedOr(&psScqRdmaIoRequest->lReqState, NVMEOF_WORK_REQUEST_STATE_RESPONSE_RECEIVED);
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x incoming RToken 0x%x My RToken 0x%x!\n", __FUNCTION__, __LINE__,
					           psSQ->usQId,
					           psScqRdmaIoRequest,
					           psScqRdmaIoRequest->usCommandID,
					           psScqRdmaIoRequest->lReqState,
					           uiRMToken,
					           psScqRdmaIoRequest->psDataBufferMdlMap->psLamSglMap->uiRemoteMemoryTokenKey);
				if (NVME_IS_IO_CMD(psScqRdmaIoRequest->psBufferMdlMap->pvBuffer)) {
					NVMeoFRdmaNVMeIoResponseCompletion(psSQ,
                                                       psRcqRdmaIoRequest,
                                                       psScqRdmaIoRequest,
                                                       Status);
				} else {
					NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received response (QId:%hu) for non-IO command %hu!\n", __FUNCTION__, __LINE__,
                                   psSQ->usQId,
                                   psScqRdmaIoRequest->usCommandID);
					*psScqRdmaIoRequest->psCqe = *(PNVME_COMPLETION_QUEUE_ENTRY)psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer;
					KeSetEvent(&psScqRdmaIoRequest->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
				}
				bResubmitReceive = TRUE;
				Status = STATUS_SUCCESS;
			} else {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Received Completion for Invalid Command (%hu) Received Bytes %u!\n",
                               __FUNCTION__,
                               __LINE__,
                               ((PNVME_COMPLETION_QUEUE_ENTRY)psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer)->usCommandID,
                               ulByteXferred);
				NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_VERBOSE,
                                         psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer,
                                         ulByteXferred,
                                         PDS_PRINT_HEX_PER_LINE_8_BYTES);
				//NVMeoFDisplayCQE(__FUNCTION__, psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer);
				bResubmitReceive = TRUE;
				Status = STATUS_INVALID_PARAMETER;
				ASSERT(0);
			}
		} else {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Invalid size (%u) of buffer Received!\n",
                           __FUNCTION__,
                           __LINE__,
                           ulByteXferred);
			bResubmitReceive = TRUE;
			Status = STATUS_INVALID_PARAMETER;
		}
	} else {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:QID %hu Receive Completed with Error (0x%X)!\n",
                       __FUNCTION__,
                       __LINE__,
                       psSQ->usQId,
                       Status);
		bResubmitReceive = FALSE;
	}

	if (bResubmitReceive) {
		Status =
			NVMeoFRdmaSubmitReceiveWorkRequest(psSQ->psParentController,
                                               psSQ->usQId,
                                               psRcqRdmaIoRequest);
		if (NT_SUCCESS(Status)) {
			_InterlockedIncrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding);
			_InterlockedIncrement64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llReceiveWorkRequestSubmitted);
		}
	} else {
		_InterlockedDecrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding);
		_InterlockedDecrement64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llReceiveWorkRequestCompleted);
	}

	return (Status);
}

static
VOID
NVMeoFRdmaResubmitSubmissionWorkRequest(__in PNVMEOF_QUEUE  psSQ)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;
	psRdmaIoReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList;
	while (psRdmaIoReq < (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList + psSQ->ulQueueDepth)) {
		if (psRdmaIoReq->pvParentReqCtx &&
			(psRdmaIoReq->lReqState &
				(NVMEOF_WORK_REQUEST_STATE_SUBMITTING |
					NVMEOF_WORK_REQUEST_STATE_SUBMITTED |
					NVMEOF_WORK_REQUEST_STATE_WAITING_FOR_RESPONSE))) {
			NTSTATUS Status =
				NVMeoFRdmaSubmitSendWorkRequest(psSQ->psParentController,
					psSQ->usQId,
					psRdmaIoReq);
			if (NT_ERROR(Status)) {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to resubmit Send Req!!!!!\n", __FUNCTION__, __LINE__);
			}
		}
		psRdmaIoReq++;
	}
}

static
VOID
NVMeoFRdmaIOQPRcqCalback(_In_ PNVMEOF_QUEUE   psSQ,
                         _In_ NTSTATUS CqStatus)
{
	ULONG ulResultsCount = 0;
	BOOLEAN bResubmitSendReqs = FALSE;
	PNDK_WORK_REQUEST psNdkWorkRequest = NULL;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	if (NT_ERROR(CqStatus)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Handle this case!!!!!\n", __FUNCTION__, __LINE__);
	}

	psNdkWorkRequest = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetReceiveResultsNDKWrq;

	RtlZeroMemory(psNdkWorkRequest, sizeof(*psNdkWorkRequest));

	psNdkWorkRequest->ulType = NDK_WREQ_SQ_GET_RECEIVE_RESULTS;
	psNdkWorkRequest->sGetRcqResultReq.pvNdkRcqResultExx = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults;
	psNdkWorkRequest->sGetRcqResultReq.usMaxNumReceiveResults = (USHORT)NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulReceiveResultsCount;
	psNdkWorkRequest->sGetRcqResultReq.usReceiveResultEx = FALSE;
	while (1) {
		ulResultsCount =
			NdkSubmitRequest(psSQ->psParentController->sSession.pvSessionFabricCtx,
                             psSQ->psFabricCtx->psRdmaCtx->psNdkQP,
                             psNdkWorkRequest);
		if (!ulResultsCount) {
			break;
		}
		PNDK_RESULT psResult = NULL;
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Results returned %u!!!!!\n", __FUNCTION__, __LINE__, ulResultsCount);
		for (ULONG i = 0; i < ulResultsCount; i++) {
			BOOLEAN bResultEx = FALSE;
			_InterlockedDecrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding);
			_InterlockedIncrement64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llReceiveWorkRequestCompleted);
			if (psNdkWorkRequest->sGetRcqResultReq.usReceiveResultEx) {
				PNDK_RESULT_EX  psResultEx = &((PNDK_RESULT_EX)NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults)[i];
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QID %hu Resultx QPCtx 0x%llx ReqCtx 0x%llx Xferred %u Status 0x%x type %u (%s) type specific %llx!\n", __FUNCTION__, __LINE__,
                               psSQ->usQId,
                               psResultEx->QPContext,
                               psResultEx->RequestContext,
                               psResultEx->BytesTransferred,
                               psResultEx->Status,
                               psResultEx->Type,
                               PdsNdkGetOperationType(psResultEx->Type),
                               psResultEx->TypeSpecificCompletionOutput);
				psResult = (PNDK_RESULT)psResultEx;
				bResultEx = TRUE;
			}
			else {
				psResult = &((PNDK_RESULT)NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psReceiveWorkRequestResults)[i];
			}

			if (psResult->Status != STATUS_CANCELLED) {
				NTSTATUS Status = STATUS_SUCCESS;
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Rcq Issuing return work request completion handling for type %u!!!!!\n",
					__FUNCTION__,
					__LINE__,
					((PNDK_WORK_REQUEST)psResult->RequestContext)->ulType);
				Status = NVMeoFRdmaIOQHandleRcqReq(psSQ, bResultEx, psResult);
			} else {
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Rcv completed with Cancel status for Q %hu!!!!!\n",
					__FUNCTION__,
					__LINE__,
					psSQ->usQId);
				if (NT_SUCCESS(CqStatus)) {
					if (!psSQ->psParentController->ulControllerState----) {  //Change it to controller state checking instead of disconnection.
						NTSTATUS Status =
							NVMeoFRdmaSubmitReceiveWorkRequest(psSQ->psParentController,
                                                               psSQ->usQId,
                                                               ((PNDK_WORK_REQUEST)psResult->RequestContext)->pvCallbackCtx);
						if (NT_SUCCESS(Status)) {
							_InterlockedIncrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding);
							_InterlockedIncrement64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llReceiveWorkRequestSubmitted);
							NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Resubmitted Rcv requests!!!!!\n",
								__FUNCTION__,
								__LINE__);
							//bResubmitSendReqs = TRUE;
						} else {
							NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Rcv req failed to resubmit for Q %hu!!!!!\n", __FUNCTION__, __LINE__,
								psSQ->usQId);
						}
					} else {
						if (!NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding) {
							NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:All Rcv req cancelled due to disconnect (%u) for Q %hu OS Rcv %ld!!!!!\n", __FUNCTION__, __LINE__,
                                           psSQ->psParentController->ulDisconnectFlag,
                                           psSQ->usQId,
                                           psSQ->lReceiveWorkReqSubmitted);
						}
					}
				} else {
					NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Receive request Cancelled for Q %hu!!!!!\n", __FUNCTION__, __LINE__, psSQ->usQId);
				}
			}
		}
		ulResultsCount = 0;
	}

	if (!psSQ->psParentController->ulDisconnectFlag ||
		NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lReceiveWorkRequestOutstanding) {
		if (bResubmitSendReqs) {
			NVMeoFRdmaResubmitSubmissionWorkRequest(psSQ);
		}

		NdkArmCompletionQueue(psSQ->psParentController->sSession.pvSessionFabricCtx,
                              NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP->psNdkRcq,
                              NDK_CQ_NOTIFY_ANY);
	}
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

static
VOID
NVMeoFRdmaIoFRCompletion(__in PNVMEOF_QUEUE            psSQ,
	__in PNDK_WORK_REQUEST psScqRequest,
	__in NTSTATUS          Status);

static
VOID
NVMeoFRdmaIoSendCompletion(__in PNVMEOF_QUEUE            psSQ,
	__in PNDK_WORK_REQUEST psScqRequest,
	__in NTSTATUS          Status);

static
VOID
NVMeoFRdmaIoCompletion(__in PNVMEOF_QUEUE                         psSQ,
                       __in PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq,
                       __in PNVMEOF_IO_REQ                 psIoReq,
                       __in NTSTATUS                       Status);

static
NTSTATUS
NVMeoFRdmaIOQHandleScqReq(_In_ PNVMEOF_QUEUE psSQ,
                          _In_ PNDK_RESULT   psNdkScqResult)
{
	PNDK_WORK_REQUEST psNdkWorkReq = psNdkScqResult->RequestContext;
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = psNdkWorkReq->pvCallbackCtx;

	if (psNdkWorkReq->ulType > NDK_WREQ_MAX || psNdkWorkReq->ulType == NDK_WREQ_SQ_UNINITIALIZE) {
		ASSERT(0);
	}

	if (psNdkWorkReq->ulType == NDK_WREQ_SQ_RECEIVE) {
		ASSERT(0);
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In IO Queue 0x%p QID %hu WrkReq 0x%p Status 0x%x WkReq type %s!\n",
                   __FUNCTION__,
                   __LINE__,
                   psSQ,
                   psSQ->usQId,
                   psNdkWorkReq,
                   psNdkScqResult->Status,
                   pcaWorkRequestString[psNdkWorkReq->ulType]);

	switch (psNdkWorkReq->ulType) {
	case NDK_WREQ_SQ_SEND:
		if (NVME_IS_IO_CMD(psRdmaIoReq->psBufferMdlMap->pvBuffer)) {
			NVMeoFRdmaIoSendCompletion(psSQ,
                                       psNdkWorkReq,
                                       psNdkScqResult->Status);
		} else {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Non-IO Request sent status %x have to wait for NVMe Response!\n",
                           __FUNCTION__,
                           __LINE__,
                           psNdkScqResult->Status);
			///KeSetEvent(&psRdmaIoReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
		}
		break;
	case NDK_WREQ_SQ_FAST_REGISTER:
		if (NVME_IS_IO_CMD(psRdmaIoReq->psBufferMdlMap->pvBuffer)) {
			NVMeoFRdmaIoFRCompletion(psSQ,
				psNdkWorkReq,
				psNdkScqResult->Status);
		}
		else {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Non-IO Request FRMR status %x!\n",
				__FUNCTION__,
				__LINE__,
				psNdkScqResult->Status);
			KeSetEvent(&psRdmaIoReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
		}
		break;
	case NDK_WREQ_SQ_INVALIDATE:
		if (NVME_IS_IO_CMD(psRdmaIoReq->psBufferMdlMap->pvBuffer)) {
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
				psSQ->usQId,
				psRdmaIoReq,
				psRdmaIoReq->usCommandID,
				psRdmaIoReq->lReqState);
			if (PDS_IS_REQUEST_IN_FREEING(psRdmaIoReq)) {
				PNVMEOF_IO_REQ psIoReq = _InterlockedExchangePointer(&psRdmaIoReq->pvParentReqCtx, (PVOID)NULL);
				if (psIoReq) {
					NVMeoFRdmaIoCompletion(psSQ,
						psRdmaIoReq,
						psIoReq,
						psNdkScqResult->Status);
					NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Completed IO %#p here!\n", __FUNCTION__, __LINE__, psIoReq);
				}
			}
		} else {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Non-IO Request Invalidated status %x!\n",
                           __FUNCTION__,
                           __LINE__,
                           psNdkScqResult->Status);
			KeSetEvent(&psRdmaIoReq->sCompletionCtx.sEvent, IO_NO_INCREMENT, FALSE);
		}
		break;
	case NDK_WREQ_SQ_SEND_INVALIDATE:
	case NDK_WREQ_SQ_READ:
	case NDK_WREQ_SQ_WRITE:
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Unsupported submitted request completion %s Status 0x%x!\n",
                       __FUNCTION__,
                       __LINE__,
                       pcaWorkRequestString[psNdkWorkReq->ulType],
                       psNdkScqResult->Status);
		ASSERT(0);
		break;
	default:
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Invalid WkReq %u(%s) on IOQ 0x%p QID %hu WrkReq 0x%p State %#x Status %#x!\n",
                       __FUNCTION__,
                       __LINE__,
                       psNdkWorkReq->ulType,
                       pcaWorkRequestString[psNdkWorkReq->ulType],
                       psSQ,
                       psSQ->usQId,
                       psNdkWorkReq,
                       psRdmaIoReq->lReqState,
                       psNdkScqResult->Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Invalid WkReq %u(%s) on IOQ 0x%p QID %hu WrkReq 0x%p State %#x Status %#x!\n",
                       __FUNCTION__,
                       __LINE__,
                       psNdkWorkReq->ulType,
                       ((psNdkWorkReq->ulType > NDK_WREQ_SQ_UNINITIALIZE) &&
                       	(psNdkWorkReq->ulType < NDK_WREQ_MAX)) ? pcaWorkRequestString[psNdkWorkReq->ulType] : "",
                       psSQ,
                       psSQ->usQId,
                       psNdkWorkReq,
                       psRdmaIoReq->lReqState,
                       psNdkScqResult->Status);
		break;
	}
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return STATUS_SUCCESS;
}

static
VOID
NVMeoFRdmaIOQPScqCallback(_In_ PNVMEOF_QUEUE psSQ,
                          _In_ NTSTATUS CqStatus)
{
	ULONG ulResultsCount = 0;
	PNDK_WORK_REQUEST psNdkWorkRequest = NULL;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	if (NT_ERROR(CqStatus)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Handle this case!!!!!\n", __FUNCTION__, __LINE__);
	}
	psNdkWorkRequest = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psGetSubmissionResultsNDKWrq;

	RtlZeroMemory(psNdkWorkRequest, sizeof(*psNdkWorkRequest));

	psNdkWorkRequest->ulType = NDK_WREQ_SQ_GET_SUBMISSION_RESULTS;
	psNdkWorkRequest->sGetScqResultReq.psNdkScqResult = 
		NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestResults;
	psNdkWorkRequest->sGetScqResultReq.usMaxNumSendResults = 
		(USHORT)NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->ulSubmissionResultsCount;
	while (1) {
		ulResultsCount =
			NdkSubmitRequest(psSQ->psParentController->sSession.pvSessionFabricCtx,
                             NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP,
                             psNdkWorkRequest);
		if (ulResultsCount) {
			for (ULONG i = 0; i < ulResultsCount; i++) {
				PNDK_RESULT psResult = &NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestResults[i];
				NTSTATUS Status = STATUS_SUCCESS;
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: Resultx QPCtx 0x%llx ReqCtx 0x%llx Req Type %u Xferred %u Status 0x%x!\n",
                               __FUNCTION__,
                               __LINE__,
                               psResult->QPContext,
                               psResult->RequestContext,
                               ((PNDK_WORK_REQUEST)psResult->RequestContext)->ulType,
                               psResult->BytesTransferred,
                               psResult->Status);
				Status =
					NVMeoFRdmaIOQHandleScqReq(psSQ, psResult);
			}
			ulResultsCount = 0;
		} else {
			break;
		}
	}

	NdkArmCompletionQueue(psSQ->psParentController->sSession.pvSessionFabricCtx,
                          NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP->psNdkScq,
                          NDK_CQ_NOTIFY_ANY);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
}

VOID
NVMeoFRdmaIOQPDisconnect(_In_ PNVMEOF_QUEUE psSQP)
{
	PNVMEOF_FABRIC_CONTROLLER psController = psSQP->psParentController;
	_InterlockedExchange(&psSQP->lFlags, SUBMISSION_QUEUE_DISCONNECTED);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Trying to see if disconnect WI can be scheduled 0x%x at Q ID %hx!\n", __FUNCTION__, __LINE__,
		psController->ulDisconnectFlag, psSQP->usQId);
	if (_InterlockedCompareExchange((volatile LONG*)&psController->ulDisconnectFlag, CONTROLLER_IO_DISCONNECTED_EVENT, 0) == 0) {
		NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Trying to schedule disconnect WI!\n", __FUNCTION__, __LINE__);
		if (psController->sOsManager.psDisconnectWorkItem && psController->sOsManager.psDisconnectWIRoutine) {
			NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Scheduling disconnect WI!\n", __FUNCTION__, __LINE__);
			IoQueueWorkItem(psController->sOsManager.psDisconnectWorkItem,
				psController->sOsManager.psDisconnectWIRoutine,
				CriticalWorkQueue,
				psController);
		}
	}
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Disconnecting IO Queue %hu!\n", __FUNCTION__, __LINE__, psSQP->usQId);
	return;
}

VOID
NVMeoFRdmaIOQPConnectEvent(_In_ PNVMEOF_QUEUE psSQP,
	_In_ PNDK_CONNECTOR psNdkConnector)
{
	UNREFERENCED_PARAMETER(psSQP);
	UNREFERENCED_PARAMETER(psNdkConnector);
}

VOID
NVMeoFRdmaIOQPRequestCompletion(_In_ PNVMEOF_QUEUE psSQP,
	_In_ NTSTATUS Status)
{
	UNREFERENCED_PARAMETER(psSQP);
	UNREFERENCED_PARAMETER(Status);
}

static
NTSTATUS
NVMeoFRdmaCreateIOQ(__in PNVMEOF_FABRIC_CONTROLLER psController,
                    __in PNVMEOF_QUEUE psSQP)
{
	NTSTATUS                    Status = STATUS_SUCCESS;
	PPDS_NDK_QP_CALLBACKS       psSQCbs = &psSQP->sQPCallBacks;
	PNVMEOF_RDMA_PRIVATE_DATA   psNvmeIOQPrivData = NVMEOF_QUEUE_GET_RDMA_CTX(psSQP)->psQueuePvtData;
	psSQCbs->fnConnectEventHandler = NVMeoFRdmaIOQPConnectEvent;
	psSQCbs->fnQPDisconnectEventHandler = NVMeoFRdmaIOQPDisconnect;
	psSQCbs->fnQPReqCompletion = NVMeoFRdmaIOQPRequestCompletion;
	psSQCbs->fnReceiveCQNotificationEventHandler = NVMeoFRdmaIOQPRcqCalback;
	psSQCbs->fnSendCQNotificationEventHandler = NVMeoFRdmaIOQPScqCallback;
	psSQCbs->fnSrqNotificationEventHandler = NULL;
	psSQCbs->pvConnectEventCtx = psSQP;
	psSQCbs->pvQPDisconnectEventCtx = psSQP;
	psSQCbs->pvQPReqCompletionCtx = psSQP;
	psSQCbs->pvReceiveCQNotificationCtx = psSQP;
	psSQCbs->pvSendCQNotificationCtx = psSQP;
	psSQCbs->pvSRQNotificationCtx = NULL;

	RtlZeroMemory(psNvmeIOQPrivData, sizeof(*psNvmeIOQPrivData));
	psNvmeIOQPrivData->usHrqsize = (USHORT)psSQP->ulQueueDepth;
	psNvmeIOQPrivData->usHsqsize = (USHORT)PDS_NVME_GET_CAP_MQES(psController->ullControllerCapability);
	psNvmeIOQPrivData->usQId = (USHORT)psSQP->usQId;
	psNvmeIOQPrivData->usControllerId = psController->usControllerID;
	psNvmeIOQPrivData->usRecfmt = (USHORT)NVMEOF_RDMA_CM_FMT_1_0;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:QID %hu RqSz %hu SqSz %hu CtrlrId %hu RecFmt %hu!\n", __FUNCTION__, __LINE__,
		psNvmeIOQPrivData->usQId,
		psNvmeIOQPrivData->usHrqsize,
		psNvmeIOQPrivData->usHsqsize,
		psNvmeIOQPrivData->usControllerId,
		psNvmeIOQPrivData->usRecfmt);

	Status = 
		NVMeoFRdmaCreateAndConnectQP(psController,
                                     psSQCbs,
                                     psNvmeIOQPrivData,
                                     NVMEOF_QUEUE_GET_RDMA_CTX(psSQP)->psNdkQP);
	return  (Status);
}

static
VOID
NVMeoFRdmaFlushIOQueue(__in PNVMEOF_FABRIC_CONTROLLER psController,
                       __in PNVMEOF_QUEUE psSQ,
                       __in NTSTATUS Status)
{
	UNREFERENCED_PARAMETER(Status);
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Flushing IO Queue for NQN %s IP Addr %#X OS Requests %u\n", __FUNCTION__, __LINE__,
                   psController->caSubsystemNQN,
                   psController->sTargetAddress.Ipv4.sin_addr.S_un.S_addr,
                   psSQPair->lOutstandingIOQP);

	psRdmaIoReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList;
	while (psRdmaIoReq < (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList + psSQ->ulQueueDepth)) {
		if (psRdmaIoReq->lReqState & NVMEOF_WORK_REQUEST_STATE_INITIALIZED) {
			PNVMEOF_IO_REQ psIoReq = _InterlockedExchangePointer((volatile PVOID*)&psRdmaIoReq->pvParentReqCtx, (PVOID)NULL);
			if (psIoReq) {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Submission Req %#p for %s Storport Ctx 0x%p and IO Ctx 0x%p Failed with Status 0x%x !\n", __FUNCTION__, __LINE__,
                               psRdmaIoReq,
                               (((PNVME_READ_WRITE_COMMAND)psRdmaIoReq->psBufferMdlMap->pvBuffer)->ucOpCode == NVME_IO_CMD_WRITE) ? "Write" : "Read",
                               psIoReq->pvHBACtx,
                               psIoReq->pvIoReqCtx,
                               Status);
				NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Submission Req %#p for %s Storport Ctx 0x%p and IO Ctx 0x%p Failed with Status 0x%x !\n", __FUNCTION__, __LINE__,
                               psRdmaIoReq,
                               (((PNVME_READ_WRITE_COMMAND)psRdmaIoReq->psBufferMdlMap->pvBuffer)->ucOpCode == NVME_IO_CMD_WRITE) ? "Write" : "Read",
                               psIoReq->pvHBACtx,
                               psIoReq->pvIoReqCtx,
                               Status);
				if ((CONTROLLER_ANA_DISCONNECT_EVENT != psController->ulDisconnectFlag) &&
					(CONTROLLER_ANA_STANDBY_DISCONNECT_EVENT != psController->ulDisconnectFlag)) {
					if (psIoReq && psIoReq->pvHBACtx && psIoReq->pvIoReqCtx && psIoReq->fnCompletion) {
						_NVMEOF_IO_OP_COMPLETION fnCompletion =
							(_NVMEOF_IO_OP_COMPLETION)_InterlockedExchangePointer((volatile PVOID*)&psIoReq->fnCompletion, NULL);
						if (fnCompletion) {
							fnCompletion(psIoReq->pvHBACtx, psIoReq->pvIoReqCtx, Status);
						}
					}
				}

				_InterlockedDecrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lSubmissionWorkRequestOutstanding);
				_InterlockedDecrement(&psController->lOutstandingRequests);
			}
		}
		psRdmaIoReq++;
	}
}

static
NTSTATUS
NVMeoFRdmaIORead(__in PNVMEOF_FABRIC_CONTROLLER ctrlr,
                 __in PNVMEOF_QUEUE psSQ,
                 __in PNVMEOF_IO_REQ psIOInfo);

static
NTSTATUS
NVMeoFRdmaIOWrite(__in PNVMEOF_FABRIC_CONTROLLER ctrlr,
                  __in PNVMEOF_QUEUE psSQ,
                  __in PNVMEOF_IO_REQ psIOInfo);

static
NTSTATUS
NVMeoFRdmaXportConnectControllerIOQeueus(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS       Status = STATUS_SUCCESS;
	USHORT		   usQId = 1;
	USHORT         usNumaNodes = KeQueryHighestNodeNumber() + 1;
	USHORT         usIOQCountPerNumaNode = (USHORT)((usNumaNodes == 1) ? 
		                                     psController->sSession.lSQQueueCount : 
		                                     ((psController->sSession.lSQQueueCount + 1) / usNumaNodes));
	USHORT         usNumaNodeCnt = 0;
	GROUP_AFFINITY sCurrGroupAffnity = { 0 }, sSavedGroupAffnity = { 0 };
	PPDS_NUMA_INFO psNumaInfo = NULL;
	if (psController->eControllerType != FABRIC_CONTROLLER_TYPE_CONNECT) {
		return (STATUS_INVALID_PARAMETER);
	}

	psController->sSession.lSQQueueCount = 0;
	psController->sSession.usIOQPerNumaNode = usIOQCountPerNumaNode;
	psController->sSession.usNumaNodeCount = usNumaNodes;
	psController->sSession.usNumaBitShift = (usIOQCountPerNumaNode & (usIOQCountPerNumaNode - 1)) ? 0 : __popcnt16(usIOQCountPerNumaNode - 1);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:IO Queue per NUMA node %hu NUMA node Count %hu NUMA bit Shift %hu\n", __FUNCTION__, __LINE__,
		usIOQCountPerNumaNode,
		usNumaNodes,
		psController->sSession.usNumaBitShift);

	psController->sSession.psNumaSQPair = NVMeoFRdmaAllocateNpp(usNumaNodes * sizeof(PDS_NUMA_INFO));
	if (!psController->sSession.psNumaSQPair) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	KeQueryNodeActiveAffinity(KeGetCurrentNodeNumber(),
                              &sSavedGroupAffnity,
                              NULL);
	psNumaInfo = psController->sSession.psNumaSQPair;
	RtlZeroMemory(psNumaInfo, usNumaNodes * sizeof(PDS_NUMA_INFO));

	for (; usNumaNodeCnt < usNumaNodes; usNumaNodeCnt++) {
		USHORT usQCount = 0;
		KeQueryNodeActiveAffinity(usNumaNodeCnt,
			&sCurrGroupAffnity,
			NULL);
		KeSetSystemGroupAffinityThread(&sCurrGroupAffnity, NULL);
		psNumaInfo[usNumaNodeCnt].usNumberOfIOQ = usIOQCountPerNumaNode;
		psNumaInfo[usNumaNodeCnt].psSQ = NVMeoFRdmaAllocateNpp((usIOQCountPerNumaNode * sizeof(NVMEOF_QUEUE)));
		if (!psNumaInfo[usNumaNodeCnt].psSQ) {
			NVMeoFRdmaIOQDisconnectAndReleaseResources(psController);
			KeSetSystemGroupAffinityThread(&sSavedGroupAffnity, NULL);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(psNumaInfo[usNumaNodeCnt].psSQ, (usIOQCountPerNumaNode * sizeof(IO_QP)));

		for (PNVMEOF_QUEUE psNodeSQ = psNumaInfo[usNumaNodeCnt].psSQ, usQCount = 0;
			(usQCount < usIOQCountPerNumaNode);
			usQCount++, psNodeSQ++, usQId++) {
			psNodeSQ->psParentController = psController;
			psNodeSQ->usQId = usQId;
			psNodeSQ->ulQueueDepth = (USHORT)psController->sSession.lSQQueueCount;

			Status = NVMeoFRdmaCreateIOQ(psController, psNodeSQ);
			if (NT_ERROR(Status)) {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NVMeoFRdmaCreateIOQ() failed with status 0x%X\n", __FUNCTION__, __LINE__, Status);
				NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:NVMeoFRdmaCreateIOQ() failed with status 0x%X\n", __FUNCTION__, __LINE__, Status);
				NVMeoFRdmaIOQDisconnectAndReleaseResources(psController);
				KeSetSystemGroupAffinityThread(&sSavedGroupAffnity, NULL);
				return (Status);
			}

			psNodeSQ->psNumaParent = &psNumaInfo[usNumaNodeCnt];
			psNodeSQ->sQueueOps._SQWriteData = NVMeoFRdmaIOWrite;
			psNodeSQ->sQueueOps._SQReadData = NVMeoFRdmaIORead;
			psNodeSQ->sQueueOps._SQFlushAllIOReq = NVMeoFRdmaFlushIOQueue;
			psNodeSQ->sQueueOps._SQIsFreeIOSlotAvailable = NVMeoFRdmaIsIOSlotAvailable;

			KeInitializeSpinLock(&NVMEOF_QUEUE_GET_RDMA_CTX(psNodeSQ)->sSubmitSpinLock);
			KeInitializeSpinLock(&NVMEOF_QUEUE_GET_RDMA_CTX(psNodeSQ)->sFRSpinLock);
			KeInitializeSpinLock(&NVMEOF_QUEUE_GET_RDMA_CTX(psNodeSQ)->sBuildLamSpinLock);
			Status = NVMeoFRdmaAllocateAndInitializeRdmaIoReq(psNodeSQ);
			if (NT_ERROR(Status)) {
				NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NVMeoFRdmaAllocateAndInitializeRdmaIoReq() failed with status 0x%X\n", __FUNCTION__, __LINE__, Status);
				NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:NVMeoFRdmaAllocateAndInitializeRdmaIoReq() failed with status 0x%X\n", __FUNCTION__, __LINE__, Status);
				NVMeoFRdmaIOQDisconnectAndReleaseResources(psController);
				KeSetSystemGroupAffinityThread(&sSavedGroupAffnity, NULL);
				return (STATUS_NO_MEMORY);
			}

			psNodeSQ->bValid = TRUE;
			psController->lIOQueueCount++;
		}
		NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Created %hu for NUMA node %hu all sockets created.\n", __FUNCTION__, __LINE__, usQCount, usNumaNodeCnt);
	}

	KeSetSystemGroupAffinityThread(&sSavedGroupAffnity, NULL);

	return  (Status);
}

static
NTSTATUS
NVMeoFRdmaInitIOQueues(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS       Status = STATUS_SUCCESS;
	USHORT         usNumaNodes = psController->sSession.usNumaNodeCount;
	USHORT         usIOQCountPerNumaNode = psController->sSession.usIOQPerNumaNode;
	USHORT         usNumaNodeCnt = 0;
	PPDS_NUMA_INFO psNumaInfo = NULL;

	psNumaInfo = psController->sSession.psNumaSQPair;

	for (; usNumaNodeCnt < usNumaNodes; usNumaNodeCnt++) {
		PNVMEOF_QUEUE psSQP = NULL;
		USHORT usQCount = 0;
		for (psSQP = psNumaInfo[usNumaNodeCnt].psSQ, usQCount = 0;
			(usQCount < usIOQCountPerNumaNode);
			usQCount++, psSQP++) {
			Status = NVMeoFRdmaIOQSubmitReceiveRequests(psController, psSQP);
			if (NT_ERROR(Status)) {
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Failed to submit Receive with status 0x%x\n", __FUNCTION__, __LINE__, Status);
				break;
			}
		}
	}

	return Status;
}

static
USHORT
NVMeoFRdmaGetIOQUniqueCommandID(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PNVMEOF_QUEUE psSQ)
{
	UNREFERENCED_PARAMETER(psController);
	USHORT usCmdID = (USHORT)((_InterlockedIncrement64(&psSQ->lNextCmdID) - 1) & MAX_CMD_ID);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Next command ID %hu for Q %hu.\n", __FUNCTION__, __LINE__, usCmdID, psSQ->usQId);
	return usCmdID;
}

static
NTSTATUS
NVMeoFRdmaReleaseIOQSubmissionWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER psController,
	                                      __in PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq)
{
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap;
	PNDK_WORK_REQUEST psNdkWorkReq = psRdmaIoReq->psNdkWorkReq;

	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_RELEASE_LAM;
	psNdkWorkReq->sReleaseLamReq.psLam = psLamSgl->psLaml;
	PdsNdkReleaseLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);
	psRdmaIoReq->psDataBufferMdlMap->psBufferMdl = NULL;
	psRdmaIoReq->psDataBufferMdlMap->pvBuffer = NULL;
	psRdmaIoReq->psDataBufferMdlMap->ulBufferLength = 0;

	psLamSgl->pvBaseVA = NULL;
	psLamSgl->psMdl = NULL;
	psLamSgl->usSgeCount = 0;
	psLamSgl->ulCurrentLamCbSize = 0;

	psRdmaIoReq->usCommandID = 0xFFFF;
	psRdmaIoReq->fSendReqCompletion = NULL;
	psRdmaIoReq->fNVMeReqCompletion = NULL;
	psRdmaIoReq->pvParentReqCtx = NULL;
	psRdmaIoReq->sCompletionCtx.Status = STATUS_SUCCESS;
	KeResetEvent(&psRdmaIoReq->sCompletionCtx.sEvent);
	psRdmaIoReq->Status = STATUS_SUCCESS;
	psRdmaIoReq->ulReqFlag = 0;
	psRdmaIoReq->ulByteXferred = 0;

	_InterlockedExchange(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_FREE);
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   ((PNVMEOF_QUEUE)psRdmaIoReq->pvParentQP)->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);
	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaIOQInvalidateDataMr(__in PNVMEOF_FABRIC_CONTROLLER psController,
                              __in PNVMEOF_QUEUE             psSQ,
                              __in PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE sLockHandle = { 0 };
	KIRQL Irql = 0;
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap;
	psRdmaIoReq->psNdkWorkReq->ulType = NDK_WREQ_SQ_INVALIDATE;
	psRdmaIoReq->psNdkWorkReq->sInvalidateReq.psNdkMrOrMw = (PVOID)psLamSgl->psNdkMr;
	psRdmaIoReq->psNdkWorkReq->ulFlags = 0;
	psRdmaIoReq->psNdkWorkReq->pvCallbackCtx = psRdmaIoReq;
	Irql = KeGetCurrentIrql();
	NVMeoFRdmaInStackSpinLockAcquire(Irql, &NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->sSubmitSpinLock, &sLockHandle);
	Status =
		NdkSubmitRequest(psController->sSession.pvSessionFabricCtx,
                         &NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP,
                         psRdmaIoReq->psNdkWorkReq);
	NVMeoFRdmaInStackSpinLockRelease(Irql, &sLockHandle);
	return (Status);
}

static
NTSTATUS
NVMeoFRdmaIOQPrepareSubmissionWorkRequest(__in PNVMEOF_FABRIC_CONTROLLER             psController,
                                          __in PNVMEOF_QUEUE                         psSQ,
                                          __in PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	LARGE_INTEGER liCommandTimeOut = { 0 };
	PNDK_WORK_REQUEST psNdkWorkReq = psRdmaIoReq->psNdkWorkReq;

	//liCommandTimeOut.QuadPart = -PDS_MILLISECONDS_TO_100NANOSECONDS(PDS_SECONDS_TO_MILLISECONDS(PDS_NVME_ADMIN_COMMAND_TIMEOUT_SEC));
	liCommandTimeOut.QuadPart = WDF_REL_TIMEOUT_IN_MS(NVMEOF_ADMIN_COMMAND_TIMEOUT_SEC * 1000ULL);

	if (!psRdmaIoReq->psDataBufferMdlMap->psBufferMdl) {
		psRdmaIoReq->psDataBufferMdlMap->psBufferMdl =
			NVMeoFRdmaAllocateMdlForBufferFromNP(psRdmaIoReq->psDataBufferMdlMap->pvBuffer,
				psRdmaIoReq->psDataBufferMdlMap->ulBufferLength);
		if (!psRdmaIoReq->psDataBufferMdlMap->psBufferMdl) {
			return STATUS_NO_MEMORY;
		}
	}

	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));
	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_BUILD_LAM;
	psNdkWorkReq->sBuildLamReq.psMdl = psRdmaIoReq->psDataBufferMdlMap->psBufferMdl;
	psNdkWorkReq->sBuildLamReq.psLam = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->psLaml;
	psNdkWorkReq->sBuildLamReq.ulLamCbSize =
		PDS_GET_LAM_BUFFERSIZE(PDS_GET_MDL_PAGE_COUNT(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl,
			psRdmaIoReq->psDataBufferMdlMap->pvBuffer));
	psNdkWorkReq->sBuildLamReq.szMdlBytesToMap = MmGetMdlByteCount(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl);
	psNdkWorkReq->fnCallBack = NULL;
	psNdkWorkReq->pvCallbackCtx = NULL;
	Status = PdsNdkBuildLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Data Lam creation Status 0x%x.\n", __FUNCTION__, __LINE__,
		Status);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Data Lam creation Status 0x%x.\n", __FUNCTION__, __LINE__,
			Status);
		return Status;
	}

	psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->ulFBO = psNdkWorkReq->sBuildLamReq.ulFboLAM;

	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));
	psNdkWorkReq->ulType = NDK_WREQ_SQ_FAST_REGISTER;
	psNdkWorkReq->pvCallbackCtx = psRdmaIoReq;
	psNdkWorkReq->sFRMRReq.psNdkMr = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->psNdkMr;
	psNdkWorkReq->sFRMRReq.ulAdapterPageCount = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->psLaml->AdapterPageCount;
	psNdkWorkReq->sFRMRReq.psAdapterPageArray = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->psLaml->AdapterPageArray;
	psNdkWorkReq->sFRMRReq.szLength = MmGetMdlByteCount(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl);
	psNdkWorkReq->sFRMRReq.ulFBO = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->ulFBO;
	psNdkWorkReq->sFRMRReq.pvBaseVirtualAddress = (PVOID)((ULONG_PTR)psRdmaIoReq->psDataBufferMdlMap->pvBuffer ^ ((ULONG_PTR)psRdmaIoReq & 0xFFFFFFFFFFFFF000));
	psNdkWorkReq->ulFlags = (NDK_OP_FLAG_ALLOW_REMOTE_READ | NDK_OP_FLAG_ALLOW_REMOTE_WRITE);
	KeResetEvent(&psRdmaIoReq->sCompletionCtx.sEvent);
	Status = PdsNdkFastRegister(psController->sSession.pvSessionFabricCtx, &psSQ->sQP, psRdmaIoReq->psNdkWorkReq);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:FRMR Status 0x%x Data buffer %#p base VA %#p len %u RemoteToken %lu\n", __FUNCTION__, __LINE__,
		Status,
		psRdmaIoReq->psBufferMdlMap->pvBuffer,
		psRdmaIoReq->psNdkWorkReq->sFRMRReq.pvBaseVirtualAddress,
		psRdmaIoReq->psDataBufferMdlMap->ulBufferLength,
		psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->uiRemoteMemoryTokenKey);
	Status = KeWaitForSingleObject(&psRdmaIoReq->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, &liCommandTimeOut);
	if (NT_TIMEOUT(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Wait complete with command timeout 0x%x issuing cleanup!\n",
                       __FUNCTION__,
                       __LINE__,
                       Status);
		Status = STATUS_IO_OPERATION_TIMEOUT;
	} else if (NT_SUCCESS(Status)) {
		psRdmaIoReq->ulType = NVMEOF_WORK_REQUEST_TYPE_IO;
		psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->pvBaseVA = psRdmaIoReq->psNdkWorkReq->sFRMRReq.pvBaseVirtualAddress;
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:wait completed with Status 0x%x!\n",
                       __FUNCTION__,
                       __LINE__,
                       Status);
		if (NT_ERROR(psRdmaIoReq->Status)) {
			Status = psRdmaIoReq->Status;
			return (Status);
		}
	}

	KeResetEvent(&psRdmaIoReq->sCompletionCtx.sEvent);

	NVMeoFRdmaNvmeofXportSetSG(psController,
		                       psSQ->usQId,
		                       psRdmaIoReq->psBufferMdlMap->pvBuffer,
		                       (ULONGLONG)psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->pvBaseVA,
		                       psRdmaIoReq->psDataBufferMdlMap->ulBufferLength,
		                       psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->uiRemoteMemoryTokenKey,
		                       NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_SINGLE);
	KeFlushIoBuffers(psRdmaIoReq->psBufferMdlMap->psBufferMdl, TRUE, TRUE);
	KeFlushIoBuffers(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl, TRUE, TRUE);

	psRdmaIoReq->psNdkWorkReq->ulType = NDK_WREQ_SQ_SEND;
	psRdmaIoReq->psNdkWorkReq->sSendReq.psNdkSgl = psRdmaIoReq->psBufferMdlMap->psLamSglMap->psSgl;
	psRdmaIoReq->psNdkWorkReq->sSendReq.usSgeCount = (USHORT)psRdmaIoReq->psBufferMdlMap->psLamSglMap->usSgeCount;
	psRdmaIoReq->psNdkWorkReq->fnCallBack = NULL;
	psRdmaIoReq->psNdkWorkReq->pvCallbackCtx = psRdmaIoReq;
	psRdmaIoReq->psNdkWorkReq->ulFlags = 0;
	psRdmaIoReq->psNdkWorkReq->sSendReq.uiRemoteMemoryRegionToken = 0;

	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaIOQPSubmitRequestSyncAsync(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                     __in PNVMEOF_QUEUE             psSQ,
                                     __in USHORT                    usCommandID,
                                     __in PNVMEOF_REQUEST_RESPONSE  psAdminReqResp)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;
	LARGE_INTEGER liCommandTimeOut = { 0 };

	NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d:Trying to send Command of Length 0x%x bytes\n", __FUNCTION__, __LINE__,
		psAdminReqResp->sSndBuffer.ulBufferLen);
	psRdmaIoReq =
		NVMeoFRdmaAllocateAndInitializeSendWorkRequest(psController,
                                                       psSQ->usQId,
                                                       psAdminReqResp->sSndBuffer.sReqRespBuffer.pvBuffer,
                                                       psAdminReqResp->sSndBuffer.sReqRespBuffer.ulBufferLen,
                                                       psAdminReqResp->sSndBuffer.sDataBuffer.pvBuffer,
                                                       psAdminReqResp->sSndBuffer.sDataBuffer.ulBufferLen);
	if (!psRdmaIoReq) {
		NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d:Failed to allocate the Send Work Request!\n", __FUNCTION__, __LINE__);
		return (STATUS_INSUFFICIENT_RESOURCES);
	}

	KeResetEvent(&psRdmaIoReq->sCompletionCtx.sEvent);
	NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d:Cmd Buff Len 0x%x Data Buff Len 0x%x bytes\n", __FUNCTION__, __LINE__,
                   psAdminReqResp->sSndBuffer.ulBufferLen,
                   psAdminReqResp->sSndBuffer.ulDataLen);
	psRdmaIoReq->pvParentReqCtx = psAdminReqResp;
	psRdmaIoReq->usCommandID = usCommandID;

	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_INITIALIZED);
	Status =
		NVMeoFRdmaIOQPrepareSubmissionWorkRequest(psController,
                                                  psSQ,
                                                  psRdmaIoReq);
	if (NT_ERROR(Status)) {
		NVMeoFRdmaReleaseSendWorkRequest(psController, psSQ->usQId, psRdmaIoReq);
		return Status;
	}

	NVMeoFDisplayNVMeCmd(__FUNCTION__, psRdmaIoReq->psBufferMdlMap->pvBuffer);

	_mm_mfence();
	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_PREPARE_SUBMITTING);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   psSQ->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);
	KeResetEvent(&psRdmaIoReq->sCompletionCtx.sEvent);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Submiting Send Work Request 0x%llx!\n",
                   __FUNCTION__,
                   __LINE__,
                   psRdmaIoReq->psNdkWorkReq);
#if PDS_RDMA_USE_HASH_FOR_IO
	if (_InterlockedCompareExchangePointer(&psSQ->ppvIoReqTable[psRdmaIoReq->usCommandID], psRdmaIoReq, NULL) != NULL) {
		NT_ASSERT(0);
	}
	else {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Inserted CMD at CmdId location %hu State 0x%x!\n", __FUNCTION__, __LINE__,
			psRdmaIoReq->usCommandID,
			&psSQ->ppvIoReqTable[psRdmaIoReq->usCommandID]);
	}
#endif

	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_SUBMITTING);
	Status =
		NVMeoFRdmaSubmitSendWorkRequest(psController,
                                        psSQ->usQId,
                                        psRdmaIoReq);
	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_SUBMITTED);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   psSQ->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Going to wait to complete Send Work Request Submit Status 0x%x!\n",
                   __FUNCTION__,
                   __LINE__,
                   Status);
	//liCommandTimeOut.QuadPart = -PDS_MILLISECONDS_TO_100NANOSECONDS(PDS_SECONDS_TO_MILLISECONDS(PDS_NVME_ADMIN_COMMAND_TIMEOUT_SEC));
	liCommandTimeOut.QuadPart = WDF_REL_TIMEOUT_IN_MS(NVMEOF_IO_COMMAND_TIMEOUT_SEC * 1000ULL);
	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_WAITING_FOR_RESPONSE);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   psSQ->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);
	Status = KeWaitForSingleObject(&psRdmaIoReq->sCompletionCtx.sEvent, Executive, KernelMode, FALSE, &liCommandTimeOut);
	if (NT_TIMEOUT(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Wait complete with command timeout 0x%x issuing cleanup!\n",
                       __FUNCTION__,
                       __LINE__,
                       Status);
		Status = STATUS_IO_OPERATION_TIMEOUT;
	} else {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psController,
                                                                           psAdminReqResp);
		//NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:wait complete for Send Work Request Command Id %hu with Status 0x%x!\n",
                       __FUNCTION__,
                       __LINE__,
                       psCqe->usCommandID,
                       Status);
		if (NT_ERROR(psRdmaIoReq->Status)) {
			Status = psRdmaIoReq->Status;
#if PDS_RDMA_USE_HASH_FOR_IO
			NVMeoFRdmaIOQGetRequestByCmdId(psSQ,
				psRdmaIoReq->usCommandID,
				TRUE);
#endif
		}
	}

	KeResetEvent(&psRdmaIoReq->sCompletionCtx.sEvent);
	Status =
		NVMeoFRdmaIOQInvalidateDataMr(psController,
                                      psSQ,
                                      psRdmaIoReq);
	if (NT_SUCCESS(Status)) {
		Status =
			KeWaitForSingleObject(&psRdmaIoReq->sCompletionCtx.sEvent,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  &liCommandTimeOut);
	}

	return NVMeoFRdmaReleaseIOQSubmissionWorkRequest(psController, psRdmaIoReq);
}

static
NTSTATUS
NVMeoFRdmaConnectNvmeofIOQueues(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PPDS_NUMA_INFO psNumaNode = psController->sSession.psNumaSQPair;
	ULONG ulReqSz = 0;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;
	PNVME_CMD psNVMeConnReq = NULL;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:In!\n", __FUNCTION__, __LINE__);
	psAdminReqResp = NVMeoFRdmaAllocateNpp(sizeof(NVMEOF_REQUEST_RESPONSE));
	if (!psAdminReqResp) {
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(psAdminReqResp, sizeof(*psAdminReqResp));

	for (USHORT usNumaNode = 0; usNumaNode < psController->sSession.usNumaNodeCount; usNumaNode++) {
		PNVMEOF_QUEUE psSQ = psNumaNode[usNumaNode].psSQ;
		for (USHORT usIOQPerNumaNode = 0;
			(usIOQPerNumaNode < psController->sSession.usIOQPerNumaNode);
			usIOQPerNumaNode++, psSQ++) {
			if (psNVMeConnReq) {
				NVMeoFRdmaFreeNpp(psNVMeConnReq);
				psNVMeConnReq = NULL;
			}

			psNVMeConnReq =
				NVMeoFRdmaAllocateConnectRequest(psController,
                                                 psSQ->usQId,
                                                 (USHORT)psSQ->ulQueueDepth,
                                                 NVMeoFRdmaGetIOQUniqueCommandID(psController, psSQ),
                                                 &ulReqSz);
			if (!psNVMeConnReq) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			psAdminReqResp->sSndBuffer.sReqRespBuffer.pvBuffer = psNVMeConnReq;
			psAdminReqResp->sSndBuffer.sReqRespBuffer.ulBufferLen = sizeof(NVME_CMD);
			psAdminReqResp->sSndBuffer.sDataBuffer.pvBuffer = (psNVMeConnReq + 1);
			psAdminReqResp->sSndBuffer.sDataBuffer.ulBufferLen = (ulReqSz - sizeof(NVME_CMD));
			psAdminReqResp->sRcvBuffer.sReqRespBuffer.pvBuffer = NULL;
			psAdminReqResp->sRcvBuffer.sReqRespBuffer.ulBufferLen = 0;
			psAdminReqResp->ulFlags = NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE_SYNC;

			Status = NVMeoFRdmaIOQPSubmitRequestSyncAsync(psController, psSQ, psNVMeConnReq->sConnect.usCommandId, psAdminReqResp);
			if (NT_SUCCESS(Status)) {
				PNVME_COMPLETION_QUEUE_ENTRY psCqe =
					psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psController,
						psAdminReqResp);
				if (psCqe->sStatus.sStatusField.SCT) {
					NVMeoFDisplayCQE(__FUNCTION__, psCqe);
					Status = NVMeoFGetWinError(psCqe);
					break;
				}
				else {
					//NVMeoFDisplayCQE(__FUNCTION__, &psAdminReqResp->sCqe);
				}
			}
		}
	}

	if (psNVMeConnReq) {
		NVMeoFRdmaFreeNpp(psNVMeConnReq);
	}
	NVMeoFRdmaFreeNpp(psAdminReqResp);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
NTSTATUS
NVMeoFRdmaSetupNvmeofIOQueues(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:IO Queues to be Initialized 0x%x!\n", __FUNCTION__, __LINE__, psController->ulIOQueueCount);

	Status = NVMeoFRdmaInitIOQueues(psController);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:IO Queues to be Connected 0x%x Init Status 0x%x!\n", __FUNCTION__, __LINE__, psController->ulIOQueueCount, Status);

	Status = NVMeoFRdmaConnectNvmeofIOQueues(psController);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Created IO Workers Queues 0x%x Connect Status 0x%x!\n", __FUNCTION__, __LINE__,
		psController->ulIOQueueCount,
		Status);

	return Status;
}

static
//FORCEINLINE
NTSTATUS
NVMeoFRdmaGetCurrentFreeIORequest(__in  PNVMEOF_FABRIC_CONTROLLER    psController,
                                  __in  PNVMEOF_QUEUE                psSQ,
                                  __inout PNVMEOF_RDMA_WORK_REQUEST* ppsFreeReq)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;
	for (psRdmaIoReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList;
		psRdmaIoReq < (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList + psSQ->ulQueueDepth);
		psRdmaIoReq++) {
		if (_InterlockedCompareExchange((volatile LONG*)&psRdmaIoReq->lReqState,
			NVMEOF_WORK_REQUEST_STATE_ALLOCATED,
			NVMEOF_WORK_REQUEST_STATE_FREE) == NVMEOF_WORK_REQUEST_STATE_FREE) {
			if (psRdmaIoReq->pvParentReqCtx) {
				NT_ASSERT(0);
			}
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                           psSQ->usQId,
                           psRdmaIoReq,
                           psRdmaIoReq->usCommandID,
                           psRdmaIoReq->lReqState);
			psRdmaIoReq->usCommandID = NVMeoFRdmaGetIOQUniqueCommandID(psController, psSQ);
			*ppsFreeReq = psRdmaIoReq;
			return (STATUS_SUCCESS);
		}
	}
	*ppsFreeReq = NULL;
	return (STATUS_NO_MORE_ENTRIES);
}

static
BOOLEAN
NVMeoFRdmaIsIOSlotAvailable(__in PNVMEOF_FABRIC_CONTROLLER psController,
	__in PNVMEOF_QUEUE psSQ)
{
	UNREFERENCED_PARAMETER(psController);
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList;
	while (psRdmaIoReq < (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList + psSQ->ulQueueDepth)) {
		if (psRdmaIoReq->lReqState == NVMEOF_WORK_REQUEST_STATE_FREE) {
			return TRUE;
		}
		psRdmaIoReq++;
	}

	return FALSE;
}

static
VOID
NVMeoFRdmaIoFRCompletion(__in PNVMEOF_QUEUE            psSQ,
                         __in PNDK_WORK_REQUEST psNdkWorkReq,
                         __in NTSTATUS          Status)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = psNdkWorkReq->pvCallbackCtx;
	ULONG ulOpCode = ((PNVME_READ_WRITE_COMMAND)psRdmaIoReq->psBufferMdlMap->pvBuffer)->ucOpCode;
	ULONG ulRdmaReqFlag = 0;
	PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psDataBuffer = psRdmaIoReq->psDataBufferMdlMap;
	KLOCK_QUEUE_HANDLE sLockHandle = { 0 };
	KIRQL Irql = 0;
	_InterlockedDecrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lFRMRReqOutstanding);
	if (NT_ERROR(Status)) {
		//PNVMEOF_IO_REQ psIoReq = _InterlockedExchangePointer(&psRdmaIoReq->pvParentReqCtx, (PVOID)NULL);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to FR (0x%x), completing User IO.\n", __FUNCTION__, __LINE__,
			Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Failed to FR (0x%x), completing User IO.\n", __FUNCTION__, __LINE__,
			Status);
		//if (psIoReq) {
		//	NVMeoFRdmaIoCompletion(psSQ, psRdmaIoReq, psIoReq, Status);
		//}
		ASSERT(0);
		return;
	}

	_InlineInterlockedAdd64(NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llFRMRCurrentCount, 
		                     (LONG64)psDataBuffer->psLamSglMap->psLaml->AdapterPageCount);
	psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->pvBaseVA = psNdkWorkReq->sFRMRReq.pvBaseVirtualAddress;
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:IO Req %#p Data 0x%p Base VA 0x%llx len %u RemoteToken %lu NdkMr %#p.\n", __FUNCTION__, __LINE__,
                   psRdmaIoReq->pvParentReqCtx,
                   psDataBuffer->pvBuffer,
                   psDataBuffer->psLamSglMap->pvBaseVA,
                   psDataBuffer->ulBufferLength,
                   psDataBuffer->psLamSglMap->uiRemoteMemoryTokenKey,
                   psDataBuffer->psLamSglMap->psNdkMr);
	if (((PNDK_SOCKET)psSQ->psParentController->sSession.pvSessionFabricCtx)->bSupportsRemoteInvalidation) {
		psRdmaIoReq->ulReqFlag = PDS_NVMEOF_WORK_REQUEST_FLAG_LOCAL_INVALIDATE;
		ulRdmaReqFlag = NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_MULTIPLE;
	} else {
		psRdmaIoReq->ulReqFlag = PDS_NVMEOF_WORK_REQUEST_FLAG_LOCAL_INVALIDATE;
		ulRdmaReqFlag = NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_SINGLE;
	}

	if (ulOpCode == NVME_IO_CMD_WRITE) {
		KeFlushIoBuffers(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl, TRUE, TRUE);
	} else {
		KeFlushIoBuffers(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl, FALSE, TRUE);
	}

	NVMeoFRdmaNvmeofXportSetSG(psSQ->psParentController,
                               psSQ->usQId,
                               psRdmaIoReq->psBufferMdlMap->pvBuffer,
                               (ULONGLONG)psDataBuffer->psLamSglMap->pvBaseVA,
                               psDataBuffer->ulBufferLength,
                               psDataBuffer->psLamSglMap->uiRemoteMemoryTokenKey,
                               ulRdmaReqFlag);

	NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_VERBOSE,
                             psRdmaIoReq->psBufferMdlMap->pvBuffer,
                             sizeof(NVME_CMD),
                             PDS_PRINT_HEX_PER_LINE_MAX_BYTES);
	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));
	psNdkWorkReq->ulType = NDK_WREQ_SQ_SEND;
	psNdkWorkReq->sSendReq.uiRemoteMemoryRegionToken = 0;
	psNdkWorkReq->sSendReq.psNdkSgl = psRdmaIoReq->psBufferMdlMap->psLamSglMap->psSgl;
	psNdkWorkReq->sSendReq.usSgeCount = (USHORT)psRdmaIoReq->psBufferMdlMap->psLamSglMap->usSgeCount;
	psNdkWorkReq->fnCallBack = NULL;
	psNdkWorkReq->pvCallbackCtx = psRdmaIoReq;
	psNdkWorkReq->ulFlags = 0;
	_mm_mfence();

#ifdef PDS_RDMA_USE_HASH_FOR_IO
	if (_InterlockedCompareExchangePointer(&psSQ->ppvIoReqTable[psRdmaIoReq->usCommandID], psRdmaIoReq, NULL) != NULL) {
		NT_ASSERT(0);
	}
	else {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Inserted CMD at CmdId location %hu State 0x%x!\n", __FUNCTION__, __LINE__,
			psRdmaIoReq->usCommandID,
			&psSQ->ppvIoReqTable[psRdmaIoReq->usCommandID]);
	}
#endif
	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_SUBMITTING);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request 0x%p CmdId %hu State 0x%x!\n", __FUNCTION__, __LINE__,
		psSQ->usQId,
		psRdmaIoReq,
		psRdmaIoReq->usCommandID,
		psRdmaIoReq->lReqState);
	Irql = KeGetCurrentIrql();
	NVMeoFRdmaInStackSpinLockAcquire(Irql, &NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->sSubmitSpinLock, &sLockHandle);
	Status =
		NVMeoFRdmaSubmitSendWorkRequest(psSQ->psParentController,
                                        psSQ->usQId,
                                        psRdmaIoReq);
	NVMeoFRdmaInStackSpinLockRelease(Irql, &sLockHandle);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Submitted IO Request %s with Status %#x\n", __FUNCTION__, __LINE__,
                   pcaNVMeIOCmd[ulOpCode],
                   Status);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to Send Request (0x%x) Issuing invalidate.\n", __FUNCTION__, __LINE__,
			Status);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Failed to Send Request (0x%x) Issuing invalidate.\n", __FUNCTION__, __LINE__,
			Status);
		_InterlockedExchange(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_FREEING);
#ifdef PDS_RDMA_USE_HASH_FOR_IO
		_InterlockedExchangePointer(&psSQ->ppvIoReqTable[psRdmaIoReq->usCommandID], NULL);
#endif
		psRdmaIoReq->Status = Status;
		Status =
			NVMeoFRdmaIOQInvalidateDataMr(psSQ->psParentController,
				psSQ,
				psRdmaIoReq);
		if (NT_ERROR(Status)) {
			ASSERT(0);
		}
		ASSERT(0);
	} else {
		_InterlockedIncrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lSubmissionWorkRequestOutstanding);
		_InterlockedIncrement64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llSubmissionWorkRequestSubmitted);
		_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_SUBMITTED);
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                       psSQ->usQId,
                       psRdmaIoReq,
                       psRdmaIoReq->usCommandID,
                       psRdmaIoReq->lReqState);
	}
}

static
VOID
NVMeoFRdmaIoSendCompletion(__in PNVMEOF_QUEUE            psSQ,
                           __in PNDK_WORK_REQUEST psNdkWorkReq,
                           __in NTSTATUS          Status)
{
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = psNdkWorkReq->pvCallbackCtx;
	_InterlockedDecrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lSubmissionWorkRequestOutstanding);
	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_WAITING_FOR_RESPONSE);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   psSQ->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);

	if (NT_SUCCESS(Status)) {
		if (_InterlockedCompareExchange(&psRdmaIoReq->lReqState,
			NVMEOF_WORK_REQUEST_STATE_FREEING,
			PDS_NVME_REQUEST_ALL_STATE_DONE) == PDS_NVME_REQUEST_ALL_STATE_DONE) {
			Status =
				NVMeoFRdmaIOQInvalidateDataMr(psSQ->psParentController,
                                              psSQ,
                                              psRdmaIoReq);
			NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Invalidate issued!\n", __FUNCTION__, __LINE__);
		}
	} else {
		//NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to Send Request (%#x) Issuing invalidate!\n", __FUNCTION__, __LINE__, Status);
		//NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Failed to Send Request (%#x) Issuing invalidate.\n", __FUNCTION__, __LINE__, Status);
		//Status =
		//	NVMeoFRdmaIOQInvalidateDataMr(psSQ->psParentController,
		//                                   psSQ,
		//                                   psRdmaIoReq);
		//if (NT_ERROR(Status)) {
		//	ASSERT(0);
		//}
		ASSERT(0);
	}
}

static
VOID
NVMeoFRdmaIOQReleaseDataLam(__in    PNVMEOF_FABRIC_CONTROLLER              psController,
	__inout PNVMEOF_RDMA_WORK_REQUEST  psRdmaIoReq)
{
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap;
	PNDK_WORK_REQUEST psNdkWorkReq = psRdmaIoReq->psNdkWorkReq;

	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_RELEASE_LAM;
	psNdkWorkReq->sReleaseLamReq.psLam = psLamSgl->psLaml;
	PdsNdkReleaseLam(psController->sSession.pvSessionFabricCtx, psNdkWorkReq);

	psLamSgl->pvBaseVA = NULL;
	psLamSgl->psMdl = NULL;
	psLamSgl->usSgeCount = 0;
	psLamSgl->ulCurrentLamCbSize = 0;
	psLamSgl->usInitialized = FALSE;
}

static
VOID
NVMeoFRdmaIoCompletion(__in PNVMEOF_QUEUE             psSQ,
                       __in PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq,
                       __in PNVMEOF_IO_REQ            psIoReq,
                       __in NTSTATUS                  Status)
{
	UNREFERENCED_PARAMETER(Status);
	if (NVME_IO_CMD_READ == ((PNVME_READ_WRITE_COMMAND)psRdmaIoReq->psBufferMdlMap->pvBuffer)->ucOpCode) {
		NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_INFO,
			psIoReq->pucDataBuffer,
			psIoReq->ulLength,
			PDS_PRINT_HEX_PER_LINE_32_BYTES);
	}

	NT_ASSERT(psRdmaIoReq->lReqState == NVMEOF_WORK_REQUEST_STATE_FREEING);

	_InterlockedExchangeAdd64(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->llFRMRCurrentCount,
		                      -(LONG64)psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->psLaml->AdapterPageCount);

	KeFlushIoBuffers(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl, TRUE, TRUE);

	psRdmaIoReq->psDataBufferMdlMap->psBufferMdl = NULL;
	psRdmaIoReq->psDataBufferMdlMap->pvBuffer = NULL;
	psRdmaIoReq->psDataBufferMdlMap->ulBufferLength = 0;

	NVMeoFRdmaIOQReleaseDataLam(psSQ->psParentController, psRdmaIoReq);

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Io Req %#p Completed Io Request %s with Status %#x!\n", __FUNCTION__, __LINE__,
                   psIoReq,
                   pcaNVMeIOCmd[((PNVME_READ_WRITE_COMMAND)psRdmaIoReq->psBufferMdlMap->pvBuffer)->ucOpCode],
                   Status);

	if (psIoReq->fnCompletion) {
		_NVMEOF_IO_OP_COMPLETION fnCompletion =
			(_NVMEOF_IO_OP_COMPLETION)_InterlockedExchangePointer((volatile PVOID*)&psIoReq->fnCompletion, NULL);
		fnCompletion(psIoReq->pvHBACtx, psIoReq->pvIoReqCtx, psRdmaIoReq->Status);
	}
	_mm_mfence();

	_InterlockedExchange(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_FREE);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   psSQ->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);
	_InterlockedDecrement((PLONG)&psSQ->lCurrentOutstandingRequest);
	_InterlockedDecrement((PLONG)&psSQ->psParentController->lOutstandingRequests);
}

static
VOID
NVMeoFRdmaNVMeIoResponseCompletion(__in PNVMEOF_QUEUE             psSQ,
                                   __in PNVMEOF_RDMA_WORK_REQUEST psRcqRdmaIoRequest,
                                   __in PNVMEOF_RDMA_WORK_REQUEST psScqRdmaIoRequest,
                                   __in NTSTATUS                  Status)
{
	psScqRdmaIoRequest->Status = NVMeoFGetWinError(psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer);
	*psScqRdmaIoRequest->psCqe = *(PNVME_COMPLETION_QUEUE_ENTRY)psRcqRdmaIoRequest->psBufferMdlMap->pvBuffer;

	if (_InterlockedCompareExchange(&psScqRdmaIoRequest->lReqState,
		NVMEOF_WORK_REQUEST_STATE_FREEING,
		PDS_NVME_REQUEST_ALL_STATE_DONE) == PDS_NVME_REQUEST_ALL_STATE_DONE) {
		Status =
			NVMeoFRdmaIOQInvalidateDataMr(psSQ->psParentController,
				psSQ,
				psScqRdmaIoRequest);
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Submitted Invalidate request with Status %#x!\n", __FUNCTION__, __LINE__, Status);
	}
}

static
NTSTATUS
NVMeoFRdmaIOQFastRegisterDataMR(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                __in NVME_IO_OPCODE            ulOpCode,
                                __in PNVMEOF_QUEUE             psSQ,
                                __in PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq)
{
	UNREFERENCED_PARAMETER(ulOpCode);
	NTSTATUS                        Status = STATUS_SUCCESS;
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSglMap = NULL;
	PNDK_WORK_REQUEST               psNdkWorkRequest = NULL;
	KLOCK_QUEUE_HANDLE              sLockHandle = { 0 };
	KIRQL                           Irql = 0;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	psNdkWorkRequest = psRdmaIoReq->psNdkWorkReq;

	RtlZeroMemory(psNdkWorkRequest, sizeof(*psNdkWorkRequest));

	psLamSglMap = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap;
	psNdkWorkRequest->ulType = NDK_WREQ_SQ_FAST_REGISTER;
	psNdkWorkRequest->pvCallbackCtx = psRdmaIoReq;
	psNdkWorkRequest->ulFlags = (NDK_OP_FLAG_ALLOW_LOCAL_WRITE | NDK_OP_FLAG_ALLOW_REMOTE_READ | NDK_OP_FLAG_ALLOW_REMOTE_WRITE);
	psNdkWorkRequest->sFRMRReq.psNdkMr = psLamSglMap->psNdkMr;
	psNdkWorkRequest->sFRMRReq.ulAdapterPageCount = psLamSglMap->psLaml->AdapterPageCount;
	psNdkWorkRequest->sFRMRReq.psAdapterPageArray = psLamSglMap->psLaml->AdapterPageArray;
	psNdkWorkRequest->sFRMRReq.ulFBO = psLamSglMap->ulFBO;
	psNdkWorkRequest->sFRMRReq.szLength = psRdmaIoReq->psDataBufferMdlMap->ulBufferLength;
	psNdkWorkRequest->sFRMRReq.pvBaseVirtualAddress =
		(PVOID)(((ULONG_PTR)psRdmaIoReq->psDataBufferMdlMap->pvBuffer) ^ PDS_BASEVA_MASK(psRdmaIoReq->psDataBufferMdlMap->ulBufferLength));

	_mm_mfence();

	NVMeoFDebugLog(LOG_LEVEL_INFO,
                   "%s:%d:Issued MR (%p) FBO (%u) Sz (%u) MdlVA %#p Mask 0x%llx Fast reg MR RToken %x!\n",
                   __FUNCTION__,
                   __LINE__,
                   psLamSglMap->psNdkMr,
                   psLamSglMap->ulFBO,
                   MmGetMdlByteCount(psLamSglMap->psMdl),
                   MmGetMdlVirtualAddress(psLamSglMap->psMdl),
                   PDS_BASEVA_MASK(MmGetMdlByteCount(psLamSglMap->psMdl)),
                   psLamSglMap->uiRemoteMemoryTokenKey);

	NVMeoFDebugLog(LOG_LEVEL_INFO,
                   "%s:%d:QID %hu FR-MR %lld Adpt Pg Cnt %lu Io Req %#p Sz (%u) BufferVA %#p VA %#p!\n",
                   __FUNCTION__,
                   __LINE__,
                   psSQ->usQId,
                   psSQ->l64FRMRCurrentCount,
                   psLamSglMap->psLaml->AdapterPageCount,
                   psRdmaIoReq->pvParentReqCtx,
                   psRdmaIoReq->psDataBufferMdlMap->ulBufferLength,
                   psRdmaIoReq->psDataBufferMdlMap->pvBuffer,
                   psNdkWorkRequest->sFRMRReq.pvBaseVirtualAddress);

	_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_PREPARE_SUBMITTING);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                   psSQ->usQId,
                   psRdmaIoReq,
                   psRdmaIoReq->usCommandID,
                   psRdmaIoReq->lReqState);

	Irql = KeGetCurrentIrql();
	NVMeoFRdmaInStackSpinLockAcquire(Irql, &NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->sSubmitSpinLock, &sLockHandle);
	Status = NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, &NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psNdkQP, psNdkWorkRequest);
	NVMeoFRdmaInStackSpinLockRelease(Irql, &sLockHandle);
	if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to Issue fast register 0x%x!\n", __FUNCTION__, __LINE__, Status);
		ASSERT(0);
	} else {
		_InterlockedIncrement(&NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->lFRMRReqOutstanding);
	}

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	return (Status);
}

static
VOID
NVMeoFRdmaIOQBuildDataLamCompletion(__inout PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq,
                                    __in    NTSTATUS                  Status)
{
	if (NT_SUCCESS(Status)) {
		PNVMEOF_QUEUE psSQ = psRdmaIoReq->pvParentQP;
		PNVME_READ_WRITE_COMMAND psNvmeRWPdu = NULL;
		psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->ulFBO = psRdmaIoReq->psNdkWorkReq->sBuildLamReq.ulFboLAM;
		psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->usInitialized = TRUE;
		psNvmeRWPdu = psRdmaIoReq->psBufferMdlMap->pvBuffer;
		Status =
			NVMeoFRdmaIOQFastRegisterDataMR(psSQ->psParentController,
				psNvmeRWPdu->ucOpCode,
				psSQ,
				psRdmaIoReq);
	} else {
		ASSERT(0);
	}
}

#ifdef PDS_DBG
static
VOID
NVMeoFRdmaPrintLAMList(__in LOG_LEVEL ulLoglevel,
	__in ULONG ulAdapterPageCount,
	__in PNDK_LOGICAL_ADDRESS psLA)
{
	PNDK_LOGICAL_ADDRESS psLATemp = psLA;
	NVMeoFDebugLog(ulLoglevel, "%s:%d: LAM!\n", __FUNCTION__, __LINE__);
	while (psLATemp < (psLA + ulAdapterPageCount)) {
		NVMeoFDebugLog(ulLoglevel, "\tLA %#llx!\n", psLATemp->QuadPart);
		psLATemp++;
	}
}
#else
#define NVMeoFRdmaPrintLAMList(ulLoglevel, ulAdapterPageCount, psLA)
#endif

static
NTSTATUS
NVMeoFRdmaIOQCreateDataLam(__in    PNVMEOF_FABRIC_CONTROLLER             psController,
	__inout PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSgl = psRdmaIoReq->psDataBufferMdlMap->psLamSglMap;
	PNDK_WORK_REQUEST psNdkWorkReq = psRdmaIoReq->psNdkWorkReq;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:In!\n", __FUNCTION__, __LINE__);

	RtlZeroMemory(psNdkWorkReq, sizeof(*psNdkWorkReq));
	NVMeoFRdmaGetMdlInfo(psRdmaIoReq->psDataBufferMdlMap->psBufferMdl,
		psRdmaIoReq->psDataBufferMdlMap->pvBuffer,
		(PULONG)&psLamSgl->ulCurrentLamCbSize);
	psLamSgl->psMdl = psRdmaIoReq->psDataBufferMdlMap->psBufferMdl;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Data Buffer LAM %#p!\n", __FUNCTION__, __LINE__, psLamSgl->psLaml);
	psNdkWorkReq->ulType = NDK_WREQ_ADAPTER_BUILD_LAM;
	psNdkWorkReq->sBuildLamReq.psMdl = psRdmaIoReq->psDataBufferMdlMap->psBufferMdl;
	psNdkWorkReq->sBuildLamReq.psLam = psLamSgl->psLaml;
	psNdkWorkReq->sBuildLamReq.ulLamCbSize = psLamSgl->ulCurrentLamCbSize;
	psNdkWorkReq->sBuildLamReq.szMdlBytesToMap = psRdmaIoReq->psDataBufferMdlMap->ulBufferLength;
	psNdkWorkReq->fnCallBack = NVMeoFRdmaIOQBuildDataLamCompletion;
	psNdkWorkReq->pvCallbackCtx = psRdmaIoReq;
	Status = NdkSubmitRequest(psController->sSession.pvSessionFabricCtx, NULL, psNdkWorkReq);
	if (NT_PENDING(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:NdkBuildLam Status Pending %#x!!\n", __FUNCTION__, __LINE__, Status);
	} else if (NT_ERROR(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NdkBuildLam Failed With Status 0x%x!\n", __FUNCTION__, __LINE__, Status);
		ASSERT(0);
	} else if (NT_SUCCESS(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Mdl (%p) LAM cb Sz %u FBO %u AdapterPageCount %u Status %x!\n", __FUNCTION__, __LINE__,
                       psRdmaIoReq->psDataBufferMdlMap->psBufferMdl,
                       psNdkWorkReq->sBuildLamReq.ulLamCbSize,
                       psNdkWorkReq->sBuildLamReq.ulFboLAM,
                       psLamSgl->psLaml->AdapterPageCount,
                       Status);
		psLamSgl->ulFBO = psNdkWorkReq->sBuildLamReq.ulFboLAM;
		psLamSgl->usInitialized = TRUE;
		_mm_mfence();
		//NVMeoFRdmaPrintLAMList(LOG_LEVEL_ERROR, psLamSgl->psLaml->AdapterPageCount, psLamSgl->psLaml->AdapterPageArray);
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Out!\n", __FUNCTION__, __LINE__);
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaPrepareRWRequest(__in PNVMEOF_FABRIC_CONTROLLER    psController,
                           __in NVME_IO_OPCODE               ulOpCode,
                           __in PNVMEOF_IO_REQ               psIoReq,
                           __inout PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq)
{
	PNVME_READ_WRITE_COMMAND psNvmeRWPdu = NULL;
	PNVME_CMD_FLAGS psNvmeCmdFlag = NULL;
	NTSTATUS Status = STATUS_SUCCESS;

	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:Entering for NVMe Request %s!\n", __FUNCTION__, __LINE__, pcaNVMeIOCmd[ulOpCode]);

	psNvmeRWPdu = psRdmaIoReq->psBufferMdlMap->pvBuffer;
	psNvmeRWPdu->ucOpCode = ulOpCode;
	psNvmeRWPdu->ucFlags = 0;
	psNvmeCmdFlag = (PNVME_CMD_FLAGS)&psNvmeRWPdu->ucFlags;
	psNvmeCmdFlag->sOptions.ucRprOrSglForXfer = NVME_CMD_SGL_METABUF;
	psNvmeRWPdu->usCommandId = psRdmaIoReq->usCommandID;
	psNvmeRWPdu->sControl.usControlWord = 0;
	psNvmeRWPdu->ulDataSetMgmt = 0;
	psNvmeRWPdu->ullMetadata = 0ULL;
	psNvmeRWPdu->ullResved2 = 0ULL;
	psNvmeRWPdu->ulExpectedInitRefTag = 0;
	psNvmeRWPdu->usExpectedLBAppTagMask = 0;
	psNvmeRWPdu->usExpectedLBAppTag = 0;
	psNvmeRWPdu->usLength = (psIoReq->usSectorCount - 1);
	psNvmeRWPdu->ulNSId = ((PNVMEOF_FABRIC_CONTROLLER_CONNECT)psController)->ulCurrentNamespaceID;
	psNvmeRWPdu->ullStartLBA = psIoReq->ullStartSector;

	NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_VERBOSE,
                             (PUCHAR)psNvmeRWPdu,
                             sizeof(NVME_CMD),
                             PDS_PRINT_HEX_PER_LINE_MAX_BYTES);
	if (_InterlockedCompareExchangePointer((volatile PVOID*)&psRdmaIoReq->pvParentReqCtx, (PVOID)psIoReq, (PVOID)NULL) != NULL) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Invalid state of Parent Ctx %#p!\n", __FUNCTION__, __LINE__, psRdmaIoReq->pvParentReqCtx);
		_InterlockedExchangePointer((volatile PVOID*)&psRdmaIoReq->pvParentReqCtx, (PVOID)psIoReq);
	}

	psRdmaIoReq->psDataBufferMdlMap->pvBuffer = psIoReq->pucDataBuffer;
	psRdmaIoReq->psDataBufferMdlMap->ulBufferLength = psIoReq->ulLength;
	psRdmaIoReq->psDataBufferMdlMap->psBufferMdl = psIoReq->psBufferMdl;

	if (NVME_IO_CMD_WRITE == ulOpCode) {
		NVMeoFRdmaPrintHexBuffer(LOG_LEVEL_INFO,
                                 psIoReq->pucDataBuffer,
                                 psIoReq->ulLength,
                                 PDS_PRINT_HEX_PER_LINE_32_BYTES);
	}

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: MR Remote Token %x with Len %u!\n", __FUNCTION__, __LINE__,
                   psRdmaIoReq->psDataBufferMdlMap->psLamSglMap->uiRemoteMemoryTokenKey,
                   psRdmaIoReq->psDataBufferMdlMap->ulBufferLength);

	Status =
		NVMeoFRdmaIOQCreateDataLam(psController,
			                       psRdmaIoReq);
	if (NT_SUCCESS(Status)) {
		_InterlockedOr(&psRdmaIoReq->lReqState, NVMEOF_WORK_REQUEST_STATE_INITIALIZED);
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:QId %hu Request %#p CmdId %hu State %#x!\n", __FUNCTION__, __LINE__,
                       ((PNVMEOF_QUEUE)psRdmaIoReq->pvParentQP)->usQId,
                       psRdmaIoReq,
                       psRdmaIoReq->usCommandID,
                       psRdmaIoReq->lReqState);
	}

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Exiting!\n", __FUNCTION__, __LINE__);
	return Status;
}

FORCEINLINE
NTSTATUS
NVMeoFRdmaIOSubmitRW(__in PNVMEOF_FABRIC_CONTROLLER psController,
                     __in PNVMEOF_QUEUE             psSQ,
                     __in NVME_IO_OPCODE            ulOpCode,
                     __in PNVMEOF_IO_REQ            psIOInfo)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NULL;

	if (psController->ulDisconnectFlag) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Disconnect is in progress!\n", __FUNCTION__, __LINE__);
		ASSERT(0);
		return STATUS_INVALID_STATE_TRANSITION;
	}

	Status =
		NVMeoFRdmaGetCurrentFreeIORequest(psController,
                                          psSQ,
                                          &psRdmaIoReq);
	if (Status == STATUS_NO_MORE_ENTRIES) {
		NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d:No Available entries!\n", __FUNCTION__, __LINE__);
		_InterlockedDecrement((PLONG)&psSQ->lCurrentOutstandingRequest);
		_InterlockedDecrement((PLONG)&psController->lOutstandingRequests);
		return Status;
	}

	Status =
		NVMeoFRdmaPrepareRWRequest(psController,
                                   ulOpCode,
                                   psIOInfo,
                                   psRdmaIoReq);
	if (NT_PENDING(Status)) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NVMeoFRdmaPrepareRWRequest Completed with pending status %#x!\n", __FUNCTION__, __LINE__,
                       Status);
		ASSERT(0);
		return Status;
	} else {
		if (NT_ERROR(Status)) {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:NVMeoFRdmaPrepareRWRequest failed with error %#x!\n", __FUNCTION__, __LINE__, Status);
			_InterlockedDecrement((PLONG)&psSQ->lCurrentOutstandingRequest);
			_InterlockedDecrement((PLONG)&psController->lOutstandingRequests);
			ASSERT(0);
			return Status;
		}
	}

	Status =
		NVMeoFRdmaIOQFastRegisterDataMR(psController,
                                        ulOpCode,
                                        psSQ,
                                        psRdmaIoReq);
	if (NT_SUCCESS(Status)) {
		_InterlockedIncrement64(&psSQ->llRequestIssued);
		if (ulOpCode == NVME_IO_CMD_WRITE) {
			_InterlockedIncrement64(&psSQ->llWriteRequestIssued);
		} else {
			_InterlockedIncrement64(&psSQ->llReadRequestIssued);
		}
		Status = STATUS_PENDING;
	} else if (NT_ERROR(Status)) {
		_InterlockedDecrement((PLONG)&psSQ->llRequestIssued);
		_InterlockedDecrement((PLONG)&psController->lOutstandingRequests);
		if (ulOpCode == NVME_IO_CMD_WRITE) {
			_InterlockedIncrement64(&psSQ->llWriteIssueError);
		} else {
			_InterlockedIncrement64(&psSQ->llReadIssueError);
		}
		ASSERT(0);
	}

	return (Status);
}

static
NTSTATUS
NVMeoFRdmaIOWrite(__in PNVMEOF_FABRIC_CONTROLLER psController,
                  __in PNVMEOF_QUEUE             psSQ,
                  __in PNVMEOF_IO_REQ            psIoReq)
{
	return
		NVMeoFRdmaIOSubmitRW(psController,
                             psSQ,
                             NVME_IO_CMD_WRITE,
                             psIoReq);
}

static
NTSTATUS
NVMeoFRdmaIORead(__in PNVMEOF_FABRIC_CONTROLLER psController,
                 __in PNVMEOF_QUEUE             psSQ,
                 __in PNVMEOF_IO_REQ            psIoReq)
{
	return
		NVMeoFRdmaIOSubmitRW(psController,
                             psSQ,
                             NVME_IO_CMD_READ,
                             psIoReq);
}

static
NTSTATUS
NVMeoFRdmaXportDisconnectController(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Entring!\n", __FUNCTION__, __LINE__);
	NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: Controller id 0x%X\n", __FUNCTION__, __LINE__, psController->usControllerID);

	if (psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->sQP.psNdkQp) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Closing Admin Queue\n", __FUNCTION__, __LINE__);
		PdsNdkDisconnectSyncAsyncRdmaSocketQP(psController->sSession.pvSessionFabricCtx,
			&psController->sSession.psAdminQueue->psFabricCtx->psRdmaCtx->sQP);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Freeing Admin Resources.\n", __FUNCTION__, __LINE__);
		NVMeoFRdmaAdminFreeResources(psController);
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Freeing RDMA Socket Resources.\n", __FUNCTION__, __LINE__);
		PdsNdkDisconnecAndCloseAsyncRdmaSocket(psController->sSession.pvSessionFabricCtx);
		psController->lAdminQueueCount--;
	}

	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return  (Status);
}

static
NTSTATUS
NVMeoFRdmaXportDisableIOQueueEvents(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS       Status = STATUS_SUCCESS;
	PPDS_NUMA_INFO psNumaSQPair = psController->sSession.psNumaSQPair;
	USHORT         usQCount = 0, usNumaNodeCnt = 0;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Entered to disable Events on IOQs 0x%p!\n", __FUNCTION__, __LINE__, psNumaSQPair);

	for (; psNumaSQPair && (usNumaNodeCnt < psController->sSession.usNumaNodeCount);
		usNumaNodeCnt++, psNumaSQPair++) {
		NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Disabling Events on IOQ on NUMA node %hu NUMA SQ Pair %hu!\n", __FUNCTION__, __LINE__, usNumaNodeCnt, psNumaSQPair->usNumberOfIOQ);
		if (psNumaSQPair->psSQ) {
			PNVMEOF_QUEUE psNodeSQPair = NULL;
			for (psNodeSQPair = psNumaSQPair->psSQ, usQCount = 0;
				usQCount < psNumaSQPair->usNumberOfIOQ;
				usQCount++, psNodeSQPair++) {
				NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Disabling Events on IOQ %d QCnt %hu!\n", __FUNCTION__, __LINE__,
					psNodeSQPair->usQId,
					usQCount);
			}
		}
	}

	return Status;
}

static
NTSTATUS
NVMeoFRdmaXportDisconnectControllerIOQueues(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS		       Status = STATUS_SUCCESS;
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Entring!\n", __FUNCTION__, __LINE__);
	NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Controller id 0x%X\n", __FUNCTION__, __LINE__, psController->usControllerID);
	if (psController->eControllerType == FABRIC_CONTROLLER_TYPE_CONNECT) {
		Status = NVMeoFRdmaIOQueuesDisconnect(psController);
	}

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return  (Status);
}

static
BOOLEAN
NVMeoFRdmaXportControllerQueuesBusy(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	PPDS_NUMA_INFO psNumaSQPair = psController->sSession.psNumaSQPair;
	USHORT         usQCount = 0, usNumaNodeCnt = 0;
	PNVMEOF_QUEUE  psNodeSQPair = NULL;

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Entring!\n", __FUNCTION__, __LINE__);
	for (; usNumaNodeCnt < psController->sSession.usNumaNodeCount;
		usNumaNodeCnt++, psNumaSQPair++) {
		if (psNumaSQPair->psSQ) {
			for (psNodeSQPair = psNumaSQPair->psSQ, usQCount = 0;
				usQCount < psNumaSQPair->usNumberOfIOQ;
				usQCount++, psNodeSQPair++) {
				if (_InterlockedCompareExchange(&psNodeSQPair->lBusy, 1, 0)) {
					return (TRUE);
				}
			}
		}
	}

	if (psController->sSession.pvSessionFabricCtx) {
		if (_InterlockedCompareExchange(&psController->sSession.psAdminQueue->lBusy, 1, 0)) {
			return (TRUE);
		}
	}

	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d: Exiting!\n", __FUNCTION__, __LINE__);
	return  (FALSE);
}

static
NTSTATUS
NVMeoFRdmaNvmeofDisconnect(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVME_CMD psRdmaConnectionTerminationPdu = NULL;

	psRdmaConnectionTerminationPdu = NVMeoFRdmaAllocateAdminDisconnectRequest(psController);
	if (!psRdmaConnectionTerminationPdu) {
		return STATUS_NO_MEMORY;
	}

	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NVMeoFRdmaAllocateNpp(sizeof(NVMEOF_REQUEST_RESPONSE));
	if (!psAdminReqResp) {
		NVMeoFRdmaFreeNpp(psRdmaConnectionTerminationPdu);
		return STATUS_NO_MEMORY;
	}

	RtlZeroMemory(psAdminReqResp, sizeof(*psAdminReqResp));

	NVMEOF_REQUEST_GET_COMMAND_BUFFER(psAdminReqResp).pvBuffer = psRdmaConnectionTerminationPdu;
	NVMEOF_REQUEST_GET_COMMAND_BUFFER(psAdminReqResp).ulBufferLen = sizeof(NVME_CMD);
	NVMEOF_RESPONSE_GET_RESPONSE_BUFFER(psAdminReqResp).pvBuffer = NULL;
	NVMEOF_RESPONSE_GET_RESPONSE_BUFFER(psAdminReqResp).ulBufferLen = 0;
	psAdminReqResp->ulFlags = NVMEOF_REQUEST_RESPONSE_TYPE_SYNC;

	Status = 
		NVMeoFRdmaAdminSubmitRequestSyncAsync(psController,
                                              psRdmaConnectionTerminationPdu->sDisconnect.usCommandId,
                                              psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		//NVMeoFDisplayCQE (__FUNCTION__, &psAdminReqResp->sCqe);
	}
	NVMeoFRdmaFreeNpp(psAdminReqResp);
	NVMeoFRdmaFreeNpp(psRdmaConnectionTerminationPdu);
	return (Status);
}

static
UINT32
NVMeoFRdmaGetPduLen(__in PNVMEOF_FABRIC_CONTROLLER psController, 
                    __in PDS_NVME_PDU_TYPE ulPduType)
{
	UNREFERENCED_PARAMETER(psController);
	switch (ulPduType) {
	case PDS_NVME_PDU_TYPE_COMMAND:
		return (sizeof(NVME_CMD));
		break;
	case PDS_NVME_PDU_TYPE_RESPONSE:
		return (sizeof(NVME_COMPLETION_QUEUE_ENTRY));
		break;
	default:
		return 0;
	}
}

static
VOID
NVMeoFRdmaPrepareSetGetPropertyCommand(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                       __in PNVME_CMD                 psNvmeCmd,
                                       __in NVMEOF_CAPSULE_COMMAND    ulGetSetFeatureCmd,
                                       __in UCHAR                     ucAttrib,
                                       __in ULONG                     ulOffset,
                                       __in_opt PULONG                pulVal)
{
	PNVME_CMD_FLAGS psFlags = NULL;
	psNvmeCmd->sPropertySet.ucOpCode = NVME_OVER_FABRICS_COMMAND;
	psNvmeCmd->sPropertySet.ucFabricCmdType = ulGetSetFeatureCmd;
	psNvmeCmd->sPropertySet.ucAttrib = ucAttrib;
	psNvmeCmd->sPropertySet.ulOffset = ulOffset;
	psNvmeCmd->sPropertySet.usCommandId = NVMeoFRdmaGetAdminCommandID(psController);
	if ((ulGetSetFeatureCmd == NVMEOF_TYPE_PROPERTY_SET) && pulVal) {
		psNvmeCmd->sPropertySet.ullValue = *pulVal;
	}

	psFlags = (PVOID)&psNvmeCmd->sPropertySet.ucResvd1;
	psFlags->sOptions.ucRprOrSglForXfer = NVME_CMD_SGL_METABUF;

	NVMeoFRdmaNvmeofXportSetSG(psController,
                               NVME_ADMIN_QUEUE_ID,
                               psNvmeCmd,
                               0,
                               0,
                               0,
                               NVMEOF_COMMAND_DESCRIPTORT_TYPE_NULL);
}

static
PNVME_CMD
NVMeoFRdmaAllocateSetGetPropertyCommand(__in PNVMEOF_FABRIC_CONTROLLER psController,
                                        __in NVMEOF_CAPSULE_COMMAND    ulGetSetFeatureCmd,
                                        __in UCHAR                     ucAttrib,
                                        __in ULONG                     ulOffset,
                                        __in_opt PULONG                pulVal)
{
	PNVME_CMD       psNvmeCmd = NULL;
	ULONG           ulCmdLen = 0;
	NTSTATUS        Status = STATUS_SUCCESS;

	Status = NVMeoFRdmaAllocateCmd(psController, 0, NVME_ADMIN_QUEUE_ID, &psNvmeCmd, &ulCmdLen);
	if (!NT_SUCCESS(Status)) {
		return NULL;
	}

	NVMeoFRdmaPrepareSetGetPropertyCommand(psController,
                                           psNvmeCmd,
                                           ulGetSetFeatureCmd,
                                           ucAttrib,
                                           ulOffset,
                                           pulVal);
	return (psNvmeCmd);
}

static
NTSTATUS
NVMeoFRdmaRead32(__in    PNVMEOF_FABRIC_CONTROLLER psController,
                 __in    ULONG  ulOffset,
                 __inout PULONG pulValue)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;
	PNVME_CMD psGetSetFeatureCmdPdu = NULL;
	Status =
		psController->sFabricOps._FabricNVMeoFRequestsAllocate(psController,
                                                               NVME_ADMIN_QUEUE_ID,
                                                               &psAdminReqResp,
                                                               0,
                                                               0);
	if (NT_ERROR(Status)) {
		return STATUS_NO_MEMORY;
	}

	psGetSetFeatureCmdPdu =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psController, psAdminReqResp);

	NVMeoFRdmaPrepareSetGetPropertyCommand(psController,
                                          psGetSetFeatureCmdPdu,
                                          NVMEOF_TYPE_PROPERTY_GET,
                                          NVMEOF_PROPERTY_COMMAND_ATTRIB_4B,
                                          ulOffset,
                                          NULL);

	psAdminReqResp->ulFlags |= NVMEOF_REQUEST_RESPONSE_TYPE_SYNC;

	Status = 
		NVMeoFRdmaAdminSubmitRequestSyncAsync(psController,
                                              psGetSetFeatureCmdPdu->sPropertyGet.usCommandId,
                                              psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY
			psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psController,
				psAdminReqResp);
		//NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (!psCqe->sStatus.sStatusField.SC && psCqe->sStatus.sStatusField.SCT) {
			NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d: Return value = 0x%llx\n", __FUNCTION__, __LINE__,
				psCqe->sCmdRespData.ullDDW);
			*pulValue = (UINT32)psCqe->sCmdRespData.ullDDW;
		}
	}

	psController->sFabricOps._FabricNVMeoFRequestsFree(psController, psAdminReqResp);

	return (Status);
}

static
NTSTATUS
NVMeoFRdmaWrite32(__in PNVMEOF_FABRIC_CONTROLLER psController,
                  __in ULONG ulOffset,
                  __in ULONG ulValue)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;
	PNVME_CMD psGetSetFeatureCmdPdu = NULL;
	Status =
		psController->sFabricOps._FabricNVMeoFRequestsAllocate(psController,
                                                               NVME_ADMIN_QUEUE_ID,
                                                               &psAdminReqResp,
                                                               0,
                                                               0);
	if (NT_ERROR(Status)) {
		return STATUS_NO_MEMORY;
	}

	psGetSetFeatureCmdPdu =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psController, psAdminReqResp);
	NVMeoFRdmaPrepareSetGetPropertyCommand(psController,
		                                   psGetSetFeatureCmdPdu,
		                                   NVMEOF_TYPE_PROPERTY_SET,
		                                   NVMEOF_PROPERTY_COMMAND_ATTRIB_4B,
		                                   ulOffset,
		                                   &ulValue);

	psAdminReqResp->ulFlags |= NVMEOF_REQUEST_RESPONSE_TYPE_SYNC;

	Status =
		NVMeoFRdmaAdminSubmitRequestSyncAsync(psController,
                                              psGetSetFeatureCmdPdu->sPropertyGet.usCommandId,
                                              psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
			psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psController,
				                                                           psAdminReqResp);
		//NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (!psCqe->sStatus.sStatusField.SC && psCqe->sStatus.sStatusField.SCT) {
			NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d: Return value = 0x%llx\n", __FUNCTION__, __LINE__,
				psCqe->sCmdRespData.ullDDW);
		}
	}

	psController->sFabricOps._FabricNVMeoFRequestsFree(psController, psAdminReqResp);

	return (Status);
}

static
NTSTATUS
NVMeoFRdmaRead64(__in    PNVMEOF_FABRIC_CONTROLLER psController,
                 __in    ULONG ulOffset,
                 __inout PUINT64 pui64Value)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_REQUEST_RESPONSE psAdminReqResp = NULL;
	PNVME_CMD psGetSetFeatureCmdPdu = NULL;
	Status =
		psController->sFabricOps._FabricNVMeoFRequestsAllocate(psController,
                                                               NVME_ADMIN_QUEUE_ID,
                                                               &psAdminReqResp,
                                                               0,
                                                               0);
	if (NT_ERROR(Status)) {
		return STATUS_NO_MEMORY;
	}

	psGetSetFeatureCmdPdu =
		psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand(psController, psAdminReqResp);
	NVMeoFRdmaPrepareSetGetPropertyCommand(psController,
                                           psGetSetFeatureCmdPdu,
                                           NVMEOF_TYPE_PROPERTY_GET,
                                           NVMEOF_PROPERTY_COMMAND_ATTRIB_8B,
                                           ulOffset,
                                           NULL);

	psAdminReqResp->ulFlags |= NVMEOF_REQUEST_RESPONSE_TYPE_SYNC;

	Status =
		NVMeoFRdmaAdminSubmitRequestSyncAsync(psController,
                                              psGetSetFeatureCmdPdu->sPropertyGet.usCommandId,
                                              psAdminReqResp);
	if (NT_SUCCESS(Status)) {
		PNVME_COMPLETION_QUEUE_ENTRY psCqe =
            psAdminReqResp->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe(psController,
				                                                           psAdminReqResp);
		//NVMeoFDisplayCQE(__FUNCTION__, psCqe);
		if (!psCqe->sStatus.sStatusField.SC && psCqe->sStatus.sStatusField.SCT) {
			NVMeoFDebugLog(LOG_LEVEL_DEBUG, "%s:%d: Return value = 0x%llx\n", __FUNCTION__, __LINE__,
				psCqe->sCmdRespData.ullDDW);
			*pui64Value = (UINT64)psCqe->sCmdRespData.ullDDW;
		}
	}

	psController->sFabricOps._FabricNVMeoFRequestsFree(psController, psAdminReqResp);

	return (Status);
}

static
USHORT
NVMeoFRdmaGetXportHeaderSize(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	UNREFERENCED_PARAMETER(psController);
	return (0);
}

static
NTSTATUS
NVMeoFRdmaResetController(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS       Status = STATUS_SUCCESS;
	USHORT         usIOQId = 1;

	for (usIOQId = 1; usIOQId < (USHORT)psController->sSession.lSQQueueCount; usIOQId++) {
		PNVMEOF_QUEUE psSQ = NVMeoFRdmaGetIOQFromQId(psController, usIOQId);
		if (psSQ) {
			PNVMEOF_RDMA_WORK_REQUEST psRdmaIoReq = NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList;
			while (psRdmaIoReq < (NVMEOF_QUEUE_GET_RDMA_CTX(psSQ)->psSubmissionWorkRequestList + psSQ->ulQueueDepth)) {
				if (psRdmaIoReq->lReqState != NVMEOF_WORK_REQUEST_STATE_FREE) {
					NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: IO Request is not completed = %#p state %#x the QID %hu\n", __FUNCTION__, __LINE__,
                                   psRdmaIoReq->pvParentReqCtx,
                                   psRdmaIoReq->lReqState,
                                   usIOQId);
					NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d: IO Request is not completed = %#p state %#x the QID %hu\n", __FUNCTION__, __LINE__,
                                   psRdmaIoReq->pvParentReqCtx,
                                   psRdmaIoReq->lReqState,
                                   usIOQId);
				}
				psRdmaIoReq++;
			}
		}
	}

	return Status;
}

static
PNVME_CMD
NVMeoFRdmaAdminRequestGetCommand(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminRequest)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);

	return  (PNVME_CMD)NVMEOF_REQUEST_GET_COMMAND_BUFFER(psAdminRequest).pvBuffer;
}

static
PVOID
NVMeoFRdmaAdminRequestGetCommandData(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminRequest)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);

	return  NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).pvBuffer;
}

static
PNVME_COMPLETION_QUEUE_ENTRY
NVMeoFRdmaAdminRequestGetCqe(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminRequest)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);

	return  (PNVME_COMPLETION_QUEUE_ENTRY)NVMEOF_RESPONSE_GET_RESPONSE_BUFFER(psAdminRequest).pvBuffer;
}

static
PVOID
NVMeoFRdmaAdminRequestGetResponseData(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminRequest)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);

	return  NVMEOF_RESPONSE_GET_DATA_BUFFER(psAdminRequest).pvBuffer;
}

static
PVOID
NVMeoFRdmaAdminRequestGetCommandPduLength(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminRequest)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);

	return  NVMEOF_REQUEST_GET_COMMAND_BUFFER(psAdminRequest).ulBufferLen;
}

static
ULONG
NVMeoFRdmaAdminRequestGetResponsePduLength(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminRequest)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);

	return  NVMEOF_RESPONSE_GET_RESPONSE_BUFFER(psAdminRequest).ulBufferLen;
}

static
NTSTATUS
NVMeoFRdmaAdminRequestSetCommandData(PNVMEOF_FABRIC_CONTROLLER psController,
                                     PNVMEOF_REQUEST_RESPONSE psAdminRequest,
                                     PVOID pvBuffer,
                                     ULONG ulBufferLength)
{
	NT_ASSERT(psController->eFabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);
	NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).pvBuffer = pvBuffer;
	NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).ulBufferLen = ulBufferLength;
	NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).psBufferMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).pvBuffer, ulBufferLength);
	if (!NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).psBufferMdl) {
		NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).pvBuffer = NULL;
		NVMEOF_REQUEST_GET_DATA_BUFFER(psAdminRequest).ulBufferLen = 0;
		return (STATUS_NO_MEMORY);
	}
	psAdminRequest->ulFlags |= NVMEOF_REQUEST_RESPONSE_TYPE_USER_SEND_DATA_BUFFER;
	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaAdminRequestSetResponseData(PNVMEOF_FABRIC_CONTROLLER psController,
                                      PNVMEOF_REQUEST_RESPONSE psAdminRequest,
                                      PVOID pvBuffer,
                                      ULONG ulBufferLength)
{
	NT_ASSERT(psController->FabricType == NVMF_TRTYPE_RDMA);
	NT_ASSERT(psAdminRequest->FabricType == NVMF_TRTYPE_RDMA);
	psAdminRequest->sRcvBuffer.pvData = pvBuffer;
	psAdminRequest->sRcvBuffer.ulDataLen = ulBufferLength;
	psAdminRequest->sRcvBuffer.psDataMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(psAdminRequest->sRcvBuffer.psDataMdl, ulBufferLength);
	if (!psAdminRequest->sRcvBuffer.psDataMdl) {
		psAdminRequest->sRcvBuffer.pvData = NULL;
		psAdminRequest->sRcvBuffer.ulDataLen = 0;
		return (STATUS_NO_MEMORY);
	}
	psAdminRequest->ulFlags |= PDS_NVMEOF_ADMIN_REQUEST_TYPE_USER_RECEIVE_DATA_BUFFER;
	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaXportFreeAdminRequests(PNVMEOF_FABRIC_CONTROLLER psController, PNVMEOF_REQUEST_RESPONSE psAdminReq)
{
	if (psAdminReq) {
		if (psAdminReq->sSndBuffer.pvData &&
			!(psAdminReq->ulFlags & PDS_NVMEOF_ADMIN_REQUEST_TYPE_USER_SEND_DATA_BUFFER)) {
			NVMeoFRdmaFreeNpp(psAdminReq->sSndBuffer.pvData);
			psAdminReq->sSndBuffer.pvData = NULL;
		}

		if (psAdminReq->sRcvBuffer.pvData &&
			!(psAdminReq->ulFlags & PDS_NVMEOF_ADMIN_REQUEST_TYPE_USER_RECEIVE_DATA_BUFFER)) {
			NVMeoFRdmaFreeNpp(psAdminReq->sRcvBuffer.pvData);
			psAdminReq->sRcvBuffer.pvData = NULL;
		}

		if (psAdminReq->sRcvBuffer.pvBuffer) {
			NVMeoFRdmaFreeNVMeCqe(psController, psAdminReq->sRcvBuffer.pvBuffer);
			psAdminReq->sRcvBuffer.pvBuffer = NULL;
		}

		if (psAdminReq->sSndBuffer.pvBuffer) {
			NVMeoFRdmaFreeNVMeCmd(psController, psAdminReq->sSndBuffer.pvBuffer);
			psAdminReq->sSndBuffer.pvBuffer = NULL;
		}

		if (psAdminReq->sSndBuffer.psBufferMdl) {
			IoFreeMdl(psAdminReq->sSndBuffer.psBufferMdl);
			psAdminReq->sSndBuffer.psBufferMdl = NULL;
		}

		if (psAdminReq->sRcvBuffer.psBufferMdl) {
			IoFreeMdl(psAdminReq->sRcvBuffer.psBufferMdl);
			psAdminReq->sRcvBuffer.psBufferMdl = NULL;
		}

		if (psAdminReq->sRcvBuffer.psDataMdl) {
			IoFreeMdl(psAdminReq->sRcvBuffer.psDataMdl);
			psAdminReq->sRcvBuffer.psDataMdl = NULL;
		}

		if (psAdminReq->sSndBuffer.psDataMdl) {
			IoFreeMdl(psAdminReq->sSndBuffer.psDataMdl);
			psAdminReq->sSndBuffer.psDataMdl = NULL;
		}

		NVMeoFRdmaFreeAdminRequest(psController, psAdminReq);
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
NVMeoFRdmaXportAllocateAdminRequests(PNVMEOF_FABRIC_CONTROLLER psController,
                                     PNVMEOF_REQUEST_RESPONSE* ppsRequest,
                                     ULONG ulSendDataLen,
                                     ULONG ulReceiveDataLen)
{
	PNVMEOF_BUFFER  psBuff = NULL;
	PNVMEOF_REQUEST_RESPONSE psAdminReq = NVMeoFRdmaAllocateAdminRequest(psController);
	if (!psAdminReq) {
		goto ExitError;
	}

	RtlZeroMemory(psAdminReq, sizeof(*psAdminReq));
	psAdminReq->FabricType = CONTROLLER_FABRIC_TYPE_RDMA;
	psBuff = &psAdminReq->sSndBuffer.sReqRespBuffer;
	psBuff->pvBuffer = NVMeoFRdmaAllocateNVMeCmd(psController);
	if (!psBuff->pvBuffer) {
		goto ExitError;
	}

	psBuff->psBufferMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(psBuff->pvBuffer, sizeof(NVME_CMD));
	if (!psBuff->psBufferMdl) {
		goto ExitError;
	}

	if (ulSendDataLen) {
		psBuff = &psAdminReq->sSndBuffer.sDataBuffer;
		psBuff->pvBuffer = NVMeoFRdmaAllocateNpp(ulSendDataLen);
		if (!psBuff->pvBuffer) {
			goto ExitError;
		}
		psBuff->psBufferMdl =
			NVMeoFRdmaAllocateMdlForBufferFromNP(psBuff->pvBuffer, ulSendDataLen);
		if (!psBuff->psBufferMdl) {
			goto ExitError;
		}
	}

	psBuff = &psAdminReq->sRcvBuffer.sReqRespBuffer;
	psBuff->pvBuffer = NVMeoFRdmaAllocateNVMeCqe(psController);
	if (!psBuff->pvBuffer) {
		goto ExitError;
	}
	psBuff->psBufferMdl =
		NVMeoFRdmaAllocateMdlForBufferFromNP(psBuff->pvBuffer, sizeof(NVME_COMPLETION_QUEUE_ENTRY));
	if (!psBuff->psBufferMdl) {
		goto ExitError;
	}

	if (ulReceiveDataLen) {
		psBuff = &psAdminReq->sRcvBuffer.sDataBuffer;
		psBuff->pvBuffer = NVMeoFRdmaAllocateNpp(ulSendDataLen);
		if (!psBuff->pvBuffer) {
			goto ExitError;
		}
		psBuff->psBufferMdl =
			NVMeoFRdmaAllocateMdlForBufferFromNP(psBuff->pvBuffer,
				sizeof(NVME_COMPLETION_QUEUE_ENTRY));
		if (!psBuff->psBufferMdl) {
			goto ExitError;
		}
	}

	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommand = NVMeoFRdmaAdminRequestGetCommand;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommandData = NVMeoFRdmaAdminRequestGetCommandData;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCommandPduLength = NVMeoFRdmaAdminRequestGetCommandPduLength;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetCqe = NVMeoFRdmaAdminRequestGetCqe;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetResponseData = NVMeoFRdmaAdminRequestGetResponseData;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetResponsePduLength = NVMeoFRdmaAdminRequestGetResponsePduLength;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseGetPduHeaderSize = NVMeoFRdmaAdminRequestGetPduHeaderSize;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseSetCommandData = NVMeoFRdmaAdminRequestSetCommandData;
	psAdminReq->sNVMeoFReqRespOps._NVMeoFRequestResponseSetResponseData = NVMeoFRdmaAdminRequestSetResponseData;


	*ppsRequest = psAdminReq;

	return (STATUS_SUCCESS);

ExitError:
	NVMeoFRdmaXportFreeAdminRequests(psController, psAdminReq);
	*ppsRequest = NULL;

	return (STATUS_NO_MEMORY);
}

static
PNVMEOF_FABRIC_CONTROLLER
NVMeoFRdmaAllocateController(__in CONTROLLER_TYPE ulControllerType,
                             __in TRANSPORT_TYPE  ulXport,
                             __in PSOCKADDR_INET  psControllerAddr,
                             __in PSOCKADDR_INET  psHostAddr,
                             __in PCHAR           pcNQN)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_FABRIC_CONTROLLER psController = NULL;
	psController = (PNVMEOF_FABRIC_CONTROLLER)NVMeoFRdmaAllocateNpp(sizeof(NVMEOF_CONTROLLER));
	if (psController) {
		RtlZeroMemory(psController, sizeof(*psController));
		psController->ulControllerType = ulControllerType;
		psController->FabricType = ulXport;
		psController->sTargetAddress = *psControllerAddr;
		psController->sHostXportAddress = *psHostAddr;
		psController->sXportOps._XportConnectAdminQueue = NVMeoFRdmaXportConnectControllerAdminQueue;
		psController->sXportOps._XportConnectIOQueues = NVMeoFRdmaXportConnectControllerIOQeueus;
		psController->sXportOps._XportDisconnect = NVMeoFRdmaXportDisconnectController;
		psController->sXportOps._XportDisconnectIOQueues = NVMeoFRdmaXportDisconnectControllerIOQueues;
		psController->sXportOps._XportDisableIOQueuesEvent = NVMeoFRdmaXportDisableIOQueueEvents;
		psController->sXportOps._XportQueuesBusy = NVMeoFRdmaXportControllerQueuesBusy;
		psController->sXportOps._XportNvmeofGetCmdID = NVMeoFRdmaGetAdminCommandID;
		psController->sXportOps._XportNvmeofGetPduLen = NVMeoFRdmaGetPduLen;
		psController->sXportOps._XportSetupNvmeofAdminConnection = NVMeoFRdmaSetupAdminConnection;
		psController->sXportOps._XportSetupNvmeofIOConnection = NVMeoFRdmaSetupNvmeofIOQueues;
		psController->sXportOps._XportNvmeofDisconnect = NVMeoFRdmaNvmeofDisconnect;
		psController->sXportOps._XportRegRead32 = NVMeoFRdmaRead32;
		psController->sXportOps._XportRegWrite32 = NVMeoFRdmaWrite32;
		psController->sXportOps._XportRegRead64 = NVMeoFRdmaRead64;
		psController->sXportOps._XportGetAdminRespDataAndLen = NVMeoFRdmaGetCQEOrData;
		psController->sXportOps._XportIsDataPacket = NVMeoFRdmaIsRespData;
		psController->sXportOps._XportGetHeaderSize = NVMeoFRdmaGetXportHeaderSize;
		psController->sXportOps._XportSetSG = NVMeoFRdmaNvmeofXportSetSG;
		psController->sXportOps._XportFlushAllQueues = NVMeoFRdmaNvmeofFlushAllQueue;
		psController->sXportOps._XportResetController = NVMeoFRdmaResetController;
		psController->sXportOps._XportReleaseIOQResources = NVMeoFRdmaIOQReleaseResources;
		psController->sFabricOps._FabricNVMeoFRequestsAllocate = NVMeoFRdmaXportAllocateAdminRequests;
		psController->sFabricOps._FabricNVMeoFRequestsFree = NVMeoFRdmaXportFreeAdminRequests;
		NVMeoFRdmaInitXportPipe(psController);
		Status = RtlStringCchCopyA(psController->caSubsystemNQN, sizeof(psController->caSubsystemNQN), pcNQN);
		if (!NT_SUCCESS(Status)) {
			NVMeoFRdmaFreeNpp(psController);
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:RtlStringCchCopyA Failed with Status = 0x%x", __FUNCTION__, Status);
			NVMeoFLogEvent(NVME_OF_ERROR, "%s:RtlStringCchCopyA Failed with Status = 0x%x", __FUNCTION__, Status);
			psController = NULL;
		}
		else {
			InitializeListHead(&psController->sFabricEntry);
		}
	}
	return (psController);
}

VOID
NVMeoFRdmaInsertControllerToConnectedList(__in PNVMEOF_FABRIC_CONTROLLER psController)
{
	KLOCK_QUEUE_HANDLE sLockHandle = { 0 };
	InitializeListHead(&psController->sFabricEntry);
	KeAcquireInStackQueuedSpinLock(&glsRdmaControllerList.sRdmaControllerListSpinLock, &sLockHandle);
	InsertTailList(&glsRdmaControllerList.sRdmaControllerHead, &psController->sFabricEntry);
	KeReleaseInStackQueuedSpinLock(&sLockHandle);
}

PNVMEOF_FABRIC_CONTROLLER
NVMeoFRdmaAllocatAndInitializeController(__in CONTROLLER_TYPE ulControllerType,
	__in TRANSPORT_TYPE  ulXport,
	__in PSOCKADDR_INET  psControllerIPAddress,
	__in PCHAR           pcNQN,
	__in PSOCKADDR_INET  psLocalIPAddress,
	__in PCHAR           pcHostNQN,
	__in PGUID           pguidHostUUID)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNVMEOF_FABRIC_CONTROLLER psController = NULL;

	if (!glsRdmaControllerList.ulRdmaModuleInitDone) {
		return NULL;
	}

	Status = PdsNdkStartup();
	if (!NT_SUCCESS(Status)) {
		if (Status != STATUS_ALREADY_REGISTERED) {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:PdsNdkStartup Failed with Status = 0x%x\n", __FUNCTION__, __LINE__, Status);
			NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:PdsNdkStartup Failed with Status = 0x%x\n", __FUNCTION__, __LINE__, Status);
			return NULL;
		}
	}

	psController = NVMeoFRdmaAllocateController(ulControllerType, ulXport, psControllerIPAddress, psLocalIPAddress, pcNQN);
	if (psController) {
		UNICODE_STRING UnicodeGUID = { 0 };
		WCHAR wcHostUID[NVMF_NQN_FIELD_LEN] = { 0 };
		DWORD lenWUUID = 0;
		Status = RtlStringFromGUID(pguidHostUUID, &UnicodeGUID);
		if (Status != STATUS_SUCCESS) {
			NVMeoFRdmaFreeNpp(psController);
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to convert to the GUID to String with Error 0x%x\n", __FUNCTION__, __LINE__, Status);
			NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Failed to convert to the GUID to String with Error 0x%x\n", __FUNCTION__, __LINE__, Status);
			return NULL;
		}

		Status = RtlStringCbCopyUnicodeString(wcHostUID, sizeof(wcHostUID), &UnicodeGUID);
		if (!NT_SUCCESS(Status)) {
			RtlFreeUnicodeString(&UnicodeGUID);
			NVMeoFRdmaFreeNpp(psController);
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d:Failed to convert to the GUID to String with Error 0x%x\n", __FUNCTION__, __LINE__, Status);
			NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d:Failed to convert to the GUID to String with Error 0x%x\n", __FUNCTION__, __LINE__, Status);
			return NULL;
		}

		lenWUUID = (DWORD)wcslen(wcHostUID);
		wcstombs(psController->caHostUID, wcHostUID + 1, (size_t)(lenWUUID - 2));
		psController->sHostID = *pguidHostUUID;
		if (pguidHostUUID) {
			Status = RtlStringCbPrintfA(psController->caHostNQN, sizeof(psController->caHostNQN), "%s", pcHostNQN);
		}
		else {
			Status = RtlStringCbPrintfA(psController->caHostNQN,
				sizeof(psController->caHostNQN),
				NVME_HOST_ID_HEADER_STRING,
				psController->caHostUID);
		}
	}
	return psController;
}

PNVMEOF_FABRIC_CONTROLLER
NVMeoFRdmaFindControllerByNQN(PCHAR pcNQN)
{
	PLIST_ENTRY psControllerListHead = &glsRdmaControllerList.sRdmaControllerHead;
	PLIST_ENTRY psEntry = NULL;
	PNVMEOF_FABRIC_CONTROLLER psController = NULL;
	KLOCK_QUEUE_HANDLE LockHandle = { 0 };

	if (!glsRdmaControllerList.ulRdmaModuleInitDone) {
		return NULL;
	}

	KeAcquireInStackQueuedSpinLock(&glsRdmaControllerList.sRdmaControllerListSpinLock, &LockHandle);
	if (!IsListEmpty(psControllerListHead)) {
		for (psEntry = psControllerListHead->Flink;
			psEntry != psControllerListHead;
			psEntry = psEntry->Flink) {
			psController = CONTAINING_RECORD(psEntry, NVMEOF_CONTROLLER, sFabricEntry);
			if (strlen(pcNQN) == strlen(psController->caSubsystemNQN) && !strncmp(psController->caSubsystemNQN, pcNQN, strlen(pcNQN))) {
				NVMeoFDebugLog(LOG_LEVEL_INFO, "%s:%d: NQN Found %s req NQN %s are same!\n", __FUNCTION__, __LINE__, psController->caSubsystemNQN, pcNQN);
				break;
			}
			psController = NULL;
		}
	}
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	return psController;
}

PNVMEOF_FABRIC_CONTROLLER
NVMeoFRdmaFindControllerByNQNAndTargetIP(__in PCHAR pcNQN,
	__in PSOCKADDR_INET psTargetAddr)
{
	PLIST_ENTRY psControllerListHead = &glsRdmaControllerList.sRdmaControllerHead;
	PLIST_ENTRY psEntry = NULL;
	PNVMEOF_FABRIC_CONTROLLER psController = NULL;
	KLOCK_QUEUE_HANDLE sLockHandle = { 0 };

	if (!glsRdmaControllerList.ulRdmaModuleInitDone) {
		return NULL;
	}

	KeAcquireInStackQueuedSpinLock(&glsRdmaControllerList.sRdmaControllerListSpinLock, &sLockHandle);
	for (psEntry = psControllerListHead->Flink;
		(psEntry != psControllerListHead);
		psEntry = psEntry->Flink) {
		psController = CONTAINING_RECORD(psEntry, NVMEOF_CONTROLLER, sFabricEntry);
		if (strlen(pcNQN) == strlen(psController->caSubsystemNQN) &&
			!strncmp(psController->caSubsystemNQN, pcNQN, strlen(pcNQN)) &&
			(psController->sTargetAddress.Ipv4.sin_addr.S_un.S_addr == psTargetAddr->Ipv4.sin_addr.S_un.S_addr)) {
			NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: NQN Found %s req NQN %s  and IP Address 0x%x!\n", __FUNCTION__, __LINE__,
				psController->caSubsystemNQN,
				pcNQN,
				psTargetAddr->Ipv4.sin_addr.S_un.S_addr);
			NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d: NQN Found %s req NQN %s  and IP Address 0x%x!\n", __FUNCTION__, __LINE__,
				psController->caSubsystemNQN,
				pcNQN,
				psTargetAddr->Ipv4.sin_addr.S_un.S_addr);
			break;
		}
		psController = NULL;
	}
	KeReleaseInStackQueuedSpinLock(&sLockHandle);
	return psController;
}

NTSTATUS
NVMeoFRdmaFreeController(PNVMEOF_FABRIC_CONTROLLER psController)
{
	NTSTATUS Status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE LockHandle = { 0 };

	if (!glsRdmaControllerList.ulRdmaModuleInitDone) {
		return STATUS_UNSUCCESSFUL;
	}

	if (!psController) {
		NVMeoFDebugLog(LOG_LEVEL_ERROR, "%s:%d: Invalid NULL Controller passed!\n", __FUNCTION__, __LINE__);
		NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d: Invalid NULL Controller passed!\n", __FUNCTION__, __LINE__);
		return STATUS_INVALID_PARAMETER;
	}

	KeAcquireInStackQueuedSpinLock(&glsRdmaControllerList.sRdmaControllerListSpinLock, &LockHandle);
	if (!IsListEmpty(&psController->sFabricEntry)) {
		RemoveEntryList(&psController->sFabricEntry);
	}
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	NVMeoFDebugLog(LOG_LEVEL_VERBOSE, "%s:%d:Removed Controller from list!\n", __FUNCTION__, __LINE__);
	NVMeoFRdmaFreeNpp(psController);
	return Status;
}

VOID
NVMeoFRdmaXportInitialize(VOID)
{
	if (!glsRdmaControllerList.ulRdmaModuleInitDone) {
#if 0
		glsRdmaControllerList.psRdmaControllersLAList = NVMeoFRdmaAllocateNpp(sizeof(*glsRdmaControllerList.psRdmaControllersLAList));
		if (!glsRdmaControllerList.psRdmaControllersLAList) {
			NVMeoFLogEvent(NVME_OF_ERROR, "%s:%d: Failed to allocate memory for RDMA Controller LA List!\n", __FUNCTION__, __LINE__);
			return;
		}
		ExInitializeLookasideListEx(glsRdmaControllerList.psRdmaControllersLAList,
			NULL,
			NULL,
			NonPagedPoolNx,
			0,
			sizeof(NVMEOF_CONTROLLER),
			NVMEF_RDMA_MEMORY_TAG,
			0);
#endif
		InitializeListHead(&glsRdmaControllerList.sRdmaControllerHead);
		KeInitializeSpinLock(&glsRdmaControllerList.sRdmaControllerListSpinLock);
		glsRdmaControllerList.ulRdmaModuleInitDone = 1;
	}
}