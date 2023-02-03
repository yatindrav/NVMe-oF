#pragma once
#include <wsk.h>
#include <wdm.h>
#include "nvme.h"
#include "nvmef.h"
#include "fabric_api.h"

typedef enum _NVMEOF_WORK_REQUEST_TYPE {
	NVMEOF_WORK_REQUEST_TYPE_RECEIVE,
	NVMEOF_WORK_REQUEST_TYPE_SEND,
	NVMEOF_WORK_REQUEST_TYPE_DATA,
	NVMEOF_WORK_REQUEST_TYPE_IO,
	NVMEOF_WORK_REQUEST_TYPE_MAX
} NVMEOF_WORK_REQUEST_TYPE, * PNVMEOF_WORK_REQUEST_TYPE;

typedef enum _NVMEOF_WORK_REQUEST_STATE {
	NVMEOF_WORK_REQUEST_STATE_FREE = 0,
	NVMEOF_WORK_REQUEST_STATE_ALLOCATED = 0x1,
	NVMEOF_WORK_REQUEST_STATE_INITIALIZED = 0x2,
	NVMEOF_WORK_REQUEST_STATE_PREPARE_SUBMITTING = 0x4,
	NVMEOF_WORK_REQUEST_STATE_SUBMITTING = 0x8,
	NVMEOF_WORK_REQUEST_STATE_SUBMITTED = 0x10,
	NVMEOF_WORK_REQUEST_STATE_WAITING_FOR_RESPONSE = 0x20,
	NVMEOF_WORK_REQUEST_STATE_RESPONSE_RECEIVED = 0x40,
	NVMEOF_WORK_REQUEST_STATE_R2T_RECEIVED = 0x80,
	NVMEOF_WORK_REQUEST_STATE_WRITE_DATA_IN_PROGRESS = 0x100,
	NVMEOF_WORK_REQUEST_STATE_WRITE_DATA_SENT = 0x200,
	NVMEOF_WORK_REQUEST_STATE_DO_NOT_ALLOCATE = 0x400,
	NVMEOF_WORK_REQUEST_STATE_FREEING = 0x800,
	NVMEOF_WORK_REQUEST_STATE_MAX
} NVMEOF_WORK_REQUEST_STATE, * PNVMEOF_WORK_REQUEST_STATE;

#define PDS_NVME_REQUEST_IN_STATE_SUBMITTING (NVMEOF_WORK_REQUEST_STATE_ALLOCATED | NVMEOF_WORK_REQUEST_STATE_INITIALIZED | NVMEOF_WORK_REQUEST_STATE_PREPARE_SUBMITTING |  \
	                                          NVMEOF_WORK_REQUEST_STATE_SUBMITTING)

#define PDS_NVME_REQUEST_ALL_STATE_DONE (NVMEOF_WORK_REQUEST_STATE_ALLOCATED | NVMEOF_WORK_REQUEST_STATE_INITIALIZED | NVMEOF_WORK_REQUEST_STATE_PREPARE_SUBMITTING |  \
	                                     NVMEOF_WORK_REQUEST_STATE_SUBMITTING | NVMEOF_WORK_REQUEST_STATE_SUBMITTED | NVMEOF_WORK_REQUEST_STATE_WAITING_FOR_RESPONSE | \
	                                     NVMEOF_WORK_REQUEST_STATE_RESPONSE_RECEIVED)


typedef VOID (*WORK_REQUEST_COPLETION)(PVOID pvSendWReqCtx, PVOID pvReceiveWReqCtx, ULONG BufferLen, NTSTATUS Status);
typedef VOID (*WORK_REQUEST_SEND_COPLETION)(PVOID pvCtx, NTSTATUS Status);
typedef struct DECLSPEC_CACHEALIGN _WORK_REQUEST_COMPLETION_OPS {
	WORK_REQUEST_COPLETION pfWorkRequestCompletion;
	WORK_REQUEST_SEND_COPLETION pfWorkRequestSendCompletion;
} WORK_REQUEST_COMPLETION_OPS, * PWORK_REQUEST_COMPLETION_OPS;

typedef struct _IO_WRITE_R2T_CTX {
	volatile LONG             lWriteCompletionCount;
	PVOID	                  pvNVMEOFWriteHeader;
	PMDL	                  psNVMEOFWriteHeaderMdl;
	PIRP	                  psWriteR2TIrp;
}IO_WRITE_R2T_CTX, * PIO_WRITE_R2T_CTX;

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_TCP_WORK_REQUEST {
	volatile USHORT                   usCmdID;
	USHORT                            usReserved;
	ULONG                             ulCommandType;
	ULONG                             ulFlags;
	volatile ULONG                    ulSectorCount;
	volatile ULONG                    ulXferLength;
	DECLSPEC_CACHEALIGN volatile LONG lReqState;
	DECLSPEC_CACHEALIGN volatile LONG lCompletionCount;
	volatile ULONG                    ulLength;
	volatile ULONG                    ulBytesCopied;
	volatile ULONG                    ulXferedDataLen;
	volatile ULONG                    ulCurrentPduLength;
	volatile ULONG                    ulCopiedPduLength;
	PMDL                              psRequestMDL;
	WSK_WORK_REQUEST                  sWorkReq;
	PVOID                             pvParentReqCtx;
	PUCHAR                            pucReqBuffer;
	UINT64                            ullStartSector;
	WORK_REQUEST_COMPLETION_OPS       sWorkReqCompletionOps;
	PVOID                             pvIoReqCtx;
	PVOID                             pvCompletionCtx;
	PVOID	                          pvNVMEOFHeader;
	PMDL	                          psNVMEOFHeaderMdl;
	PIRP	                          psReqIRP;
	IO_WRITE_R2T_CTX                  sR2TWriteData[MAX_R2T_WRITE_RESPONSE_COUNT];
	PNVMEOF_QUEUE                     psSQParent;
}NVMEOF_TCP_WORK_REQUEST, * PNVMEOF_TCP_WORK_REQUEST;

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_RDMA_SGL_LAM_MAPPING {
	PNDK_MR                               psNdkMr;
	PNDK_SGE                              psSgl;
	PMDL                                  psMdl;
	volatile PNDK_LOGICAL_ADDRESS_MAPPING psLaml;
	volatile PVOID                        pvBaseVA;
	volatile ULONG                        ulFBO;
	volatile ULONG                        ulCurrentLamCbSize;
	UINT32                                uiRemoteMemoryTokenKey;
	USHORT                                usSgeCount;
	USHORT                                usInitialized;
} NVMEOF_RDMA_SGL_LAM_MAPPING, * PNVMEOF_RDMA_SGL_LAM_MAPPING;

typedef struct DECLSPEC_CACHEALIGN _PDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL {
	PNVMEOF_RDMA_SGL_LAM_MAPPING psLamSglMap;
	PVOID                        pvBuffer;
	PMDL                         psBufferMdl;
	ULONG                        ulBufferLength;
	ULONG                        ulBufferFlag;
} PDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL, * PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL;

typedef struct _NVMEOF_WORK_REQUEST_CONTEXT {
	KEVENT   sEvent;
	PVOID    pvAddnlCtx;
	NTSTATUS Status;
} NVMEOF_WORK_REQUEST_CONTEXT, * PNVMEOF_WORK_REQUEST_CONTEXT;

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_RDMA_WORK_REQUEST {
	volatile DECLSPEC_CACHEALIGN LONG    lReqState;
	volatile USHORT                      usCommandID;
	USHORT                               usResvd;
	ULONG                                ulNVMeCommandType;
	NDK_WORK_REQUEST_TYPE                ulType;
	ULONG                                ulReqFlag;
	ULONG                                ulByteXferred;
	volatile NTSTATUS                    Status;
	ULONG                                ulResvd2;
	PVOID                                pvParentQP;
	PNDK_OBJECT_HEADER                   psNdkObject;
	WORK_REQUEST_COMPLETION_OPS          sWorkReqCompletionOps;
	PVOID                                pvParentReqCtx;
	PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psDataBufferMdlMap;
	PPDS_NVMEF_TRANSPORT_RDMA_BUFFER_MDL psBufferMdlMap;
	PNDK_WORK_REQUEST                    psNdkWorkReq;
	PNVME_COMPLETION_QUEUE_ENTRY         psCqe;
	NVMEOF_WORK_REQUEST_CONTEXT          sCompletionCtx;
} NVMEOF_RDMA_WORK_REQUEST, * PNVMEOF_RDMA_WORK_REQUEST;
