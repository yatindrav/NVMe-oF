#pragma once

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4200) // nameless struct/union
#pragma warning(disable:4214) // bit field types other than int

#include <wdm.h>
#include "fabric_api.h"

typedef signed int INT, * PINT;
typedef unsigned int UINT, * PUINT;
typedef struct _NVMEOF_REQUEST_RESPONSE NVMEOF_REQUEST_RESPONSE, *PNVMEOF_REQUEST_RESPONSE;
typedef struct _NVMEOF_QUEUE NVMEOF_QUEUE, *PNVMEOF_QUEUE;
typedef struct _NVMEOF_FABRIC_CONTROLLER NVMEOF_FABRIC_CONTROLLER, *PNVMEOF_FABRIC_CONTROLLER;
typedef struct _NVMEOF_FABRIC_CONTROLLER_DISCOVERY NVMEOF_FABRIC_CONTROLLER_DISCOVERY, *PNVMEOF_FABRIC_CONTROLLER_DISCOVERY;
typedef struct _NVMEOF_FABRIC_CONTROLLER_CONNECT NVMEOF_FABRIC_CONTROLLER_CONNECT, *PNVMEOF_FABRIC_CONTROLLER_CONNECT;

typedef enum _NVMEOF_COMMAND_DESCRIPTORT_TYPE {
	NVMEOF_COMMAND_DESCRIPTORT_TYPE_NULL,
	NVMEOF_COMMAND_DESCRIPTORT_TYPE_INLINE,
	NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_SINGLE,
	NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_MULTIPLE,
	NVMEOF_COMMAND_DESCRIPTORT_TYPE_SG_MAX
} NVMEOF_COMMAND_DESCRIPTORT_TYPE, * PNVMEOF_COMMAND_DESCRIPTORT_TYPE;

typedef struct _NVMEOF_FABRIC_OPS {
	////Sync Connect, Disconnect, fabric initialization
	////Sync Submit Send and receive ops
	////Async Submit Send and receive ops
	// Control operation ops
	// Fabric specific additional ops
	////Scatter/Gether ops and flags
	NTSTATUS(*_FabricConnectQueue)(PNVMEOF_FABRIC_CONTROLLER);
	NTSTATUS(*_FabricDisconnectQueue)(PNVMEOF_FABRIC_CONTROLLER);
	NTSTATUS(*_FabricDisableIOQueuesEvent)(PNVMEOF_FABRIC_CONTROLLER);
	BOOLEAN(*_FabricIsQueuesBusy)(PNVMEOF_FABRIC_CONTROLLER);
	NTSTATUS(*_FabricSetupNVMoFSession)(PNVMEOF_FABRIC_CONTROLLER);
	NTSTATUS(*_FabricNVMeoFDisconnectSession)(PNVMEOF_FABRIC_CONTROLLER);
	NTSTATUS(*_FabricRegisterRead32)(PNVMEOF_FABRIC_CONTROLLER , ULONG off, PULONG val);
	NTSTATUS(*_FabricRegisterWrite32)(PNVMEOF_FABRIC_CONTROLLER , ULONG off, ULONG val);
	NTSTATUS(*_FabricRegisterRead64)(PNVMEOF_FABRIC_CONTROLLER psFabricController, ULONG off, PULONG64 val);
	UCHAR(*_FabricIsDataPacket)(PNVMEOF_FABRIC_CONTROLLER, PVOID);
	USHORT(*_FabricGetHeaderSize)(PNVMEOF_FABRIC_CONTROLLER);
	NTSTATUS(*_FabricSetSGFlag)(PNVMEOF_FABRIC_CONTROLLER psFabricController, ULONG ulIOQp, PNVME_CMD psNvmeCmd, ULONGLONG ullAddress, ULONG Length, UINT32 uiProtocolSpecific, NVMEOF_COMMAND_DESCRIPTORT_TYPE ulCmdDesc);
	NTSTATUS(*_FabricFlushSession)(PNVMEOF_FABRIC_CONTROLLER psFabricController, NTSTATUS Status);
	NTSTATUS(*_FabricReleaseQResources)(PNVMEOF_FABRIC_CONTROLLER psFabricController, PNVMEOF_QUEUE);
	NTSTATUS(*_FabricResetController)(PNVMEOF_FABRIC_CONTROLLER psFabricController);
	NTSTATUS(*_FabricNVMeoFRequestsAllocate)(PNVMEOF_FABRIC_CONTROLLER psFabricController, ULONG ulSendDataLen, ULONG ulReceiveDataLen, PNVMEOF_REQUEST_RESPONSE* ppsRequest);
	VOID(*_FabricNVMeoFRequestsFree)(PNVMEOF_FABRIC_CONTROLLER psFabricController, PNVMEOF_REQUEST_RESPONSE psRequest);
} NVMEOF_FABRIC_OPS, * PNVMEOF_FABRIC_OPS;

typedef struct _NVMEOF_OS_OPS {
	///All OS Stack specific management commands
	INT resvd;
} NVMEOF_OS_OPS, * PNVMEOF_OS_OPS;

#define MAX_NAMESPACE_COUNT 16
#define MAX_R2T_WRITE_RESPONSE_COUNT 4

#define SHIFT_1BIT  1
#define SHIFT_2BIT  2
#define SHIFT_3BIT  3
#define SHIFT_4BIT  4
#define SHIFT_5BIT  5
#define SHIFT_6BIT  6
#define SHIFT_7BIT  7
#define SHIFT_8BIT  8
#define SHIFT_9BIT  9
#define SHIFT_10BIT 10
#define SHIFT_11BIT 11
#define SHIFT_12BIT 12
#define SHIFT_13BIT 13
#define SHIFT_14BIT 14
#define SHIFT_15BIT 15
#define SHIFT_16BIT 16
#define SHIFT_17BIT 17
#define SHIFT_18BIT 18
#define SHIFT_19BIT 19
#define SHIFT_20BIT 20
#define SHIFT_21BIT 21
#define SHIFT_22BIT 22
#define SHIFT_23BIT 23
#define SHIFT_24BIT 24
#define SHIFT_25BIT 25
#define SHIFT_26BIT 26
#define SHIFT_27BIT 27
#define SHIFT_28BIT 28
#define SHIFT_29BIT 29
#define SHIFT_30BIT 30
#define SHIFT_31BIT 31
#define SHIFT_32BIT 32

#define WORD_MASK        0xFFFF
#define DOUBLE_WORD_MASK 0xFFFFFFFF

#define MAX_CMD_ID                  ((64*1024) - 1)



typedef struct _NVMEOF_FABRIC_TCP_CONTEXT {
	PNVMEOF_TCP_WORK_REQUEST psSQWorkRequestList;
} NVMEOF_FABRIC_TCP_CONTEXT, * PNVMEOF_FABRIC_TCP_CONTEXT;

typedef struct _NVMEOF_FABRIC_RDMA_CONTEXT {
	volatile LONG             lSubmissionWorkRequestOutstanding;
	volatile LONG             lReceiveWorkRequestOutstanding;
	volatile LONG             lFRMRReqOutstanding;
	ULONG                     ulSubmissionResultsCount;
	ULONG                     ulReceiveResultsCount;
	ULONG                     ulSubmissionWorkRequestCount;
	ULONG                     ulReceiveWorkRequestCount;
	LONGLONG                  llFRMRCurrentCount;
	LONGLONG                  llSubmissionWorkRequestSubmitted;
	LONGLONG                  llReceiveWorkRequestSubmitted;
	LONGLONG                  llSendWorkRequestCompleted;
	LONGLONG                  llReceiveWorkRequestCompleted;
	PNVMEOF_RDMA_WORK_REQUEST psSubmissionWorkRequestList;
	PNVMEOF_RDMA_WORK_REQUEST psReceiveWorkRequestList;
	PNVME_NDK_RESULT          psSubmissionWorkRequestResults;
	PNVME_NDK_RESULT          psReceiveWorkRequestResults;
	PNDK_WORK_REQUEST         psGetSubmissionResultsNDKWrq;
	PNDK_WORK_REQUEST         psGetReceiveResultsNDKWrq;
	KSPIN_LOCK                sSubmitSpinLock;
	KSPIN_LOCK                sFRSpinLock;
	KSPIN_LOCK                sBuildLamSpinLock;
	PNVMEOF_RDMA_PRIVATE_DATA psQueuePvtData;
	PNDK_QUEUE_PAIR_BUNDLE    psNdkQP;
	NDK_QP_CBS                psSQCbs;
} NVMEOF_FABRIC_RDMA_CONTEXT, *PNVMEOF_FABRIC_RDMA_CONTEXT;

typedef union _NVMEOF_FABRIC_CONTEXT {
	PNVMEOF_FABRIC_RDMA_CONTEXT psRdmaCtx;
	PNVMEOF_FABRIC_TCP_CONTEXT  psTcpCtx;
} NVMEOF_FABRIC_CONTEXT, * PNVMEOF_FABRIC_CONTEXT;

typedef struct _NVMEOF_QUEUE_OPS {
	USHORT(*_NVMeoFQueueGetCmdID)(PNVMEOF_QUEUE);
	BOOLEAN(*_NVMeoFQueueIsSlotAvailable)(PNVMEOF_QUEUE);
	NTSTATUS(*_NVMeoFQueueSubmitRequest)(PNVMEOF_QUEUE, PNVMEOF_REQUEST_RESPONSE);
} NVMEOF_QUEUE_OPS, * PNVMEOF_QUEUE_OPS;

struct DECLSPEC_CACHEALIGN _NVMEOF_QUEUE {
	BOOLEAN                   bValid;
	UCHAR                     ucReserved;
	USHORT                    usQId;
	volatile LONG             lNextCmdID;
	volatile LONG             lCurrentOutstandingRequest;
	LONG                      lFlags;
	LONG                      lBusy;
	ULONG                     ulQueueDepth;
	ULONG                     ulIOHashBucketSz;
	ULONG                     ulWorkRequestCount;
	INT64                     llReadRequestIssued;
	INT64                     llWriteRequestIssued;
	INT64                     llReadRequestCompleted;
	INT64                     llWriteRequestCompleted;
	INT64                     llReadIssueError;
	INT64                     llWriteIssueError;
	INT64                     llRequestIssued;
	PNVMEOF_FABRIC_CONTROLLER psParentController;
	PVOID                     pvParentNUMACtx;
	PVOID                     pvTransportContext;
	PVOID                     *ppvIoReqTable;
	PNVMEOF_FABRIC_CONTEXT    psFabricCtx;
	NVMEOF_QUEUE_OPS          sQueueOps;
};

#define NVMEOF_QUEUE_DATA_SZ sizeof(NVMEOF_QUEUE)

typedef struct _NVMEOF_SQ_NUMA_CONTEXT {
	volatile LONG lCurrentOutstandingRequest;
	PNVMEOF_QUEUE psSQ;
} NVMEOF_SQ_NUMA_CONTEXT, * PNVMEOF_SQ_NUMA_CONTEXT;

typedef struct _NVMEOF_FABRIC_CONNECT_SESSION {
	PVOID                    pvSessionFabricCtx;
	PNVMEOF_QUEUE            psAdminQueue;
	PNVMEOF_SQ_NUMA_CONTEXT* psNUMACtx;
	LONG                     lSQDepth;
	LONG                     lSQQueueCount;
	USHORT		             usIOQPerNumaNode;
	USHORT                   usNumaNodeCount;
	USHORT		             usNumaBitShift;
	USHORT		             usResvd;
} NVMEOF_FABRIC_CONNECT_SESSION, *PNVMEOF_FABRIC_CONNECT_SESSION;

#define NVMEOF_FABRIC_COMMON_DATA_SZ sizeof(NVMEOF_FABRIC_COMMON_DATA)

typedef struct _NVMEOF_FABRIC_CONNECT_DATA {
	NVME_IDENTYFY_NAMESPACE   sNVMEoFNameSpaceId;
	PNVME_ANA_ACCESS_LOG_PAGE psNVMeoFNSANALogPage;
	ULONG					  ulaValidNameSpaces[MAX_NAMESPACE_COUNT];
	ULONG					  ulCurrentNamespaceID;
} NVMEOF_FABRIC_CONNECT_DATA, * PNVMEOF_FABRIC_CONNECT_DATA;

#define NVMEOF_FABRIC_CONNECT_DATA_SZ sizeof(NVMEOF_FABRIC_CONNECT_DATA)

typedef enum _NVMEOF_CONTROLLER_STATE {
	NVMEOF_CONTROLLER_STATE_CREATED = 0x0,
	NVMEOF_CONTROLLER_STATE_DISCONNECTED = 0x1,
	NVMEOF_CONTROLLER_STATE_CONNECTING = 0x2,
	NVMEOF_CONTROLLER_STATE_CONNECTED = 0x4,
	NVMEOF_CONTROLLER_STATE_ANA_MIGRATION = 0x8,
	NVMEOF_CONTROLLER_STATE_DISCONNECTING = 0x10,
} NVMEOF_CONTROLLER_STATE, *PNVMEOF_CONTROLLER_STATE;

typedef enum _CONTROLLER_FABRIC_TYPE {
	CONTROLLER_FABRIC_TYPE_RDMA,
	CONTROLLER_FABRIC_TYPE_TCP,
	CONTROLLER_FABRIC_TYPE_MAX
} CONTROLLER_FABRIC_TYPE, * PCONTROLLER_FABRIC_TYPE;

typedef enum _FABRIC_CONTROLLER_TYPE {
	FABRIC_CONTROLLER_TYPE_DISCOVER,
	FABRIC_CONTROLLER_TYPE_CONNECT,
	FABRIC_CONTROLLER_TYPE_MAX
} FABRIC_CONTROLLER_TYPE, *PFABRIC_CONTROLLER_TYPE;

struct DECLSPEC_CACHEALIGN _NVMEOF_FABRIC_CONTROLLER {
	NVME_IDENTIFY_CONTROLLER      sNVMoFControllerId;
	CHAR                          caSubsystemNQN[NVMF_NQN_FIELD_LEN];
	CHAR                          caHostNQN[NVMF_NQN_FIELD_LEN];
	CHAR                          caHostUID[NVMF_NQN_FIELD_LEN];
	SOCKADDR_INET                 sTargetAddress;
	SOCKADDR_INET                 sHostFabricAddress;
	GUID                          sHostID;
	LIST_ENTRY			          sFabricListNode;
	LIST_ENTRY                    sControllerListNode;
	NET_IFINDEX                   ulInterfaceIndex;
	MIB_IPFORWARD_ROW2            sIpRoute;
	ULONGLONG                     ullControllerCapability;
	ULONG                         ulControllerState;
	ULONG                         ulVendorSpecific;
	ULONG                         ulAlignment;
	const ULONG                   ulKATO;
	volatile LONG                 lOutstandingRequests;
	CONTROLLER_FABRIC_TYPE        eFabricType;
	FABRIC_CONTROLLER_TYPE        eControllerType;
	USHORT                        usControllerID;
	NVMEOF_FABRIC_CONNECT_SESSION sSession;
	NVMEOF_FABRIC_OPS             sFabricOps;
	NVMEOF_OS_OPS                 sOSOps;
};
#define NVMEOF_FABRIC_CONTROLLER_SZ sizeof(NVMEOF_FABRIC_CONTROLLER)

struct DECLSPEC_CACHEALIGN _NVMEOF_FABRIC_CONTROLLER_DISCOVERY {
	NVMEOF_FABRIC_CONTROLLER;
	LIST_ENTRY              sDiscoveryListNode;
};
#define NVMEOF_FABRIC_CONTROLLER_DISCOVERY_SZ sizeof(NVMEOF_FABRIC_CONTROLLER_DISCOVERY)

struct DECLSPEC_CACHEALIGN _NVMEOF_FABRIC_CONTROLLER_CONNECT {
	NVMEOF_FABRIC_CONTROLLER;
	NVMEOF_FABRIC_CONNECT_DATA;
	LIST_ENTRY                    sConnectListNode;
};
#define NVMEOF_FABRIC_CONTROLLER_CONNECT_SZ sizeof(NVMEOF_FABRIC_CONTROLLER_CONNECT)

typedef struct _NVMEOF_BUFFER {
	PVOID pvBuffer;
	PMDL  psBufferMdl;
	ULONG ulBufferLen;
	ULONG ulReserved;
} NVMEOF_BUFFER, *PNVMEOF_BUFFER;

typedef struct _NVMEOF_REQUEST_BUFFER {
	NVMEOF_BUFFER sReqRespBuffer;
	NVMEOF_BUFFER sDataBuffer;
} NVMEOF_REQUEST_BUFFER, * PNVMEOF_REQUEST_BUFFER, NVMEOF_RESPONSE_BUFFER, *PNVMEOF_RESPONSE_BUFFER;

#define NVMEOF_REQUEST_BUFFER_SZ sizeof(NVMEOF_REQUEST_BUFFER)

typedef enum _NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE {
	NVMEOF_REQUEST_RESPONSE_TYPE_SYNC = 0x0,
	NVMEOF_REQUEST_RESPONSE_TYPE_SYNC_SEND_DATA = 0x1,
	NVMEOF_REQUEST_RESPONSE_TYPE_SYNC_RECEIVE_DATA = 0x2,
	NVMEOF_REQUEST_RESPONSE_TYPE_ASYNC = 0x4,
	NVMEOF_REQUEST_RESPONSE_TYPE_SEND_ONLY = 0x8,
	NVMEOF_REQUEST_RESPONSE_TYPE_USER_SEND_DATA_BUFFER = 0x100,
	NVMEOF_REQUEST_RESPONSE_TYPE_USER_RECEIVE_DATA_BUFFER = 0x200,
	NVMEOF_REQUEST_RESPONSE_TYPE_MAX
} NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE, *PNVMEOF_ADMIN_REQUEST_RESPONSE_TYPE;

typedef enum _NVMEOF_BUFFER_FLAGS {
	NVMEOF_BUFFER_FLAGS_COMMAND_REPLACED = 0x0,
	NVMEOF_BUFFER_FLAGS_COMMAND_DATA_REPLACED = 0x1,
	NVMEOF_BUFFER_FLAGS_RESPONSE_REPLACED = 0x2,
	NVMEOF_BUFFER_FLAGS_RESPONSE_DATA_REPLACED = 0x4,
	NVMEOF_BUFFER_FLAGS_MAX
} NVMEOF_BUFFER_FLAGS, *PNVMEOF_BUFFER_FLAGS;

typedef struct _NVMEOF_REQUEST_RESPONSE_OPERATIONS {
	PNVME_CMD(*_NVMeoFRequestResponseGetCommand)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE);
	PVOID(*_NVMeoFRequestResponseGetCommandData)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE);
	UINT32(*_NVMeoFRequestResponseGetCommandPduLength)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE);
	PNVME_COMPLETION_QUEUE_ENTRY(*_NVMeoFRequestResponseGetCqe)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE);
	PVOID(*_NVMeoFRequestResponseGetResponseData)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE);
	UINT32(*_NVMeoFRequestResponseGetResponsePduLength)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE);
	UINT32(*_NVMeoFRequestResponseSetCommandData)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE, PVOID, ULONG);
	USHORT(*_NVMeoFRequestResponseSetResponseData)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE, PVOID, ULONG);
	UINT32(*_NVMeoFRequestResponseSetCommand)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE, PVOID, ULONG);
	USHORT(*_NVMeoFRequestResponseSetResponse)(PNVMEOF_FABRIC_CONTROLLER, PNVMEOF_REQUEST_RESPONSE, PVOID, ULONG);
} NVMEOF_REQUEST_RESPONSE_OPERATIONS, * PNVMEOF_REQUEST_RESPONSE_OPERATIONS;


struct _NVMEOF_REQUEST_RESPONSE {
	NVMEOF_REQUEST_BUFFER               sSndBuffer;
	NVMEOF_RESPONSE_BUFFER              sRcvBuffer;
	NVMEOF_REQUEST_RESPONSE_OPERATIONS  sNVMeoFReqRespOps;
	PNVMEOF_FABRIC_CONTROLLER           psParentController;
	CONTROLLER_FABRIC_TYPE              FabricType;
	NVMEOF_ADMIN_REQUEST_RESPONSE_TYPE  ulRequestType;
	NVMEOF_BUFFER_FLAGS                 ulBufferFlags;
	NTSTATUS                            Status;
};

#define NVMEOF_REQUEST_GET_COMMAND_BUFFER(psReqRespBuff) ((PNVMEOF_REQUEST_RESPONSE)psReqRespBuff)->sSndBuffer.sReqRespBuffer
#define NVMEOF_REQUEST_GET_DATA_BUFFER(psReqRespBuff) ((PNVMEOF_REQUEST_RESPONSE)psReqRespBuff)->sSndBuffer.sDataBuffer
#define NVMEOF_RESPONSE_GET_RESPONSE_BUFFER(psReqRespBuff) ((PNVMEOF_REQUEST_RESPONSE)psReqRespBuff)->sRcvBuffer.sReqRespBuffer
#define NVMEOF_RESPONSE_GET_DATA_BUFFER(psReqRespBuff) ((PNVMEOF_REQUEST_RESPONSE)psReqRespBuff)->sRcvBuffer.sDataBuffer


#define NVMEOF_REQUEST_RESPONSE_SZ sizeof(NVMEOF_REQUEST_RESPONSE)
#pragma warning(pop)
