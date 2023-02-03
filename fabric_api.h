#pragma once
#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4200) // nameless struct/union
#pragma warning(disable:4214) // bit field types other than int

#include <ndis.h>  
#include <wsk.h>
#include <ndkpi.h>
//#include <wskndk.h>
//#include <wdmsec.h>
#include <netioapi.h>
#include "win_sock.h"


///NVMe Fabric WSK TCP API interface
typedef struct _WSK_WORK_REQUEST {
    PIRP    psIrp;
    WSK_BUF sSkBuffer;
    ULONG   ulFlags;
}WSK_WORK_REQUEST, * PWSK_WORK_REQUEST;

NTSTATUS
NTAPI
WskDisconnectSocket(__in PWSK_SOCKET psWskSocket);

NTSTATUS
NTAPI
WskDisconnectSocketAbortive(__in PWSK_SOCKET WskSocket);

PWSK_SOCKET
NTAPI
WskCreateAsyncSocket(__in ADDRESS_FAMILY AddressFamily,
    __in USHORT         SocketType,
    __in ULONG          Protocol,
    __in ULONG          Flags,
    __in PVOID          pvCtx,
    __in PVOID          pvDispatch);

NTSTATUS
NTAPI
WskEnableCallbacks(__in PWSK_SOCKET WskSocket,
    __in ULONG ulMaskOfEventsToEnable);

// Only one event at a time can be disabled.
NTSTATUS
NTAPI
WskDisableCallbacks(__in PWSK_SOCKET WskSocket,
    __in ULONG ulMaskOfEventsToDisable);

NTSTATUS
NTAPI
WskSendAsync(__in PWSK_SOCKET       WskSocket,
             __in PWSK_WORK_REQUEST	psWorkReq);

NTSTATUS
NTAPI
WskReceiveAsync(__in PWSK_SOCKET       WskSocket,
                __in PWSK_WORK_REQUEST psWorkReq);

NTSTATUS
NTAPI
WskControlSocket(__in PWSK_SOCKET  psWskSocket,
                 __in ULONG        ulRequestType,
                 __in ULONG        ulControlCode,
                 __in ULONG	      ulLevel,
                 __in SIZE_T       szInputSize,
                 __in_opt PVOID	  pvInputBuffer,
                 __in SIZE_T	      szOutputSize,
                 __out_opt PVOID   pvOutputBuffer,
                 __out_opt SIZE_T* pszOutputSizeReturned);

/// <summary>
/// All NDK interfaces and strcutures declarations needed manage NDK sessions;
/// </summary>
/// <param name="pvEntry"></param>
/// <returns></returns>
///NDK Socket and adapter structures
/// 
typedef enum _NDK_WORK_REQUEST_TYPE {
	NDK_WREQ_SQ_UNINITIALIZE = 0,
	NDK_WREQ_SQ_SEND,
	NDK_WREQ_SQ_SEND_INVALIDATE,
	NDK_WREQ_SQ_RECEIVE,
	NDK_WREQ_SQ_READ,
	NDK_WREQ_SQ_WRITE,
	NDK_WREQ_SQ_FAST_REGISTER,
	NDK_WREQ_SQ_INVALIDATE,
	NDK_WREQ_SQ_GET_SUBMISSION_RESULTS,
	NDK_WREQ_SQ_GET_RECEIVE_RESULTS,
	NDK_WREQ_ADAPTER_BUILD_LAM,
	NDK_WREQ_ADAPTER_RELEASE_LAM,
	NDK_WREQ_PD_CREATE_MR,
	NDK_WREQ_PD_INITIALIZE_MR,
	NDK_WREQ_PD_GET_REMOTE_TOKEN_FROM_MR,
	NDK_WREQ_PD_GET_LOCAL_TOKEN_FROM_MR,
	NDK_WREQ_PD_CLOSE_MR,
	NDK_WREQ_MAX,
} NDK_WORK_REQUEST_TYPE;

static PCHAR pcaWorkRequestString[] = {
	"WorkRequestInvalid",
	"WorkRequestSend",
	"WorkRequestInvalidate",
	"WorkRequestReceive",
	"WorkRequestRead",
	"WorkRequestWrite",
	"WorkRequestFastRegister",
	"WorkRequestInvalidate",
	"WorkRequestBuildLam",
	"WorkRequestReleaseLam",
	"WorkRequestGetSendResults",
	"WorkRequestGetReceiveResults",
	"WorkRequestCreateMemoryRegion",
	"WorkRequestInitMemoryRegion",
	"WorkRequestGetRemoteMemoryRegionToken",
	"WorkRequestGetLocalMemoryRegionToken",
	"WorkRequestCloseMemoryRegion",
	"WorkRequestInvalid",
};

typedef struct _NDK_SQ_SEND_WRQ {
	PNDK_SGE psNdkSgl;
	USHORT   usSgeCount;
	USHORT   usResvd;
	BOOLEAN  uiRemoteMemoryRegionToken;
} NDK_SQ_SEND_WRQ, * PNDK_SQ_SEND_WRQ;

typedef struct _NDK_SQ_RECEIVE_WRQ {
	PNDK_SGE psNdkSgl;
	USHORT   usSgeCount;
	USHORT   usResvd;
	ULONG    ulBytesToReceive;
} NDK_SQ_RECEIVE_WRQ, * PNDK_SQ_RECEIVE_WRQ;

typedef struct _NDK_SQ_READ_WRITE_WRQ {
	UINT64   uiiRemoteMemoryAddress;
	PNDK_SGE psNdkSgl;
	USHORT   usSgeCount;
	USHORT   usResvd;
	ULONG    ulBytesToTransfer;
	UINT32   uiRemoteToken;
} NDK_SQ_READ_WRITE_WRQ, * PNDK_SQ_READ_WRITE_WRQ;

typedef struct _NDK_SQ_FAST_REGISTER_WRQ {
	PNDK_MR                       psNdkMr;
	volatile PNDK_LOGICAL_ADDRESS psAdapterPageArray;
	volatile PVOID                pvBaseVirtualAddress;
	volatile SIZE_T               szLength;
	volatile ULONG                ulFBO;
	volatile ULONG                ulAdapterPageCount;
} NDK_SQ_FAST_REGISTER_WRQ, * PNDK_SQ_FAST_REGISTER_WRQ;

typedef struct _NDK_SQ_INVALIDATE_WRQ {
	PNDK_OBJECT_HEADER psNdkMrOrMw;
} NDK_SQ_INVALIDATE_WRQ, * PNDK_SQ_INVALIDATE_WRQ;

typedef struct _NDK_SQ_BUILD_LAM_WRQ {
	PMDL                         psMdl;
	PNDK_LOGICAL_ADDRESS_MAPPING psLam;
	SIZE_T                       szMdlBytesToMap;
	ULONG                        ulLamCbSize;
	ULONG                        ulFboLAM;
} NDK_SQ_BUILD_LAM_WRQ, * PNDK_SQ_BUILD_LAM_WRQ;

typedef struct _NDK_SQ_RELEASE_LAM_WRQ {
	PNDK_LOGICAL_ADDRESS_MAPPING psLam;
} NDK_SQ_RELEASE_LAM_WRQ, * PNDK_SQ_RELEASE_LAM_WRQ;

typedef struct _NDK_SQ_GET_RECEIVE_RESULTS {
	PVOID  pvNdkRcqResultExx;
	USHORT usMaxNumReceiveResults;
	USHORT usReceiveResultEx;
} NDK_SQ_GET_RECEIVE_RESULTS, * PNDK_SQ_GET_RECEIVE_RESULTS;

typedef struct _NDK_SQ_GET_SEND_RESULTS {
	PNDK_RESULT psNdkScqResult;
	USHORT      usMaxNumSendResults;
} NDK_SQ_GET_SEND_RESULTS, * PNDK_SQ_GET_SEND_RESULTS;


/// <summary>
/// Protection domain requests
/// </summary>
typedef struct _NDK_PD_CREATE_FAST_REGISTER_MR_WRQ {
	PNDK_MR                  psNdkMr;
	NDK_FN_CREATE_COMPLETION fnCreateMrCompletionCb;
	PVOID                    pvCreateMrCompletionCtx;
} NDK_PD_CREATE_FAST_REGISTER_MR_WRQ, * PNDK_PD_CREATE_FAST_REGISTER_MR_WRQ;

typedef struct _NDK_PD_CLOSE_MR_WRQ {
	PNDK_MR                 psNdkMr;
	NDK_FN_CLOSE_COMPLETION fnCloseCompletion;
	PVOID                   pvContext;
} NDK_PD_CLOSE_MR_WRQ, * PNDK_PD_CLOSE_MR_WRQ;

typedef struct _NDK_PD_GET_MR_TOKEN_WRQ {
	PNDK_MR                  psNdkMr;
} NDK_PD_GET_MR_TOKEN_WRQ, * PNDK_PD_GET_MR_TOKEN_WRQ;

typedef struct _NDK_PD_INITIALIZE_FAST_REGISTER_MR_WRQ {
	PNDK_MR                  psNdkMr;
	ULONG                    ulInitAdapterPageCount;
	BOOLEAN                  bInitAllowRemoteAccess;
} NDK_PD_INITIALIZE_FAST_REGISTER_MR_WRQ, * PNDK_PD_INITIALIZE_FAST_REGISTER_MR_WRQ;

typedef struct DECLSPEC_CACHEALIGN _NDK_WORK_REQUEST {
	NDK_WORK_REQUEST_TYPE              ulType;
	ULONG                              ulFlags;
	NDK_FN_REQUEST_COMPLETION          fnCallBack;
	PVOID                              pvCallbackCtx;
	union {
		NDK_SQ_READ_WRITE_WRQ                  sRWReq;
		NDK_SQ_SEND_WRQ                        sSendReq;
		NDK_SQ_RECEIVE_WRQ                     sReceiveReq;
		NDK_SQ_FAST_REGISTER_WRQ               sFRMRReq;
		NDK_SQ_INVALIDATE_WRQ                  sInvalidateReq;
		NDK_SQ_BUILD_LAM_WRQ                   sBuildLamReq;
		NDK_SQ_RELEASE_LAM_WRQ                 sReleaseLamReq;
		NDK_SQ_GET_RECEIVE_RESULTS             sGetRcqResultReq;
		NDK_SQ_GET_SEND_RESULTS                sGetScqResultReq;
		NDK_PD_CREATE_FAST_REGISTER_MR_WRQ     sCreateFRMRReq;
		NDK_PD_INITIALIZE_FAST_REGISTER_MR_WRQ sInitFRMRReq;
		NDK_PD_CLOSE_MR_WRQ                    sCloseFRMRReq;
		NDK_PD_GET_MR_TOKEN_WRQ                sGetMRTokenReq;
	};
} NDK_WORK_REQUEST, * PNDK_WORK_REQUEST;

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
NdkSubmitRequest(__in    PNDK_SOCKET            psSocket,
                 __in    PNDK_QUEUE_PAIR_BUNDLE psSQair,
                 __inout PNDK_WORK_REQUEST      psWkReq);

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NdkArmCompletionQueue(__in PNDK_SOCKET psClientSocket,
	                  __in PNDK_CQ psNdkCq,
	                  __in ULONG ulTriggerType);

_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NdkCreateRdmaQPair(__in PNDK_SOCKET            psRDMASocket,
                   __in PNDK_QP_CBS            psSQCallbacks,
                   __in ULONG                  ulInQD,
                   __in ULONG                  ulOutQD,
                   __in PNDK_QUEUE_PAIR_BUNDLE psSQ);

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
NTAPI
NdkCreateRdmaSocket(__in    PSOCKADDR_INET      psLocalAddress,
                    __in    PSOCKADDR_INET      psRemoteAddress,
                    __in    IF_INDEX            ulAdapterIfIndex,
                    __in    ULONG               ulFlags,
                    __in    PNDK_COMPLETION_CBS psRDMASocketCbs,
                    __inout PNDK_SOCKET* ppsRDMASocket);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NTAPI
NdkConnectRdmaQPair(__in PNDK_SOCKET            psRDMASocket,
                    __in PNDK_QUEUE_PAIR_BUNDLE psSQ,
                    __in ULONG                  ulFlags,
                    __in PVOID                  pvPrivateData,
                    __in ULONG                  ulPrivateDataLen,
                    __in BOOLEAN                bSync);

NTSTATUS
NTAPI
NdkDisconnectSyncAsyncRdmaSocketQP(__inout PNDK_SOCKET         psRDMASocket,
                                   __in PNDK_QUEUE_PAIR_BUNDLE psSQ);

#pragma warning(pop)