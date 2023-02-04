/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once

typedef enum _USER_NVMF_TRTYPE {
	USER_NVMF_TRTYPE_RDMA = 1,	/* RDMA */
	USER_NVMF_TRTYPE_FC = 2,	/* Fibre Channel */
	USER_NVMF_TRTYPE_TCP = 3,	/* TCP Channel */
	USER_NVMF_TRTYPE_LOOP = 254,  /* Reserved for host usage */
} USER_NVMF_TRTYPE, * PUSER_NVMF_TRTYPE;

typedef struct _NVMEOF_USER_DISCOVERY_RESPONSE_ENTRY {
	USHORT				portid;
	UCHAR				addrfamily;
	UCHAR				nqntype;
	USER_NVMF_TRTYPE	transport;
	SOCKADDR_IN         traddr;
	CHAR				subnqn[NVMF_NQN_FIELD_LEN];
} NVMEOF_USER_DISCOVERY_RESPONSE_ENTRY, * PNVMEOF_USER_DISCOVERY_RESPONSE_ENTRY;

typedef struct _NVMEOF_USER_DISCOVERY_RESPONSE {
	ULONG64                              NumEntries;
	NVMEOF_USER_DISCOVERY_RESPONSE_ENTRY Entries[0];
} NVMEOF_USER_DISCOVERY_RESPONSE, * PNVMEOF_USER_DISCOVERY_RESPONSE;
