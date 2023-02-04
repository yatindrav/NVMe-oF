/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once
#include "sp.h"
NTSTATUS
NVMeoFUtilsGetIfIndexAndLocalIpAddress(__in    PSOCKADDR_INET      psDestAddr,
                                       __inout PSOCKADDR_INET      psLocalAddress,
                                       __inout PMIB_IPFORWARD_ROW2 psIpRoute,
                                       __inout PNET_IFINDEX        pdwIfIndex);