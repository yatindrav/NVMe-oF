;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 2019-2020  Pavilio Data Systems Inc.
;
;Module Name:
;
;    ntiologc.h
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;--*/
;
;#ifndef _NVME_OF_
;#define _NVME_OF_
;
;//
;//  Status values are 32 bit values layed out as follows:
;//
;//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
;//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;//  +---+-+-------------------------+-------------------------------+
;//  |Sev|C|       Facility          |               Code            |
;//  +---+-+-------------------------+-------------------------------+
;//
;//  where
;//
;//      Sev - is the severity code
;//
;//          00 - Success
;//          01 - Informational
;//          10 - Warning
;//          11 - Error
;//
;//      C - is the Customer code flag
;//
;//      Facility - is the facility code
;//
;//      Code - is the facility's status code
;//
;
MessageIdTypedef=NTSTATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               RpcRuntime=0x2:FACILITY_RPC_RUNTIME
               RpcStubs=0x3:FACILITY_RPC_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
               NVMeoF=0x6:FACILITY_NVMEOF_ERROR_CODE
              )


MessageId=0x0001 Facility=NVMeoF Severity=Success SymbolicName=NVME_OF_SUCCESS
Language=English
%2.
.

MessageId=0x0002 Facility=NVMeoF Severity=Informational SymbolicName=NVME_OF_INFORMATIONAL
Language=English
%2.
.

MessageId=0x0003 Facility=NVMeoF Severity=Warning SymbolicName=NVME_OF_WARNING
Language=English
%2.
.

MessageId=0x0004 Facility=NVMeoF Severity=Error SymbolicName=NVME_OF_ERROR
Language=English
%2.
.

;#endif /* _NTIOLOGC_ */