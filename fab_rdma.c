#include "fabric_api.h"
#include "trace.h"
#include "fab_rdma.tmh"

#pragma warning(push)
#pragma warning(disable:6001) // nameless struct/union
#pragma warning(disable:6101) // bit field types other than int

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
NdkCreateFnCompletionCallback(__in PNDK_FN_ASYNC_COMPLETION_CONTEXT psCtx,
                              __in NTSTATUS Status,
                              __in PNDK_OBJECT_HEADER psNdkObject)
{
	NT_ASSERT(psCtx);

	psCtx->Status = Status;
	psCtx->psNdkObject = psNdkObject;
	KeSetEvent(&psCtx->sEvent, IO_NO_INCREMENT, FALSE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
NdkCloseFnCompletionCallback(__in PNDK_FN_ASYNC_COMPLETION_CONTEXT psCtx)
{
	NT_ASSERT(psCtx);
	KeSetEvent(&psCtx->sEvent, IO_NO_INCREMENT, FALSE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
NdkCloseFnDoNothingCompletionCallback(__in PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
NdkFnCompletionCallback(__in PNDK_FN_ASYNC_COMPLETION_CONTEXT psCtx,
                        __in NTSTATUS Status)
{
	NT_ASSERT(psCtx);

	psCtx->Status = Status;
	KeSetEvent(&psCtx->sEvent, IO_NO_INCREMENT, FALSE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
NdkDisconnectEventCallback(__in PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);
	NT_ASSERT(Context);
}


_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkOpenAdapter(__in NET_IFINDEX InterfaceIndex,
	_Outptr_ PNDK_ADAPTER* ppsAdapter)
{
	NDK_VERSION ndkVersion = { 1, 0 };

	PAGED_CODE();

	return gsNdkDispatch.WskOpenNdkAdapter(gsNkdProviderNpi.Client,
                                           ndkVersion,
                                           InterfaceIndex,
                                           ppsAdapter);
}

_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkQueryAdapterInfo(__in PNDK_ADAPTER psAdapter,
	_Out_ PNDK_ADAPTER_INFO psAdapterInfo)
{
	ULONG ulAdapterInfoSize = sizeof(NDK_ADAPTER_INFO);

	PAGED_CODE();

	RtlZeroMemory(psAdapterInfo, ulAdapterInfoSize);

	return psAdapter->Dispatch->NdkQueryAdapterInfo(psAdapter,
		psAdapterInfo,
		&ulAdapterInfoSize);
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
NdkCloseAdapter(__in _Frees_ptr_ PNDK_ADAPTER psAdapter)
{
	PAGED_CODE();

	gsNdkDispatch.WskCloseNdkAdapter(gsNkdProviderNpi.Client, psAdapter);
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateCompletionQueue(__in PNDK_CREATE_CQ_PARAMS psCQParam,
                         _Outref_result_nullonfailure_ PNDK_CQ* ppsCompletionQueue)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status = psCQParam->psAdapter->Dispatch->NdkCreateCq(psCQParam->psAdapter,
		psCQParam->ulDepth,
		psCQParam->NotificationEventHandler,
		psCQParam->Context,
		&psCQParam->Affinity,
		NdkCreateFnCompletionCallback,
		&sCtx,
		ppsCompletionQueue);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
		*ppsCompletionQueue = (PNDK_CQ)sCtx.psNdkObject;
	}

	if (NT_ERROR(Status)) {
		*ppsCompletionQueue = NULL;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkResizeCompletionQueue(__in PNDK_CQ psCompletionQueue,
__in ULONG ulDepth)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		psCompletionQueue->Dispatch->NdkResizeCq(psCompletionQueue,
			ulDepth,
			NdkFnCompletionCallback,
			&sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
	}

	return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NdkArmCompletionQueue(__in PNDK_SOCKET psClientSocket,
                      __in PNDK_CQ psNdkCq,
                      __in ULONG ulTriggerType)
{
	UNREFERENCED_PARAMETER(psClientSocket);
	NT_ASSERT((NDK_CQ_NOTIFY_ERRORS == ulTriggerType) ||
		(NDK_CQ_NOTIFY_ANY == ulTriggerType) ||
		(NDK_CQ_NOTIFY_SOLICITED == ulTriggerType));

	psNdkCq->Dispatch->NdkArmCq(psNdkCq, ulTriggerType);
}

_IRQL_requires_max_(APC_LEVEL)
VOID
NdkCloseCompletionQueue(__in _Frees_ptr_ PNDK_CQ psCompletionQueue)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status = psCompletionQueue->Dispatch->NdkCloseCq(&psCompletionQueue->Header,
		NdkCloseFnCompletionCallback,
		&sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
	}
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateProtectionDomain(__in PNDK_ADAPTER psAdapter,
                          _Outptr_result_nullonfailure_ PNDK_PD* ppsProtectionDomain)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	if (!ppsProtectionDomain) {
		return STATUS_INVALID_PARAMETER;
	}

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status = 
		psAdapter->Dispatch->NdkCreatePd(psAdapter,
		                                 NdkCreateFnCompletionCallback,
		                                 &sCtx,
		                                 ppsProtectionDomain);

	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
		*ppsProtectionDomain = (PNDK_PD)sCtx.psNdkObject;
	}

	if (NT_ERROR(Status)) {
		*ppsProtectionDomain = NULL;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
VOID
NdkCloseProtectionDomain(__in _Frees_ptr_ PNDK_PD psProtectionDomain)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status = psProtectionDomain->Dispatch->NdkClosePd(&psProtectionDomain->Header,
		                                              NdkCloseFnCompletionCallback,
		                                              &sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
	}
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateQueuePair(__in PNDK_CREATE_QP_PARAMS psSQParams,
	_Outptr_ PNDK_QP* ppsQueuePair)
{
	NTSTATUS Status;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		psSQParams->psProtectionDomain->Dispatch->NdkCreateQp(psSQParams->psProtectionDomain,
			                                                  psSQParams->psReceiveCompletionQueue,
			                                                  psSQParams->psSubmissionCompletionQueue,
			                                                  psSQParams->pvContext,
			                                                  psSQParams->ulReceiveQueueDepth,
			                                                  psSQParams->ulSubmissionQueueDepth,
			                                                  psSQParams->ulMaxReceiveSges,
			                                                  psSQParams->ulMaxSubmissionRequestSges,
			                                                  0, // we don't use inline data
			                                                  NdkCreateFnCompletionCallback,
			                                                  &sCtx,
			                                                  ppsQueuePair);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
		*ppsQueuePair = (NDK_QP*)sCtx.psNdkObject;
	}

	if (NT_ERROR(Status)) {
		*ppsQueuePair = NULL;
	}

	return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NdkFlushQueuePair(__in PNDK_QP psQueuePair)
{
	psQueuePair->Dispatch->NdkFlush(psQueuePair);
}

_IRQL_requires_max_(APC_LEVEL)
VOID
NdkCloseQueuePair(__in _Frees_ptr_ PNDK_QP psQueuePair)
{
	NTSTATUS Status;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status = psQueuePair->Dispatch->NdkCloseQp(&psQueuePair->Header,
                                               NdkCloseFnCompletionCallback,
                                               &sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
	}
	else {
		NT_ASSERT(NT_SUCCESS(Status));
	}
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateConnector(__in PNDK_ADAPTER psAdapter,
                   _Outptr_ PNDK_CONNECTOR* ppsConnector)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		psAdapter->Dispatch->NdkCreateConnector(psAdapter,
                                                NdkCreateFnCompletionCallback,
                                                &sCtx,
                                                ppsConnector);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
		*ppsConnector = (NDK_CONNECTOR*)sCtx.psNdkObject;
	}

	if (NT_ERROR(Status)) {
		*ppsConnector = NULL;
	}

	return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkGetConnectionData(__in PNDK_CONNECTOR psConnector,
                     _Out_opt_ PULONG pulInboundReadLimit,
                     _Out_opt_ PULONG pulOutboundReadLimit)
{
	ULONG ulPrivateDataLen = 0;

	return 
		psConnector->Dispatch->NdkGetConnectionData(psConnector,
		                                            pulInboundReadLimit,
		                                            pulOutboundReadLimit,
		                                            NULL,
		                                            &ulPrivateDataLen);
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkConnect(__in PNDK_SOCKET                psClientSocket,
           __in PVOID                  pvPrivateData,
           __in ULONG                  ulPrivateDataLen,
           __in PNDK_QUEUE_PAIR_BUNDLE psSQ)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	NT_ASSERT(psClientSocket->ulType == NdkSocketTypeClient);

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);
	//
	// Since the socket isn't connected yet, grab read limits from the adapter.  The socket limits
	// get set later
	//
	Status =
		psSQ->psNdkConnector->Dispatch->NdkConnect(psSQ->psNdkConnector,
			psSQ->psNdkQp,
			(PSOCKADDR)&psSQ->sLocalAddress,
			NdkGetSockAddrCbSize(&psClientSocket->sLocalAddress),
			(PSOCKADDR)&psClientSocket->sRemoteAddress,
			NdkGetSockAddrCbSize(&psClientSocket->sRemoteAddress),
			psClientSocket->sRdmaAdapter.usMaxInboundReadLimit,
			psClientSocket->sRdmaAdapter.usMaxOutboundReadLimit,
			pvPrivateData,
			ulPrivateDataLen,
			NdkFnCompletionCallback,
			&sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkDisconnect(__in PNDK_SOCKET psClientSocket,
              __in PNDK_QUEUE_PAIR_BUNDLE psSQ)
{
	UNREFERENCED_PARAMETER(psClientSocket);
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	NT_ASSERT(psClientSocket->ulType == NdkSocketTypeClient);

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	//
	// Since the socket isn't connected yet, grab read limits from the adapter.  The socket limits
	// get set later
	//
	Status =
		psSQ->psNdkConnector->Dispatch->NdkDisconnect(psSQ->psNdkConnector,
			NdkFnCompletionCallback,
			&sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCompleteConnect(__in PNDK_SOCKET         psClientSocket,
                   __in PNDK_QUEUE_PAIR_BUNDLE psSQ)
{
	UNREFERENCED_PARAMETER(psClientSocket);
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	NT_ASSERT(psClientSocket->ulType == NdkSocketTypeClient);

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status = 
		psSQ->psNdkConnector->Dispatch->NdkCompleteConnect(psSQ->psNdkConnector,
		                                                   psSQ->sQPCBs.fnQPDisconnectEventHandler,
		                                                   psSQ->sQPCBs.pvQPDisconnectEventCtx,
		                                                   NdkFnCompletionCallback,
		                                                   &sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkAccept(__in PNDK_SOCKET psServerSocket,
          __in PNDK_QUEUE_PAIR_BUNDLE psSQ)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	NT_ASSERT(psServerSocket->ulType == NdkSocketTypeServer);

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		psSQ->psNdkConnector->Dispatch->NdkAccept(psSQ->psNdkConnector,
			                                      psSQ->psNdkQp,
			                                      psServerSocket->usSockMaxInboundReadLimit,
			                                      psServerSocket->usSockMaxOutboundReadLimit,
			                                      NULL,
			                                      0,
			                                      NdkDisconnectEventCallback,
			                                      psServerSocket,
			                                      NdkFnCompletionCallback,
			                                      &sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
VOID
NdkCloseConnector(__in _Frees_ptr_ PNDK_CONNECTOR psConnector)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		psConnector->Dispatch->NdkCloseConnector(&psConnector->Header,
                                                 NdkCloseFnCompletionCallback,
                                                 &sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
	}
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NdkCloseConnectorAsyncNoCallback(__in _Frees_ptr_ PNDK_CONNECTOR psConnector)
{
	psConnector->Dispatch->NdkCloseConnector(&psConnector->Header,
		NdkCloseFnDoNothingCompletionCallback,
		NULL);
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateListener(__in NDK_ADAPTER* Adapter,
                  __in NDK_FN_CONNECT_EVENT_CALLBACK ConnectEventHandler,
                  __in_opt PVOID pvContext,
                  _Outptr_ NDK_LISTENER** Listener)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		Adapter->Dispatch->NdkCreateListener(Adapter,
			ConnectEventHandler,
			pvContext,
			NdkCreateFnCompletionCallback,
			&sCtx,
			Listener);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
		*Listener = (NDK_LISTENER*)sCtx.psNdkObject;
	}

	if (NT_ERROR(Status)) {
		*Listener = NULL;
	}

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkListen(__in NDK_LISTENER* Listener,
	      __in CONST PSOCKADDR Address,
          __in ULONG AddressCbLength)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent, NotificationEvent, FALSE);

	Status =
		Listener->Dispatch->NdkListen(Listener,
			                          Address,
			                          AddressCbLength,
			                          NdkFnCompletionCallback,
			                          &sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
		Status = sCtx.Status;
	}

	return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NdkPauseListener(__in NDK_LISTENER* Listener)
{
	Listener->Dispatch->NdkControlConnectEvents(Listener, TRUE);
}

_IRQL_requires_max_(APC_LEVEL)
VOID
NdkCloseListener(__in _Frees_ptr_ NDK_LISTENER* Listener)
{
	NTSTATUS Status = STATUS_SUCCESS;
	NDK_FN_ASYNC_COMPLETION_CONTEXT sCtx = { 0 };

	PAGED_CODE();

	KeInitializeEvent(&sCtx.sEvent,
		NotificationEvent,
		FALSE);

	Status = Listener->Dispatch->NdkCloseListener(&Listener->Header,
		NdkCloseFnCompletionCallback,
		&sCtx);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCtx.sEvent, Executive, KernelMode, FALSE, 0);
	}
}

/// <summary>
/// NDK socket level operations
/// </summary>
static PCHAR pcaNdkRdmaTech[] = {
	"NdkUndefined",
	"NdkiWarp",
	"NdkInfiniBand",
	"NdkRoCE",
	"NdkRoCEv2",
	"NdkMaxTechnology"
};

FORCEINLINE
VOID
NdkPrintAdapterInfo(PNDK_ADAPTER_INFO psAdapterInfo)
{
	DbgPrint("\n%s:%d:"
		"\n  Version:%u.%u, VendorID:%x, DeviceID:%x"
		"\n  MaxRegistrationSize %llu"
		"\n  MaxWindowSize %llu"
		"\n  FRMRPageCount %lu"
		"\n  MaxInitiatorRequestSge %lu"
		"\n  MaxReceiveRequestSge %lu"
		"\n  MaxReadRequestSge %lu"
		"\n  MaxTransferLength %lu"
		"\n  MaxInlineDataSize %lu",
		__FUNCTION__, __LINE__,
		psAdapterInfo->Version.Major,
		psAdapterInfo->Version.Minor,
		psAdapterInfo->VendorId,
		psAdapterInfo->DeviceId,
		psAdapterInfo->MaxRegistrationSize,
		psAdapterInfo->MaxWindowSize,
		psAdapterInfo->FRMRPageCount,
		psAdapterInfo->MaxInitiatorRequestSge,
		psAdapterInfo->MaxReceiveRequestSge,
		psAdapterInfo->MaxReadRequestSge,
		psAdapterInfo->MaxTransferLength,
		psAdapterInfo->MaxInlineDataSize);
	DbgPrint("\n  MaxInboundReadLimit %lu"
		"\n  MaxOutboundReadLimit %lu"
		"\n  MaxReceiveQueueDepth %lu"
		"\n  MaxInitiatorQueueDepth %lu"
		"\n  MaxSrqDepth %lu"
		"\n  MaxCqDepth %lu"
		"\n  LargeRequestThreshold %lu"
		"\n  MaxCallerData %lu"
		"\n  MaxCalleeData %lu"
		"\n  AdapterFlags %lu"
		"\n  RdmaTechnology %lu (%s)\n",
		psAdapterInfo->MaxInboundReadLimit,
		psAdapterInfo->MaxOutboundReadLimit,
		psAdapterInfo->MaxReceiveQueueDepth,
		psAdapterInfo->MaxInitiatorQueueDepth,
		psAdapterInfo->MaxSrqDepth,
		psAdapterInfo->MaxCqDepth,
		psAdapterInfo->LargeRequestThreshold,
		psAdapterInfo->MaxCallerData,
		psAdapterInfo->MaxCalleeData,
		psAdapterInfo->AdapterFlags,
		psAdapterInfo->RdmaTechnology,
		pcaNdkRdmaTech[psAdapterInfo->RdmaTechnology]);
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
NdkCloseRdmaAdapter(__in _Frees_ptr_ PMY_NDK_ADAPTER psAdapter)
{
	PAGED_CODE();

	if (psAdapter->psNdkAdapter) {
		NdkCloseAdapter(psAdapter->psNdkAdapter);
	}

	if (psAdapter->pwsInterfaceAlias) {
		NdkFreeNpp(psAdapter->pwsInterfaceAlias);
	}
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NdkOpenRdmaAdapter(__in IF_INDEX IfIndex,
                   __in PMY_NDK_ADAPTER psAdapter)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDK_ADAPTER_INFO psAdapterInfo = NULL;

	PAGED_CODE();


	RtlZeroMemory(psAdapter, sizeof(*psAdapter));

	Status = NdkOpenAdapter(IfIndex, &psAdapter->psNdkAdapter);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to open adapter with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

	psAdapterInfo = &psAdapter->sAdapterInfo;

	Status = NdkQueryAdapterInfo(psAdapter->psNdkAdapter, &psAdapter->sAdapterInfo);
	if (NT_ERROR(Status)) {
		NdkCloseRdmaAdapter(psAdapter);
		DbgPrint("%s:%d:Failed to Query adapter with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

	NdkPrintAdapterInfo(psAdapterInfo);

	if ((psAdapterInfo->AdapterFlags & NDK_ADAPTER_FLAG_RDMA_READ_SINK_NOT_REQUIRED) == 0) {
		NdkCloseRdmaAdapter(psAdapter);
		return (STATUS_NOT_SUPPORTED);
	}

	psAdapter->ulInterfaceIndex = IfIndex;

	psAdapter->bSupportsInterruptModeration = ((psAdapterInfo->AdapterFlags & NDK_ADAPTER_FLAG_CQ_INTERRUPT_MODERATION_SUPPORTED) != 0);

	psAdapter->usRqDepth = (USHORT)min3(MAXUSHORT, psAdapterInfo->MaxCqDepth, psAdapterInfo->MaxReceiveQueueDepth);
	psAdapter->usSqDepth = (USHORT)min3(MAXUSHORT, psAdapterInfo->MaxCqDepth, psAdapterInfo->MaxInitiatorQueueDepth);
	psAdapter->usMaxInboundReadLimit = (USHORT)min(MAXUSHORT, psAdapterInfo->MaxInboundReadLimit);
	psAdapter->usMaxOutboundReadLimit = (USHORT)min(MAXUSHORT, psAdapterInfo->MaxOutboundReadLimit);
	psAdapter->usMaxFrmrPageCount = (USHORT)min(MAXUSHORT, psAdapterInfo->FRMRPageCount);

	psAdapter->usMaxReceiveSges = (USHORT)min(NDK_MAX_NUM_SGE_PER_LIST, psAdapterInfo->MaxReceiveRequestSge);
	psAdapter->ulMaxReceiveCbSize = (ULONG)min4(NDK_RECEIVE_MAX_BUFFER_SIZE, (ULONG)(psAdapter->usMaxReceiveSges * NDK_MAX_SGE_SIZE),
		psAdapterInfo->MaxTransferLength, (ULONG)psAdapterInfo->MaxRegistrationSize);

	psAdapter->usMaxSqSges = (USHORT)min(NDK_MAX_NUM_SGE_PER_LIST, psAdapterInfo->MaxInitiatorRequestSge);
	psAdapter->ulMaxSqCbSize = (ULONG)min3((SIZE_T)(psAdapter->usMaxSqSges * NDK_MAX_SGE_SIZE), (SIZE_T)psAdapterInfo->MaxTransferLength, psAdapterInfo->MaxRegistrationSize);

	psAdapter->usMaxReadSges = (USHORT)min(NDK_MAX_NUM_SGE_PER_LIST, psAdapterInfo->MaxReadRequestSge);
	psAdapter->ulMaxReadCbSize = (ULONG)min3((SIZE_T)(psAdapter->usMaxReadSges * NDK_MAX_SGE_SIZE), (SIZE_T)psAdapterInfo->MaxTransferLength, psAdapterInfo->MaxRegistrationSize);

	if (psAdapterInfo->Version.Major > 1 || (psAdapterInfo->Version.Major == 1 && psAdapterInfo->Version.Minor >= 2)) {
		psAdapter->bSupportsRemoteInvalidation = TRUE;
		psAdapter->bSupportsDeferredPosting = TRUE;
	}

	//
	// Retrieve the adapter's alias
	//
	psAdapter->pwsInterfaceAlias = (PWSTR)NdkAllocateNpp(sizeof(WCHAR) * (NDIS_IF_MAX_STRING_SIZE + 1));
	if (!psAdapter->pwsInterfaceAlias) {
		NdkCloseRdmaAdapter(psAdapter);
		return (STATUS_NO_MEMORY);
	}

	Status = ConvertInterfaceIndexToLuid(IfIndex, &psAdapter->sIfLuid);
	if (NT_ERROR(Status)) {
		NdkCloseRdmaAdapter(psAdapter);
		return (Status);
	}

	Status = ConvertInterfaceLuidToAlias(&psAdapter->sIfLuid, psAdapter->pwsInterfaceAlias, (NDIS_IF_MAX_STRING_SIZE + 1));
	if (NT_ERROR(Status)) {
		NdkCloseRdmaAdapter(psAdapter);
		return (Status);
	}

	return Status;
}

_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateRdmaQPair(__in PNDK_SOCKET            psRDMASocket,
                   __in PNDK_QP_CBS            psSQCallbacks,
                   __in ULONG                  ulInQD,
                   __in ULONG                  ulOutQD,
                   __in PNDK_QUEUE_PAIR_BUNDLE psSQ)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDK_CREATE_QP_PARAMS psSQParam = NULL;
	PNDK_CREATE_CQ_PARAMS psCQParams = NdkAllocateNpp(sizeof(*psCQParams));
	if (!psCQParams) {
		return STATUS_NO_MEMORY;
	}

	psSQParam = NdkAllocateNpp(sizeof(*psSQParam));
	if (!psSQParam) {
		NdkFreeNpp(psCQParams);
		return STATUS_NO_MEMORY;
	}

	psSQ->sQPCBs = *psSQCallbacks;

	KeQueryNodeActiveAffinity(KeGetCurrentNodeNumber(),
		&psCQParams->Affinity,
		NULL);
	psCQParams->psAdapter = psRDMASocket->sRdmaAdapter.psNdkAdapter;
	psCQParams->Context = psSQ->sQPCBs.pvReceiveCQNotificationCtx;
	psCQParams->NotificationEventHandler = psSQ->sQPCBs.fnReceiveCQNotificationEventHandler;
	psCQParams->ulDepth = ulInQD;

	Status = NdkCreateCompletionQueue(psCQParams,
		&psSQ->psNdkRcq);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to Create Receive Completion Queue with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		NdkFreeNpp(psCQParams);
		NdkFreeNpp(psSQParam);
		return (Status);
	}

	psCQParams->Context = psSQ->sQPCBs.pvSendCQNotificationCtx;
	psCQParams->NotificationEventHandler = psSQ->sQPCBs.fnSendCQNotificationEventHandler;
	psCQParams->ulDepth = ulOutQD;
	//
	// Create the socket's Send Completion Queue (SCQ)
	//
	Status = NdkCreateCompletionQueue(psCQParams,
		&psSQ->psNdkScq);
	if (NT_ERROR(Status)) {
		NdkCloseCompletionQueue(psSQ->psNdkRcq);
		DbgPrint("%s:%d:Failed to Create Send Completion Queue with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		NdkFreeNpp(psCQParams);
		NdkFreeNpp(psSQParam);
		return (Status);
	}

	//
	// If the adapter supports interrupt moderation then set the moderation parameters
	//
	if (psRDMASocket->sRdmaAdapter.bSupportsInterruptModeration) {
		// Set RCQ interrupt moderation parameters
		Status =
			psSQ->psNdkRcq->Dispatch->NdkControlCqInterruptModeration(psSQ->psNdkRcq,
                                                                      CQ_INTERRUPT_MODERATION_INTERVAL,
                                                                      CQ_INTERRUPT_MODERATION_COUNT);
		if (NT_ERROR(Status)) {
			NdkCloseCompletionQueue(psSQ->psNdkRcq);
			NdkCloseCompletionQueue(psSQ->psNdkScq);
			DbgPrint("%s:%d:Failed to set Moderation for Rcq with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
			NdkFreeNpp(psCQParams);
			NdkFreeNpp(psSQParam);
			return (Status);
		}

		// Set SCQ interrupt moderation parameters
		Status =
			psSQ->psNdkScq->Dispatch->NdkControlCqInterruptModeration(psSQ->psNdkScq,
                                                                      CQ_INTERRUPT_MODERATION_INTERVAL,
                                                                      CQ_INTERRUPT_MODERATION_COUNT);
		if (NT_ERROR(Status)) {
			NdkCloseCompletionQueue(psSQ->psNdkRcq);
			NdkCloseCompletionQueue(psSQ->psNdkScq);
			DbgPrint("%s:%d:Failed to set Moderation for Scq with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
			NdkFreeNpp(psCQParams);
			NdkFreeNpp(psSQParam);
			return (Status);
		}
	}

	psSQParam->psProtectionDomain = psRDMASocket->psNdkPd;
	psSQParam->psReceiveCompletionQueue = psSQ->psNdkRcq;
	psSQParam->psSubmissionCompletionQueue = psSQ->psNdkScq;
	psSQParam->pvContext = psSQ;
	psSQParam->ulReceiveQueueDepth = ulInQD;
	psSQParam->ulSubmissionQueueDepth = ulOutQD;
	psSQParam->ulMaxReceiveSges = psRDMASocket->usSockMaxReceiveSges;
	psSQParam->ulMaxSubmissionRequestSges = psRDMASocket->usSockMaxSqSges;


	Status = NdkCreateQueuePair(psSQParam,
		&psSQ->psNdkQp);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to Create Queue Pair with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		NdkCloseCompletionQueue(psSQ->psNdkRcq);
		NdkCloseCompletionQueue(psSQ->psNdkScq);
		NdkFreeNpp(psCQParams);
		NdkFreeNpp(psSQParam);
		return (Status);
	}

	_InterlockedIncrement16((PSHORT)&psRDMASocket->usSockQPCount);

	psSQ->psParentSocket = psRDMASocket;
	psSQ->uiPrivilagedMemoryToken = psRDMASocket->uiPrivilegedMrToken;
	psSQ->ulReceiveSize = psRDMASocket->ulSockMaxReceiveCbSize;
	psSQ->ulSendSize = psRDMASocket->ulSockMaxSqCbSize;
	psSQ->usMaxSubmissionSGEs = min(NDK_NVME_RDMA_MAX_INLINE_SEGMENTS, psRDMASocket->usSockMaxSqSges);

	NdkFreeNpp(psCQParams);
	NdkFreeNpp(psSQParam);

	return (STATUS_SUCCESS);
}

_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
NdkDestroyRdmaSocket(__notnull PNDK_SOCKET psRDMASocket)
{
	if (psRDMASocket) {
		if (psRDMASocket->psNdkPd) {
			DbgPrint("%s:%d:Trying to close Protection domain !\n ", __FUNCTION__, __LINE__);
			NdkCloseProtectionDomain(psRDMASocket->psNdkPd);
			psRDMASocket->psNdkPd = NULL;
		}

		if (psRDMASocket->sRdmaAdapter.psNdkAdapter) {
			DbgPrint("%s:%d:Trying to close Adapter!\n ", __FUNCTION__, __LINE__);
			NdkCloseRdmaAdapter(&psRDMASocket->sRdmaAdapter);
			psRDMASocket->sRdmaAdapter.psNdkAdapter = NULL;
		}

		DbgPrint("%s:%d:Freeing Socket!\n ", __FUNCTION__, __LINE__);
		NdkFreeNpp(psRDMASocket);
	}
	return (STATUS_SUCCESS);
}

static
VOID
NdkPrintSocketInfo(PNDK_SOCKET psRdmaSocket)
{
	DbgPrint("%s:%d:RDMA Socket Information:"
		"\n  Interface Index %llu"
		"\n  Ref Count %d"
		"\n  PrivilegedMrToken %u"
		"\n  SockMaxReceiveCbSize %lu"
		"\n  SockMaxSqCbSize %lu"
		"\n  SockMaxReadCbSize %lu"
		"\n  SockRqDepth %hu"
		"\n  SockSqDepth %hu"
		"\n  SockMaxInboundReadLimit %hu"
		"\n  SockMaxOutboundReadLimit %hu"
		"\n  SockMaxFrmrPageCount %hu"
		"\n  SockMaxSqSges %hu"
		"\n  SockMaxReadSges %hu"
		"\n  SockMaxReceiveSges %hu"
		"\n  SockMaxNumReceives %hu"
		"\n  SockMaxNumSends %hu"
		"\n  SockQPCount %hu"
		"\n  DisconnectCallbackEnabled %hu"
		"\n  SupportsRemoteInvalidation %lu\n",
		__FUNCTION__, __LINE__,
		psRdmaSocket->ulInterfaceIndex,
		psRdmaSocket->lRefCount,
		psRdmaSocket->uiPrivilegedMrToken,
		psRdmaSocket->ulSockMaxReceiveCbSize,
		psRdmaSocket->ulSockMaxSqCbSize,
		psRdmaSocket->ulSockMaxReadCbSize,
		psRdmaSocket->usSockRqDepth,
		psRdmaSocket->usSockSqDepth,
		psRdmaSocket->usSockMaxInboundReadLimit,
		psRdmaSocket->usSockMaxOutboundReadLimit,
		psRdmaSocket->usSockMaxFrmrPageCount,
		psRdmaSocket->usSockMaxSqSges,
		psRdmaSocket->usSockMaxReadSges,
		psRdmaSocket->usSockMaxReceiveSges,
		psRdmaSocket->usSockMaxNumReceives,
		psRdmaSocket->usSockMaxNumSends,
		psRdmaSocket->usSockQPCount,
		psRdmaSocket->bDisconnectCallbackEnabled,
		psRdmaSocket->bSupportsRemoteInvalidation);
}
//
// Performs socket creation tasks that are common to both client and server sockets
//
// @param[in] AdapterIfIndex The interface index of the RDMA adapter to associate 
// with the socket.
//
// @param[out] oSocket A pointer to an RDMA_SOCKET pointer that will receive the
// address of the newly created socket.
//
// @retval STATUS_SUCCESS The socket was successfully created
//
// @retval "An NTSTATUS error code" An error occurred
//
// @irql PASSIVE_LEVEL
//
_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
NdkCreateRdmaSocket(__in IF_INDEX ulAdapterIfIndex,
	__in PNDK_COMPLETION_CBS  psRDMASocketCbs,
	_Outptr_result_maybenull_ PNDK_SOCKET* ppsRDMASocket)
{
	NTSTATUS Status;
	PNDK_SOCKET psRDMASocket = NULL;

	PAGED_CODE();

	psRDMASocket = (PNDK_SOCKET)NdkAllocateNpp(sizeof(*psRDMASocket));
	if (!psRDMASocket) {
		DbgPrint("%s:%d:Failed to allocate memory for Socket!\n ", __FUNCTION__, __LINE__);
		return (STATUS_NO_MEMORY);
	}

	RtlZeroMemory(psRDMASocket, sizeof(NDK_SOCKET));

	KeInitializeSpinLock(&psRDMASocket->sSockSpinLock);

	_No_competing_thread_begin_
		psRDMASocket->ulSockState = NdkSocketStateInitializing;

	KeInitializeEvent(&psRDMASocket->sSocketDisconnectedEvent, NotificationEvent, FALSE);

	Status = NdkOpenRdmaAdapter(ulAdapterIfIndex, &psRDMASocket->sRdmaAdapter);
	if (NT_ERROR(Status)) {
		NdkFreeNpp(psRDMASocket);
		DbgPrint("%s:%d:Failed Open RDMA Adapter with Status 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return (Status);
	}


	Status = NdkCreateProtectionDomain(psRDMASocket->sRdmaAdapter.psNdkAdapter, &psRDMASocket->psNdkPd);
	if (NT_ERROR(Status) || !psRDMASocket->psNdkPd) {
		NdkCloseAdapter(psRDMASocket->sRdmaAdapter.psNdkAdapter);
		NdkFreeNpp(psRDMASocket);
		DbgPrint("%s:%d:Failed Create Protection domain with Status 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

	psRDMASocket->psNdkPd->Dispatch->NdkGetPrivilegedMemoryRegionToken(psRDMASocket->psNdkPd, &psRDMASocket->uiPrivilegedMrToken);

	psRDMASocket->usSockRqDepth = psRDMASocket->sRdmaAdapter.usRqDepth;
	psRDMASocket->usSockSqDepth = psRDMASocket->sRdmaAdapter.usSqDepth;
	psRDMASocket->usSockMaxFrmrPageCount = psRDMASocket->sRdmaAdapter.usMaxFrmrPageCount;

	psRDMASocket->usSockMaxNumReceives = min(NDK_RECEIVE_MAX_BATCH_SIZE, psRDMASocket->usSockRqDepth);

	psRDMASocket->usSockMaxReceiveSges = psRDMASocket->sRdmaAdapter.usMaxReceiveSges;
	psRDMASocket->usSockMaxSqSges = psRDMASocket->sRdmaAdapter.usMaxSqSges;
	psRDMASocket->usSockMaxReadSges = psRDMASocket->sRdmaAdapter.usMaxReadSges;

	psRDMASocket->ulSockMaxReceiveCbSize = psRDMASocket->sRdmaAdapter.ulMaxReceiveCbSize;
	psRDMASocket->ulSockMaxSqCbSize = psRDMASocket->sRdmaAdapter.ulMaxSqCbSize;
	psRDMASocket->ulSockMaxReadCbSize = psRDMASocket->sRdmaAdapter.ulMaxReadCbSize;
	psRDMASocket->bSupportsRemoteInvalidation = psRDMASocket->sRdmaAdapter.bSupportsRemoteInvalidation;
	psRDMASocket->sNdkCompletionCbs = *psRDMASocketCbs;

	NdkPrintSocketInfo(psRDMASocket);

	*ppsRDMASocket = psRDMASocket;

	return Status;
}

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
NTAPI
NdkCreateRdmaSocket(__in    PSOCKADDR_INET      psLocalAddress,
                    __in    PSOCKADDR_INET      psRemoteAddress,
                    __in    IF_INDEX            ulAdapterIfIndex,
                    __in    ULONG               ulFlags,
                    __in    PNDK_COMPLETION_CBS psRDMASocketCbs,
                    __inout PNDK_SOCKET*        ppsRDMASocket)
{
	UNREFERENCED_PARAMETER(ulFlags);

	NTSTATUS Status = STATUS_INVALID_PARAMETER;

	PAGED_CODE();

	if (!ppsRDMASocket) {
		return Status;
	}

	Status = NdkCreateRdmaSocket(ulAdapterIfIndex, psRDMASocketCbs, ppsRDMASocket);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to create RDMA Socket with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		if (!*ppsRDMASocket) {
			return Status;
		}

		NdkDestroyRdmaSocket(*ppsRDMASocket);
		*ppsRDMASocket = NULL;
		return (Status);
	}

	(*ppsRDMASocket)->sNdkCompletionCbs = *psRDMASocketCbs;

	(*ppsRDMASocket)->ulType = NdkSocketTypeClient;

	(*ppsRDMASocket)->sLocalAddress = *psLocalAddress;

	(*ppsRDMASocket)->sRemoteAddress = *psRemoteAddress;

	//NdkPrintSocketInfo(*ppsRDMASocket);


	_No_competing_thread_begin_
	(*ppsRDMASocket)->ulSockState = NdkSocketStateInitialized;
	_No_competing_thread_end_

		return (STATUS_SUCCESS);
}

NTSTATUS
NTAPI
NdkDisconnectSyncAsyncRdmaSocketQP(__inout PNDK_SOCKET         psRDMASocket,
                                      __in PNDK_QUEUE_PAIR_BUNDLE psSQ)
{
	DbgPrint("%s:%d:Socket State %lu and RefCount %d!\n ", __FUNCTION__, __LINE__,
		psRDMASocket->ulSockState,
		psRDMASocket->lRefCount);
	if (psRDMASocket &&
		(psRDMASocket->ulSockState > NdkSocketStateConnecting)) {
		NdkFlushQueuePair(psSQ->psNdkQp);
		NdkCloseConnector(psSQ->psNdkConnector);
		NdkCloseQueuePair(psSQ->psNdkQp);
		NdkCloseCompletionQueue(psSQ->psNdkRcq);
		NdkCloseCompletionQueue(psSQ->psNdkScq);
		if (!psRDMASocket->lRefCount) {
			psRDMASocket->ulSockState = NdkSocketStateDisconnected;
		}
		else if (psRDMASocket->lRefCount > 0) {
			_InterlockedDecrement(&psRDMASocket->lRefCount);
		}
	}
	return (STATUS_SUCCESS);
}


NTSTATUS
NTAPI
NdkDisconnecAndCloseAsyncRdmaSocket(__inout PNDK_SOCKET psRDMASocket)
{
	NdkDestroyRdmaSocket(psRDMASocket);

	return (STATUS_SUCCESS);
}

NTSTATUS
NTAPI
NdkConnectRdmaQPair(__in PNDK_SOCKET            psRDMASocket,
                    __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
                    __in ULONG                  ulFlags,
                    __in PVOID                  pvPrivateData,
                    __in ULONG                  ulPrivateDataLen,
                    __in BOOLEAN                bSync)
{
	UNREFERENCED_PARAMETER(ulFlags);
	UNREFERENCED_PARAMETER(bSync);


	NTSTATUS Status = STATUS_SUCCESS;
	ULONG ulLocalAddressSize = 0, ulInboundReadLimit = 0, ulOutboundReadLimit = 0;

	DbgPrint("%s:%d:In Socket %p QP %p flags 0x%x ptivate data and len %p %u!\n ", __FUNCTION__, __LINE__,
             psRDMASocket,
             psSQ,
             ulFlags,
             pvPrivateData,
             ulPrivateDataLen);

	psSQ->sLocalAddress = psRDMASocket->sLocalAddress;

	Status = NdkCreateConnector(psRDMASocket->sRdmaAdapter.psNdkAdapter, &psSQ->psNdkConnector);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to create connector with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return (Status);
	}

	//
	// Call NdkConnect to request an NDK-level connection to the remote address
	//
	Status = NdkConnect(psRDMASocket, pvPrivateData, ulPrivateDataLen, psSQ);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to Connect with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		NdkCloseConnector(psSQ->psNdkConnector);
		return (Status);
	}

	//
	// Complete the connection
	//
	Status = NdkCompleteConnect(psRDMASocket, psSQ);
	if (NT_ERROR(Status)) {
		DbgPrint("%s:%d:Failed to complete connect with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return Status;
	}

	//
	// Get read limits on the connection
	//
	Status = NdkGetConnectionData(psSQ->psNdkConnector,
		&ulInboundReadLimit,
		&ulOutboundReadLimit);
	if (NT_ERROR(Status)) {
		NdkDisconnect(psRDMASocket, psSQ);
		DbgPrint("%s:%d:Failed to Get connection data with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return Status;
	}

	psRDMASocket->usSockMaxInboundReadLimit = (USHORT)min(MAXUSHORT, ulInboundReadLimit);
	psRDMASocket->usSockMaxOutboundReadLimit = (USHORT)min(MAXUSHORT, ulOutboundReadLimit);

	//
	// The original request to connect might have specified a local port of 0 meaning that
	// the NDK provider gets to chose any unused local port for the connection. Query the 
	// local address now that the NDK-level connection is established to determine what 
	// local port was chosen. 
	//
	ulLocalAddressSize = NdkGetSockAddrCbSize(&psRDMASocket->sLocalAddress);
	Status = psSQ->psNdkConnector->Dispatch->NdkGetLocalAddress(psSQ->psNdkConnector,
		(PSOCKADDR)&psRDMASocket->sLocalConnectedAddress,
		&ulLocalAddressSize);
	if (NT_ERROR(Status)) {
		NdkDisconnect(psRDMASocket, psSQ);
		DbgPrint("%s:%d:Failed to Get local connected address with Status = 0x%x!\n ", __FUNCTION__, __LINE__, Status);
		return Status;
	}

	//
	// Arm completion queues
	//
	NdkArmCompletionQueue(psRDMASocket, psSQ->psNdkRcq, NDK_CQ_NOTIFY_ANY);
	NdkArmCompletionQueue(psRDMASocket, psSQ->psNdkScq, NDK_CQ_NOTIFY_ANY);

	//
	// Set socket state to connected
	//
	_InterlockedExchange((LONG*)&psRDMASocket->ulSockState, NdkSocketStateConnected);
	psRDMASocket->bDisconnectCallbackEnabled = TRUE;
	//NdkPrintSocketInfo(psRDMASocket);
	_InterlockedIncrement(&psRDMASocket->lRefCount);


	return Status;
}

static NDK_OPERATION saOperationTypeString[] = {
	{NdkOperationTypeReceive, "NdkOperationTypeReceive"},
	{NdkOperationTypeReceiveAndInvalidate, "NdkOperationTypeReceiveAndInvalidate"},
	{NdkOperationTypeSend, "NdkOperationTypeSend"},
	{NdkOperationTypeFastRegister, "NdkOperationTypeFastRegister"},
	{NdkOperationTypeBind, "NdkOperationTypeBind"},
	{NdkOperationTypeInvalidate, "NdkOperationTypeInvalidate"},
	{NdkOperationTypeRead, "NdkOperationTypeRead"},
	{NdkOperationTypeWrite, "NdkOperationTypeWrite"},
};

PCHAR
NTAPI
NdkGetOperationType(NDK_OPERATION_TYPE ulOpType)
{
	ULONG ulCount = 0;
	while (ulCount < (sizeof(saOperationTypeString) / sizeof(NDK_OPERATION))) {
		if (saOperationTypeString[ulCount].ulOpType == ulOpType) {
			return saOperationTypeString[ulCount].pcOpTypeString;
		}
		ulCount++;
	}
	return NULL;
}

/// <summary>
/// NDK submission request type oprations
/// </summary>
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateFastRegisterMemoryRegion(__in PNDK_SOCKET  psSocket,
                                  __in PNDK_WORK_REQUEST psWkReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	CONST NDK_PD_DISPATCH* psPdDispatch = psSocket->psNdkPd->Dispatch;
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_PD_CREATE_MR);
	Status = psPdDispatch->NdkCreateMr(psSocket->psNdkPd,
                                       TRUE,
                                       psWkReq->sCreateFRMRReq.fnCreateMrCompletionCb,
                                       psWkReq->sCreateFRMRReq.pvCreateMrCompletionCtx,
                                       &psWkReq->sCreateFRMRReq.psNdkMr);
	return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkInitializeFastRegisterMemoryRegion(__in PNDK_SOCKET  psSocket,
                                      __in PNDK_WORK_REQUEST psWkReq)
{
	UNREFERENCED_PARAMETER(psSocket);
	NTSTATUS Status = STATUS_SUCCESS;
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_PD_INITIALIZE_MR);
	CONST NDK_MR_DISPATCH *psMrDispatch = psWkReq->sCloseFRMRReq.psNdkMr->Dispatch;
	Status =
		psMrDispatch->NdkInitializeFastRegisterMr(psWkReq->sInitFRMRReq.psNdkMr,
                                                  psWkReq->sInitFRMRReq.ulInitAdapterPageCount,
                                                  psWkReq->sInitFRMRReq.bInitAllowRemoteAccess,
                                                  psWkReq->fnCallBack,
                                                  psWkReq->pvCallbackCtx);
	return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
UINT32
NdkGetMemoryRegionRemoteToken(__in PNDK_SOCKET  psSocket,
                              __in PNDK_WORK_REQUEST psWkReq)
{
	UNREFERENCED_PARAMETER(psSocket);
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_PD_GET_REMOTE_TOKEN_FROM_MR);
	CONST NDK_MR_DISPATCH* psMrDispatch = psWkReq->sCloseFRMRReq.psNdkMr->Dispatch;
	return psMrDispatch->NdkGetRemoteTokenFromMr(psWkReq->sGetMRTokenReq.psNdkMr);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
UINT32
NdkGetMemoryRegionLocalToken(__in PNDK_SOCKET  psSocket,
                             __in PNDK_WORK_REQUEST psWkReq)
{
	UNREFERENCED_PARAMETER(psSocket);
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_PD_GET_LOCAL_TOKEN_FROM_MR);
	CONST NDK_MR_DISPATCH* psMrDispatch = psWkReq->sCloseFRMRReq.psNdkMr->Dispatch;
	return
		psMrDispatch->NdkGetLocalTokenFromMr(psWkReq->sGetMRTokenReq.psNdkMr);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
NdkCloseMemoryRegionAsync(__in PNDK_SOCKET  psSocket,
                          __in _Frees_ptr_ PNDK_WORK_REQUEST psWkReq)
{
	UNREFERENCED_PARAMETER(psSocket);
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_PD_CLOSE_MR);
	CONST NDK_MR_DISPATCH* psMrDispatch = psWkReq->sCloseFRMRReq.psNdkMr->Dispatch;
	return 
		psMrDispatch->NdkCloseMr(&psWkReq->sCloseFRMRReq.psNdkMr->Header,
		                         psWkReq->sCloseFRMRReq.fnCloseCompletion ?
		                         psWkReq->sCloseFRMRReq.fnCloseCompletion :
		                         NdkCloseFnDoNothingCompletionCallback,
		                         psWkReq->sCloseFRMRReq.pvContext ?
		                         psWkReq->sCloseFRMRReq.pvContext :
		                         NULL);
}

FORCEINLINE
VOID
NdkPrintSGL(__in USHORT usSgeCount, __in PNDK_SGE psSgl)
{
	while (usSgeCount) {
		KdPrint(("%s:%d: SGE length 0x%x Token %x and Addr 0x%llx!\n",
			__FUNCTION__,
			__LINE__,
			psSgl->Length,
			psSgl->MemoryRegionToken,
			psSgl->LogicalAddress.QuadPart));
		psSgl++;
		usSgeCount--;
	}
}


_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkReceive(__in PNDK_SOCKET            psSocket,
           __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
           __in PNDK_WORK_REQUEST      psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_RECEIVE);
	CONST NDK_QP_DISPATCH* psSQDispatch = psSQ->psNdkQp->Dispatch;
	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_CONNECTION);
	}

#if 0
	KdPrint(("%s:%d: Context for the receive requests 0x%p SGE count %hx and SGL:\n",
		__FUNCTION__,
		__LINE__,
		psWkReq->pvCallbackCtx,
		psWkReq->usSgeCount));
	NdkPrintSGL(psWkReq->usSgeCount, psWkReq->psNdkSgl);
#endif
	return
		psSQDispatch->NdkReceive(psSQ->psNdkQp,
                                 psWkReq,
                                 psWkReq->sReceiveReq.psNdkSgl,
                                 psWkReq->sReceiveReq.usSgeCount);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkSend(__in PNDK_SOCKET         psSocket,
        __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
        __in PNDK_WORK_REQUEST        psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_SEND);
	CONST NDK_QP_DISPATCH *psSQDispatch = psSQ->psNdkQp->Dispatch;

	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_CONNECTION);
	}
#if 0
	KdPrint(("%s:%d: SGE Count 0x%u Flags %x!\n",
		__FUNCTION__,
		__LINE__,
		psWkReq->usSgeCount,
		psWkReq->ulFlags));
	NdkPrintSGL(psWkReq->usSgeCount, psWkReq->psNdkSgl);
#endif
	NT_ASSERT((psWkReq->ulFlags == 0));
	return
		psSQDispatch->NdkSend(psSQ->psNdkQp,
                              psWkReq,
                              psWkReq->sSendReq.psNdkSgl,
                              psWkReq->sSendReq.usSgeCount,
                              psWkReq->ulFlags);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkSendAndInvalidate(__in PNDK_SOCKET            psSocket,
                     __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
                     __in PNDK_WORK_REQUEST      psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_SEND_INVALIDATE);
	CONST NDK_QP_DISPATCH* psSQDispatch = psSQ->psNdkQp->Dispatch;
	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_CONNECTION);
	}

	return
		psSQDispatch->NdkSendAndInvalidate(psSQ->psNdkQp,
                                           psWkReq,
                                           psWkReq->sSendReq.psNdkSgl,
                                           psWkReq->sSendReq.usSgeCount,
                                           psWkReq->ulFlags,
                                           psWkReq->sSendReq.uiRemoteMemoryRegionToken);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkRead(__in PNDK_SOCKET         psSocket,
        __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
        __in PNDK_WORK_REQUEST        psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_READ);
	CONST NDK_QP_DISPATCH* psSQDispatch = psSQ->psNdkQp->Dispatch;

	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_CONNECTION);
	}

	return
		psSQDispatch->NdkRead(psSQ->psNdkQp,
                              psWkReq,
                              psWkReq->sRWReq.psNdkSgl,
                              psWkReq->sRWReq.usSgeCount,
                              psWkReq->sRWReq.uiiRemoteMemoryAddress,
                              psWkReq->sRWReq.uiRemoteToken,
                              psWkReq->ulFlags);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkWrite(__in PNDK_SOCKET            psSocket,
         __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
         __in PNDK_WORK_REQUEST      psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_WRITE);
	CONST NDK_QP_DISPATCH* psSQDispatch = psSQ->psNdkQp->Dispatch;
	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_CONNECTION);
	}

	return
		psSQDispatch->NdkWrite(psSQ->psNdkQp,
                               psWkReq,
                               psWkReq->sRWReq.psNdkSgl,
                               psWkReq->sRWReq.usSgeCount,
                               psWkReq->sRWReq.uiiRemoteMemoryAddress,
                               psWkReq->sRWReq.uiRemoteToken,
                               psWkReq->ulFlags);
}

FORCEINLINE
VOID
NdkPrintLAMList(__in ULONG ulAdapterPageCount, __in PNDK_LOGICAL_ADDRESS psLA)
{
	while (ulAdapterPageCount) {
		KdPrint(("%s:%d: LA 0x%llx!\n",
			__FUNCTION__,
			__LINE__,
			psLA->QuadPart));
		ulAdapterPageCount--;
		psLA++;
	}
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkFastRegister(__in PNDK_SOCKET            psSocket,
                __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
                __in PNDK_WORK_REQUEST      psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_FAST_REGISTER);
	CONST NDK_QP_DISPATCH* psSQDispatch = psSQ->psNdkQp->Dispatch;
	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_OWNER);
	}

#if 0
	KdPrint(("%s:%d: Workreq 0x%p Page count %u Length %llu Addr %p Flags %x Adapter Pages:!\n",
		__FUNCTION__,
		__LINE__,
		psWkReq,
		psWkReq->ulAdapterPageCount,
		psWkReq->szLength,
		psWkReq->pvBaseVirtualAddress,
		psWkReq->ulFlags));
	//NdkPrintLAMList(psWkReq->ulAdapterPageCount, psWkReq->psAdapterPageArray);
#endif

	return
		psSQDispatch->NdkFastRegister(psSQ->psNdkQp,
			                          psWkReq,
			                          psWkReq->sFRMRReq.psNdkMr,
			                          psWkReq->sFRMRReq.ulAdapterPageCount,
			                          psWkReq->sFRMRReq.psAdapterPageArray,
			                          psWkReq->sFRMRReq.ulFBO,
			                          psWkReq->sFRMRReq.szLength,
			                          psWkReq->sFRMRReq.pvBaseVirtualAddress,
			                          psWkReq->ulFlags);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
NdkGetRcqResults(__inout PNDK_SOCKET         psSocket,
                 __in PNDK_QUEUE_PAIR_BUNDLE psSQs,
                 __in PNDK_WORK_REQUEST      psWkReq)
{
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_GET_RECEIVE_RESULTS);
	CONST NDK_CQ_DISPATCH* psCqDispatch = psSQs->psNdkRcq->Dispatch;
	if (psSocket->sRdmaAdapter.bSupportsRemoteInvalidation) {
		psWkReq->sGetRcqResultReq.usReceiveResultEx = TRUE;
		return
			psCqDispatch->NdkGetCqResultsEx(psSQs->psNdkRcq,
				                            psWkReq->sGetRcqResultReq.pvNdkRcqResultExx,
				                            psWkReq->sGetRcqResultReq.usMaxNumReceiveResults);
	} else {
		psWkReq->sGetRcqResultReq.usReceiveResultEx = FALSE;
		return
			psCqDispatch->NdkGetCqResults(psSQs->psNdkRcq,
				                          psWkReq->sGetRcqResultReq.pvNdkRcqResultExx,
				                          psWkReq->sGetRcqResultReq.usMaxNumReceiveResults);
	}
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
NdkGetScqResults(__inout PNDK_SOCKET         psSocket,
                 __in PNDK_QUEUE_PAIR_BUNDLE psSQs,
                 __in PNDK_WORK_REQUEST      psWkReq)
{
	UNREFERENCED_PARAMETER(psSocket);

	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_GET_SEND_RESULTS);
	CONST NDK_CQ_DISPATCH* psCqDispatch = psSQs->psNdkScq->Dispatch;
	return
		psCqDispatch->NdkGetCqResults(psSQs->psNdkScq,
			                          psWkReq->sGetScqResultReq.psNdkScqResult,
			                          psWkReq->sGetScqResultReq.usMaxNumSendResults);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkInvalidate(__in PNDK_SOCKET            psSocket,
              __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
              __in PNDK_WORK_REQUEST      psWkReq)
{
	CONST NDK_QP_DISPATCH* psSQDispatch = psSQ->psNdkQp->Dispatch;
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_SQ_INVALIDATE);
	if (psSQ->psParentSocket != psSocket) {
		return (STATUS_INVALID_CONNECTION);
	}

	return
		psSQDispatch->NdkInvalidate(psSQ->psNdkQp,
                                    psWkReq,
                                    psWkReq->sInvalidateReq.psNdkMrOrMw,
                                    psWkReq->ulFlags);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkBuildLam(__in    PNDK_SOCKET  psSocket,
            __inout PNDK_WORK_REQUEST psWkReq)
{
	CONST NDK_ADAPTER_DISPATCH* psAdpDispatch = psSocket->sRdmaAdapter.psNdkAdapter->Dispatch;
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_ADAPTER_BUILD_LAM);

	return
		psAdpDispatch->NdkBuildLAM(psSocket->sRdmaAdapter.psNdkAdapter,
                                   psWkReq->sBuildLamReq.psMdl,
                                   psWkReq->sBuildLamReq.szMdlBytesToMap,
                                   psWkReq->fnCallBack,
                                   psWkReq->pvCallbackCtx,
                                   psWkReq->sBuildLamReq.psLam,
                                   &psWkReq->sBuildLamReq.ulLamCbSize,
                                   &psWkReq->sBuildLamReq.ulFboLAM);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NdkReleaseLam(__in    PNDK_SOCKET  psSocket,
              __inout PNDK_WORK_REQUEST psWkReq)
{
	CONST NDK_ADAPTER_DISPATCH* psAdpDispatch = psSocket->sRdmaAdapter.psNdkAdapter->Dispatch;
	NT_ASSERT(psWkReq->ulType == NDK_WREQ_ADAPTER_RELEASE_LAM);
	psAdpDispatch->NdkReleaseLAM(psSocket->sRdmaAdapter.psNdkAdapter,
		                         psWkReq->sReleaseLamReq.psLam);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
NdkSubmitRequest(__in    PNDK_SOCKET            psSocket,
	             __in    PNDK_QUEUE_PAIR_BUNDLE psSQair,
                 __inout PNDK_WORK_REQUEST      psWkReq)
{
	NTSTATUS Status = STATUS_SUCCESS;
	switch (psWkReq->ulType) {
	case NDK_WREQ_PD_CREATE_MR:
		Status = NdkCreateFastRegisterMemoryRegion(psSocket, psWkReq);
		break;
	case NDK_WREQ_PD_INITIALIZE_MR:
		Status = NdkInitializeFastRegisterMemoryRegion(psSocket, psWkReq);
		break;
	case NDK_WREQ_PD_GET_REMOTE_TOKEN_FROM_MR:
		Status = NdkGetMemoryRegionRemoteToken(psSocket, psWkReq);
		break;
	case NDK_WREQ_PD_GET_LOCAL_TOKEN_FROM_MR:
		Status = NdkGetMemoryRegionLocalToken(psSocket, psWkReq);
		break;
	case NDK_WREQ_PD_CLOSE_MR:
		Status = NdkCloseMemoryRegionAsync(psSocket, psWkReq);
		break;
	case NDK_WREQ_SQ_RECEIVE:
		Status = NdkReceive(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_SEND:
		Status = NdkSend(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_SEND_INVALIDATE:
		Status = NdkSendAndInvalidate(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_READ:
		Status = NdkRead(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_WRITE:
		Status = NdkWrite(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_FAST_REGISTER:
		Status = NdkFastRegister(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_GET_RECEIVE_RESULTS:
		Status = NdkGetRcqResults(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_GET_SEND_RESULTS:
		Status = NdkGetScqResults(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_SQ_INVALIDATE:
		Status = NdkInvalidate(psSocket, psSQair, psWkReq);
		break;
	case NDK_WREQ_ADAPTER_BUILD_LAM:
		Status = NdkBuildLam(psSocket, psWkReq);
		break;
	case NDK_WREQ_ADAPTER_RELEASE_LAM:
		NdkReleaseLam(psSocket, psWkReq);
		break;
	default:
		Status = STATUS_INVALID_PARAMETER_MIX;
		break;
	}
	return Status;
}