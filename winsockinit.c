/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#include "fabric_api.h"
#include "trace.h"
#include "winsockinit.tmh"

static LONG                      glWskState = DEINITIALIZED;
static WSK_REGISTRATION          gsWskRegistration;
WSK_PROVIDER_NPI                 gsWskProvider;
static WSK_CLIENT_DISPATCH       gsWskClientDispatch = { MAKE_WSK_VERSION(1,0), 0, NULL };
static WSK_CLIENT_NPI            gsWskClient = { 0 };

static LONG                 glNdkState = DEINITIALIZED;
static WSK_REGISTRATION     gsNdkRegistration = { 0 };
WSK_PROVIDER_NPI            gsNkdProviderNpi = { 0 };
static WSK_CLIENT_DISPATCH  gsNdkClientDispatch = { MAKE_WSK_VERSION(1, 0), 0, NULL };
WSK_PROVIDER_NDK_DISPATCH   gsNdkDispatch = { 0 };

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


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NVMeoFWskStartup)
#pragma alloc_text(PAGE, NVMeoFWskCleanup)
#pragma alloc_text(PAGE, NVMeoFNdkStartup)
#pragma alloc_text(PAGE, NVMeoFNdkCleanup)
#endif //ALLOC_PRAGMA


BOOLEAN NTAPI IsWskinitialized(VOID)
{
	if (glWskState != INITIALIZED)
		return (FALSE);
	return (TRUE);
}

BOOLEAN NTAPI IsNdkinitialized(VOID)
{
	if (glNdkState != INITIALIZED)
		return (FALSE);
	return (TRUE);
}


_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NVMeoFNdkStartup(VOID)
{
	NTSTATUS Status;
	WSK_CLIENT_NPI sNdkClientNpi = { NULL,  &gsNdkClientDispatch };

	PAGED_CODE();
	if (_InterlockedCompareExchange(&glNdkState, INITIALIZING, DEINITIALIZED) != DEINITIALIZED)
		return STATUS_ALREADY_REGISTERED;

	Status = WskRegister(&sNdkClientNpi, &gsNdkRegistration);
	if (NT_ERROR(Status)) {
		InterlockedExchange(&glNdkState, DEINITIALIZED);
		return (Status);
	}

	Status = WskCaptureProviderNPI(&gsNdkRegistration,
		WSK_NO_WAIT,
		&gsNkdProviderNpi);
	if (NT_ERROR(Status)) {
		WskDeregister(&gsNdkRegistration);
		InterlockedExchange(&glNdkState, DEINITIALIZED);
		return (Status);
	}

	Status =
		gsNkdProviderNpi.Dispatch->WskControlClient(gsNkdProviderNpi.Client,
			WSKNDK_GET_WSK_PROVIDER_NDK_DISPATCH,
			0,
			NULL,
			sizeof(gsNdkDispatch),
			&gsNdkDispatch,
			NULL,
			NULL);
	if (NT_ERROR(Status)) {
		WskReleaseProviderNPI(&gsNdkRegistration);
		WskDeregister(&gsNdkRegistration);
		InterlockedExchange(&glNdkState, DEINITIALIZED);
	}
	else {
		InterlockedExchange(&glNdkState, INITIALIZED);
	}

	return Status;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
NVMeoFNdkCleanup(VOID)
{
	PAGED_CODE();
	if (InterlockedCompareExchange(&glNdkState, INITIALIZED, DEINITIALIZING) != INITIALIZED)
		return;

	WskReleaseProviderNPI(&gsNdkRegistration);
	WskDeregister(&gsNdkRegistration);
	InterlockedExchange(&glNdkState, DEINITIALIZED);
}

_IRQL_requires_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NTAPI
NVMeoFWskStartup(VOID)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	PAGED_CODE();

	if (_InterlockedCompareExchange(&glWskState, INITIALIZING, DEINITIALIZED) != DEINITIALIZED)
		return STATUS_ALREADY_REGISTERED;

	gsWskClient.ClientContext = NULL;
	gsWskClient.Dispatch = &gsWskClientDispatch;

	Status = WskRegister(&gsWskClient, &gsWskRegistration);
	if (NT_ERROR(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		InterlockedExchange(&glWskState, DEINITIALIZED);
		return Status;
	}

	Status = WskCaptureProviderNPI(&gsWskRegistration, WSK_NO_WAIT, &gsWskProvider);
	if (NT_ERROR(Status)) {
		KdPrint(("%s:%d: Failed with status 0x%08X.\n", __FUNCTION__, __LINE__, Status));
		WskDeregister(&gsWskRegistration);
		InterlockedExchange(&glWskState, DEINITIALIZED);
		return Status;
	}

	InterlockedExchange(&glWskState, INITIALIZED);
	return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
NTAPI
NVMeoFWskCleanup(VOID)
{
	PAGED_CODE();

	if (InterlockedCompareExchange(&glWskState, INITIALIZED, DEINITIALIZING) != INITIALIZED)
		return;

	WskReleaseProviderNPI(&gsWskRegistration);
	WskDeregister(&gsWskRegistration);

	InterlockedExchange(&glWskState, DEINITIALIZED);
}