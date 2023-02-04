/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once
#include <wdm.h>

#define NVMEOF_RDMA_MAX_SEGMENTS  256
#define NVMEOF_RDMA_USE_HASH_FOR_IO 1
#define NVMEOF_RDMA_RECEIVE_REQUEST 2

#define NVMEOF_RDMA_RECEIVE_BUFFER_SIZE_128B 128

#define MAX_SGE_COUNT 12
#define MAX_DATA_SGE_BUFFER_SIZE (MAX_SGE_COUNT * sizeof(NDK_SGE))

#define PAGES_FOR_SEND_RECEIVE  3
#define SEND_RECEIVE_LAM_MAX_SIZE (sizeof(NDK_LOGICAL_ADDRESS_MAPPING) + (sizeof(NDK_LOGICAL_ADDRESS) * PAGES_FOR_SEND_RECEIVE)) 

#define NVMEOF_MTU_SIZE 256*1024
#define MTU_IN_PAGE_SIZE (NVMEOF_MTU_SIZE>>PAGE_SHIFT) 
#define MTU_IN_PAGE_SIZE_WITH_ADDITIONAL_PAGES (MTU_IN_PAGE_SIZE + 8)
#define DATA_LAM_MAX_SIZE (sizeof(NDK_LOGICAL_ADDRESS_MAPPING) + (sizeof(NDK_LOGICAL_ADDRESS) * (MTU_IN_PAGE_SIZE_WITH_ADDITIONAL_PAGES)))  //Change this if MTU changes

#define NVMEOF_ADMIN_COMMAND_TIMEOUT_SEC 60 
#define NVMEOF_IO_COMMAND_TIMEOUT_SEC    60 
#define NVMEOF_DISCOVER_HOST_SUBSYS_NAME  "nqn.2022-07.com.yat.discovery"
#define NVMEOF_CONNECT_HOST_SUBSYS_NAME   "nqn.2022-07.com.yat.connect"
#define NVMEOF_SECONDS_TO_MILLISECONDS(durationSec) ((durationSec) * 1000)
#define NVMEOF_RDMA_MEMORY_TAG 'amdR'

#define NVMEOF_QUEUE_GET_RDMA_CTX(psSQ) ((PNVMEOF_QUEUE)psSQ)->psFabricCtx->psRdmaCtx


typedef struct _NVMEOF_RDMA_GLOBAL {
	ULONG64                ulRdmaModuleInitDone;
	LIST_ENTRY             sRdmaControllerHead;
	PNPAGED_LOOKASIDE_LIST psRdmaControllersLAList;
	PNPAGED_LOOKASIDE_LIST psNdkWorkRequestLAList;
	PNPAGED_LOOKASIDE_LIST psRequestLAList; //NVMEOF_REQUEST_RESPONSE
	PLOOKASIDE_LIST_EX     psBufferMdlMapLAList;
	PNPAGED_LOOKASIDE_LIST psLamSglLAList;
	PNPAGED_LOOKASIDE_LIST psLAMLAList;
	PNPAGED_LOOKASIDE_LIST psSGLLAList;
	PLOOKASIDE_LIST_EX     psNVMeCqeLAList;
	PLOOKASIDE_LIST_EX     psNVMeCmdLAList;
	PLOOKASIDE_LIST_EX     psDataLAMLAList;
	KSPIN_LOCK             sRdmaControllerListSpinLock;
} NVMEOF_RDMA_GLOBAL, *PNVMEOF_RDMA_GLOBAL;


FORCEINLINE
PVOID
NVMeoFRdmaAllocateNpp(_In_ SIZE_T szNumBytes)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	return ExAllocatePool3(POOL_FLAG_NON_PAGED, szNumBytes, NVMEOF_RDMA_MEMORY_TAG, &sParams, 1);
}

FORCEINLINE
PVOID
NVMeoFRdmaAllocateNppCacheAligned(_In_ SIZE_T szNumBytes)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	return ExAllocatePool3(POOL_FLAG_NON_PAGED | POOL_FLAG_CACHE_ALIGNED, szNumBytes, NVMEOF_RDMA_MEMORY_TAG, &sParams, 1);
}

FORCEINLINE
VOID
NVMeoFRdmaFreeNpp(_In_ PVOID pvEntry)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	ExFreePool2(pvEntry, NVMEOF_RDMA_MEMORY_TAG, &sParams, 1);
}

FORCEINLINE
_Requires_lock_not_held_(*psLockHandle)
_Acquires_lock_(*psLockHandle)
_Post_same_lock_(*psSpinLock, *psLockHandle)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_raises_(DISPATCH_LEVEL)
_IRQL_saves_global_(QueuedSpinLock, Irql)
VOID
NVMeoFRdmaInStackSpinLockAcquire(__in KIRQL Irql,
                                 __in PKSPIN_LOCK psSpinLock,
                                 __in PKLOCK_QUEUE_HANDLE psLockHandle)
{
	if (Irql >= DISPATCH_LEVEL) {
		KeAcquireInStackQueuedSpinLockAtDpcLevel(psSpinLock, psLockHandle);
	} else {
		KeAcquireInStackQueuedSpinLock(psSpinLock, psLockHandle);
	}
}

FORCEINLINE
_Requires_lock_held_(*psLockHandle)
_Releases_lock_(*psLockHandle)
_IRQL_requires_min_(DISPATCH_LEVEL)
_IRQL_restores_global_(QueuedSpinLock, Irql)
VOID
NVMeoFRdmaInStackSpinLockRelease(__in KIRQL Irql,
                                 __in PKLOCK_QUEUE_HANDLE psLockHandle)
{
	if (Irql >= DISPATCH_LEVEL) {
		KeReleaseInStackQueuedSpinLockFromDpcLevel(psLockHandle);
	} else {
		KeReleaseInStackQueuedSpinLock(psLockHandle);
	}
}