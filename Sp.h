/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once
#if !defined(_MP_User_Mode_Only)                      // User-mode only.

#if !defined(_MP_H_skip_WDM_includes)

#include <wdm.h>

#endif // _MP_H_skip_WDM_includes

#if !defined(__MP_SKIP_INCLUDES__)
#include <stdio.h>
#include <stdarg.h>
#endif // !defined(__MP_SKIP_INCLUDES__)

#pragma warning( disable : 4200 )
#pragma warning( disable : 4201 )

#include <wdf.h>
#include <ntdef.h>  
#include <storport.h>  
#include <devioctl.h>
#include <ntddscsi.h>
#include <scsiwmi.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include <ata.h>
#include "nvme.h"
#include "nvmeftcp.h"
#include "nvmef.h"
#include "errdbg.h"


#define NVMEOF_STORPORT_VERSION     "1.0.0.0"
#define VENDOR_ID                   L"NVMeoF"
#define VENDOR_ID_ascii             "NVMeoF"
#define PRODUCT_ID                  L"NVMeoF"
#define PRODUCT_ID_ascii            "NVMeoF"
#define PRODUCT_REV                 L"1"
#define PRODUCT_REV_ascii           "1"
#define NVMEOF_SP_TAG_GENERAL       'MtSP'
#define NVMEOF_CRYPTOGRAPHY_REG_ABSOULUTE_PATH L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Cryptography"
#define NVMEOF_MACHINE_GUID_STR L"MachineGuid"


#define MAX_TARGETS                          1
#define MAX_LUNS                             1
#define MAX_BUSES                            1
#define ALIGNMENT_MASK                       0x3
#define MP_MAX_TRANSFER_SIZE                 (32 * 1024)
#define TIME_INTERVAL                        (1 * 1000 * 1000) //1 second.
#define DEVLIST_BUFFER_SIZE                  1024
#define DEVICE_NOT_FOUND                     0xFF
#define SECTOR_NOT_FOUND                     0xFFFF
#define NVMEOF_DEFAULT_IO_QUEUE_DEPTH           127
#define NVMEOF_NAMESPACE_ATTR_WRITE_PROTECTED   0x1
#define NVMEOF_SECTOR_SIZE_512                  0x200
#define NVMEOF_PR_SPACE_SIZE                    4096
#define NVMEOF_PAVILION_STRIPE_SIZE             (64*1024)
#define NVMEOF_MACHINE_GUID_READ_DATA_LEN       128
#define NVMEOF_CTRL_NACA_MASK                   0x4
#define NVMEOF_CDB6_CONTROL_BYTE_OFFSET         5

#define NVMEOF_IMPLICIT_ALUA                    1

//Request Sense Definition
#define NVMEOF_REQUEST_SENSE_DESC_FORMAT_OFFSET 1
#define NVMEOF_REQUEST_SENSE_DESC_FORMAT_MASK   0x1
#define NVMEOF_DESC_FORMAT_SENSE_DATA_TYPE      0x1
#define NVMEOF_DESC_FORMAT_SENSE_DATA           0x72
#define NVMEOF_FIXED_SENSE_DATA_ADD_LENGTH      0x10
#define NVMEOF_FIXED_SENSE_DATA                 0x70

//Mode Sense Definition
#define NVMEOF_MODE_SENSE_CACHING_PARAM_SAVEABLE_DISABLED   FALSE
#define NVMEOF_MODE_SENSE_CACHING_PARAM_WRITE_CACHE_DISABLE FALSE
#define NVMEOF_MODE_SENSE_CACHING_PAGE_LENGTH               0x12
#define NVMEOF_MODE_SENSE_EXCEPTION_PAGE_LENGTH             0x0A
#define NVMEOF_MODE_SENSE_CACHING_BLK_DESC_MASK             0x8
#define NVMEOF_MODE_SENSE_PARAM_LONG_LBA_MASK               0x10
#define NVMEOF_MODE_SENSE_CACHING_PARAM_LONG_LBA_SZ         16
#define NVMEOF_MODE_SENSE_CACHING_PARAM_SHORT_LBA_SZ        8
#define NVMEOF_MODE_SENSE_BLOCK_DESC_MAX_SIZE               0xFFFFFFFF
#define NVMEOF_MODE_SENSE_BLOCK_DESC_MAX_BYTE               0xFF
#define NVMEOF_MODE_SENSE_PC_CURRENT_VALUES                 0
#define NVMEOF_MODE_SENSE_PC_CHANGEABLE_VALUES              1
#define NVMEOF_MODE_SENSE_PC_DEFAULT_VALUES                 2


//Mode select definitions
#define NVMEOF_MODE_SELECT_CDB_PAGE_FORMAT_MASK             0x10
#define NVMEOF_MODE_SELECT_CDB_SAVE_PAGES_MASK              0x1
#define NVMEOF_MODE_SELECT_CDB_PAGE_FORMAT_OFFSET           1
#define NVMEOF_MODE_SELECT_CDB_SAVE_PAGES_OFFSET            1
#define NVMEOF_MODE_SELECT_6_CDB_PARAM_LIST_LENGTH_OFFSET   4
#define NVMEOF_MODE_SELECT_10_CDB_PARAM_LIST_LENGTH_OFFSET  7
#define NVMEOF_MODE_SELECT_LONG_LBA_MASK                    0x1
#define NVMEOF_MODE_SELECT_PAGE_CODE_MASK                   0x3F

#define MINIMUM_DISK_SIZE                  (1540 * 1024)    // Minimum size required for Disk Manager
#define MAXIMUM_MAP_DISK_SIZE              (256 * 1024)

#define NVMEOF_DEFAULT_MAX_XFER_SIZE          (128 * 1024)

#define DEFAULT_BREAK_ON_ENTRY             0                // No break
#define DEFAULT_DEBUG_LEVEL                2               
#define DEFAULT_INITIATOR_ID               7
#define DEFAULT_VIRTUAL_DISK_SIZE          (8 * 1024 * 1024)  // 8 MB.  JAntogni, 03.12.2005.
#define DEFAULT_PHYSICAL_DISK_SIZE         DEFAULT_VIRTUAL_DISK_SIZE
#define DEFAULT_USE_LBA_LIST               0
#define DEFAULT_NUMBER_OF_BUSES            1
#define DEFAULT_NUMBER_OF_LUNS             1
#define DEFAULT_ENABLE_PR_IN_DRIVER        FALSE
#define DEFAULT_MAX_INITIATOR_COUNT        8

#define GET_FLAG(Flags, Bit)               ((Flags) & (Bit))
#define SET_FLAG(Flags, Bit)               ((Flags) |= (Bit))
#define CLEAR_FLAG(Flags, Bit)             ((Flags) &= ~(Bit))

#define INQUIRY_EVPD_BYTE_OFFSET                      1
#define INQUIRY_PAGE_CODE_BYTE_OFFSET                 2
#define INQUIRY_CDB_ALLOCATION_LENGTH_OFFSET          3

#define INQUIRY_EVPD_BIT_MASK                         1
#define INQ_NUM_SUPPORTED_VPD_PAGES                   3

#define NVMEOF_BYTE_OFFSET_0                             0
#define NVMEOF_BYTE_OFFSET_1                             1
#define NVMEOF_BYTE_OFFSET_2                             2
#define NVMEOF_BYTE_OFFSET_3                             3
#define NVMEOF_BYTE_OFFSET_4                             4
#define NVMEOF_BYTE_OFFSET_5                             5
#define NVMEOF_BYTE_OFFSET_6                             6
#define NVMEOF_BYTE_OFFSET_7                             7
#define NVMEOF_BYTE_OFFSET_8                             8
#define NVMEOF_BYTE_OFFSET_9                             9
#define NVMEOF_BYTE_OFFSET_10                            10
#define NVMEOF_BYTE_OFFSET_11                            11
#define NVMEOF_BYTE_OFFSET_12                            12
#define NVMEOF_BYTE_OFFSET_13                            13
#define NVMEOF_BYTE_OFFSET_14                            14
#define NVMEOF_BYTE_OFFSET_15                            15

#define NVMEOF_UTILS_BUFFER_SIZE_1B                      1
#define NVMEOF_UTILS_BUFFER_SIZE_2B                      2
#define NVMEOF_UTILS_BUFFER_SIZE_4B                      4
#define NVMEOF_UTILS_BUFFER_SIZE_8B                      8
#define NVMEOF_UTILS_BUFFER_SIZE_16B                     16
#define NVMEOF_UTILS_BUFFER_SIZE_32B                     32
#define NVMEOF_UTILS_BUFFER_SIZE_64B                     64
#define NVMEOF_UTILS_BUFFER_SIZE_128B                    128
#define NVMEOF_UTILS_BUFFER_SIZE_256B                    256
#define NVMEOF_UTILS_BUFFER_SIZE_512B                    512
#define NVMEOF_UTILS_BUFFER_SIZE_1KB                     1024
#define NVMEOF_UTILS_BUFFER_SIZE_2KB                     2048
#define NVMEOF_UTILS_BUFFER_SIZE_4KB                     4096


#define ASCII_SPACE_CHAR_VALUE                        0x20

#define INQUIRY_RESERVED                              0
#define NIBBLES_PER_LONGLONG                          16
#define NIBBLES_PER_LONG                              8
#define NIBBLE_SHIFT                                  4
#define INQUIRY_SN_FROM_EUI64_LENGTH                  0
#define INQUIRY_SN_FROM_NGUID_LENGTH                  0x28
#define INQUIRY_V10_SN_LENGTH                         0x1E
#define INQUIRY_SN_SUB_LENGTH                         4
#define NAA_IEEE_EX                                   6
#define INQUIRY_STANDARD_INQUIRY_PAGE			      0x00
#define STANDARD_INQUIRY_LENGTH                       36
#define REPORT_LUNS_CDB_ALLOC_LENGTH_OFFSET           6
#define REPORT_LUNS_SELECT_REPORT_OFFSET              2

#define REMOVABLE_MEDIA							      FALSE
#define VERSION_SPC_4								  6
#define ACA_UNSUPPORTED								  0
#define HIERARCHAL_ADDR_UNSUPPORTED                   0
#define RESPONSE_DATA_FORMAT_SPC_4                    2
#define ADDITIONAL_STD_INQ_LENGTH                     31
#define EMBEDDED_ENCLOSURE_SERVICES_UNSUPPORTED       0
#define MEDIUM_CHANGER_UNSUPPORTED                    0
#define COMMAND_MANAGEMENT_MODEL                      1
#define WIDE_32_BIT_XFERS_SUPPORTED                   TRUE
#define WIDE_32_BIT_ADDRESES_SUPPORTED                TRUE
#define SYNCHRONOUS_DATA_XFERS_SUPPORTED              TRUE
#define RESERVED_FIELD                                0

#define LOWER_NIBBLE_MASK                             0xF
#define UPPER_NIBBLE_MASK                             0xF0

#define PRODUCT_ID_SIZE                               16
#define PRODUCT_REVISION_LEVEL_SIZE                   4
#define DEBUG_STRING_BUFFER_SIZE                      1024


#define GET_U8_FROM_CDB(psSrb, index)    (((PCDB)psSrb->Cdb)->AsByte[index])
#define GET_U16_FROM_CDB(psSrb, index)    ((((PCDB)psSrb->Cdb)->AsByte[index] << 8) | \
                                         (((PCDB)psSrb->Cdb)->AsByte[index+1] <<  0))
#define GET_U32_FROM_CDB(psSrb, index)   (((psSrb)->Cdb[index]     << 24) | \
                                         ((psSrb)->Cdb[index + 1] << 16) | \
                                         ((psSrb)->Cdb[index + 2] <<  8) | \
                                         ((psSrb)->Cdb[index + 3] <<  0))

#define GET_SRB_EXTENSION(pSrb)      (pSrb)->SrbExtension
#define GET_OPCODE(pSrb)             (pSrb)->Cdb[0]
#define GET_DATA_BUFFER(pSrb)        (pSrb)->DataBuffer
#define GET_DATA_LENGTH(pSrb)        (pSrb)->DataTransferLength
#define SET_DATA_LENGTH(pSrb, len)   ((pSrb)->DataTransferLength = len)
#define GET_PATH_ID(pSrb)            (pSrb)->PathId
#define GET_TARGET_ID(pSrb)          (pSrb)->TargetId
#define GET_LUN_ID(pSrb)             (pSrb)->Lun
#define GET_CDB_LENGTH(pSrb)         (pSrb)->CdbLength


#define GET_INQUIRY_EVPD_BIT(psSrb)                                          \
            ((GET_U8_FROM_CDB(psSrb, INQUIRY_EVPD_BYTE_OFFSET) &         \
              INQUIRY_EVPD_BIT_MASK) ? TRUE : FALSE)

#define GET_INQUIRY_PAGE_CODE(psSrb)                                         \
            (GET_U8_FROM_CDB(psSrb, INQUIRY_PAGE_CODE_BYTE_OFFSET))

#define GET_INQUIRY_ALLOC_LENGTH(psSrb)                                      \
            (GET_U16_FROM_CDB(psSrb, INQUIRY_CDB_ALLOCATION_LENGTH_OFFSET))

#define GET_REPORT_LUNS_ALLOC_LENGTH(psSrb)                              \
            (GET_U32_FROM_CDB(psSrb, REPORT_LUNS_CDB_ALLOC_LENGTH_OFFSET))


#define VPD_ID_DESCRIPTOR_HDR_LENGTH (sizeof(VPD_IDENTIFICATION_DESCRIPTOR))

#define EUI64_DATA_SIZE                              8
#define EUI64_ASCII_SIZE                             EUI64_DATA_SIZE*2
#define NGUID_DATA_SIZE                              16
#define NGUID_ASCII_SIZE                             NGUID_DATA_SIZE*2
#define PRODUCT_ID_ASCII_SIZE                        16
#define VENDOR_ID_DATA_SIZE                          2
#define VENDOR_ID_ASCII_SIZE                         VENDOR_ID_DATA_SIZE*2
#define VENDOR_SPEC_V10_SN_ASCII_SIZE                13
#define VENDOR_SPEC_V10_NSID_ASCII_SIZE              8

#define EUI_ASCII_SIZE                               4
#define NGUID_ID_SIZE                                4
#define SCSI_NAME_PCI_VENDOR_ID_SIZE	             4
#define SCSI_NAME_NAMESPACE_ID_SIZE                  4	
#define SCSI_NAME_MODEL_NUM_SIZE                     40
#define SCSI_NAME_SERIAL_NUM_SIZE                    20
#define LOGICAL_AND_SUBS_LUNS_RET                    0x12
#define ADMIN_AND_LOGICAL_LUNS_RET                   0x11
#define ADMIN_LUNS_RETURNED                          0x10
#define ALL_LUNS_RETURNED                            0x02
#define ALL_WELL_KNOWN_LUNS_RETURNED                 0x01
#define RESTRICTED_LUNS_RETURNED                     0x00
#define LUN_ENTRY_SIZE                               8
#define REPORT_LUNS_FIRST_LUN_OFFSET                 8
#define NVMEOF_DWORD_MASK_BYTE_3                        0xFF000000
#define NVMEOF_DWORD_MASK_BYTE_2                        0x00FF0000
#define NVMEOF_DWORD_MASK_BYTE_1                        0x0000FF00
#define NVMEOF_DWORD_MASK_BYTE_0                        0x000000FF
#define DWORD_MASK_LOW_WORD                          0x0000FFFF
#define DWORD_MASK_HIGH_WORD                         0xFFFF0000
#define NVMEOF_BYTE_SHIFT_3                             24
#define NVMEOF_BYTE_SHIFT_2                             16
#define NVMEOF_BYTE_SHIFT_1                             8
#define LUN_DATA_HEADER_SIZE                         8

#define NVMEOF_PATH_STATE_ACTIVE                        0
#define NVMEOF_PATH_STATE_STANDBY                       1
#define NVMEOF_PATH_STATE_UNAVAILABLE                   2  


#define BLOCK_DEVICE_CHARACTERSTIC_PAGE_LENGTH       0x3C

#pragma pack(1)
/********************************************************************************
* VPD_DESRIPTOR_FLAGS
********************************************************************************/
typedef struct _VPD_DESCRIPTOR_FLAGS {
	USHORT NaaIeeEui64Des : 1;
	USHORT NaaIeeV10Des : 1;
	USHORT T10VidEui64Des : 1;
	USHORT T10VidNguidDes : 1;
	USHORT T10VidV10Des : 1;
	USHORT ScsiEui64Des : 1;
	USHORT ScsiNguidDes : 1;
	USHORT ScsiV10Des : 1;
	USHORT Eui64Des : 1;
	USHORT Eui64NguidDes : 1;
	USHORT Reserved : 6;
} VPD_DESCRIPTOR_FLAGS, * PVPD_DESCRIPTOR_FLAGS;

typedef struct _DESCRIPTIO_FORMAT_SENSE_DATA
{
	UINT8 ResponseCode : 7;
	UINT8 Reserved1 : 1;
	UINT8 SenseKey : 4;
	UINT8 Reserved2 : 4;
	UINT8 AdditionalSenseCode;
	UINT8 AdditionalSenseCodeQualifier;
	UINT8 Reserved3[3];
	UINT8 AdditionalSenseLength;

	/* Determine what Descriptor Types will be needed, if any */
	PUCHAR SenseDataDescriptor[1];
} NVMEOF_DESCRIPTOR_FORMAT_SENSE_DATA, * PNVMEOF_DESCRIPTOR_FORMAT_SENSE_DATA;

#pragma pack(1)
typedef struct _NVMEOF_CACHING_MODE_PAGE
{
	UINT8 PageCode : 6; /* 0x08 */
	UINT8 Reserved1 : 1;
	UINT8 PageSavable : 1;
	UINT8 PageLength;                      /* 0x12 */
	UINT8 ReadDisableCache : 1;
	UINT8 MultiplicationFactor : 1;
	UINT8 WriteCacheEnable : 1;
	UINT8 Reserved2 : 5;
	UINT8 WriteRetensionPriority : 4;
	UINT8 ReadRetensionPriority : 4;
	UINT8 DisablePrefetchTransfer[2];
	UINT8 MinimumPrefetch[2];
	UINT8 MaximumPrefetch[2];
	UINT8 MaximumPrefetchCeiling[2];
	UINT8 NV_DIS : 1;
	UINT8 Reserved3 : 2;
	UINT8 VendorSpecific : 2;
	UINT8 DRA : 1;
	UINT8 LBCSS : 1;
	UINT8 FSW : 1;
	UINT8 NumberOfCacheSegments;
	UINT8 CacheSegmentSize[2];
	UINT8 Reserved4[4];
} NVMEOF_CACHING_MODE_PAGE, * PNVMEOF_CACHING_MODE_PAGE;

typedef struct _NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_SHORT_LBA {  //Mode parameter for direct access devices
	ULONG NumberOfBlocks;
	UCHAR Reserved;
	UCHAR BlockLength[3];
}NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_SHORT_LBA, * PNVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_SHORT_LBA;

typedef struct _NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LONG_LBA {  //Mode parameter for direct access devices
	ULONGLONG NumberOfBlocks;
	ULONG Reserved;
	ULONG BlockLength;
}NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LONG_LBA, * PNVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LONG_LBA;
#pragma pack()

typedef union _NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBAU {  //Mode parameter for direct access devices
	NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LONG_LBA sLongLBA;
	NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_SHORT_LBA sShortLBA;
}NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBAU, * PNVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBAU;


typedef struct _NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBA {  //Mode parameter for direct access devices
	BOOLEAN bLongLBA;
	NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBAU sLBA;
}NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBA, * PNVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBA;



#pragma pack(1)
/********************************************************************************
* SNTI_NAA_IEEE_REG_DESCRIPTOR
********************************************************************************/
typedef struct _NAA_IEEE_EXT_DESCRIPTOR
{
	UCHAR IeeeCompIdMSB : 4;
	UCHAR Naa : 4;
	USHORT IeeeCompId;
	UCHAR VendIdMSB : 4;
	UCHAR IeeeCompIdLSB : 4;
	ULONG VendId;
	UINT64 VenSpecIdExt;
} NAA_IEEE_EXT_DESCRIPTOR, * PNAA_IEEE_EXT_DESCRIPTOR;
#pragma pack()

#pragma pack(1)
/********************************************************************************
* SNTI_T10_VID_DESCRIPTOR
********************************************************************************/
typedef struct _T10_VID_DESCRIPTOR
{
	UCHAR VendorId[8];
	UCHAR VendorSpecific[1];
} T10_VID_DESCRIPTOR, * PT10_VID_DESCRIPTOR;
#pragma pack()

#define T10_VID_DES_VID_FIELD_SIZE  (sizeof(T10_VID_DESCRIPTOR) - 1)

#define NVMEOF_SRB_TO_SCSI_STATUS(_SrbStatus_) (((_SrbStatus_)!=SRB_STATUS_SUCCESS)?SCSISTAT_CHECK_CONDITION:SCSISTAT_GOOD)

#define NVMEOF_COMPLETE_SRB(psHbaExt, psSrb, ScsiStatus, SrbStatus)                          \
{                                                                                         \
     psSrb->ScsiStatus = ScsiStatus;                                                      \
     psSrb->SrbStatus = SrbStatus;                                                        \
     StorPortNotification(RequestComplete, psHbaExt, psSrb);                              \
}

typedef struct _DEVICE_LIST          DEVICE_LIST, * PDEVICE_LIST;
typedef struct _NVMEOF_MP_DRIVER_INFO   NVMEOF_MP_DRIVER_INFO, * PNVMEOF_MP_DRIVER_INFO;
typedef struct _MP_REG_INFO          MP_REG_INFO, * PMP_REG_INFO;
typedef struct _HW_LU_EXTENSION      HW_LU_EXTENSION, * PHW_LU_EXTENSION;
typedef struct _HW_LU_EXTENSION_MPIO HW_LU_EXTENSION_MPIO, * PHW_LU_EXTENSION_MPIO;
typedef struct _LBA_LIST             LBA_LIST, * PLBA_LIST;

extern PNVMEOF_MP_DRIVER_INFO psNVMeoFGlobalInfo;

typedef struct _MP_REG_INFO {
	ULONG   ulDebugLevel;          // Debug log level
	ULONG   ulIOQueueDepth;        // IO Queue Depth that can be handled by driver
	ULONG   ulEnablePR;            // Enabling PR by this flag
	ULONG   ulInitiatorID;         // Initiator ID for PR
	ULONG   ulMaxInitiatorPR;      // Initiator count for PR
	WCHAR   wcaInitiatorIDGUID[NVMEOF_MACHINE_GUID_READ_DATA_LEN];
	GUID    sInitiatorIDGUID;
} MP_REG_INFO, * PMP_REG_INFO;

typedef struct _NVMEOF_MP_DRIVER_INFO {                        // The master miniport object. In effect, an extension of the driver object for the miniport.
	MP_REG_INFO      MPRegInfo;
	KSPIN_LOCK       DrvInfoLock;
	KSPIN_LOCK       MPIOExtLock;       // Lock for ListMPIOExt, header of list of HW_LU_EXTENSION_MPIO objects, 
	LIST_ENTRY       ListMPHBAObj;      // Header of list of NVMEOF_HBA_EXT objects.
	LIST_ENTRY       ListMPIOExt;       // Header of list of HW_LU_EXTENSION_MPIO objects.
	PDRIVER_OBJECT   psDriverObj;
	LONG             DrvInfoNbrMPHBAObj;// Count of items in ListMPHBAObj.
	ULONG            DrvInfoNbrMPIOExtObj; // Count of items in ListMPIOExt.
} NVMEOF_MP_DRIVER_INFO, * PNVMEOF_MP_DRIVER_INFO;

typedef struct _LUN_INFO {
	UCHAR     bReportLUNsDontUse;
	UCHAR     bIODontUse;
} LUN_INFO, * PLUN_INFO;

typedef enum _LOG_LEVEL {
	LOG_LEVEL_ERROR,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_VERBOSE,
	LOG_LEVEL_MAX,
} LOG_LEVEL, * PLOG_LEVEL;

#define NVMEOF_LUN_INFO_MAX 8
#define NVMEOF_MAX_SUPPORTED_NUMA_NODE 128

typedef struct DECLSPEC_CACHEALIGN _NUMA_IO_SCHED {
	PLIST_ENTRY  psNUMAIOListHead;
	PVOID        pvIOSchedThread;
	KEVENT       sIOSchedEvent;
	BOOLEAN      bExit;
	UCHAR        ucaReserved[3];
}NUMA_IO_SCHED, * PNUMA_IO_SCHED;

typedef struct DECLSPEC_CACHEALIGN _NUMA_SCHED_INFO {
	ULONG          ulNUMANodeCount;
	PNUMA_IO_SCHED psNUMAIOschedList[NVMEOF_MAX_SUPPORTED_NUMA_NODE];
}NUMA_SCHED_INFO, * PNUMA_SCHED_INFO;

typedef
VOID (*_NVMEOF_IO_OP_COMPLETION)(__in PVOID pvCtx,
                                  __in PVOID pvIOCtx,
                                  __in NTSTATUS Status);

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_IO_REQ {
	PMDL                      psBufferMdl;
	PUCHAR                    pucDataBuffer;
	PVOID                     pvIoReqCtx;
	PVOID                     pvHBACtx;
	_NVMEOF_IO_OP_COMPLETION  fnCompletion;
	PVOID                     pvParentQ;
	UINT64                    ullStartSector;
	ULONG                     ulLength;
	USHORT                    usSectorCount;
	USHORT                    usFlags;
}NVMEOF_IO_REQ, * PNVMEOF_IO_REQ;

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_HBA_EXT {                          // Adapter device-object extension allocated by StorPort.
	LIST_ENTRY                     sList;              // Pointers to next and previous NVMEOF_HBA_EXT objects.
	LIST_ENTRY                     sLUList;            // Pointers to HW_LU_EXTENSION objects.
	LIST_ENTRY                     sMPIOLunList;
	PNVMEOF_MP_DRIVER_INFO         psNVMEOFParentStorportMPDrvObj;
	PDRIVER_OBJECT                 psDrvObj;
	PDEVICE_OBJECT                 psLowerDO;
	PDEVICE_OBJECT                 psBusFDO;
	//PNVMEOF_NVMEOF_DEVICE_LIST     psDeviceList;
	//PNVMEOF_NVMEOF_DEVICE_LIST     psPrevDeviceList;
	SCSI_WMILIB_CONTEXT            WmiLibContext;
	GUID                           sInitiatorID;
	PIRP                           psReverseCallIrp;
	KSPIN_LOCK                     WkItemsLock;
	KSPIN_LOCK                     WkRoutinesLock;
	KSPIN_LOCK                     MPHBAObjLock;
	KSPIN_LOCK                     LUListLock;
	//PNVMEOF_INTERFACE_STANDARD     psNVMeOFInterface;
	NUMA_SCHED_INFO                sNUMAIOSchedular;
	LUN_INFO                       LUNInfoArray[NVMEOF_LUN_INFO_MAX]; // To be set only by a kernel debugger.
	PUCHAR                         pucPRBuffer;
	LONG64                         llReadSrb;
	LONG64                         llWriteSrb;
	LONG64                         llSRBsSeen;
	ULONG64                        ullPRStartBlockID;
	ULONG                          WMISRBsSeen;
	ULONG                          NbrMPIOLuns;
	ULONG                          NbrLUNsperHBA;
	ULONG                          ulSetQD;
	ULONG                          ulMTU;
	ULONG                          ulValidNUMANodeCount;
	LONG                           lRemoved;
	LONG                           lLowerDevicePathInaccessible;
	LONG                           lStopped;
	LONG						   lBusy;
	LONG                           lOutstandingSrb;
	ULONG                          lIOQueueDepth;
	STOR_DEVICE_POWER_STATE        ulPowerState;
	UCHAR                          HostTargetId;
	UCHAR                          ucAdapterState;
	UCHAR                          ucAdapterPathState;
	BOOLEAN                        bEnablePR;
	BOOLEAN                        bDontReport;
	BOOLEAN                        bReportAdapterDone;
} NVMEOF_HBA_EXT, * PNVMEOF_HBA_EXT;

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_LU_EXTENSION_MPIO {                // Collector for LUNs that are represented by MPIO as 1 pseudo-LUN.
	LIST_ENTRY            sList;                       // Pointers to next and previous HW_LU_EXTENSION_MPIO objects.
	LIST_ENTRY            sLUExtList;                  // Header of list of HW_LU_EXTENSION objects.
	KSPIN_LOCK            sLUExtMPIOLock;
	ULONG                 ulNbrRealLUNs;
	SCSI_ADDRESS          sScsiAddr;
	USHORT                usMaxBlocks;
	BOOLEAN               bIsMissingOnAnyPath;        // At present, this is set only by a kernel debugger, for testing.
} NVMEOF_LU_EXTENSION_MPIO, *PNVMEOF_LU_EXTENSION_MPIO;

// Flag definitions for LUFlags.

#define LU_DEVICE_INITIALIZED   0x0001
#define LU_MPIO_MAPPED          0x0004

#define NVMEOF_MAX_PR_INITIATORS_COUNT                 8
#define NVMEOF_INITIATOR_ID_SIZE                       16
#define NVMEOF_PR_KEY_LENGTH                           8
#define NVMEOF_PR_OUT_PARAMETER_LIST_LENGTH            24
#define NVMEOF_PR_OUT_SERVICE_ACTION_REGISTER_AND_MOVE 0x07
#define NVMEOF_READ_PR_DATA                            0
#define NVMEOF_WRITE_PR_DATA                           1
#define NVMEOF_PR_SIGNATURE                            0x5044534159534450
#define NVMEOF_PR_DS_VERSION                           1

#pragma pack(1)

typedef struct _NVMEOF_PR_REG {
	UCHAR	ucaInitiatorID[NVMEOF_INITIATOR_ID_SIZE];
	ULONG64	ullResKey;
	USHORT 	ucType : 4;
	USHORT 	ucScope : 4;
	USHORT 	ucAllTgPt : 1;
	USHORT 	ucAptpl : 1;
	USHORT 	ucResHolder : 1;
	USHORT	ucBusy : 1;
	UCHAR	ucPathID;
	UCHAR	ucTarget;
	UCHAR	ucLUN;
} NVMEOF_PR_REG, * PNVMEOF_PR_REG;

typedef struct _NVMEOF_PR_TABLE {
	ULONGLONG     ullSignature;
	ULONG         ulVersion;
	ULONG         ulGeneration;
	ULONG         ulEntry;
	ULONG         ulReserved;
	NVMEOF_PR_REG saPREntry[0];
}NVMEOF_PR_TABLE, * PNVMEOF_PR_TABLE;

typedef struct _NVMEOF_PR_READ_RESERVATION {
	ULONG     ulGeneration;
	ULONG     ulLen;
	ULONGLONG ullResKey;
	ULONG     ulObsolate1;
	UCHAR     ulReserved;
	UCHAR     ucFlag;
	UCHAR     ucaObsolate[2];
}NVMEOF_PR_READ_RESERVATION, * PNVMEOF_PR_READ_RESERVATION;

typedef struct _NVMEOF_PR_READ_KEYS {
	ULONG     ulGeneration;
	ULONG     ulLen;
	ULONGLONG ullResKey[0];
}NVMEOF_PR_READ_KEYS, * PNVMEOF_PR_READ_KEYS;
#pragma pack()

#pragma pack(push, p_reserve_out_stuff, 1)
typedef struct {
	UCHAR ReservationKey[NVMEOF_PR_KEY_LENGTH];
	UCHAR ServiceActionReservationKey[NVMEOF_PR_KEY_LENGTH];
	UCHAR ScopeSpecificAddress[4];
	UCHAR ActivatePersistThroughPowerLoss : 1;
	UCHAR Reserved1 : 1;
	UCHAR AllTargetPort : 1;
	UCHAR SpecifyInitiatorTargetPort : 1;
	UCHAR Reserved2 : 4;
	UCHAR Reserved3;
	UCHAR Obsolete[2];
} NVMEOF_PRO_PARAMETER_LIST, * PNVMEOF_PRO_PARAMETER_LIST;
#pragma pack(pop, p_reserve_out_stuff)


typedef struct _NVMEOF_PR_INFO {
	__in ULONG64 ullResKey;
	__in ULONG64 ullServiceActionPRKey;
	__in UCHAR   ucAPTPL;
	__in UCHAR   ucAllTargetPort;
	__in UCHAR   ucSpecIPt;
	__in UCHAR   ucType;
	__in UCHAR   ucScope;
	__in UCHAR   ucIgnore;
	__in UCHAR   ucAbort;
	__in UCHAR   ucUnreg;
} NVMEOF_PR_INFO, * PNVMEOF_PR_INFO;

#define NVMEOF_PR_LU_OWNER_NO_INITIRTOR  0
#define NVMEOF_PR_LU_OWNER_MYSELF        1
#define NVMEOF_PR_LU_OWNER_OTHER         2

typedef struct _NVMEOF_PR_REQUEST {
	PNVMEOF_PR_TABLE psBuffer;
	PMDL          psPRBufferMdl;
	ULONGLONG     ullPRStartBlockID;
	NVMEOF_PR_REG    sPRLuEntry;
	ULONG         ulPRBlockCount;
	ULONG         ulBufferLen;
	ULONG         ulGeneration;
	LONG          lLUOwnerStatus;
}NVMEOF_PR_REQUEST, * PNVMEOF_PR_REQUEST;

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_LU_EXTENSION {         // LUN extension allocated by StorPort.
	LIST_ENTRY                                 sList;                       // Pointers to next and previous HW_LU_EXTENSION objects, used in NVMEOF_HBA_EXT.
	LIST_ENTRY                                 sMPIOList;                   // Pointers to next and previous HW_LU_EXTENSION objects, used in HW_LU_EXTENSION_MPIO.
	PNVMEOF_LU_EXTENSION_MPIO                  psLUMPIOExt;
	NVMEOF_PR_REQUEST                          sPRInfo;
	ULONG                                      ulMaxInitiatorCount;
	NVMEOF_MODE_PARAMETER_BLOCK_DA_DEVICES_LBA sLBA;  // For mode select command
	NVMEOF_CACHING_MODE_PAGE                   sCacheModePage;
	ULONG                                      ulLUFlags;
	USHORT                                     BlocksUsed;
	BOOLEAN                                    bIsMissing;                 // At present, this is set only by a kernel debugger, for testing.
	UCHAR                                      DeviceType;
	UCHAR                                      TargetId;
	UCHAR                                      Lun;
	UCHAR                                      PathId;
	BOOLEAN                                    bLunExtensionInit;
	ULONG64				                       ullCapacity;                 //Capacity is in Number of blocks 
	ULONG				                       ulBlockSize;
	USHORT				                       usBitShiftBS;
	BOOLEAN                                    bIsWriteProtected;
} NVMEOF_LU_EXTENSION, * PNVMEOF_LU_EXTENSION;


#define MODE_SENSE_MAX_BUF_SIZE      256
#define DFT_TX_SIZE                 (128*1024)
#define MIN_TX_SIZE                 (4*1024)
#define MAX_TX_SIZE                 (1024*1024)

typedef struct DECLSPEC_CACHEALIGN _NVMEOF_SRB_EXTENSION
{
	//NVMEOF_NVMEOF_IO         sIODesc;
	PNVMEOF_HBA_EXT				   psHBAExt;
	PSCSI_REQUEST_BLOCK        psSrb;
	BOOLEAN					   ModeSenseWaitState;
	SCSIWMI_REQUEST_CONTEXT    WmiRequestContext;
	NVMEOF_PR_INFO             sPRInfo;
} NVMEOF_SRB_EXTENSION, * PNVMEOF_SRB_EXTENSION;

#pragma pack(1)
typedef struct _REPORT_TARGET_PORT_GROUPS {
	UCHAR OperationCode;       // Byte  0          : SCSIOP_MAINTENANCE_IN
	UCHAR ServiceAction : 5;   // Byte  1, bit 0-4 : SERVICE_ACTION_REPORT_TIMESTAMP
	UCHAR Reserved1 : 3;       // Byte  1, bit 5-7
	UCHAR Reserved2[4];        // Byte  2-5
	UCHAR AllocationLength[4]; // Byte  6-9
	UCHAR Reserved3;           // Byte 10
	UCHAR Control;             // Byte 11
} REPORT_TARGET_PORT_GROUPS, * PREPORT_TARGET_PORT_GROUPS;

typedef struct _TARGET_PORT_DESC {
	USHORT usObsolete;
	USHORT usRelativeTargetPortId;
} TARGET_PORT_DESC, * PTARGET_PORT_DESC;

typedef struct _REPORT_TARGET_PORT_GROUP_DESC {
	UCHAR  ucAAState : 4;
	UCHAR  ucResvd1 : 3;
	UCHAR  ucPref : 1;
	UCHAR  ucActiveOptimizedSupported : 1;
	UCHAR  ucActiveNonOptimizedSupported : 1;
	UCHAR  ucStandbySupported : 1;
	UCHAR  ucUnavailableSupported : 1;
	UCHAR  ucResvd2 : 3;
	UCHAR  ucTransitioningSupported : 1;
	USHORT usTPG;
	UCHAR  ucResvd4;
	UCHAR  ucStatusCode;
	UCHAR  ucVendorSpec;
	UCHAR  ucTPCount;
	TARGET_PORT_DESC sTargetPortDesc[1];
} REPORT_TARGET_PORT_GROUP_DESC, * PREPORT_TARGET_PORT_GROUP_DESC;
#pragma pack()


typedef union _NVME_VERSION
{
	struct {
		USHORT MNR;
		USHORT MJR;
	};
	ULONG AsUlong;
} NVME_VERSION, * PNVME_VERSION;

enum ResultType {
	ResultDone,
	ResultQueued
};

__declspec(dllexport)                                 // Ensure DriverEntry entry point visible to WinDbg even without a matching .pdb.            
NTSTATUS
DriverEntry(__in PVOID,
	        __in PUNICODE_STRING);

ULONG
NVMeoFHwFindAdapter(__in    PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                    __in    PVOID HwContext,
                    __in    PVOID BusInfo,
                    __in    PCHAR ArgumentString,
                    __inout PPORT_CONFIGURATION_INFORMATION ConfigInfo,
                    __out   PBOOLEAN pbAgain);

BOOLEAN
NVMeoFHwInitialize(__in PNVMEOF_HBA_EXT);

VOID
NVMeoFHwReportAdapter(__in PNVMEOF_HBA_EXT);

void
NVMeoFHwReportLink(__in PNVMEOF_HBA_EXT);

void
NVMeoFHwReportLog(__in PNVMEOF_HBA_EXT);

VOID
NVMeoFHwFreeAdapterResources(__in PNVMEOF_HBA_EXT);

NTSTATUS
NVMeoFHwHandlePnP(__in PNVMEOF_HBA_EXT              pHBAExt,  // Adapter device-object extension from StorPort.
                  __in PSCSI_PNP_REQUEST_BLOCK  pSrb);

NTSTATUS
NVMeoFHandleIOCTL(__in PNVMEOF_HBA_EXT         psHBAExt,// Adapter device-object extension from StorPort.
                  __in PSCSI_REQUEST_BLOCK psSrb);

BOOLEAN
NVMeoFHwStartIo(__in PNVMEOF_HBA_EXT,
                __in PSCSI_REQUEST_BLOCK);

BOOLEAN
NVMeoFHwResetBus(__in PNVMEOF_HBA_EXT psHBAExt,
                 __in ULONG);

SCSI_ADAPTER_CONTROL_STATUS
NVMeoFHwAdapterControl(__in PNVMEOF_HBA_EXT DevExt,
	                   __in SCSI_ADAPTER_CONTROL_TYPE ControlType,
	                   __in PVOID Parameters);

UCHAR
NVMeoFScsiExecuteMain(__in PNVMEOF_HBA_EXT      psNVMeoFHbaExt,
                      __in PSCSI_REQUEST_BLOCK  psSrb,
                      __in PNVMEOF_LU_EXTENSION psLUExt,
                      __in PUCHAR               pucStatus);

VOID
NVMeoFQueryRegParameters(__in PUNICODE_STRING,
	__in PMP_REG_INFO);

NTSTATUS
NVMeoFCreateDeviceList(__in PNVMEOF_HBA_EXT,
	__in ULONG);

UCHAR
NVMeoFGetDeviceType(__in PNVMEOF_HBA_EXT DevExt,
	__in UCHAR PathId,
	__in UCHAR TargetId,
	__in UCHAR Lun);

UCHAR
NVMeoFFindRemovedDevice(__in PNVMEOF_HBA_EXT,
	__in PSCSI_REQUEST_BLOCK);

VOID
NVMeoFStopAdapter(__in PNVMEOF_HBA_EXT DevExt);

VOID
NVMeoFTracingInit(__in PVOID,
                  __in PVOID);

VOID
NVMeoFTracingCleanup(__in PVOID);

VOID
NVMeoFProcessServiceRequest(__in PNVMEOF_HBA_EXT,
                            __in PIRP);

VOID
NVMeoFCompleteServiceRequest(__in PNVMEOF_HBA_EXT);

UCHAR
NVMeoFScsiOpVPD(__in PNVMEOF_HBA_EXT,
	__in PHW_LU_EXTENSION,
	__in PSCSI_REQUEST_BLOCK);

VOID
NVMeoFInitializeWmiContext(__in PNVMEOF_HBA_EXT);

BOOLEAN
NVMeoFHandleWmiSrb(__in    PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                   __inout PSCSI_WMI_REQUEST_BLOCK psWmiSrb);

VOID
NVMeoFQueueServiceIrp(__in PNVMEOF_HBA_EXT psNVMeoFHbaExt,
                      __in PIRP            psIrp);

BOOLEAN
NVMeoFSetScsiSenseData(__inout PSCSI_REQUEST_BLOCK psSrb,
	__in UCHAR ucScsiStatus,
	__in UCHAR ucSenseKey,
	__in UCHAR ucASC,
	__in UCHAR ucASCQ);

#ifdef NVMEOF_DBG
VOID
NVMeoFPrintBuffer(__in PUCHAR buf,
	__in ULONG bufLen,
	__in ULONG printAlignment);
#else
#define NVMeoFPrintBuffer(x, y, z)
#endif

VOID
NVMeoFUpdatePathStatus(__in PNVMEOF_HBA_EXT psHBAExt);

VOID
NVMeoFBusChangeDetectedNotification(__in PNVMEOF_HBA_EXT psHBAExt,
                                    __in NTSTATUS Status);

VOID
NVMeoFLogEvent(__in NTSTATUS ulLogLevel,
	           __in __nullterminated PCHAR pcFormatString,
	           ...);

typedef VOID(*fnNVMeoFDebugLog)(__in ULONG ulLogLevel,
                                __in __nullterminated PCHAR pcFormatString,
                                ...);

fnNVMeoFDebugLog
NVMeoFGetDebugLoggingFunction(VOID);

#define NVMEOF_IOCTL_MINIPORT_SIGNATURE_NVMEOF_INFO  "NVMeoF"

#define IOCTL_NVMEOF_SCSI_MINIPORT_GET_NQN             CTL_CODE(FILE_DEVICE_SCSI, 0x0600, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NVMEOF_SCSI_MINIPORT_GET_NSGUID          CTL_CODE(FILE_DEVICE_SCSI, 0x0601, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NVMEOF_SCSI_MINIPORT_INIT_PR_REGION      CTL_CODE(FILE_DEVICE_SCSI, 0x0602, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NVMEOF_SCSI_MINIPORT_READ_PR_REGION      CTL_CODE(FILE_DEVICE_SCSI, 0x0603, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NVMEOF_SCSI_MINIPORT_WRITE_PR_REGION     CTL_CODE(FILE_DEVICE_SCSI, 0x0604, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NVMEOF_SCSI_MINIPORT_ENABLE_PR           CTL_CODE(FILE_DEVICE_SCSI, 0x0605, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define NVMEOF_PROCESS_SERVICE_IRP_MIN_LEN 16UL

FORCEINLINE
PVOID
NVMeoFAllocateNpp(_In_ SIZE_T szNumBytes)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	return ExAllocatePool3(POOL_FLAG_NON_PAGED, szNumBytes, NVMEOF_SP_TAG_GENERAL, &sParams, 1);
}

FORCEINLINE
VOID
NVMeoFFreeNpp(_In_ PVOID pvEntry)
{
	POOL_EXTENDED_PARAMETER sParams = { 0 };
	sParams.Type = PoolExtendedParameterPriority;
	sParams.Priority = NormalPoolPriority;
	ExFreePool2(pvEntry, NVMEOF_SP_TAG_GENERAL, &sParams, 1);
}

#endif    //   #if !defined(_MP_User_Mode_Only)
