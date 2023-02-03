#pragma once
#include <ntdef.h>
#include <guiddef.h>
#include <uuids.h>
#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4200) // nameless struct/union
#pragma warning(disable:4214) // bit field types other than int

#define NVMF_NQN_FIELD_LEN	       256
#define NVMF_NQN_SIZE		       223
#define NVME_DISCOVER_SUBSYS_NAME  "nqn.2014-08.org.nvmexpress.discovery"
#define NVME_HOST_ID_HEADER_STRING "nqn.2014-08.org.nvmexpress:uuid:%s"
#define NVME_TCP_DISC_PORT	       8009
#define NVME_RDMA_IP_PORT	       4420

#define NVMEOF_MAX_CMD_ID	       1024*64

#define NVME_NSID_ALL		       0xffffffff

#define NVME_ADMIN_QUEUE_ID                         0
#define NVME_ADMIN_QUEUE_REQUEST_COUNT              32
#define NVME_DEFAULT_IO_QUEUE_DEPTH                 127
#define NVME_AEN_COMMANDS_COUNT                     1
#define NVME_ADMIN_QUEUE_DEPTH_WO_AEN_COMMAND       (NVME_ADMIN_QUEUE_REQUEST_COUNT - NVME_AEN_COMMANDS_COUNT)
#define NVME_ASYNC_EVENT_COMMAND_ID                 NVME_ADMIN_QUEUE_DEPTH_WO_AEN_COMMAND

/*
 * Subtract one to leave an empty queue entry for 'Full Queue' condition. See
 * NVM-Express 1.2 specification, section 4.1.2.
 */
#define NVME_AQ_MQ_TAG_DEPTH	(NVME_AQ_BLK_MQ_DEPTH - 1)


 /* Address Family codes for Discovery Log Page entry ADRFAM field */
enum {
	NVMF_ADDR_FAMILY_PCI = 0,    /* PCIe */
	NVMF_ADDR_FAMILY_IP4 = 1,    /* IP4 */
	NVMF_ADDR_FAMILY_IP6 = 2,    /* IP6 */
	NVMF_ADDR_FAMILY_IB = 3,    /* InfiniBand */
	NVMF_ADDR_FAMILY_FC = 4,    /* Fibre Channel */
};

/* Transport Type codes for Discovery Log Page entry TRTYPE field */
typedef enum _TRANSPORT_TYPE {
	NVMF_TRTYPE_RDMA = 1,    /* RDMA */
	NVMF_TRTYPE_FC = 2,    /* Fibre Channel */
	NVMF_TRTYPE_TCP = 3,    /* TCP/IP */
	NVMF_TRTYPE_LOOP = 254,  /* Reserved for host usage */
	NVMF_TRTYPE_MAX,
} TRANSPORT_TYPE, * PTRANSPORT_TYPE;

/* Transport Requirements codes for Discovery Log Page entry TREQ field */
enum {
	NVMF_TREQ_NOT_SPECIFIED = 0,		/* Not specified */
	NVMF_TREQ_REQUIRED = 1,		/* Required */
	NVMF_TREQ_NOT_REQUIRED = 2,		/* Not Required */
#define NVME_TREQ_SECURE_CHANNEL_MASK \
	(NVMF_TREQ_REQUIRED | NVMF_TREQ_NOT_REQUIRED)
	NVMF_TREQ_DISABLE_SQFLOW = (1 << 2),	/* Supports SQ flow control disable */
};

/* RDMA QP Service Type codes for Discovery Log Page entry TSAS
 * RDMA_QPTYPE field
 */
typedef enum _RDMA_CONNECTION_TYPE_CODE {
	NVMF_RDMA_QPTYPE_CONNECTED = 1, /* Reliable Connected */
	NVMF_RDMA_QPTYPE_DATAGRAM = 2, /* Reliable Datagram */
} RDMA_CONNECTION_TYPE_CODE, * PRDMA_CONNECTION_TYPE_CODE;

/*
 * RDMA QP Service Type codes for Discovery Log Page entry TSAS
 * RDMA_QPTYPE field
 */
typedef enum _RDMA_QPAIR_SERVICE_TYPE_CODE {
	NVMF_RDMA_PRTYPE_NOT_SPECIFIED = 1, /* No Provider Specified */
	NVMF_RDMA_PRTYPE_IB = 2, /* InfiniBand */
	NVMF_RDMA_PRTYPE_ROCE = 3, /* InfiniBand RoCE */
	NVMF_RDMA_PRTYPE_ROCEV2 = 4, /* InfiniBand RoCEV2 */
	NVMF_RDMA_PRTYPE_IWARP = 5, /* IWARP */
} RDMA_QPAIR_SERVICE_TYPE_CODE, * PRDMA_QPAIR_SERVICE_TYPE_CODE;

/*
 * RDMA Connection Management Service Type codes for Discovery Log Page
 * entry TSAS RDMA_CMS field
 */
enum {
	NVMF_RDMA_CMS_RDMA_CM = 1, /* Sockets based endpoint addressing */
};

enum {
	NVME_PROPERTY_OFFSET_CAP = 0x0000,	/* Controller Capabilities */
	NVME_PROPERTY_OFFSET_VS = 0x0008,	/* Version */
	NVME_PROPERTY_OFFSET_INTMS = 0x000c,	/* Interrupt Mask Set */
	NVME_PROPERTY_OFFSET_INTMC = 0x0010,	/* Interrupt Mask Clear */
	NVME_PROPERTY_OFFSET_CC = 0x0014,	/* Controller Configuration */
	NVME_PROPERTY_OFFSET_CSTS = 0x001c,	/* Controller Status */
	NVME_PROPERTY_OFFSET_NSSR = 0x0020,	/* NVM Subsystem Reset */
	NVME_PROPERTY_OFFSET_AQA = 0x0024,	/* Admin Queue Attributes */
	NVME_PROPERTY_OFFSET_ASQ = 0x0028,	/* Admin SQ Base Address */
	NVME_PROPERTY_OFFSET_ACQ = 0x0030,	/* Admin CQ Base Address */
	NVME_PROPERTY_OFFSET_CMBLOC = 0x0038,	/* Controller Memory Buffer Location */
	NVME_PROPERTY_OFFSET_CMBSZ = 0x003c,	/* Controller Memory Buffer Size */
	NVME_PROPERTY_OFFSET_DBS = 0x1000,	/* SQ 0 Tail Doorbell */
};


#define NVME_CAPABILITY_TIMEOUT_BIT_SHIFT 24
#define NVME_CAPABILITY_STRIDE_BIT_SHIFT  32
#define NVME_CAPABILITY_NSSRC_BIT_SHIFT   36
#define NVME_CAPABILITY_MPSMIN_BIT_SHIFT  48
#define NVME_CAPABILITY_MPSMAX_BIT_SHIFT  52


#define NVME_GET_CAP_MQES(Capability)       ((Capability) & 0xffff)
#define NVME_GET_CAP_READY_TIMEOUT(Capability)    (((Capability) >> NVME_CAPABILITY_TIMEOUT_BIT_SHIFT) & 0xff)
#define NVME_GET_CAP_STRIDE(Capability)	    (((Capability) >> NVME_CAPABILITY_STRIDE_BIT_SHIFT) & 0xf)
#define NVME_GET_CAP_NSSRC(Capability)      (((Capability) >> NVME_CAPABILITY_NSSRC_BIT_SHIFT) & 0x1)
#define NVME_GET_CAP_MPSMIN(Capability)	    (((Capability) >> NVME_CAPABILITY_MPSMIN_BIT_SHIFT) & 0xf)
#define NVME_GET_CAP_MPSMAX(Capability)	    (((Capability) >> NVME_CAPABILITY_MPSMAX_BIT_SHIFT) & 0xf)

typedef struct _NVME_CONTROLLER_CAPIBILITY {
	USHORT usMaxQEntSupported;
	USHORT usContQReq : 1;
	USHORT usArbMechSupp : 2;
	USHORT usResved1 : 5;
	USHORT usCtrlrEnTimeout : 8;
	USHORT usDoorbellStride : 4;
	USHORT usNVMeSSResetSupported : 1;
	USHORT usCmdSetSupported : 8;
	USHORT usBootPartitionSupported : 1;
	USHORT usResved2 : 2;
	USHORT usMemoryPgSizeMinimum : 4;
	USHORT usMemorPgSizeMaximum : 4;
	USHORT usPersistentMemoryRegSupported : 1;
	USHORT usCtrlrMemBufSupported : 1;
	USHORT usResved3 : 6;
} NVME_CONTROLLER_CAPIBILITY, * PNVME_CONTROLLER_CAPIBILITY;

#define NVME_CMB_BIR(cmbloc)	((cmbloc) & 0x7)
#define NVME_CMB_OFST(cmbloc)	(((cmbloc) >> 12) & 0xfffff)

enum {
	NVME_CMBSZ_SQS = 1 << 0,
	NVME_CMBSZ_CQS = 1 << 1,
	NVME_CMBSZ_LISTS = 1 << 2,
	NVME_CMBSZ_RDS = 1 << 3,
	NVME_CMBSZ_WDS = 1 << 4,

	NVME_CMBSZ_SZ_SHIFT = 12,
	NVME_CMBSZ_SZ_MASK = 0xfffff,

	NVME_CMBSZ_SZU_SHIFT = 8,
	NVME_CMBSZ_SZU_MASK = 0xf,
};

/*
 * Submission and Completion Queue Entry Sizes for the NVM command set.
 * (In bytes and specified as a power of two (2^n)).
 */
#define NVME_NVM_IOSQES		6
#define NVME_NVM_IOCQES		4

enum {
	NVME_CC_ENABLE = 1 << 0,
	NVME_CC_CSS_NVM = 0 << 4,
	NVME_CC_EN_SHIFT = 0,
	NVME_CC_CSS_SHIFT = 4,
	NVME_CC_MPS_SHIFT = 7,
	NVME_CC_AMS_SHIFT = 11,
	NVME_CC_SHN_SHIFT = 14,
	NVME_CC_IOSQES_SHIFT = 16,
	NVME_CC_IOCQES_SHIFT = 20,
	NVME_CC_AMS_RR = 0 << NVME_CC_AMS_SHIFT,
	NVME_CC_AMS_WRRU = 1 << NVME_CC_AMS_SHIFT,
	NVME_CC_AMS_VS = 7 << NVME_CC_AMS_SHIFT,
	NVME_CC_SHN_NONE = 0 << NVME_CC_SHN_SHIFT,
	NVME_CC_SHN_NORMAL = 1 << NVME_CC_SHN_SHIFT,
	NVME_CC_SHN_ABRUPT = 2 << NVME_CC_SHN_SHIFT,
	NVME_CC_SHN_MASK = 3 << NVME_CC_SHN_SHIFT,
	NVME_CC_IOSQES = NVME_NVM_IOSQES << NVME_CC_IOSQES_SHIFT,
	NVME_CC_IOCQES = NVME_NVM_IOCQES << NVME_CC_IOCQES_SHIFT,
	NVME_CSTS_RDY = 1 << 0,
	NVME_CSTS_CFS = 1 << 1,
	NVME_CSTS_NSSRO = 1 << 4,
	NVME_CSTS_PP = 1 << 5,
	NVME_CSTS_SHST_NORMAL = 0 << 2,
	NVME_CSTS_SHST_OCCUR = 1 << 2,
	NVME_CSTS_SHST_CMPLT = 2 << 2,
	NVME_CSTS_SHST_MASK = 3 << 2,
};

typedef struct _NVME_POWER_STATE_DESCRIPTOR {
	USHORT	usMaxPower;	 /* in CentiWatts */
	UCHAR	ucResved2;
	UCHAR	ucFlags;
	ULONG	ulEentryLat; /*in usecs */
	ULONG	ulExitLat;   /*in usecs */
	UCHAR	ucReadThroughput;
	UCHAR	ucReadLat;
	UCHAR	ucWriteThroughput;
	UCHAR	ucWriteLat;
	USHORT	usIdlePower;
	UCHAR	ucIdleScale;
	UCHAR	ucResved19;
	USHORT	usActivePower;
	UCHAR	ucActiveWorkScale;
	UCHAR	ucaResved23[9];
}NVME_POWER_STATE_DESCRIPTOR, * PNVME_POWER_STATE_DESCRIPTOR;

enum {
	NVME_PS_FLAGS_MAX_POWER_SCALE = 1 << 0,
	NVME_PS_FLAGS_NON_OP_STATE = 1 << 1,
};

enum nvme_ctrl_attr {
	NVME_CTRL_ATTR_HID_128_BIT = (1 << 0),
	NVME_CTRL_ATTR_TBKAS = (1 << 6),
};


#define NVME_IDENTIFY_MGMT_INTF_RESERVED_AT_OFFSET_240 13

typedef  struct _NVME_IDENTIFY_MGMT_INTERFACE_ATTR {
	UCHAR   ucaResvd240[NVME_IDENTIFY_MGMT_INTF_RESERVED_AT_OFFSET_240];
	UCHAR   ucNVMSubsystemReport;
	UCHAR   ucVPDWriteCycleInformation;
	UCHAR   ucMgmtEPCap;
}NVME_IDENTIFY_MGMT_INTERFACE_ATTR, * PNVME_IDENTIFY_MGMT_INTERFACE_ATTR;



#define NVMEOF_IDENTIFY_1_2_RESERVED_AT_OFFSET_1804		244

typedef  struct _NVMEOF_IDENTIFY_CONTROLLER_ATTR {
	ULONG   ulIOQueueCmdCapSupportedSz;
	ULONG   ulIOQueueRespCapSupportedSz;
	USHORT  usInCapDataOffset;
	UCHAR   ucCtrlrAttr;
	UCHAR   ucMaxSGLDataBlkDesc;
	UCHAR   ucaResvd1804[NVMEOF_IDENTIFY_1_2_RESERVED_AT_OFFSET_1804];
}NVMEOF_IDENTIFY_CONTROLLER_ATTR, * PNVMEOF_IDENTIFY_CONTROLLER_ATTR;


#define NVME_IDENTIFY_SERIAL_NUMBER_STRING_SZ			20
#define NVME_IDENTIFY_MODEL_NUMBER_STRING_SZ			40
#define NVME_IDENTIFY_FIRMWARE_VERSION_STRING_SZ		8
#define NVME_IDENTIFY_IEEE_OUI_SZ						3
#define NVME_IDENTIFY_RESERVED_AT_OFFSET_134			104
#define NVME_IDENTIFY_TOTAL_NVM_CAPACITY_BUF_SZ			16
#define NVME_IDENTIFY_UNALLOCATED_NVM_CAPACITY_BUF_SZ   16
#define NVME_IDENTIFY_RESERVED_AT_OFFSET_356			156
#define NVME_IDENTIFY_RESERVED_AT_OFFSET_534			2
#define NVME_IDENTIFY_RESERVED_AT_OFFSET_544			224
#define NVME_IDENTIFY_SUBNQN_BUFFER_SZ					256
#define NVME_IDENTIFY_RESERVED_AT_OFFSET_1024			768
#define NVME_IDENTIFY_POWER_STATE_DISCRIPTOR_SZ			32
#define NVME_IDENTIFY_VENDOR_SPECIFIC_BUFFER_SZ			1024

/*using NVME Base specification 1.4 for identify structure*/
typedef struct _NVME_IDENTIFY_CONTROLLER {
	/*	Controller Capabilities and Features	*/
	USHORT                            usVID;
	USHORT                            usSSVID;
	CHAR                              caSerialNumber[NVME_IDENTIFY_SERIAL_NUMBER_STRING_SZ];
	CHAR                              caModelNumber[NVME_IDENTIFY_MODEL_NUMBER_STRING_SZ];
	CHAR                              caFirmwareRevision[NVME_IDENTIFY_FIRMWARE_VERSION_STRING_SZ];
	UCHAR                             ucRecoArbiBurst;
	UCHAR                             ucaIEEE_OUI[NVME_IDENTIFY_IEEE_OUI_SZ];
	UCHAR                             ucaCtrlrMPathIONSSharingCap;
	UCHAR                             ucMaxDataXferSz;
	USHORT                            usCtrlrID;
	ULONG                             ulVersion;
	ULONG                             ulRTD3ResumeLatency;
	ULONG                             ulRTD3EntryLatency;
	ULONG                             ulOptAsyncEvtSupported;
	ULONG                             ulCtrlrAttr;
	USHORT                            usRdRecoLevelsSupported;
	UCHAR                             ucaResved102[9];
	UCHAR                             ucaCtrlrType;
	GUID                              sFRUGUID;
	USHORT                            usComdRetryDelayTime[3];
	UCHAR                             ucaResved100[NVME_IDENTIFY_RESERVED_AT_OFFSET_134];
	NVME_IDENTIFY_MGMT_INTERFACE_ATTR sNvmeMgmtIfAttr;
	/*	Admin Command Set Attributes & Optional Controller Capabilities */
	USHORT                            usOptAdmCmdSupport;
	UCHAR                             ucAbortCmdLimit;
	UCHAR                             ucAsyncEventReqLimit;
	UCHAR                             ucFirmwareUpdates;
	UCHAR                             ucLogPageAttr;
	UCHAR                             ucErrLogPageEntries;
	UCHAR                             ucNumPowerStateSupport;
	UCHAR                             ucAdminVendorSpecCmdCfg;
	UCHAR                             ucAutoPowerStateXsitionAttr;
	USHORT							  usWarnCompTempThresh;
	USHORT                            usCriticalCompTempThresh;
	USHORT                            usMaxTimeForFirmwareActivatio;
	ULONG                             ulHostMemBufPreSize;
	ULONG                             ulHostMemBufMinSize;
	UCHAR                             ucaTotalNVMCap[NVME_IDENTIFY_TOTAL_NVM_CAPACITY_BUF_SZ];
	UCHAR                             ucaUnallocNVMCap[NVME_IDENTIFY_UNALLOCATED_NVM_CAPACITY_BUF_SZ];
	ULONG                             ulReplyProtMemBlkSupport;
	USHORT                            usExtDevSelfTestTime;
	UCHAR                             ucDevSelfTestOptions;
	UCHAR                             ucFirmwareUpdateGran;
	USHORT                            usKeepAliveSupport;
	USHORT                            usHostCtrledThermMgmtAttr;
	USHORT                            usMinThermMgmtTemp;
	USHORT                            usMaxThermMgmtTemp;
	ULONG                             ulSanitizeCaps;
	ULONG                             ulHostMemBufMinDescEntrySz;
	USHORT                            usHostMemBufMaxDescEntries;
	USHORT                            usNVMSetIdMax;
	USHORT                            usEnduranceGrpIdMax;
	UCHAR                             ucANAXitionTime;
	UCHAR                             ucAsymNSAccessCap;
	ULONG                             ulANAGrpIdMax;
	ULONG                             ulNumANAGrpId;
	ULONG                             ulPersistentEvtLogSz;
	UCHAR                             ucaResved352[NVME_IDENTIFY_RESERVED_AT_OFFSET_356];
	/*	NVME Command set Attributes		*/
	UCHAR                             ucSubmissionQEntSz;
	UCHAR                             ucCompletionQEntSz;
	USHORT                            usMadOSCmd;
	ULONG                             ulNumberOfNSCnt;
	USHORT                            usOptionalNVMCmdSupport;
	USHORT                            usFusedOpsSupport;
	UCHAR                             ucFormatNVMAttr;
	UCHAR                             ucVolatileWriteCache;
	USHORT                            usAtomicWriteUnitNormal;
	USHORT                            usAtomicWriteUnitPowFail;
	UCHAR                             ucNVMVendorSpecCmdCfg;
	UCHAR                             ucNameSpaceWrProtCap;
	USHORT                            usAtomicCmpWriteUnit;
	UCHAR                             ucaResved534[NVME_IDENTIFY_RESERVED_AT_OFFSET_534];
	ULONG                             ulSGLSupport;
	ULONG                             ulMaxNumAllowedNS;
	UCHAR                             ucaResved544[NVME_IDENTIFY_RESERVED_AT_OFFSET_544];
	CHAR                              caSubNQN[NVME_IDENTIFY_SUBNQN_BUFFER_SZ];
	UCHAR                             ucaResved1024[NVME_IDENTIFY_RESERVED_AT_OFFSET_1024];
	/*	NVMEOF Controller Attributes		*/
	NVMEOF_IDENTIFY_CONTROLLER_ATTR   sNVMeOFControllerAttr;
	/*	Power State Descriptors			*/
	NVME_POWER_STATE_DESCRIPTOR		  saPowerStateDesc[NVME_IDENTIFY_POWER_STATE_DISCRIPTOR_SZ];
	/*	Vendor Specific					*/
	UCHAR							  ucaVendorSpecific[NVME_IDENTIFY_VENDOR_SPECIFIC_BUFFER_SZ];
}NVME_IDENTIFY_CONTROLLER, * PNVME_IDENTIFY_CONTROLLER;

enum {
	NVME_CTRL_ONCS_COMPARE = 1 << 0,
	NVME_CTRL_ONCS_WRITE_UNCORRECTABLE = 1 << 1,
	NVME_CTRL_ONCS_DSM = 1 << 2,
	NVME_CTRL_ONCS_WRITE_ZEROES = 1 << 3,
	NVME_CTRL_ONCS_TIMESTAMP = 1 << 6,
	NVME_CTRL_VWC_PRESENT = 1 << 0,
	NVME_CTRL_OACS_SEC_SUPP = 1 << 0,
	NVME_CTRL_OACS_DIRECTIVES = 1 << 5,
	NVME_CTRL_OACS_DBBUF_SUPP = 1 << 8,
	NVME_CTRL_LPA_CMD_EFFECTS_LOG = 1 << 1,
};

typedef struct _NVME_LBA_FORMAT {
	USHORT			usMetaDataSize;
	UCHAR			ucLBADataSize;
	UCHAR			ucRelativePerformance;
} NVME_LBA_FORMAT, * PNVME_LBA_FORMAT;

typedef struct _NVME_IDENTYFY_NAMESPACE {
	UINT64          ullNameSpaceSize;
	UINT64          ullNameSpaceCapacity;
	UINT64          ullNameSpaceUtilization;
	UCHAR           ucNameSpaceFeatures;
	UCHAR           ucNumLBAFormat;
	UCHAR           ucFormattedLBASize;
	UCHAR           ucMetadataCapabilities;
	UCHAR           ucEnd2endDataProtactionCapabilities;
	UCHAR           ucEnd2endDataProtactionTypeSettings;
	UCHAR           ucNameSpaceMPIOAndNSShCapabilities;
	UCHAR           ucResevervationCapabilities;
	UCHAR           ucFormatProgressIndicator;
	UCHAR           ucDeallocateLBFeature;
	USHORT          usNameSpaceAtomicWriteUnitNormal;
	USHORT          usNameSpaceAtomicWriteUnitPowerFail;
	USHORT          usNameSpaceAtomicCmpAndWriteUnit;
	USHORT          usNameSpaceAtomicBoundarySizeNormal;
	USHORT          usNameSpaceAtomicBoundaryOffset;
	USHORT          usNameSpaceAtomicBoundarySizePwrFail;
	USHORT          usNameSpaceOptimalBoundary;
	UCHAR           ucaNVMCapacity[16];
	USHORT          usNameSpacePreferredWrGranularity;
	USHORT          usNameSpacePreferredWrAlignment;
	USHORT          usNameSpacePreferredDeallocGranularity;
	USHORT          usNameSpacePreferredDeallocAlignment;
	USHORT          usNameSpaceOptimalWriteSize;
	UCHAR           ucaResvd74[18];
	ULONG           ulANAGroupIdentifier;
	UCHAR           ucaResved96[3];
	UCHAR           ucNameSpaceAttr;
	USHORT          usNVMSetIdentifier;
	USHORT          usEnduranceGroupId;
	UCHAR           ucaNameSpaceGUID[16];
	UCHAR           ucaIEEEExtendedUniqueIdentifier[8];
	NVME_LBA_FORMAT saLBAFormat[16];
	UCHAR           ucaResved192[192];
	UCHAR           ucaVendorSpecific[3712];
} NVME_IDENTYFY_NAMESPACE, * PNVME_IDENTYFY_NAMESPACE;

typedef enum _NVME_IDENTIFY_CNS_SUBCOMMAND {
	NVME_IDENTIFY_CNS_NS = 0x00,
	NVME_IDENTIFY_CNS_CTRL = 0x01,
	NVME_IDENTIFY_CNS_NS_ACTIVE_LIST = 0x02,
	NVME_IDENTIFY_CNS_NS_DESC_LIST = 0x03,
	NVME_IDENTIFY_CNS_NS_PRESENT_LIST = 0x10,
	NVME_IDENTIFY_CNS_NS_PRESENT = 0x11,
	NVME_IDENTIFY_CNS_CTRL_NS_LIST = 0x12,
	NVME_IDENTIFY_CNS_CTRL_LIST = 0x13,
} NVME_IDENTIFY_CNS_SUBCOMMAND, * PNVME_IDENTIFY_CNS_SUBCOMMAND;

typedef enum _NVME_DIRECTIVE_SUB_COMMAND {
	NVME_DIRECTIVE_IDENTIFY = 0x00,
	NVME_DIRECTIVE_STREAMS = 0x01,
	NVME_DIRECTIVE_SND_ID_OP_ENABLE = 0x01,
	NVME_DIRECTIVE_SND_ST_OP_REL_ID = 0x01,
	NVME_DIRECTIVE_SND_ST_OP_REL_RSC = 0x02,
	NVME_DIRECTIVE_RCV_ID_OP_PARAM = 0x01,
	NVME_DIRECTIVE_RCV_ST_OP_PARAM = 0x01,
	NVME_DIRECTIVE_RCV_ST_OP_STATUS = 0x02,
	NVME_DIRECTIVE_RCV_ST_OP_RESOURCE = 0x03,
	NVME_DIRECTIVE_ENDIR = 0x01,
} NVME_DIRECTIVE_SUB_COMMAND, * PNVME_DIRECTIVE_SUB_COMMAND;

enum {
	NVME_NS_FEAT_THIN = 1 << 0,
	NVME_NS_FLBAS_LBA_MASK = 0xf,
	NVME_NS_FLBAS_META_EXT = 0x10,
	NVME_LBAF_RP_BEST = 0,
	NVME_LBAF_RP_BETTER = 1,
	NVME_LBAF_RP_GOOD = 2,
	NVME_LBAF_RP_DEGRADED = 3,
	NVME_NS_DPC_PI_LAST = 1 << 4,
	NVME_NS_DPC_PI_FIRST = 1 << 3,
	NVME_NS_DPC_PI_TYPE3 = 1 << 2,
	NVME_NS_DPC_PI_TYPE2 = 1 << 1,
	NVME_NS_DPC_PI_TYPE1 = 1 << 0,
	NVME_NS_DPS_PI_FIRST = 1 << 3,
	NVME_NS_DPS_PI_MASK = 0x7,
	NVME_NS_DPS_PI_TYPE1 = 1,
	NVME_NS_DPS_PI_TYPE2 = 2,
	NVME_NS_DPS_PI_TYPE3 = 3,
};

typedef struct _NVME_NAMESPACE_ID_DESCRIPTOR {
	UCHAR  ucNSIdt;
	UCHAR  ucNSIdl;
	USHORT usReserved;
}NVME_NAMESPACE_ID_DESCRIPTOR, * PNVME_NAMESPACE_ID_DESCRIPTOR;

#define NVME_NIDT_EUI64_LEN	8
#define NVME_NIDT_NGUID_LEN	16
#define NVME_NIDT_UUID_LEN	16

enum {
	NVME_NIDT_EUI64 = 0x01,
	NVME_NIDT_NGUID = 0x02,
	NVME_NIDT_UUID = 0x03,
};

typedef struct _NVME_SMART_LOG {
	UCHAR       ucCriticalWarning;
	UCHAR       ucaTemperature[2];
	UCHAR       ucAvailableSpare;
	UCHAR       ucSpareThreshold;
	UCHAR       ucPercentUsed;
	UCHAR       ucaResvd6[26];
	UCHAR       ucaDataUnitsRead[16];
	UCHAR       ucaDataUnitsWritten[16];
	UCHAR       ucaHostReads[16];
	UCHAR       ucaHostWrites[16];
	UCHAR       ucaCtrlBusyTime[16];
	UCHAR       ucaPowerCycles[16];
	UCHAR       ucaPowerOnHours[16];
	UCHAR       ucaUnsafeShutdowns[16];
	UCHAR       ucaMediaErrors[16];
	UCHAR       ucaNumErrLogEntries[16];
	ULONG       ulWarningTempTime;
	ULONG       ulCriticalCompTime;
	USHORT      usaTempSensor[8];
	UCHAR       ucaResvd216[296];
} NVME_SMART_LOG, * PNVME_SMART_LOG;

typedef struct _NVME_FIRMWARE_SLOT_INFO_LOG {
	UCHAR    ucAfi;
	UCHAR    ucaResvd1[7];
	UINT64   ullFRS[7];
	UCHAR    ucaResvd64[448];
} NVME_FIRMWARE_SLOT_INFO_LOG, * PNVME_FIRMWARE_SLOT_INFO_LOG;

enum {
	NVME_CMD_EFFECTS_CSUPP = 1 << 0,
	NVME_CMD_EFFECTS_LBCC = 1 << 1,
	NVME_CMD_EFFECTS_NCC = 1 << 2,
	NVME_CMD_EFFECTS_NIC = 1 << 3,
	NVME_CMD_EFFECTS_CCC = 1 << 4,
	NVME_CMD_EFFECTS_CSE_MASK = 3 << 16,
};

typedef struct _NVME_EFFECT_LOG {
	ULONG   ulaAcs[256];
	ULONG   ulaIOCs[256];
	UCHAR   ucaResvd[2048];
}NVME_EFFECT_LOG, * PNVME_EFFECT_LOG;

typedef enum _NVME_ANA_STATE {
	NVME_ANA_STATE_OPTIMIZED = 0x01,
	NVME_ANA_STATE_NONOPTIMIZED = 0x02,
	NVME_ANA_STATE_INACCESSIBLE = 0x03,
	NVME_ANA_STATE_PERSISTENT_LOSS = 0x04,
	NVME_ANA_STATE_CHANGE = 0x0f,
}NVME_ANA_STATE, * PNVME_ANA_STATE;

typedef struct _NVME_ANA_GROUP_DESCRIPTOR {
	ULONG     ulGroupID;
	ULONG     ulNSIds;
	ULONGLONG ullChangeCount;
	UCHAR     ucState;
	UCHAR     ucaResvd17[15];
	ULONG     ulaNSIds[0];
} NVME_ANA_GROUP_DESCRIPTOR, * PNVME_ANA_GROUP_DESCRIPTOR;

/* flag for the log specific field of the ANA log */
#define NVME_ANA_LOG_RGO	(1 << 0)

typedef struct _NVME_ANA_ACCESS_LOG_PAGE {
	ULONGLONG  ullChageCount;
	USHORT     usANAGroupDescCount;
	UCHAR      ucaReserved[6];
	UCHAR      ucaANAGroupDescriptor[0];
} NVME_ANA_ACCESS_LOG_PAGE, * PNVME_ANA_ACCESS_LOG_PAGE;

typedef struct _NVME_NS_ANA_GROUP_DESC {
	ULONG     ulGroupID;
	ULONG     ulNSIds;
	ULONGLONG ullChangeCount;
	UCHAR     ucState;
	UCHAR     ucaResvd17[15];
	ULONG     ulaNSId;
} NVME_NS_ANA_GROUP_DESC, * PNVME_NS_ANA_GROUP_DESC;


enum {
	NVME_SMART_LOG_CRIT_SPARE = 1 << 0,
	NVME_SMART_LOG_CRIT_TEMPERATURE = 1 << 1,
	NVME_SMART_LOG_CRIT_RELIABILITY = 1 << 2,
	NVME_SMART_LOG_CRIT_MEDIA = 1 << 3,
	NVME_SMART_LOG_CRIT_VOLATILE_MEMORY = 1 << 4,
};

enum {
	NVME_AER_NOTICE_ERROR = 0,
	NVME_AER_NOTICE_SMART = 1,
	NVME_AER_NOTICE_EVENT = 2,
	NVME_AER_NOTICE_CSS = 6,
	NVME_AER_NOTICE_VS = 7,
};

enum {
	NVMEOF_AER_NOTIFY_NS_CHANGED = 0x00,
	NVMEOF_AER_NOTIFY_FW_ACT_STARTING = 0x01,
	NVMEOF_AER_NOTIFY_ANA = 0x03,
	NVMEOF_AER_NOTIFY_DISC_CHANGED = 0xf0,
};

typedef enum _ASYNC_EVENTS_BIT_POS {
	NVME_AEN_BIT_POS_NSATTR = 8,
	NVME_AEN_BIT_POS_FW_ACT = 9,
	NVME_AEN_BIT_POS_ANA_CHANGE = 11,
	NVME_AEN_BIT_POS_DISC_CHANGE = 31,
}ASYNC_EVENTS_BIT_POS, * PASYNC_EVENTS_BIT_POS;

typedef enum _ENUM_ASYNC_EVENTS {
	NVME_ASYNC_EVENT_CONFIG_NS_ATTR = 1UL << NVME_AEN_BIT_POS_NSATTR,
	NVME_ASYNC_EVENT_CONFIG_FW_ACT = 1UL << NVME_AEN_BIT_POS_FW_ACT,
	NVME_ASYNC_EVENT_CONFIG_ANA_CHANGE = 1UL << NVME_AEN_BIT_POS_ANA_CHANGE,
	NVME_ASYNC_EVENT_CONFIG_DISC_CHANGE = 1UL << NVME_AEN_BIT_POS_DISC_CHANGE,
} ENUM_ASYNC_EVENTS, * PENUM_ASYNC_EVENTS;

#define NVME_SUPPORTED_ASYNC_EVENT \
	(NVME_ASYNC_EVENT_CONFIG_NS_ATTR | NVME_ASYNC_EVENT_CONFIG_FW_ACT | NVME_ASYNC_EVENT_CONFIG_ANA_CHANGE | NVME_ASYNC_EVENT_CONFIG_DISC_CHANGE)


typedef struct _NVME_LBA_RANGE_TYPE {
	UCHAR    ucType;
	UCHAR    ucAttributes;
	UCHAR    ucaResved2[14];
	UINT64   ullStartLBA;
	UINT64   ullEndLBA;
	UCHAR    ucaGUID[16];
	UCHAR    ucaResved48[16];
} NVME_LBA_RANGE_TYPE, * PNVME_LBA_RANGE_TYPE;

enum {
	NVME_LBART_TYPE_FS = 0x01,
	NVME_LBART_TYPE_RAID = 0x02,
	NVME_LBART_TYPE_CACHE = 0x03,
	NVME_LBART_TYPE_SWAP = 0x04,

	NVME_LBART_ATTRIB_TEMP = 1 << 0,
	NVME_LBART_ATTRIB_HIDE = 1 << 1,
};

typedef struct _NVME_RESERVATION_STATUS {
	ULONG	gen;
	UCHAR	rtype;
	UCHAR	regctl[2];
	UCHAR	Resvd5[2];
	UCHAR	ptpls;
	UCHAR	Resvd10[13];
	struct {
		USHORT	cntlid;
		UCHAR	rcsts;
		UCHAR	Resvd3[5];
		UINT64	hostid;
		UINT64	rkey;
	} regctl_ds[0];
} NVME_RESERVATION_STATUS, * PNVME_RESERVATION_STATUS;

enum nvme_async_event_type {
	NVME_AER_TYPE_ERROR = 0,
	NVME_AER_TYPE_SMART = 1,
	NVME_AER_TYPE_NOTICE = 2,
};

/* I/O commands */
typedef enum _NVME_IO_OPCODE {
	NVME_IO_CMD_FLUSH = 0x00,
	NVME_IO_CMD_WRITE = 0x01,
	NVME_IO_CMD_READ = 0x02,
	NVME_IO_CMD_WRITE_UNCORRECTABLE = 0x04,
	NVME_IO_CMD_COMPARE = 0x05,
	NVME_IO_CMD_WRITE_ZEROES = 0x08,
	NVME_IO_CMD_DATA_SET_MANAGEMENT = 0x09,
	NVME_IO_CMD_RESERVATION_REGISTER = 0x0d,
	NVME_IO_CMD_RESERVATION_REPORT = 0x0e,
	NVME_IO_CMD_RESERVATION_ACQUIRE = 0x11,
	NVME_IO_CMD_RESERVATION_RELEASE = 0x15,
}NVME_IO_OPCODE, * PNVME_IO_OPCODE;

static PCHAR  pcaNVMeIOCmd[] = {
	"NVMe Flush",
	"NVMe Write",
	"NVMe Read",
	"Unknown3",
	"NVMe Write Uncorrectable",
	"NVMe Compare",
	"Unknown6",
	"Unknown7",
	"NVMe Write Zero",
	"NVMe Data Set Mgmt",
	"UnknownA",
	"UnknownB",
	"UnknownC",
	"NVMe Reservation Register",
	"NVMe Reservation Report",
	"UnknownF",
	"Unknown10",
	"NVMe Reservation Acquire",
	"Unknown12",
	"Unknown13",
	"Unknown14",
	"NVMe Reservation Release",
};

#define NVME_IS_IO_CMD(psNVMeCmd)  ((((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_FLUSH) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_WRITE) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_READ)  || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_WRITE_UNCORRECTABLE) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_COMPARE) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_WRITE_ZEROES) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_DATA_SET_MANAGEMENT) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_RESERVATION_REGISTER) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_RESERVATION_REPORT) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_RESERVATION_ACQUIRE) || \
                                   (((PNVME_READ_WRITE_COMMAND)psNVMeCmd)->ucOpCode == NVME_IO_CMD_RESERVATION_RELEASE))

/*
 * Descriptor subtype - lower 4 bits of nvme_(keyed_)sgl_desc identifier
 *
 * @NVME_SGL_FMT_ADDRESS:     absolute address of the data block
 * @NVME_SGL_FMT_OFFSET:      relative Offset of the in-capsule data block
 * @NVME_SGL_FMT_TRANSPORT_A: transport defined format, value 0xA
 * @NVME_SGL_FMT_INVALIDATE:  RDMA transport specific remote invalidation
 *                            request subtype
 */
enum {
	NVME_SGL_FMT_ADDRESS = 0x00,
	NVME_SGL_FMT_OFFSET = 0x01,
	NVME_SGL_FMT_TRANSPORT_A = 0x0A,
	NVME_SGL_FMT_INVALIDATE = 0x0f,
};

typedef enum _NVME_SGL_DESCRIPTOR_TYPE {
	NVME_SGL_DESCRIPTOR_DATA,
	NVME_SGL_DESCRIPTOR_BIT_BUCKET,
	NVME_SGL_DESCRIPTOR_SEGMENT,
	NVME_SGL_DESCRIPTOR_LAST_SEGMENT,
	NVME_KEYED_SGL_DESCRIPTOR_DATA,
	NVME_SGL_DESCRIPTOR_TRANSPORT_DATA = 0x5,
	NVME_SGL_DESCRIPTOR_VENDOR_SPECIFIC = 0xF,
} NVME_SGL_DESCRIPTOR_TYPE, * PNVME_SGL_DESCRIPTOR_TYPE;

typedef struct _NVME_SGL_TYPE {
	UCHAR SGLDescSubType : 4;
	UCHAR SGLDescType : 4;
} NVME_SGL_TYPE, * PNVME_SGL_TYPE;

typedef struct _NVME_SGL_DATA_BLOCK_DESCRIPTOR {
	UINT64  Address;
	ULONG	Length;
	UCHAR	Rsvd[3];
	UCHAR	SGLType;
} NVME_SGL_DATA_BLOCK_DESCRIPTOR, * PNVME_SGL_DATA_BLOCK_DESCRIPTOR;

typedef struct _NVME_SGL_BIT_BUCKET_DESCRIPTOR {
	UCHAR   Rsvd1[8];
	ULONG	Length;
	UCHAR	Rsvd2[3];
	UCHAR	SGLType;
} NVME_SGL_BIT_BUCKET_DESCRIPTOR, * PNVME_SGL_BIT_BUCKET_DESCRIPTOR;

typedef struct _NVME_SGL_SEGMENT_DESCRIPTOR {
	UINT64  Address;
	ULONG	Length;
	UCHAR	Rsvd1[3];
	UCHAR	SGLType;
} NVME_SGL_SEGMENT_DESCRIPTOR, * PNVME_SGL_SEGMENT_DESCRIPTOR;

typedef struct _NVME_SGL_LAST_SEGMENT_DESCRIPTOR {
	UINT64  Address;
	ULONG	Length;
	UCHAR	Rsvd1[3];
	UCHAR	SGLType;
} NVME_SGL_LAST_SEGMENT_DESCRIPTOR, * PNVME_SGL_LAST_SEGMENT_DESCRIPTOR;

typedef struct _NVME_KEYED_SGL_DATA_BLOCK_DESCRIPTOR {
	UINT64	Address;
	UCHAR	ucaLength[3];
	UCHAR	Key[4];
	UCHAR	SGLType;
} NVME_KEYED_SGL_DATA_BLOCK_DESCRIPTOR, * PNVME_KEYED_SGL_DATA_BLOCK_DESCRIPTOR;

typedef struct _PHYSICAL_REGION_PAGE_ENTRY {
	UINT64 PRP1;
	UINT64 PRP2;
} PHYSICAL_REGION_PAGE_ENTRY, * PPHYSICAL_REGION_PAGE_ENTRY;

typedef union _NVME_DATA_BUFFER_LIST {
	PHYSICAL_REGION_PAGE_ENTRY		     PRP;
	NVME_SGL_DATA_BLOCK_DESCRIPTOR		 DataBlockDiscriptor;
	NVME_SGL_BIT_BUCKET_DESCRIPTOR		 BitBucketDiscriptor;
	NVME_SGL_SEGMENT_DESCRIPTOR			 SegmentDiscriptor;
	NVME_SGL_LAST_SEGMENT_DESCRIPTOR	 LastSegmentDiscriptor;
	NVME_KEYED_SGL_DATA_BLOCK_DESCRIPTOR KeyedSGLDataBlkDiscriptor;
} NVME_DATA_BUFFER_LIST, * PNVME_DATA_BUFFER_LIST;

enum _FUSE_OPS {
	NVME_CMD_FUSE_FIRST = (1 << 0),
	NVME_CMD_FUSE_SECOND = (1 << 1),
};

enum _META_BUFF_FLAGS {
	NVME_CMD_SGL_METABUF = (1 << 0),
	NVME_CMD_SGL_METASEG = (1 << 1),
};

typedef struct _NVME_CMD_OPTIONS {
	UCHAR ucFusedOps : 2;
	UCHAR ucResvd : 4;
	UCHAR ucRprOrSglForXfer : 2;
} NVME_CMD_OPTIONS, * PNVME_CMD_OPTIONS;

typedef union _NVME_CMD_FLAGS {
	NVME_CMD_OPTIONS sOptions;
	UCHAR            ucFlags;
} NVME_CMD_FLAGS, * PNVME_CMD_FLAGS;

typedef struct _NVME_CMD_WORD_0 {
	UCHAR          ucOpCode;
	NVME_CMD_FLAGS sFlags;
	USHORT         usCommandID;
} NVME_CMD_WORD_0, * PNVME_CMD_WORD_0;

typedef struct _NVME_CMD_WORD_2_3 {
	ULONG ulaReserved[2];
} NVME_CMD_WORD_2_3, * PNVME_CMD_WORD_2_3;

typedef struct _NVME_CMD_WORD_4_5 {
	UINT64 ullMetaDataPointer;
} NVME_CMD_WORD_4_5, * PNVME_CMD_WORD_4_5;

typedef union _NVME_CMD_WORD_5678 {
	UINT64 ullaDataPointer[2];
} NVME_CMD_WORD_5678, * PNVME_CMD_WORD_5678;


typedef struct _NVME_GENERIC_COMMAND {
	NVME_CMD_WORD_0       sCDW0;
	ULONG                 ulNSId;
	NVME_CMD_WORD_2_3     sCDW2_3;
	NVME_CMD_WORD_4_5     sCDW4_5;
	NVME_DATA_BUFFER_LIST sDataPtr;
	ULONG			      ulCDW10;
	ULONG			      ulCDW11;
	ULONG			      ulCDW12;
	ULONG			      ulCDW13;
	ULONG			      ulCDW14;
	ULONG			      ulCDW15;
}NVME_GENERIC_COMMAND, * PNVME_GENERIC_COMMAND;

enum {
	NVME_RW_CTRL_FLAG_LR = 1 << 15,
	NVME_RW_CTRL_FLAG_FUA = 1 << 14,
	NVME_RW_CTRL_FLAG_DSM_FREQ_UNSPEC = 0,
	NVME_RW_CTRL_FLAG_DSM_FREQ_TYPICAL = 1,
	NVME_RW_CTRL_FLAG_DSM_FREQ_RARE = 2,
	NVME_RW_CTRL_FLAG_DSM_FREQ_READS = 3,
	NVME_RW_CTRL_FLAG_DSM_FREQ_WRITES = 4,
	NVME_RW_CTRL_FLAG_DSM_FREQ_RW = 5,
	NVME_RW_CTRL_FLAG_DSM_FREQ_ONCE = 6,
	NVME_RW_CTRL_FLAG_DSM_FREQ_PREFETCH = 7,
	NVME_RW_CTRL_FLAG_DSM_FREQ_TEMP = 8,
	NVME_RW_CTRL_FLAG_DSM_LATENCY_NONE = 0 << 4,
	NVME_RW_CTRL_FLAG_DSM_LATENCY_IDLE = 1 << 4,
	NVME_RW_CTRL_FLAG_DSM_LATENCY_NORM = 2 << 4,
	NVME_RW_CTRL_FLAG_DSM_LATENCY_LOW = 3 << 4,
	NVME_RW_CTRL_FLAG_DSM_SEQ_REQ = 1 << 6,
	NVME_RW_CTRL_FLAG_DSM_COMPRESSED = 1 << 7,
	NVME_RW_CTRL_FLAG_PRINFO_PRCHK_REF = 1 << 10,
	NVME_RW_CTRL_FLAG_PRINFO_PRCHK_APP = 1 << 11,
	NVME_RW_CTRL_FLAG_PRINFO_PRCHK_GUARD = 1 << 12,
	NVME_RW_CTRL_FLAG_PRINFO_PRACT = 1 << 13,
	NVME_RW_CTRL_FLAG_DTYPE_STREAMS = 1 << 4,
};

typedef struct _NVME_RW_CONTROL_FLAGS {
	USHORT usReserved16 : 8;
	USHORT usStorageTagCheck : 1;
	USHORT usReserved25 : 1;
	USHORT usProtectionInfo : 4;
	USHORT usFUA : 1;
	USHORT usLR : 1;
}NVME_RW_CONTROL_FLAGS, * PNVME_RW_CONTROL_FLAGS;

typedef union _NVME_RW_CTRL_FLAGS {
	USHORT usControlWord;
	NVME_RW_CONTROL_FLAGS sControlFlags;
}NVME_RW_CTRL_FLAGS, * PNVME_RW_CTRL_FLAGS;


typedef struct _NVME_READ_WRITE_COMMAND {
	UCHAR                   ucOpCode;
	UCHAR                   ucFlags;
	USHORT                  usCommandId;
	ULONG                   ulNSId;
	UINT64                  ullResved2;
	UINT64                  ullMetadata;
	NVME_DATA_BUFFER_LIST	sDataPtr;
	UINT64                  ullStartLBA;
	USHORT                  usLength;
	NVME_RW_CTRL_FLAGS      sControl;
	ULONG                   ulDataSetMgmt;
	ULONG                   ulExpectedInitRefTag;
	USHORT                  usExpectedLBAppTagMask;
	USHORT                  usExpectedLBAppTag;
} NVME_READ_WRITE_COMMAND, * PNVME_READ_WRITE_COMMAND;

typedef struct _NVME_DSM_CMD {
	UCHAR                   ucOpCode;
	UCHAR                   ucFlags;
	USHORT                  usCommandId;
	ULONG                   ulNSId;
	UINT64                  ullaResvd2[2];
	NVME_DATA_BUFFER_LIST	sDataPtr;
	ULONG					ulNumReq;
	ULONG					ulAttribs;
	ULONG					ulResvd12[4];
} NVME_DSM_CMD, * PNVME_DSM_CMD;

enum {
	NVME_DSMGMT_IDR = 1 << 0,
	NVME_DSMGMT_IDW = 1 << 1,
	NVME_DSMGMT_AD = 1 << 2,
};

#define NVME_DSM_MAX_RANGES	256

struct NVME_DATA_SET_MGMT_RANGE {
	ULONG   ulCAttr;
	ULONG   ulNLB;
	UINT64  ullStartLBA;
};

typedef struct _NVME_WRITE_ZERO_COMMAND {
	UCHAR                   ucOpCode;
	UCHAR                   ucFlags;
	USHORT                  usCommandId;
	ULONG                   ulNSId;
	UINT64                  ullResvd2;
	UINT64                  ullMetadata;
	NVME_DATA_BUFFER_LIST   sDataPtr;
	UINT64                  ullStartLBA;
	USHORT                  usLength;
	USHORT                  usCtrl;
	ULONG                   ulDSMgmt;
	ULONG                   ulRefTag;
	USHORT                  usAppTag;
	USHORT                  usAppMask;
} NVME_WRITE_ZERO_COMMAND, * PNVME_WRITE_ZERO_COMMAND;

/* Features */

struct nvme_feat_auto_pst {
	UINT64 entries[32];
};

enum {
	NVME_HOST_MEM_ENABLE = (1 << 0),
	NVME_HOST_MEM_RETURN = (1 << 1),
};

struct nvme_feat_host_behavior {
	UCHAR acre;
	UCHAR Resvd1[511];
};

enum {
	NVME_ENABLE_ACRE = 1,
};

/* Admin commands */

typedef enum _NVME_ADMIN_OPCODE {
	NVME_ADMIN_OPCODE_DELETE_SQ = 0x00,
	NVME_ADMIN_OPCODE_CREATE_SQ = 0x01,
	NVME_ADMIN_OPCODE_GET_LOG_PAGE = 0x02,
	NVME_ADMIN_OPCODE_DELETE_CQ = 0x04,
	NVME_ADMIN_OPCODE_CREATE_CQ = 0x05,
	NVME_ADMIN_OPCODE_IDENTIFY = 0x06,
	NVME_ADMIN_OPCODE_ABORT_COMMAND = 0x08,
	NVME_ADMIN_OPCODE_SET_FEATURES = 0x09,
	NVME_ADMIN_OPCODE_GET_FEATURES = 0x0a,
	NVME_ADMIN_OPCODE_ASYNC_EVENT = 0x0c,
	NVME_ADMIN_OPCODE_NS_MGMT = 0x0d,
	NVME_ADMIN_OPCODE_ACTIVATE_FW = 0x10,
	NVME_ADMIN_OPCODE_DOWNLOAD_FW = 0x11,
	NVME_ADMIN_OPCODE_NS_ATTACH = 0x15,
	NVME_ADMIN_OPCODE_KEEP_ALIVE = 0x18,
	NVME_ADMIN_OPCODE_DIRECTIVE_SEND = 0x19,
	NVME_ADMIN_OPCODE_DIRECTIVE_RECEIVE = 0x1a,
	NVME_ADMIN_OPCODE_DBBUF = 0x7C,
	NVME_ADMIN_OPCODE_FORMAT_NVM = 0x80,
	NVME_ADMIN_OPCODE_SECURITY_SEND = 0x81,
	NVME_ADMIN_OPCODE_SECURITY_RECEIVE = 0x82,
	NVME_ADMIN_OPCODE_SANITIZE_NVM = 0x84,
} NVME_ADMIN_OPCODE, * PNVME_ADMIN_OPCODE;

enum {
	NVME_QUEUE_PHYS_CONTIG = (1 << 0),
	NVME_CQ_IRQ_ENABLED = (1 << 1),
	NVME_SQ_PRIO_URGENT = (0 << 1),
	NVME_SQ_PRIO_HIGH = (1 << 1),
	NVME_SQ_PRIO_MEDIUM = (2 << 1),
	NVME_SQ_PRIO_LOW = (3 << 1),
	NVME_FEATURE_ARBITRATION = 0x01,
	NVME_FEATURE_POWER_MGMT = 0x02,
	NVME_FEATURE_LBA_RANGE = 0x03,
	NVME_FEATURE_TEMP_THRESH = 0x04,
	NVME_FEATURE_ERR_RECOVERY = 0x05,
	NVME_FEATURE_VOLATILE_WC = 0x06,
	NVME_FEATURE_NUM_QUEUES = 0x07,
	NVME_FEATURE_IRQ_COALESCE = 0x08,
	NVME_FEATURE_IRQ_CONFIG = 0x09,
	NVME_FEATURE_WRITE_ATOMIC = 0x0a,
	NVME_FEATURE_ASYNC_EVENT = 0x0b,
	NVME_FEATURE_AUTO_PST = 0x0c,
	NVME_FEATURE_HOST_MEM_BUF = 0x0d,
	NVME_FEATURE_TIMESTAMP = 0x0e,
	NVME_FEATURE_KATO = 0x0f,
	NVME_FEATURE_HCTM = 0x10,
	NVME_FEATURE_NOPSC = 0x11,
	NVME_FEATURE_RRL = 0x12,
	NVME_FEATURE_PLM_CONFIG = 0x13,
	NVME_FEATURE_PLM_WINDOW = 0x14,
	NVME_FEATURE_HOST_BEHAVIOR = 0x16,
	NVME_FEATURE_SW_PROGRESS = 0x80,
	NVME_FEATURE_HOST_ID = 0x81,
	NVME_FEATURE_RESV_MASK = 0x82,
	NVME_FEATURE_RESV_PERSIST = 0x83,
	NVME_FEATURE_WRITE_PROTECT = 0x84,
	NVME_LOG_ERROR = 0x01,
	NVME_LOG_SMART = 0x02,
	NVME_LOG_FW_SLOT = 0x03,
	NVME_LOG_CHANGED_NS = 0x04,
	NVME_LOG_CMD_EFFECTS = 0x05,
	NVME_LOG_ANA = 0x0c,
	NVME_LOG_DISC = 0x70,
	NVME_LOG_RESERVATION = 0x80,
	NVME_FWACT_REPL = (0 << 3),
	NVME_FWACT_REPL_ACTV = (1 << 3),
	NVME_FWACT_ACTV = (2 << 3),
};

/* NVMe Namespace Write Protect State */
enum {
	NVME_NS_NO_WRITE_PROTECT = 0,
	NVME_NS_WRITE_PROTECT,
	NVME_NS_WRITE_PROTECT_POWER_CYCLE,
	NVME_NS_WRITE_PROTECT_PERMANENT,
};

#define NVME_MAX_CHANGED_NAMESPACES	1024

typedef struct _NVME_IDENTIFY_COMMAND {
	UCHAR                 ucOpCode;
	UCHAR                 ucFlags;
	USHORT                usCommandId;
	ULONG                 ulNSId;
	UINT64                ullResvd2[2];
	NVME_DATA_BUFFER_LIST sDataPtr;
	UCHAR                 ucCNS;
	UCHAR                 ucResvd3;
	USHORT                usControllerId;
	ULONG			      ulResvd11[5];
} NVME_IDENTIFY_COMMAND, * PNVME_IDENTIFY_COMMAND;

#define NVME_IDENTIFY_DATA_SIZE 4096

typedef struct _NVME_FEATURES_COMMAND {
	UCHAR                 ucOpCode;
	UCHAR                 ucFlags;
	USHORT                usCommandId;
	ULONG                 ulNSId;
	UINT64                ullResvd2[2];
	NVME_DATA_BUFFER_LIST sDataPtr;
	ULONG			      ulFeatureID;
	ULONG                 ulDWord11;
	ULONG                 ulDWord12;
	ULONG                 ulDWord13;
	ULONG                 ulDWord14;
	ULONG                 ulDWord15;
} NVME_FEATURES_COMMAND, * PNVME_FEATURES_COMMAND;

typedef struct _NVME_HOST_BUFF_DESCRIPTOR {
	UINT64 ullAddress;
	ULONG  ulSize;
	ULONG  ulResved;
} NVME_HOST_BUFF_DESCRIPTOR, * PNVME_HOST_BUFF_DESCRIPTOR;

typedef struct _NVME_CREATE_COMPLETION_QUEUE {
	UCHAR	ucOpCode;
	UCHAR	ucFlags;
	USHORT	usCommandId;
	ULONG	ulResvd1[5];
	UINT64	ullPRP1;
	UINT64	ullResvd8;
	USHORT	usCQId;
	USHORT	usQSize;
	USHORT	usCQFlags;
	USHORT	usIRQVector;
	ULONG	ulaResvd12[4];
} NVME_CREATE_COMPLETION_QUEUE, * PNVME_CREATE_COMPLETION_QUEUE;

typedef struct _NVME_CREATE_SUBMISSION_QUEUE {
	UCHAR	ucOpCode;
	UCHAR	ucFlags;
	USHORT	usCommandId;
	ULONG	ulaResvd1[5];
	UINT64	ullPRP1;
	UINT64	ullResvd8;
	USHORT	usSQId;
	USHORT	usQSize;
	USHORT	usSQFlags;
	USHORT	usCQId;
	ULONG	ulResvd12[4];
} NVME_CREATE_SUBMISSION_QUEUE, * PNVME_CREATE_SUBMISSION_QUEUE;

typedef struct _NVME_DELETE_QUEUE {
	UCHAR	ucOpCode;
	UCHAR	ucFlags;
	USHORT	usCommandId;
	ULONG	ulaResvd1[9];
	USHORT	usQId;
	USHORT	usResvd10;
	ULONG	ulaResvd11[5];
}NVME_DELETE_QUEUE, * PNVME_DELETE_QUEUE;

typedef struct _NVME_ABORT_COMMAND {
	UCHAR	ucOpCode;
	UCHAR	ucFlags;
	USHORT	usCommandId;
	ULONG	ulaResvd1[9];
	USHORT	usSQId;
	USHORT	usCId;
	ULONG	ulaResvd11[5];
} NVME_ABORT_COMMAND, * PNVME_ABORT_COMMAND;

typedef struct _NVME_DOWNLOAD_FIRMWARE {
	UCHAR			      ucOpCode;
	UCHAR			      ucFlags;
	USHORT			      usCommandId;
	ULONG			      ulaResvd1[5];
	NVME_DATA_BUFFER_LIST sDataPtr;
	ULONG			      ulNumDW;
	ULONG			      ulOffset;
	ULONG			      ulaResvd12[4];
}NVME_DOWNLOAD_FIRMWARE, * PNVME_DOWNLOAD_FIRMWARE;

typedef struct _NVME_FORMAT_COMMAND {
	UCHAR	ucOpCode;
	UCHAR	ucFlags;
	USHORT	usCommandId;
	ULONG	ulNSId;
	UINT64	ullResvd2[4];
	ULONG	ulCDW10;
	ULONG	ulResvd11[5];
} NVME_FORMAT_COMMAND, * PNVME_FORMAT_COMMAND;

typedef struct _NVME_GET_LOG_PAGE_CMD {
	UCHAR			      OpCode;
	UCHAR			      ucFlags;
	USHORT			      usCommandId;
	ULONG			      ulNSId;
	UINT64			      ullaResvd2[2];
	NVME_DATA_BUFFER_LIST sDataPtr;
	UCHAR			      ucLId;
	UCHAR			      ucLSp; /* upper 4 bits reserved */
	USHORT			      usNumDWLower;
	USHORT			      usNumDWUpper;
	USHORT			      usResvd11;
	union {
		struct {
			ULONG ulLpOl;
			ULONG ulLpOu;
		};
		UINT64 ullLpO;
	};
	ULONG			ulaResvd14[2];
} NVME_GET_LOG_PAGE_CMD, * PNVME_GET_LOG_PAGE_CMD;

typedef struct _NVMEOF_DIRECTIVE_CMD {
	UCHAR			      ucOpCode;
	UCHAR			      ucFlags;
	USHORT			      usCommandId;
	ULONG			      ulNSId;
	UINT64			      ullResvd2[2];
	NVME_DATA_BUFFER_LIST sDataPtr;
	ULONG			      ulNumDwords;
	UCHAR			      ucDirectiveOperation;
	UCHAR			      ucDirectiveType;
	USHORT			      ucDirectiveSpecific;
	UCHAR			      ucEnableDirective;
	UCHAR			      ucTdType;
	USHORT			      usResvd15;
	ULONG			      ucaResvd16[3];
} NVMEOF_DIRECTIVE_CMD, * PNVMEOF_DIRECTIVE_CMD;

/*
 * Fabrics subcommands.
 */
enum NVMEOF_FABRICS_OPCODE {
	NVME_OVER_FABRICS_COMMAND = 0x7f,
};

typedef enum _NVMEOF_CAPSULE_COMMAND {
	NVMEOF_TYPE_PROPERTY_SET = 0x00,
	NVMEOF_TYPE_CONNECT = 0x01,
	NVMEOF_TYPE_PROPERTY_GET = 0x04,
	NVMEOF_TYPE_AUTHENTICATION_SEND = 0x04,
	NVMEOF_TYPE_AUTHENTICATION_RECEIVE = 0x05,
	NVMEOF_TYPE_DISCONNECT = 0x08,
}NVMEOF_CAPSULE_COMMAND, * PNVMEOF_CAPSULE_COMMAND;

typedef struct _NVMEOF_GENERIC_COMMAND {
	UCHAR	ucOpCode;
	UCHAR	ucResvd1;
	USHORT	usCommandId;
	UCHAR	ucFabricCmdType;
	UCHAR	ucaResvd2[35];
	UCHAR	ucaTS[24];
}NVMEOF_GENERIC_COMMAND, * PNVMEOF_GENERIC_COMMAND;

/*
 * The legal cntlid range a NVMe Target will provide.
 * Note that cntlid of value 0 is coNSIdered illegal in the fabrics world.
 * Devices based on earlier specs did not have the subsystem concept;
 * therefore, those devices had their cntlid value set to 0 as a result.
 */
#define NVME_CNTLID_MIN		1
#define NVME_CNTLID_MAX		0xffef
#define NVME_CNTLID_DYNAMIC	0xffff

#define MAX_DISC_LOGS	255

#define NVMEOF_NUM_LOG_PAGE_ENTRIES_READ_BYTES  16
 /* Discovery log page entry */
#define NVMOF_XPORT_SERVICE_PROVIDER_SIZE			32
#define NVMOF_XPORT_ADDRESS_SIZE					256
#define NVMOF_XPORT_SPEC_ADDRESS_SUBTYPE_SIZE		256

#define NVMEOF_DISCOVERY_RESP_ENTRY_RESVD_10_SIZE 22
#define NVMEOF_DISCOVERY_RESP_ENTRY_RESVD_64_SIZE 192
typedef struct _RDMA_SPEC_SUBTYPE {
	UCHAR	ucQPType;
	UCHAR	ucPRType;
	UCHAR	ucCMS;
	UCHAR	ucaResvd3[5];
	USHORT	usPKey;
	UCHAR	ucaResvd10[246];
} RDMA_SPEC_SUBTYPE, * PRDMA_SPEC_SUBTYPE;

typedef union _XPORT_SPECIFIC_ADDRESS_SUBTYPE {
	UCHAR             ucaXportSpecST[NVMOF_XPORT_SPEC_ADDRESS_SUBTYPE_SIZE];
	RDMA_SPEC_SUBTYPE sRDMASpecSubType;
} XPORT_SPECIFIC_ADDRESS_SUBTYPE, * PXPORT_SPECIFIC_ADDRESS_SUBTYPE;

typedef struct _NVMEOF_DISCOVERY_RESP_PAGE_ENTRY {
	UCHAR		                   ucXportType;
	UCHAR		                   ucAddressFamily;
	UCHAR		                   ucSubsystemType;
	UCHAR		                   ucXportRequirement;
	USHORT		                   usPortID;
	USHORT		                   usControllerID;
	USHORT		                   usAdminMaxSQSize;
	UCHAR		                   ucaResvd10[NVMEOF_DISCOVERY_RESP_ENTRY_RESVD_10_SIZE];
	CHAR		                   caXportServiceIdentifier[NVMOF_XPORT_SERVICE_PROVIDER_SIZE];
	UCHAR		                   ucaResvd64[NVMEOF_DISCOVERY_RESP_ENTRY_RESVD_64_SIZE];
	CHAR		                   caNVMSubSystemNQN[NVMF_NQN_FIELD_LEN];
	CHAR		                   caXportAddress[NVMOF_XPORT_ADDRESS_SIZE];
	XPORT_SPECIFIC_ADDRESS_SUBTYPE sXportSpecificSubtype;
}NVMEOF_DISCOVERY_RESP_PAGE_ENTRY, * PNVMEOF_DISCOVERY_RESP_PAGE_ENTRY;

#define NVMEOF_DISCOVERY_RESP_HEADER_RESVD_SIZE 1006
/* Discovery log page header */
typedef struct _NVMEOF_DISCOVERY_RESP_PAGE_HEADER {
	UINT64		                     ullGenerationCounter;
	UINT64		                     ullRecordCount;
	USHORT		                     usRecordFormat;
	UCHAR		                     ucaResvd18[NVMEOF_DISCOVERY_RESP_HEADER_RESVD_SIZE];
	NVMEOF_DISCOVERY_RESP_PAGE_ENTRY sDiscoveryLogEntries[0];
} NVMEOF_DISCOVERY_RESP_PAGE_HEADER, * PNVMEOF_DISCOVERY_RESP_PAGE_HEADER;

enum {
	NVME_CONNECT_DISABLE_SQFLOW = (1 << 2),
};

typedef struct _NVMEOF_CONNECT_COMMAND {
	UCHAR		          ucOpCode;
	UCHAR		          ucResvd1;
	USHORT		          usCommandId;
	UCHAR		          ucFabricCmdType;
	UCHAR		          ucaResvd2[19];
	NVME_DATA_BUFFER_LIST sDataPtr;
	USHORT		          usRecordFormat;
	USHORT		          usQId;
	USHORT		          usSQSize;
	UCHAR		          ucCAttr;
	UCHAR		          ucResvd3;
	ULONG		          ulKATO;
	UCHAR		          ucaResvd4[12];
} NVMEOF_CONNECT_COMMAND, * PNVMEOF_CONNECT_COMMAND;

typedef struct _NVMOF_CONNECT_DATA {
	GUID    sHostId;
	USHORT  usCtrlId;
	UCHAR   caResved1[238];
	CHAR    caSubsysNQN[NVMF_NQN_FIELD_LEN];
	CHAR    caHostNQN[NVMF_NQN_FIELD_LEN];
	UCHAR   caResved2[256];
} NVMOF_CONNECT_DATA, * PNVMOF_CONNECT_DATA;

typedef struct _NVMEOF_CONNECT_RESPONSE {
	ULONG   ullStatusDetails;
	ULONG   ullResvd1;
	USHORT  usSQHeadPtr;
	USHORT  usResvd2;
	USHORT  usCmdId;
	USHORT  usStatus;
} NVMEOF_CONNECT_RESPONSE, * PNVMEOF_CONNECT_RESPONSE;

typedef struct _NVMEOF_DISCONNECT_COMMAND {
	UCHAR		          ucOpCode;
	UCHAR		          ucResvd1;
	USHORT		          usCommandId;
	UCHAR		          ucFabricCmdType;
	UCHAR		          ucaResvd2[18];
	NVME_DATA_BUFFER_LIST sDataPtr;
	USHORT		          usRecordFormat;
	UCHAR		          ucaResvd4[15];
} NVMEOF_DISCONNECT_COMMAND, * PNVMEOF_DISCONNECT_COMMAND;

typedef enum _NVMEOF_PROPERTY_COMMAND_ATTRIB {
	NVMEOF_PROPERTY_COMMAND_ATTRIB_4B,
	NVMEOF_PROPERTY_COMMAND_ATTRIB_8B,
	NVMEOF_PROPERTY_COMMAND_ATTRIB_INVALID,
} NVMEOF_PROPERTY_COMMAND_ATTRIB, * PNVMEOF_PROPERTY_COMMAND_ATTRIB;

typedef struct _NVMEOF_SET_PROPERTY_COMMAND {
	UCHAR   ucOpCode;
	UCHAR   ucResvd1;
	USHORT  usCommandId;
	UCHAR   ucFabricCmdType;
	UCHAR   ucaResvd2[35];
	UCHAR   ucAttrib;
	UCHAR   ucaResvd3[3];
	ULONG   ulOffset;
	UINT64  ullValue;
	UCHAR   ucaResvd4[8];
} NVMEOF_SET_PROPERTY_COMMAND, * PNVMEOF_SET_PROPERTY_COMMAND;

typedef struct _NVMEOF_GET_PROPERTY_COMMAND {
	UCHAR   ucOpCode;
	UCHAR   ucResvd1;
	USHORT  usCommandId;
	UCHAR   ucFabricCmdType;
	UCHAR   ucaResvd2[35];
	UCHAR   ucAttrib;
	UCHAR   ucaResvd3[3];
	ULONG   ulOffset;
	UCHAR   ucaResvd4[16];
} NVMEOF_GET_PROPERTY_COMMAND, * PNVMEOF_GET_PROPERTY_COMMAND;

typedef struct _NVME_DBBUF {
	UCHAR	ucOpCode;
	UCHAR	ucFlags;
	USHORT	usCommandId;
	ULONG	ulaResvd1[5];
	UINT64	ullPRP1;
	UINT64	ullPRP2;
	ULONG	ulaResvd12[6];
} NVME_DBBUF, * PNVME_DBBUF;

typedef struct _NVMEOF_KEEP_ALIVE_COMMAND {
	UCHAR	ucOpCode;
	NVME_CMD_FLAGS sFlags;
	USHORT	usCommandId;
	ULONG	ulaResvd1[5];
	UINT64	ullPRP1;
	UINT64	ullPRP2;
	ULONG	ulaResvd12[6];
} NVMEOF_KEEP_ALIVE_COMMAND, * PNVMEOF_KEEP_ALIVE_COMMAND;

typedef struct _NVME_STREAM_DIRECTIVE_PARAMETERS {
	USHORT	usMaxStreamLimit;
	USHORT	usNVMSubsysStreamAvailable;
	USHORT	usNVMSubsysStreamOpen;
	UCHAR   ucNVMSubsysStreamCapability;
	UCHAR	ucaResved[9];
	ULONG	ulStreamWriteSize;
	USHORT	usStreamGranSize;
	USHORT	usNamespaceStreamAllocated;
	USHORT	usNamespaceStreamOpen;
	UCHAR	ucaResvd2[6];
} NVME_STREAM_DIRECTIVE_PARAMETERS, * PNVME_STREAM_DIRECTIVE_PARAMETERS;

typedef struct NVME_CMD {
	union {
		NVME_GENERIC_COMMAND         sGeneric;
		NVME_READ_WRITE_COMMAND      sReadWrite;
		NVME_IDENTIFY_COMMAND        sIdentify;
		NVME_FEATURES_COMMAND        sFeatures;
		NVME_CREATE_COMPLETION_QUEUE sCreateCQ;
		NVME_CREATE_SUBMISSION_QUEUE sCreateSQ;
		NVME_DELETE_QUEUE            sDeleteQueue;
		NVME_DOWNLOAD_FIRMWARE       sDownloadFW;
		NVME_FORMAT_COMMAND          sFormat;
		NVME_DSM_CMD                 sDSM;
		NVME_WRITE_ZERO_COMMAND      sWrZeroes;
		NVME_ABORT_COMMAND           sAbort;
		NVME_GET_LOG_PAGE_CMD        sGetLogPage;
		NVMEOF_GENERIC_COMMAND       sFabricsCmd;
		NVMEOF_CONNECT_COMMAND       sConnect;
		NVMEOF_DISCONNECT_COMMAND    sDisconnect;
		NVMEOF_SET_PROPERTY_COMMAND  sPropertySet;
		NVMEOF_GET_PROPERTY_COMMAND  sPropertyGet;
		NVME_DBBUF                   sDBBuf;
		NVMEOF_DIRECTIVE_CMD         sDirective;
		NVMEOF_KEEP_ALIVE_COMMAND    sKeepAlive;
	};
} NVME_CMD, * PNVME_CMD;

typedef struct _NVME_ERROR_INFORMATION_LOG_ENTRY {
	UINT64		ullErrorCount;
	USHORT		usSQId;
	USHORT		usCmdId;
	USHORT		usStatusField;
	USHORT		usParamErrorLoc;
	UINT64		ullLBA;
	ULONG		ulNSId;
	UCHAR		ucVendorSpecific;
	UCHAR		ucResvd1[3];
	UINT64		ullCmdSpecific;
	UCHAR		ucaResv2[24];
} NVME_ERROR_INFORMATION_LOG_ENTRY, * PNVME_ERROR_INFORMATION_LOG_ENTRY;

typedef enum _NVME_COMMAND_STATUS_CODE_TYPE { //Status Code Type Values
	NVME_COMMAND_SCT_GENERIC = 0x00,
	NVME_COMMAND_SCT_CMD_SPCIFIC = 0x01,
	NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY = 0x02,
	NVME_COMMAND_SCT_PATH_RELATED = 0x03,
	NVME_COMMAND_SCT_RESERVED4 = 0x04,
	NVME_COMMAND_SCT_RESERVED5 = 0x05,
	NVME_COMMAND_SCT_RESERVED6 = 0x06,
	NVME_COMMAND_SCT_VENDOR_SPECIFIC = 0x07,
} NVME_COMMAND_STATUS_CODE_TYPE, * PNVME_COMMAND_STATUS_CODE_TYPE;


typedef enum _NVME_COMMAND_STATUS {
	/*
	 * Generic Command Status:
	 * NVME Base Spec 1.4 Page #80
	 */
	NVME_SC_GEN_SUCCESS = (NVME_COMMAND_SCT_GENERIC << 9) | (0x0 << 1),
	NVME_SC_GEN_INVALID_OPCODE = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1 << 1),
	NVME_SC_GEN_INVALID_FIELD = (NVME_COMMAND_SCT_GENERIC << 9) | (0x2 << 1),
	NVME_SC_GEN_CMDID_CONFLICT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x3 << 1),
	NVME_SC_GEN_DATA_XFER_ERROR = (NVME_COMMAND_SCT_GENERIC << 9) | (0x4 << 1),
	NVME_SC_GEN_POWER_LOSS_ABORT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x5 << 1),
	NVME_SC_GEN_INTERNAL_ERROR = (NVME_COMMAND_SCT_GENERIC << 9) | (0x6 << 1),
	NVME_SC_GEN_CMD_ABORT_REQESTED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x7 << 1),
	NVME_SC_GEN_SQ_DEL_ABORT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x8 << 1),
	NVME_SC_GEN_FUSED_FAIL_ABORT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x9 << 1),
	NVME_SC_GEN_FUSED_MISSING_ABORT = (NVME_COMMAND_SCT_GENERIC << 9) | (0xA << 1),
	NVME_SC_GEN_INVALID_NS_FORMAT = (NVME_COMMAND_SCT_GENERIC << 9) | (0xB << 1),
	NVME_SC_GEN_CMD_SEQ_ERROR = (NVME_COMMAND_SCT_GENERIC << 9) | (0xC << 1),
	NVME_SC_GEN_INVALID_SGL_SEG_DESC = (NVME_COMMAND_SCT_GENERIC << 9) | (0xD << 1),
	NVME_SC_GEN_INVALID_SGL_DESC_COUNT = (NVME_COMMAND_SCT_GENERIC << 9) | (0xE << 1),
	NVME_SC_GEN_INVALID_DATA_SGL_LENGTH = (NVME_COMMAND_SCT_GENERIC << 9) | (0xF << 1),
	NVME_SC_GEN_INVALID_METADATA_SGL_LENGTH = (NVME_COMMAND_SCT_GENERIC << 9) | (0x10 << 1),
	NVME_SC_GEN_INVALID_SGL_DESC_TYPE = (NVME_COMMAND_SCT_GENERIC << 9) | (0x11 << 1),
	NVME_SC_GEN_INVALID_USE_CTRLR_MEM_BUFF = (NVME_COMMAND_SCT_GENERIC << 9) | (0x12 << 1),
	NVME_SC_GEN_INVALID_PRP_OFFSET = (NVME_COMMAND_SCT_GENERIC << 9) | (0x13 << 1),
	NVME_SC_GEN_ATOMIC_WRITE_UNIT_EXCEED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x14 << 1),
	NVME_SC_GEN_OPERATION_DENIED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x15 << 1),
	NVME_SC_GEN_INVALID_SGL_OFFSET = (NVME_COMMAND_SCT_GENERIC << 9) | (0x16 << 1),
	NVME_SC_GEN_INVALID_SGL_SUBTYPE = (NVME_COMMAND_SCT_GENERIC << 9) | (0x17 << 1),
	NVME_SC_GEN_HOST_ID_INCONSISTENT_FMT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x18 << 1),
	NVME_SC_GEN_KEEP_ALIVE_TIMER_EXPIRED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x19 << 1),
	NVME_SC_GEN_INVALID_KEEP_ALIVE_TIMEOUT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1A << 1),
	NVME_SC_GEN_CMD_ABORT_PREEMPT_ABORT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1B << 1),
	NVME_SC_GEN_SANITIZE_FAILED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1C << 1),
	NVME_SC_GEN_SANITIZE_IN_PROGRESS = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1D << 1),
	NVME_SC_GEN_INVALID_SGL_DATA_BLK_GRAN = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1E << 1),
	NVME_SC_GEN_UNSUPPORTED_CMD_CMB_QUEUE = (NVME_COMMAND_SCT_GENERIC << 9) | (0x1F << 1),
	NVME_SC_NS_WRITE_PROTECTED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x20 << 1),
	NVME_SC_CMD_INTERRUPTED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x21 << 1),
	NVME_SC_XIENT_XPORT_ERROR = (NVME_COMMAND_SCT_GENERIC << 9) | (0x22 << 1),
	NVME_SC_LBA_RANGE = (NVME_COMMAND_SCT_GENERIC << 9) | (0x80 << 1),
	NVME_SC_CAP_EXCEEDED = (NVME_COMMAND_SCT_GENERIC << 9) | (0x81 << 1),
	NVME_SC_NS_NOT_READY = (NVME_COMMAND_SCT_GENERIC << 9) | (0x82 << 1),
	NVME_SC_RESERVATION_CONFLICT = (NVME_COMMAND_SCT_GENERIC << 9) | (0x83 << 1),
	NVME_SC_FORMAT_IN_PROGRESS = (NVME_COMMAND_SCT_GENERIC << 9) | (0x84 << 1),

	/*
	 * Command Specific Status:
	 * NVME Base Spec 1.4 Page #82
	 */
	 NVME_SC_INVALID_CQ = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x00 << 1),
	 NVME_SC_INVALID_QID = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x01 << 1),
	 NVME_SC_INVALID_QUEUE_SIZE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x02 << 1),
	 NVME_SC_ABORT_CMD_LIMIT_EXCEEDED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x03 << 1),
	 NVME_SC_ABORT_CMD_MISSING = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x04 << 1),
	 NVME_SC_ASYNC_EVT_REQ_LIMIT_EXCEEDED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x05 << 1),
	 NVME_SC_INVALID_FIRMWARE_SLOT = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x06 << 1),
	 NVME_SC_INVALID_FIRMWARE_IMAGE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x07 << 1),
	 NVME_SC_INVALID_INTERRUPT_VECTOR = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x08 << 1),
	 NVME_SC_INVALID_LOG_PAGE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x09 << 1),
	 NVME_SC_INVALID_FORMAT = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x0A << 1),
	 NVME_SC_FW_ACTIVATION_NEEDS_CONV_RESET = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x0B << 1),
	 NVME_SC_INVALID_QUEUE_DELETE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x0C << 1),
	 NVME_SC_FEATURE_ID_NOT_SAVEABLE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x0D << 1),
	 NVME_SC_FEATURE_NOT_CHANGEABLE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x0E << 1),
	 NVME_SC_FEATURE_NOT_FOR_NAMESPACE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x0F << 1),
	 NVME_SC_FW_ACTIV_NEEDS_NVM_SUBSYS_RESET = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x10 << 1),
	 NVME_SC_FW_ACTIVATION_NEEDS_CTRLR_RESET = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x11 << 1),
	 NVME_SC_FW_ACT_NEEDS_MAX_TIME_VIOLATION = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x12 << 1),
	 NVME_SC_FW_ACIVATION_PROHIBITED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x13 << 1),
	 NVME_SC_OVERLAPPING_RANGE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x14 << 1),
	 NVME_SC_NS_INSUFFICENT_CAP = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x15 << 1),
	 NVME_SC_NS_UNAVAILABLE_ID = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x16 << 1),
	 NVME_SC_NS_RESERVED_17 = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x17 << 1),
	 NVME_SC_NS_ALREADY_ATTACHED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x18 << 1),
	 NVME_SC_NS_IS_PRIVATE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x19 << 1),
	 NVME_SC_NS_NOT_ATTACHED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x1A << 1),
	 NVME_SC_NS_THIN_PROVISIONING_UNSUPPORTED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x1B << 1),
	 NVME_SC_NS_CTRLR_INVALID_LIST = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x1C << 1),
	 NVME_SC_DEV_SELF_TEST_IN_PROGRESS = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x1D << 1),
	 NVME_SC_BOOT_PARTITION_WRITE_PROHIBITED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x1E << 1),
	 NVME_SC_INVALID_CTRLR_IDENTIFIER = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x1F << 1),
	 NVME_SC_INVALID_SECONDARY_CTRLR_STATE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x20 << 1),
	 NVME_SC_INVALID_NUM_CTRLR_RESOURCE = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x21 << 1),
	 NVME_SC_INVALID_RESOURCE_IDENTIFIER = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x22 << 1),
	 NVME_SC_PROHIBITED_SANITIZE_PMR_ENABLED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x23 << 1), //Sanitize Prohibited While Persistent Memory Region is Enabled
	 NVME_SC_INVALID_ANA_GROUP_IDENTIFIER = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x24 << 1),
	 NVME_SC_FAILED_ANA_ATTACHED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x25 << 1),

	 /*
	  * I/O Command Set Specific - NVM commands:
	  */
	  NVME_SC_CONFLICTING_ATTRIBUTES = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x80 << 1),
	  NVME_SC_INVALID_PROTACTION_INFORMAITON = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x81 << 1),
	  NVME_SC_ATTEMPT_WRITE_RO_REGION = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x82 << 1), //Attempted Write to Read Only Range
	  NVME_SC_ONCS_NOT_SUPPORTED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x83 << 1),

	  /*
	   * I/O Command Set Specific Status- Fabrics commands:
	   * NVMEoF Spec 1.0 Gold Page #16
	   */
	   NVME_SC_CONNECT_INCOMPATIBLE_FORMAT = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x80 << 1),
	   NVME_SC_CONNECT_CTROLLER_BUSY = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x81 << 1),
	   NVME_SC_CONNECT_INVALID_PARAMS = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x82 << 1),
	   NVME_SC_CONNECT_RESTART_DISCOVERY = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x83 << 1),
	   NVME_SC_CONNECT_INVALID_HOST = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x84 << 1),
	   //Reserved Status range 0x85 to 0x89
	   NVME_SC_RESTART_DISCOVERY = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x90 << 1),
	   NVME_SC_AUTH_REQUIRED = (NVME_COMMAND_SCT_CMD_SPCIFIC << 9) | (0x91 << 1),

	   /*
		* Media and Data Integrity Errors:
		* NVME Base Spec 1.4 Page #83
		*/
		NVME_SC_WRITE_FAULT = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x80 << 1),
		NVME_SC_UNRECOVERED_READ_ERROR = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x81 << 1),
		NVME_SC_E2E_GUARD_CHECK_ERROR = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x82 << 1), //End-to-end Guard Check Error
		NVME_SC_E2E_APPN_TAG_CHECK_ERROR = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x83 << 1), //End-to-end Application Tag Check Error
		NVME_SC_E2E_REF_TAG_CHECK_ERROR = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x84 << 1), //End-to-end Reference Tag Check Error
		NVME_SC_COMPARE_FAILED = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x85 << 1),
		NVME_SC_ACCESS_DENIED = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x86 << 1),
		NVME_SC_DEALLOCATED_UNWRITTEN_LBLK = (NVME_COMMAND_SCT_MEDIA_DATA_INTEGRITY << 9) | (0x87 << 1), //Deallocated or Unwritten Logical Block

		/*
		 * Path-related Errors:
		 */
		 NVME_SC_INTERNAL_PATH_ERROR = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x00 << 1),
		 NVME_SC_ASYM_ACCESS_PERSISTENT_LOSS = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x01 << 1),
		 NVME_SC_ASYM_ACCESS_INACCESSIBLE = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x02 << 1),
		 NVME_SC_ASYM_ACCESS_TRANSITION = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x03 << 1),
		 NVME_SC_CONTROLLER_PATH_ERROR = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x60 << 1),
		 NVME_SC_HOST_PATH_ERROR = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x70 << 1),
		 NVME_SC_CMD_ABORTED_BY_HOST = (NVME_COMMAND_SCT_PATH_RELATED << 9) | (0x71 << 1),

		 NVME_SC_CRD = 0x1800,
		 NVME_SC_DNR = 0x4000,
} NVME_COMMAND_STATUS, * PNVME_COMMAND_STATUS;

#define NVME_STATUS_MASK 0x7FF

typedef struct _NVME_CQE_CMD_SPECIFIC_DWORDS {
	ULONG	ulDW0;
	ULONG	ulDW1;
}NVME_CQE_CMD_SPECIFIC_DWORDS, * PNVME_CQE_CMD_SPECIFIC_DWORDS;

typedef union _NVME_CMD_SPECIFIC_DATA {
	UINT64	                     ullDDW;
	NVME_CQE_CMD_SPECIFIC_DWORDS sCmdSpecData;
}NVME_CMD_SPECIFIC_DATA, * PNVME_CMD_SPECIFIC_DATA;

typedef struct _STATUS_FIEELD {
	USHORT PhaseTag : 1;
	USHORT SC : 8;
	USHORT SCT : 3;
	USHORT Resvd : 2;
	USHORT More : 1;
	USHORT DNR : 1;
} STATUS_FIEELD, * PSTATUS_FIEELD;

typedef union _NVME_COMP_Q_ENTRY_STATUS_FIELD {
	USHORT        usStatus;
	STATUS_FIEELD sStatusField;
} NVME_COMP_Q_ENTRY_STATUS_FIELD, * PNVME_COMP_Q_ENTRY_STATUS_FIELD;

typedef struct _NVME_COMPLETION_QUEUE_ENTRY {
	NVME_CMD_SPECIFIC_DATA          sCmdRespData;
	USHORT	                        usSQHead;
	USHORT	                        usSQId;
	USHORT	                        usCommandID;
	NVME_COMP_Q_ENTRY_STATUS_FIELD	sStatus;
} NVME_COMPLETION_QUEUE_ENTRY, * PNVME_COMPLETION_QUEUE_ENTRY;

#define NVMEOF_CAPABILITY_TIMEOUT_BIT_SHIFT 24
#define NVMEOF_CAPABILITY_STRIDE_BIT_SHIFT  32
#define NVMEOF_CAPABILITY_NSSRC_BIT_SHIFT   36
#define NVMEOF_CAPABILITY_MPSMIN_BIT_SHIFT  48
#define NVMEOF_CAPABILITY_MPSMAX_BIT_SHIFT  52

#define NVMEOF_SEC_TO_MS(ulTimeInSec)             ((ulTimeInSec) * 1000)
#define NVMEOF_GET_CAP_MQES(Capability)           ((Capability) & 0xffff)
#define NVMEOF_GET_CAP_READY_TIMEOUT(Capability)  (((Capability) >> NVMEOF_CAPABILITY_TIMEOUT_BIT_SHIFT) & 0xff)
#define NVMEOF_GET_CAP_STRIDE(Capability)	      (((Capability) >> NVMEOF_CAPABILITY_STRIDE_BIT_SHIFT) & 0xf)
#define NVMEOF_GET_CAP_NSSRC(Capability)          (((Capability) >> NVMEOF_CAPABILITY_NSSRC_BIT_SHIFT) & 0x1)
#define NVMEOF_GET_CAP_MPSMIN(Capability)	      (((Capability) >> NVMEOF_CAPABILITY_MPSMIN_BIT_SHIFT) & 0xf)
#define NVMEOF_GET_CAP_MPSMAX(Capability)	      (((Capability) >> NVMEOF_CAPABILITY_MPSMAX_BIT_SHIFT) & 0xf)

typedef struct _NVMEOF_RDMA_PRIVATE_DATA {
	USHORT		usRecfmt;
	USHORT		usQId;
	USHORT		usHrqsize;
	USHORT		usHsqsize;
	USHORT      usControllerId;
	UCHAR		ucaRsvd[22];
}NVMEOF_RDMA_PRIVATE_DATA, *PNVMEOF_RDMA_PRIVATE_DATA;

typedef enum _NVMEOF_RDMA_NVME_CM_FMT {
	NVMEOF_RDMA_CM_FMT_1_0 = 0x0,
} NVMEOF_RDMA_NVME_CM_FMT, * PNVMEOF_RDMA_NVME_CM_FMT;

#define NVMEOF_DISCOVERY_CONTROLLER_ID 0xFFFF
#pragma warning(pop)
