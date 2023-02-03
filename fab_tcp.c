/****************************** Module Header Starts *********************************************************************************
* Module Name:  KSockClient.c
* Project:      Pavilion Virtual Bus Driver part of NVMeOF Solution
* Writer:       Yatindra Vaishanv
* Description: WinSockKernel library to abstract the Socket, IRP and meory manangemnt from application and for following calls:
*          1. Creating Socket (SOCK_STREAM and SOCK_DGRAM), Connecting socket
*          2. Setting socket options
*          3. Sending and Reciving data Synchronously and Asynchronously
*          5. IRP and buffer management for blocked Send and Receive calls
****************************** Module Header Ends***********************************************************************************/

#include "fabric_api.h"
#include "trace.h"
#include "fab_tcp.tmh"

#pragma warning(push)
#pragma warning(disable:6001) // nameless struct/union
#pragma warning(disable:6101) // bit field types other than int

PWSK_SOCKET
NTAPI
WskCreateSocket(__in ADDRESS_FAMILY  AddressFamily,
	            __in USHORT          usSocketType,
	            __in ULONG           ulProtocol,
	            __in ULONG           ulFlags);

NTSTATUS
NTAPI
WskCloseSocket(__in PWSK_SOCKET WskSocket);

NTSTATUS
NTAPI
WskConnect(__in PWSK_SOCKET psWskSocket,
           __in PSOCKADDR   RemoteAddress);

NTSTATUS
NTAPI
WskSocketConnect(__in USHORT                          usSocketType,
                 __in ULONG                           ulProtocol,
                 __in PSOCKADDR                       psRemoteAddress,
                 __in PSOCKADDR                       psLocalAddress,
                 __in PWSK_CLIENT_CONNECTION_DISPATCH psDispacher,
                 __in PVOID                           pvCtx,
                 __out PWSK_SOCKET*                   psConnectedSocket);

NTSTATUS
NTAPI
WskSend(__in PWSK_SOCKET psWskSocket,
        __in PVOID       pvBuffer,
        __in ULONG       ulBufferSize,
        __in ULONG       ulFlags,
        __in  PULONG     pulSentBufferSize);

NTSTATUS
NTAPI
WskSendTo(__in PWSK_SOCKET   psWskSocket,
	      __in PVOID         pvBuffer,
	      __in ULONG         ulBufferSize,
	      __in_opt PSOCKADDR psRemoteAddress,
	      __inout PLONG      plBytesSent);

NTSTATUS
NTAPI
WskReceive(__in  PWSK_SOCKET WskSocket,
           __out PVOID       Buffer,
           __in  ULONG       BufferSize,
           __in  ULONG       Flags,
           __in  PULONG      pulRcvdBufferSize);

NTSTATUS
NTAPI
WskReceiveFrom(__in  PWSK_SOCKET   WskSocket,
               __out PVOID         Buffer,
               __in  ULONG         BufferSize,
               __out_opt PSOCKADDR RemoteAddress,
               __out_opt PULONG    ControlFlags,
               __out     PLONG     plBytesReceived);

NTSTATUS
NTAPI
WskBind(__in PWSK_SOCKET psWskSocket,
        __in PSOCKADDR   psLocalAddress);

PWSK_SOCKET
NTAPI
WskAccept(__in PWSK_SOCKET        WskSocket,
          __out_opt PSOCKADDR     LocalAddress,
          __out_opt PSOCKADDR     RemoteAddress);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, WskCreateSocket)
#pragma alloc_text(PAGE, WskCloseSocket)
#pragma alloc_text(PAGE, WskConnect)
#pragma alloc_text(PAGE, WskSocketConnect)
#pragma alloc_text(PAGE, WskSend)
#pragma alloc_text(PAGE, WskSendTo)
#pragma alloc_text(PAGE, WskReceive)
#pragma alloc_text(PAGE, WskReceiveFrom)
#pragma alloc_text(PAGE, WskBind)
#pragma alloc_text(PAGE, WskAccept)
#pragma alloc_text(PAGE, WskDisconnectSocket)
#endif //ALLOC_PRAGMA

static
NTSTATUS
NTAPI
WskCompletionRoutine(__in PDEVICE_OBJECT 		psDeviceObject,
	                 __in PIRP                  psIrp,
	                 __in PKEVENT               psCompletionEvent)
{
	ASSERT(psCompletionEvent);

	UNREFERENCED_PARAMETER(psIrp);
	UNREFERENCED_PARAMETER(psDeviceObject);

	KeSetEvent(psCompletionEvent, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

static
NTSTATUS
WskInitWskData(__out PIRP* psIrp,
	__out PKEVENT   CompletionEvent)
{
	ASSERT(psIrp);
	ASSERT(CompletionEvent);

	*psIrp = IoAllocateIrp(1, FALSE);
	if (!*psIrp) {
		KdPrint(("%s:%d:psIrp Aloocation Failed with status\n", __FUNCTION__, __LINE__));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	KeInitializeEvent(CompletionEvent, SynchronizationEvent, FALSE);
	IoSetCompletionRoutine(*psIrp, WskCompletionRoutine, CompletionEvent, TRUE, TRUE, TRUE);
	return STATUS_SUCCESS;
}

static
NTSTATUS
WskInitWskBuffer(__in  PVOID    Buffer,
                 __in  ULONG    BufferSize,
                 __out PWSK_BUF WskBuffer)
{
	NTSTATUS Status = STATUS_SUCCESS;

	ASSERT(Buffer);
	ASSERT(BufferSize);
	ASSERT(WskBuffer);

	WskBuffer->Offset = 0;
	WskBuffer->Length = BufferSize;

	WskBuffer->Mdl = IoAllocateMdl(Buffer, BufferSize, FALSE, FALSE, NULL);
	if (!WskBuffer->Mdl) {
		KdPrint(("%s:%d:Mdl allocation failed with status.\n", __FUNCTION__, __LINE__));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	__try {
		MmProbeAndLockPages(WskBuffer->Mdl, KernelMode, IoModifyAccess);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KdPrint(("%s:%d: MmProbeAndLockPages failed with status.\n", __FUNCTION__, __LINE__));
		IoFreeMdl(WskBuffer->Mdl);
		Status = STATUS_ACCESS_VIOLATION;
	}

	return Status;
}

static
VOID
WskFreeWskBuffer(__in PWSK_BUF WskBuffer)
{
	ASSERT(WskBuffer);

	MmUnlockPages(WskBuffer->Mdl);
	IoFreeMdl(WskBuffer->Mdl);
}


PWSK_SOCKET
NTAPI
WskCreateSocket(__in ADDRESS_FAMILY  AddressFamily,
                __in USHORT          usSocketType,
                __in ULONG           ulProtocol,
                __in ULONG           ulFlags)
{
	KEVENT                  CompletionEvent = { 0 };
	PIRP                    psIrp = NULL;
	PWSK_SOCKET             WskSocket = NULL;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized())
		return NULL;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return NULL;
	}

	Status = gsWskProvider.Dispatch->WskSocket(gsWskProvider.Client,
		AddressFamily,
		usSocketType,
		ulProtocol,
		ulFlags,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	WskSocket = NT_SUCCESS(Status) ? (PWSK_SOCKET)psIrp->IoStatus.Information : NULL;

	IoFreeIrp(psIrp);
	return (PWSK_SOCKET)WskSocket;
}


NTSTATUS
NTAPI
WskCloseSocket(__in PWSK_SOCKET WskSocket)
{
	KEVENT          CompletionEvent = { 0 };
	PIRP            psIrp = NULL;
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() || !WskSocket)
		return STATUS_INVALID_PARAMETER;

	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return (Status);
	}

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = ((PWSK_PROVIDER_BASIC_DISPATCH)WskSocket->Dispatch)->WskCloseSocket(WskSocket, psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	IoFreeIrp(psIrp);
	return Status;
}

NTSTATUS
NTAPI
WskConnect(__in PWSK_SOCKET        WskSocket,
           __in PSOCKADDR          RemoteAddress)
{
	KEVENT          CompletionEvent = { 0 };
	PIRP            psIrp = NULL;
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() || !WskSocket || !RemoteAddress)
		return STATUS_INVALID_PARAMETER;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = ((PWSK_PROVIDER_CONNECTION_DISPATCH)WskSocket->Dispatch)->WskConnect(WskSocket,
		RemoteAddress,
		0,
		psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	IoFreeIrp(psIrp);
	return Status;
}

NTSTATUS
NTAPI
WskSocketConnect(__in USHORT                          usSocketType,
                 __in ULONG                           ulProtocol,
                 __in PSOCKADDR                       psRemoteAddress,
                 __in PSOCKADDR                       psLocalAddress,
                 __in PWSK_CLIENT_CONNECTION_DISPATCH psDispacher,
                 __in PVOID                           pvCtx,
                 __out PWSK_SOCKET*                   psConnectedSocket)
{
	KEVENT                  CompletionEvent = { 0 };
	PIRP                    psIrp = NULL;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() || !psLocalAddress || !psRemoteAddress)
		return STATUS_INVALID_PARAMETER;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = 
		gsWskProvider.Dispatch->WskSocketConnect(gsWskProvider.Client,
		                                         usSocketType,
		                                         ulProtocol,
		                                         psLocalAddress,
		                                         psRemoteAddress,
		                                         0,
		                                         pvCtx,
		                                         psDispacher,
		                                         NULL,
		                                         NULL,
		                                         NULL,
		                                         psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	*psConnectedSocket = NT_SUCCESS(Status) ? (PWSK_SOCKET)psIrp->IoStatus.Information : NULL;

	IoFreeIrp(psIrp);
	return Status;
}

NTSTATUS
NTAPI
WskDisconnectSocket(__in PWSK_SOCKET psWskSocket)
{
	KEVENT          CompletionEvent = { 0 };
	PIRP            psIrp = NULL;
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() || !psWskSocket)
		return STATUS_INVALID_PARAMETER;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = 
		((PWSK_PROVIDER_CONNECTION_DISPATCH)psWskSocket->Dispatch)->WskDisconnect(psWskSocket, NULL, 0, psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
	}

	IoFreeIrp(psIrp);
	return Status;
}

NTSTATUS
NTAPI
WskDisconnectSocketAbortive(__in PWSK_SOCKET WskSocket)
{
	KEVENT          CompletionEvent = { 0 };
	PIRP            psIrp = NULL;
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() || !WskSocket)
		return STATUS_INVALID_PARAMETER;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = ((PWSK_PROVIDER_CONNECTION_DISPATCH)WskSocket->Dispatch)->WskDisconnect(WskSocket, NULL, WSK_FLAG_ABORTIVE, psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
	}

	IoFreeIrp(psIrp);
	return Status;
}

NTSTATUS
NTAPI
WskSend(__in PWSK_SOCKET psWskSocket,
        __in PVOID       pvBuffer,
        __in ULONG       ulBufferSize,
        __in ULONG       ulFlags,
        __in  PULONG     pulSentBufferSize)
{
	KEVENT                            CompletionEvent = { 0 };
	PIRP                              psIrp = NULL;
	WSK_BUF                           WskBuffer = { 0 };
	NTSTATUS                          Status = STATUS_UNSUCCESSFUL;
	PWSK_PROVIDER_CONNECTION_DISPATCH psSkDispatch = NULL;

	if (!IsWskinitialized() || !psWskSocket || !pvBuffer || !ulBufferSize)
		return STATUS_INVALID_PARAMETER;

	Status = WskInitWskBuffer(pvBuffer, ulBufferSize, &WskBuffer);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		WskFreeWskBuffer(&WskBuffer);
		return Status;
	}

	psSkDispatch = (PWSK_PROVIDER_CONNECTION_DISPATCH)psWskSocket->Dispatch;

	Status = 
		psSkDispatch->WskSend(psWskSocket,
		                      &WskBuffer,
		                      ulFlags,
		                      psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	if (pulSentBufferSize) {
		*pulSentBufferSize = NT_SUCCESS(Status) ? (LONG)psIrp->IoStatus.Information : 0;
	}

	IoFreeIrp(psIrp);
	WskFreeWskBuffer(&WskBuffer);
	return Status;
}

NTSTATUS
NTAPI
WskSendTo(__in PWSK_SOCKET        psWskSocket,
          __in PVOID              pvBuffer,
          __in ULONG              ulBufferSize,
          __in_opt PSOCKADDR      psRemoteAddress,
	      __inout PLONG           plBytesSent)
{
	KEVENT                          CompletionEvent = { 0 };
	PIRP                            psIrp = NULL;
	WSK_BUF                         WskBuffer = { 0 };
	NTSTATUS                        Status = STATUS_UNSUCCESSFUL;
	PWSK_PROVIDER_DATAGRAM_DISPATCH psSkDispatcher = NULL;

	if (!IsWskinitialized() || !psWskSocket || !pvBuffer || !ulBufferSize)
		return STATUS_INVALID_PARAMETER;

	psSkDispatcher = (PWSK_PROVIDER_DATAGRAM_DISPATCH)psWskSocket->Dispatch;

	Status = WskInitWskBuffer(pvBuffer, ulBufferSize, &WskBuffer);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		WskFreeWskBuffer(&WskBuffer);
		return Status;
	}

	Status = 
		psSkDispatcher->WskSendTo(psWskSocket,
		                          &WskBuffer,
		                          0,
		                          psRemoteAddress,
		                          0,
		                          NULL,
		                          psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	if (NT_SUCCESS(Status)) {
		*plBytesSent = (LONG)psIrp->IoStatus.Information;
	}

	IoFreeIrp(psIrp);
	WskFreeWskBuffer(&WskBuffer);
	return Status;
}

NTSTATUS
NTAPI
WskReceive(__in  PWSK_SOCKET               WskSocket,
           __out PVOID                     Buffer,
           __in  ULONG                     BufferSize,
           __in  ULONG                     Flags,
           __in  PULONG                    pulRcvdBufferSize)
{
	KEVENT                            sCompletionEvent = { 0 };
	PIRP                              psIrp = NULL;
	WSK_BUF                           sWskBuffer = { 0 };
	NTSTATUS                          Status = STATUS_UNSUCCESSFUL;
	PWSK_PROVIDER_CONNECTION_DISPATCH psSkDispatcher = NULL;

	if (!IsWskinitialized() || !WskSocket || !Buffer || !BufferSize)
		return STATUS_INVALID_PARAMETER;

	psSkDispatcher = (PWSK_PROVIDER_CONNECTION_DISPATCH)WskSocket->Dispatch;

	Status = WskInitWskBuffer(Buffer, BufferSize, &sWskBuffer);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = WskInitWskData(&psIrp, &sCompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		WskFreeWskBuffer(&sWskBuffer);
		return Status;
	}

	Status = 
		psSkDispatcher->WskReceive(WskSocket,
                                   &sWskBuffer,
                                   Flags,
                                   psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	if (pulRcvdBufferSize && NT_SUCCESS(Status)) {
		*pulRcvdBufferSize =  (ULONG)psIrp->IoStatus.Information;
	}

	IoFreeIrp(psIrp);
	WskFreeWskBuffer(&sWskBuffer);
	return Status;
}

NTSTATUS
NTAPI
WskReceiveFrom(__in  PWSK_SOCKET       WskSocket,
               __out PVOID             Buffer,
               __in  ULONG             BufferSize,
               __out_opt PSOCKADDR     RemoteAddress,
               __out_opt PULONG        ControlFlags,
               __out     PLONG         plBytesReceived)
{
	KEVENT                          CompletionEvent = { 0 };
	PIRP                            psIrp = NULL;
	WSK_BUF                         WskBuffer = { 0 };
	NTSTATUS                        Status = STATUS_UNSUCCESSFUL;
	PWSK_PROVIDER_DATAGRAM_DISPATCH psSkDispatcher = NULL;

	if (!IsWskinitialized() || !WskSocket || !Buffer || !BufferSize || !RemoteAddress)
		return STATUS_INVALID_PARAMETER;

	psSkDispatcher = (PWSK_PROVIDER_DATAGRAM_DISPATCH)WskSocket->Dispatch;

	Status = WskInitWskBuffer(Buffer, BufferSize, &WskBuffer);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		WskFreeWskBuffer(&WskBuffer);
		return Status;
	}

	Status = 
		psSkDispatcher->WskReceiveFrom(WskSocket,
                                       &WskBuffer,
                                       0,
                                       RemoteAddress,
                                       0,
                                       NULL,
                                       ControlFlags,
                                       psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	if (plBytesReceived && NT_SUCCESS(Status))
		*plBytesReceived =  (LONG)psIrp->IoStatus.Information;

	IoFreeIrp(psIrp);
	WskFreeWskBuffer(&WskBuffer);
	return Status;
}

NTSTATUS
NTAPI
WskBind(__in PWSK_SOCKET        psWskSocket,
        __in PSOCKADDR          psLocalAddress)
{
	KEVENT                            CompletionEvent = { 0 };
	PIRP                              psIrp = NULL;
	NTSTATUS                          Status = STATUS_UNSUCCESSFUL;
	PWSK_PROVIDER_CONNECTION_DISPATCH psSkDispatcher = NULL;

	if (!IsWskinitialized() || !psWskSocket || !psLocalAddress)
		return STATUS_INVALID_PARAMETER;

	psSkDispatcher = (PWSK_PROVIDER_CONNECTION_DISPATCH)psWskSocket->Dispatch;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = 
		psSkDispatcher->WskBind(psWskSocket,
		                        psLocalAddress,
		                        0,
		                        psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	IoFreeIrp(psIrp);
	return Status;
}

PWSK_SOCKET
NTAPI
WskAccept(__in PWSK_SOCKET        WskSocket,
          __out_opt PSOCKADDR     LocalAddress,
          __out_opt PSOCKADDR     RemoteAddress)
{
	KEVENT                  CompletionEvent = { 0 };
	PIRP                    psIrp = NULL;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;
	PWSK_SOCKET             AcceptedSocket = NULL;
	PWSK_PROVIDER_LISTEN_DISPATCH psListenDispatcher = NULL;

	if (!IsWskinitialized() || !WskSocket)
		return NULL;

	psListenDispatcher = (PWSK_PROVIDER_LISTEN_DISPATCH)WskSocket->Dispatch;

	Status = WskInitWskData(&psIrp, &CompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return NULL;
	}

	Status = psListenDispatcher->WskAccept(WskSocket,
		0,
		NULL,
		NULL,
		LocalAddress,
		RemoteAddress,
		psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	AcceptedSocket = NT_SUCCESS(Status) ? (PWSK_SOCKET)psIrp->IoStatus.Information : NULL;

	IoFreeIrp(psIrp);
	return AcceptedSocket;
}

#ifdef DBG
static
VOID
WskSockPrintBuffer(__in PUCHAR buff,
                   __in ULONG bufSize,
                   __in ULONG printCnt)
{
	ULONG cnt = 0;
	for (; cnt < bufSize; cnt += printCnt) {
		KdPrint(("%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x\n",
			buff[cnt], buff[cnt + 1], buff[cnt + 2], buff[cnt + 3], buff[cnt + 4], buff[cnt + 5], buff[cnt + 6], buff[cnt + 7],
			buff[cnt + 8], buff[cnt + 9], buff[cnt + 10], buff[cnt + 11], buff[cnt + 12], buff[cnt + 13], buff[cnt + 14], buff[cnt + 15]));
		if ((cnt + (printCnt << 1)) > bufSize) {
			cnt += printCnt;
			for (; cnt < bufSize; cnt++)
				KdPrint(("%.02x ", buff[cnt]));
		}
	}
	DbgPrint("\n");
}

static
VOID
WskSockPrintBufferMdl(__in PMDL  psBufferMdl,
                      __in ULONG ulBufferLength)
{
	UNREFERENCED_PARAMETER(ulBufferLength);
	PUCHAR pucBuffer = NULL;
	ULONG ulBuffLen = 0;
	PMDL  psMdl = psBufferMdl;
	while (psMdl) {
		ulBuffLen = MmGetMdlByteCount(psMdl);
		pucBuffer = MmGetSystemAddressForMdlSafe(psMdl, LowPagePriority | MdlMappingNoExecute);
		KdPrint(("%s:%d: Buffer Lenght %u\n", __FUNCTION__, __LINE__, ulBuffLen));
		if (pucBuffer) {
			WskSockPrintBuffer(pucBuffer, ulBuffLen, 16);
			psMdl = psMdl->Next;
		}
		else {
			break;
		}
	}
}
#else
#define WskSockPrintBufferMdl(psBufferMdl, ulBufferLength) 
#endif

PWSK_SOCKET
NTAPI
WskCreateAsyncSocket(__in ADDRESS_FAMILY AddressFamily,
	                 __in USHORT         SocketType,
	                 __in ULONG          Protocol,
	                 __in ULONG          Flags,
	                 __in PVOID          pvCtx,
	                 __in PVOID          pvDispatch)
{
	KEVENT                  sCompletionEvent = { 0 };
	PIRP                    psIrp = NULL;
	PWSK_SOCKET             psWskSocket = NULL;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized())
		return NULL;

	Status = WskInitWskData(&psIrp, &sCompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return NULL;
	}

	Status = 
		gsWskProvider.Dispatch->WskSocket(gsWskProvider.Client,
		                                  AddressFamily,
		                                  SocketType,
		                                  Protocol,
		                                  Flags,
		                                  pvCtx,
		                                  pvDispatch,
		                                  NULL,
		                                  NULL,
		                                  NULL,
		                                  psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	psWskSocket = NT_SUCCESS(Status) ? (PWSK_SOCKET)psIrp->IoStatus.Information : NULL;

	IoFreeIrp(psIrp);
	return (PWSK_SOCKET)psWskSocket;
}

NTSTATUS
NTAPI
WskEnableCallbacks(__in PWSK_SOCKET WskSocket,
                   __in ULONG ulMaskOfEventsToEnable)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatcher = NULL;
	WSK_EVENT_CALLBACK_CONTROL EventCallbackControl;
	NTSTATUS Status;
	if (!IsWskinitialized() || !WskSocket)
		return STATUS_INVALID_PARAMETER;

	Dispatcher = (PWSK_PROVIDER_CONNECTION_DISPATCH)(WskSocket->Dispatch);

	// Specify the WSK NPI identifier
	EventCallbackControl.NpiId = &NPI_WSK_INTERFACE_ID;

	// Set the event flags for the event callback functions that
	// are to be enabled on the socket
	EventCallbackControl.EventMask = ulMaskOfEventsToEnable;

	// Initiate the control operation on the socket
	Status = 
		Dispatcher->WskControlSocket(WskSocket,
		                             WskSetOption,
		                             SO_WSK_EVENT_CALLBACK,
		                             SOL_SOCKET,
		                             sizeof(WSK_EVENT_CALLBACK_CONTROL),
		                             &EventCallbackControl,
		                             0,
		                             NULL,
		                             NULL,
		                             NULL);// No IRP for this control operation

	// Return the status of the call to WskControlSocket()
	return Status;
}

// Only one event at a time can be disabled.
NTSTATUS
NTAPI
WskDisableCallbacks(__in PWSK_SOCKET WskSocket,
	__in ULONG ulMaskOfEventsToDisable)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatcher = NULL;
	WSK_EVENT_CALLBACK_CONTROL        EventCallbackControl = { 0 };
	NTSTATUS                          Status = STATUS_SUCCESS;

	if (!IsWskinitialized() || !WskSocket || (ulMaskOfEventsToDisable & (ulMaskOfEventsToDisable - 1)))
		return STATUS_INVALID_PARAMETER;


	Dispatcher = (PWSK_PROVIDER_CONNECTION_DISPATCH)(WskSocket->Dispatch);

	// Specify the WSK NPI identifier
	EventCallbackControl.NpiId = &NPI_WSK_INTERFACE_ID;

	// Set the event flags for the event callback functions that
	// are to be disabled on the socket
	EventCallbackControl.EventMask = ulMaskOfEventsToDisable | WSK_EVENT_DISABLE;

	// Initiate the control operation on the socket
	Status = 
		Dispatcher->WskControlSocket(WskSocket,
                                     WskSetOption,
                                     SO_WSK_EVENT_CALLBACK,
                                     SOL_SOCKET,
                                     sizeof(WSK_EVENT_CALLBACK_CONTROL),
                                     &EventCallbackControl,
                                     0,
                                     NULL,
                                     NULL,
                                     NULL);// No IRP for this control operation

	return Status;
}

NTSTATUS
NTAPI
WskSendAsync(__in PWSK_SOCKET       WskSocket,
             __in PWSK_WORK_REQUEST	psWorkReq)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH psSkDispatcher = NULL;
	NTSTATUS						  Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() ||
		!WskSocket ||
		!psWorkReq->sSkBuffer.Mdl ||
		!psWorkReq->sSkBuffer.Length ||
		!psWorkReq->psIrp)
		return STATUS_INVALID_PARAMETER;

	psSkDispatcher = (PWSK_PROVIDER_CONNECTION_DISPATCH)WskSocket->Dispatch;

	Status = psSkDispatcher->WskSend(WskSocket, &psWorkReq->sSkBuffer, psWorkReq->ulFlags, psWorkReq->psIrp);

	return Status;
}

NTSTATUS
NTAPI
WskReceiveAsync(__in PWSK_SOCKET       WskSocket,
                __in PWSK_WORK_REQUEST psWorkReq)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH psSkDispatcher = NULL;
	NTSTATUS						  Status = STATUS_UNSUCCESSFUL;

	if (!IsWskinitialized() ||
		!WskSocket ||
		!psWorkReq->sSkBuffer.Mdl ||
		!psWorkReq->sSkBuffer.Length ||
		!psWorkReq->psIrp)
		return STATUS_INVALID_PARAMETER;

	psSkDispatcher = (PWSK_PROVIDER_CONNECTION_DISPATCH)WskSocket->Dispatch;

	Status = psSkDispatcher->WskReceive(WskSocket, &psWorkReq->sSkBuffer, psWorkReq->ulFlags, psWorkReq->psIrp);
	return Status;
}

NTSTATUS
NTAPI
WskControlSocket(__in PWSK_SOCKET  psWskSocket,
	             __in ULONG        ulRequestType,
	             __in ULONG        ulControlCode,
	             __in ULONG	       ulLevel,
	             __in SIZE_T       szInputSize,
	             __in_opt PVOID	   pvInputBuffer,
	             __in SIZE_T	   szOutputSize,
	             __out_opt PVOID   pvOutputBuffer,
	             __out_opt SIZE_T* pszOutputSizeReturned)
{
	KEVENT		sCompletionEvent = { 0 };
	PIRP		psIrp = NULL;
	NTSTATUS	Status = STATUS_UNSUCCESSFUL;
	PWSK_PROVIDER_CONNECTION_DISPATCH psWskDispatch = 
		(PWSK_PROVIDER_CONNECTION_DISPATCH)psWskSocket->Dispatch;

	if (!IsWskinitialized() || !psWskSocket)
		return STATUS_INVALID_PARAMETER;

	Status = WskInitWskData(&psIrp, &sCompletionEvent);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		return Status;
	}

	Status = 
		psWskDispatch->WskControlSocket(psWskSocket,
		                                ulRequestType,        // WskSetOption, 
		                                ulControlCode,        // SIO_WSK_QUERY_RECEIVE_BACKLOG, 
		                                ulLevel,              // IPPROTO_IPV6,
		                                szInputSize,          // sizeof(optionValue),
		                                pvInputBuffer,        // NULL, 
		                                szOutputSize,         // sizeof(int), 
		                                pvOutputBuffer,       // &backlog, 
		                                pszOutputSizeReturned, // NULL,
		                                psIrp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&sCompletionEvent, Executive, KernelMode, FALSE, NULL);
		Status = psIrp->IoStatus.Status;
	}

	IoFreeIrp(psIrp);
	return Status;
}