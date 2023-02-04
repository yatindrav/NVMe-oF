/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once

#pragma warning(push)
#pragma warning(disable: 4201) /// NAMELESS_STRUCT_UNION
#pragma warning(disable: 4324) /// STRUCTURE_PADDED
#pragma warning(disable: 4100) /// UNREFERENCED FORMAL PARAMETER
#pragma warning(disable: 4214) /// BIT FIELD TYPE OTHER THAN INT
//#pragma warning(disable: 2220)

//#include <ntddk.h>                    /// Inc
//#include <wdm.h>                      /// Inc\WDF\KMDF\1.9
//#include <ndis.h>
#include <wsk.h>
#include <ndkpi.h>
#include <wskndk.h>
//#include <wdmsec.h>
//#include <netioapi.h>

#pragma warning(pop)

enum
{
	DEINITIALIZED,
	DEINITIALIZING,
	INITIALIZING,
	INITIALIZED
};

#define NVMEOF_NDK_TAG_GENERAL 'tkdN'
#define NVMEOF_WSK_TAG_GENERAL 'tksW'

#define CQ_INTERRUPT_MODERATION_INTERVAL        20  // us
#define CQ_INTERRUPT_MODERATION_COUNT           MAXULONG
#define NDK_MAX_NUM_SGE_PER_LIST                128
#define NDK_RECEIVE_MAX_BUFFER_SIZE             (64 * 1024)
#define NDK_MAX_SGE_SIZE                        (2 * 1024 * 1024)
#define NDK_RECEIVE_MAX_BATCH_SIZE              32
#define NDK_RECEIVE_MAX_BUFFER_SIZE             (64 * 1024)     // 64K
#define NDK_NVME_RDMA_MAX_INLINE_SEGMENTS       4

#define min3(a, b, c) min(min((a), (b)), (c))
#define min4(a, b, c, d) min(min((a), (b)), min((c), (d)))

extern WSK_PROVIDER_NPI          gsWskProvider;
extern WSK_PROVIDER_NPI          gsNkdProviderNpi;
extern WSK_PROVIDER_NDK_DISPATCH gsNdkDispatch;

extern BOOLEAN NTAPI IsWskinitialized(VOID);
extern BOOLEAN NTAPI IsNdkinitialized(VOID);
_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NVMeoFNdkStartup(VOID);

_IRQL_requires_(PASSIVE_LEVEL)
VOID
NVMeoFNdkCleanup(VOID);

_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NTAPI
NVMeoFWskStartup(VOID);

_IRQL_requires_(PASSIVE_LEVEL)
VOID
NTAPI
NVMeoFWskCleanup(VOID);


typedef NDK_RESULT* PNDK_RESULT;
typedef NDK_RESULT_EX* PNDK_RESULT_EX;
typedef NDK_CQ* PNDK_CQ;
typedef NDK_QP* PNDK_QP;
typedef NDK_PD* PNDK_PD;
typedef NDK_CONNECTOR* PNDK_CONNECTOR;
typedef NDK_OBJECT_HEADER* PNDK_OBJECT_HEADER;
typedef NDK_ADAPTER* PNDK_ADAPTER;
typedef NDK_ADAPTER_INFO* PNDK_ADAPTER_INFO;
typedef NDK_LISTENER* PNDK_LISTENER;
typedef NDK_LOGICAL_ADDRESS_MAPPING* PNDK_LOGICAL_ADDRESS_MAPPING;
typedef NDK_SGE* PNDK_SGE;
typedef NDK_MR* PNDK_MR;
typedef NDK_LOGICAL_ADDRESS* PNDK_LOGICAL_ADDRESS;
typedef NDK_CQ_DISPATCH* PNDK_CQ_DISPATCH;
typedef NDK_QP_DISPATCH* PNDK_QP_DISPATCH;
typedef NDK_MR_DISPATCH* PNDK_MR_DISPATCH;
typedef NDK_PD_DISPATCH* PNDK_PD_DISPATCH;
typedef NDK_ADAPTER_DISPATCH* PNDK_ADAPTER_DISPATCH;


typedef struct _NDK_FN_ASYNC_COMPLETION_CONTEXT {
	KEVENT             sEvent;
	NTSTATUS           Status; 
	PNDK_OBJECT_HEADER psNdkObject;
} NDK_FN_ASYNC_COMPLETION_CONTEXT, *PNDK_FN_ASYNC_COMPLETION_CONTEXT;

typedef struct _NDK_CREATE_CQ_PARAMS {
	PNDK_ADAPTER                    psAdapter;
	ULONG                           ulDepth;
	NDK_FN_CQ_NOTIFICATION_CALLBACK NotificationEventHandler;
	PVOID                           Context;
	GROUP_AFFINITY                  Affinity;
} NDK_CREATE_CQ_PARAMS, * PNDK_CREATE_CQ_PARAMS;

typedef struct _NDK_CREATE_QP_PARAMS {
	PNDK_PD psProtectionDomain;
	PNDK_CQ psReceiveCompletionQueue;
	PNDK_CQ psSubmissionCompletionQueue;
	PVOID   pvContext;
	ULONG   ulReceiveQueueDepth;
	ULONG   ulSubmissionQueueDepth;
	ULONG   ulMaxReceiveSges;
	ULONG   ulMaxSubmissionRequestSges;
} NDK_CREATE_QP_PARAMS, * PNDK_CREATE_QP_PARAMS;

typedef struct _NDK_COMPLETION_CBS {
	NDK_FN_CREATE_COMPLETION  fnCreateCompletionCb;
	PVOID                     pvCreateCompletionCtx;
	NDK_FN_CLOSE_COMPLETION   fnCloseCompletionCb;
	PVOID                     pvCloseCompletionCtx;
	NDK_FN_REQUEST_COMPLETION fnRequestCompletionCb;
	PVOID                     pvRequestCompletionCtx;
} NDK_COMPLETION_CBS, * PNDK_COMPLETION_CBS;

typedef struct _NDK_QP_CBS {
	NDK_FN_SRQ_NOTIFICATION_CALLBACK fnSrqNotificationEventHandler;
	PVOID                            pvSRQNotificationCtx;
	NDK_FN_CQ_NOTIFICATION_CALLBACK  fnReceiveCQNotificationEventHandler;
	PVOID                            pvReceiveCQNotificationCtx;
	NDK_FN_CQ_NOTIFICATION_CALLBACK  fnSendCQNotificationEventHandler;
	PVOID                            pvSendCQNotificationCtx;
	NDK_FN_DISCONNECT_EVENT_CALLBACK fnQPDisconnectEventHandler;
	PVOID                            pvQPDisconnectEventCtx;
	NDK_FN_CONNECT_EVENT_CALLBACK    fnConnectEventHandler;
	PVOID                            pvConnectEventCtx;
	NDK_FN_REQUEST_COMPLETION        fnQPReqCompletion;
	PVOID                            pvQPReqCompletionCtx;
} NDK_QP_CBS, * PNDK_QP_CBS;

typedef struct DECLSPEC_CACHEALIGN _NDK_QUEUE_PAIR_BUNDLE {
	struct _NDK_SOCKET* psParentSocket;
	PNDK_CQ             psNdkRcq;
	PNDK_CQ             psNdkScq;
	PNDK_QP             psNdkQp;
	PNDK_CONNECTOR      psNdkConnector;
	UINT32              uiPrivilagedMemoryToken;
	ULONG               ulReceiveSize;
	ULONG               ulSendSize;
	ULONG               ulReserved;
	USHORT              usIOQRdDepth;
	USHORT              usIOQSqDepth;
	USHORT              usMaxSubmissionSGEs;
	USHORT              usReserved;
	SOCKADDR_INET       sLocalAddress;
	NDK_QP_CBS          sQPCBs;
} NDK_QUEUE_PAIR_BUNDLE, * PNDK_QUEUE_PAIR_BUNDLE;

typedef struct _MY_NDK_ADAPTER {
	NDK_ADAPTER_INFO sAdapterInfo;
	PNDK_ADAPTER     psNdkAdapter;
	PWSTR            pwsInterfaceAlias;
	NET_LUID         sIfLuid;
	IF_INDEX         ulInterfaceIndex;
	ULONG            ulMaxReceiveCbSize;
	ULONG            ulMaxSqCbSize;
	ULONG            ulMaxReadCbSize;
	USHORT           usRqDepth;
	USHORT           usSqDepth;
	USHORT           usMaxInboundReadLimit;
	USHORT           usMaxOutboundReadLimit;
	USHORT           usMaxFrmrPageCount;
	USHORT           usMaxSqSges;
	USHORT           usMaxReadSges;
	USHORT           usMaxReceiveSges;
	BOOLEAN          bSupportsRemoteInvalidation : 1;
	BOOLEAN          bSupportsDeferredPosting : 1;
	BOOLEAN          bSupportsInterruptModeration : 1;
	USHORT           usPadding;
} MY_NDK_ADAPTER, * PMY_NDK_ADAPTER;

typedef union _NVME_NDK_RESULT {
	PNDK_RESULT    psNdkRcqResult;
	PNDK_RESULT_EX psNdkRcqResultEx;
}NVME_NDK_RESULT, *PNVME_NDK_RESULT;

typedef enum _NDK_SOCKET_TYPE {
	NdkSocketTypeClient = 0,
	NdkSocketTypeServer
} NDK_SOCKET_TYPE, *PNDK_SOCKET_TYPE;

typedef enum _NDK_SOCKET_STATE {
	NdkSocketStateInitializing,
	NdkSocketStateInitialized,
	NdkSocketStateConnecting,
	NdkSocketStateConnected,
	NdkSocketStateDisconnecting,
	NdkSocketStateDisconnected,
	NdkSocketStateClosing
} NDK_SOCKET_STATE, * PNDK_SOCKET_STATE;

typedef struct _NDK_OPERATION {
	NDK_OPERATION_TYPE ulOpType;
	PCHAR              pcOpTypeString;
}NDK_OPERATION, * PNDK_OPERATION;

typedef struct _NDK_SOCKET {
	MY_NDK_ADAPTER            sRdmaAdapter;
	PNDK_PD                   psNdkPd;
	SOCKADDR_INET             sLocalAddress;
	SOCKADDR_INET             sRemoteAddress;
	SOCKADDR_INET             sLocalConnectedAddress;
	NDK_COMPLETION_CBS        sNdkCompletionCbs;
	NVME_NDK_RESULT           uNdkResult;
	KSPIN_LOCK                sSockSpinLock;
	KEVENT                    sSocketDisconnectedEvent;
	NDK_SOCKET_TYPE           ulType;
	volatile NDK_SOCKET_STATE ulSockState;
	NET_IFINDEX               ulInterfaceIndex;
	volatile LONG             lRefCount;
	UINT32                    uiPrivilegedMrToken;
	ULONG                     ulSockMaxReceiveCbSize;
	ULONG                     ulSockMaxSqCbSize;
	ULONG                     ulSockMaxReadCbSize;
	USHORT                    usSockRqDepth;
	USHORT                    usSockSqDepth;
	USHORT                    usSockMaxInboundReadLimit;
	USHORT                    usSockMaxOutboundReadLimit;
	USHORT                    usSockMaxFrmrPageCount;
	USHORT                    usSockMaxSqSges;
	USHORT                    usSockMaxReadSges;
	USHORT                    usSockMaxReceiveSges;
	USHORT                    usSockMaxNumReceives;
	USHORT                    usSockMaxNumSends;
	USHORT                    usSockQPCount;
	BOOLEAN                   bDisconnectCallbackEnabled;
	BOOLEAN                   bSupportsRemoteInvalidation;
} NDK_SOCKET, * PNDK_SOCKET;


/// <summary>
/// All function declarations goes here onwards;
/// </summary>
/// <param name="pvEntry"></param>
/// <returns></returns>




//====================================================================================
/// <summary>
/// All inline and force inline function and Macro definitions defines here onwards;
/// </summary>
/// <param name="pvEntry"></param>
/// <returns></returns>
FORCEINLINE
_Ret_range_(0, sizeof(SOCKADDR_IN6))
ULONG
NdkGetSockAddrCbSize(_In_opt_ CONST SOCKADDR_INET* Sockaddr)
{
	if (!ARGUMENT_PRESENT(Sockaddr)) {
		return 0;
	}

	switch (Sockaddr->si_family) {
	case AF_INET:
		return sizeof(SOCKADDR_IN);
	case AF_INET6:
		return sizeof(SOCKADDR_IN6);
	default:
		NT_ASSERT(FALSE);
		return 0;
	}
}

FORCEINLINE
PVOID
NdkAllocateNpp(_In_ SIZE_T szNumBytes)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	return ExAllocatePool3(POOL_FLAG_NON_PAGED, szNumBytes, NVMEOF_NDK_TAG_GENERAL, &sParams, 1);
}

FORCEINLINE
VOID
NdkFreeNpp(_In_ PVOID pvEntry)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	ExFreePool2(pvEntry, NVMEOF_NDK_TAG_GENERAL, &sParams, 1);
}

#pragma warning(pop)
