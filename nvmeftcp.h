/**
 * Copyright (c) 2022-2023, Leap Distributed Tech LLC. All rights reserved.
 * See file LICENSE.md for terms.
 */
#pragma once
#include "nvme.h"

#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4200) // nameless struct/union
#pragma warning(disable:4214) // bit field types other than int


#define NVMEOTCP_DISC_PORT	        8009
#define NVMEOTCP_ADMIN_CCSZ	        8*1024
#define NVMEOTCP_DIGEST_LENGTH	    4
#define NVMEOTCP_PDU_FIXED_LENGTH	24

typedef enum _NVMEOTCP_PDU_FORMAT_VERSION {
	NVMEOTCP_PDU_FORMAT_VERSION_1_0 = 0x0,
} NVMEOTCP_PDU_FORMAT_VERSION, * PNVMEOTCP_PDU_FORMAT_VERSION;

typedef enum _NVMEOTCP_FATAL_ERROR_STATUS {
	NVMEOTCP_FATAL_ERROR_STATUS_INVALID_PDU_HDR = 0x01,
	NVMEOTCP_FATAL_ERROR_STATUS_PDU_SEQ_ERR = 0x02,
	NVMEOTCP_FATAL_ERROR_STATUS_HDR_DIGEST_ERR = 0x03,
	NVMEOTCP_FATAL_ERROR_STATUS_DATA_OUT_OF_RANGE = 0x04,
	NVMEOTCP_FATAL_ERROR_STATUS_R2T_LIMIT_EXCEEDED = 0x05,
	NVMEOTCP_FATAL_ERROR_STATUS_DATA_LIMIT_EXCEEDED = 0x05,
	NVMEOTCP_FATAL_ERROR_STATUS_UNSUPPORTED_PARAM = 0x06,
} NVMEOTCP_FATAL_ERROR_STATUS, * PNVMEOTCP_FATAL_ERROR_STATUS;

typedef enum _NVMEOTCP_DIGEST_OPTIONS {
	NVMEOTCP_DIGEST_HDR = (1 << 0),
	NVMEOTCP_DIGEST_DATA = (1 << 1),
} NVMEOTCP_DIGEST_OPTIONS, * PNVMEOTCP_DIGEST_OPTIONS;

typedef enum _NVMEOTCP_PDU_TYPE {
	NVMEOTCP_PDU_ICREQ = 0x0,
	NVMEOTCP_PDU_ICRESP = 0x1,
	NVMEOTCP_PDU_HOST_2_CTRLR_CONN_TERM = 0x2,
	NVMEOTCP_PDU_CTRLR_2_HOST_CONN_TERM = 0x3,
	NVMEOTCP_PDU_NVME_CMD = 0x4,
	NVMEOTCP_PDU_NVME_RESP = 0x5,
	NVMEOTCP_PDU_HOST_2_CTRLR_DATA = 0x6,
	NVMEOTCP_PDU_CTRLR_2_HOST_DATA = 0x7,
	NVMEOTCP_PDU_RDY_2_XFER = 0x9,
} NVMEOTCP_PDU_TYPE, * PNVMEOTCP_PDU_TYPE;

typedef enum _NVMEOTCP_PDU_FLAGS {
	NVMEOTCP_PDU_FLAG_HDGST = (1 << 0),
	NVMEOTCP_PDU_FLAG_DDGST = (1 << 1),
	NVMEOTCP_PDU_FLAG_DATA_LAST = (1 << 2),
	NVMEOTCP_PDU_FLAG_DATA_SUCCESS = (1 << 3),
} NVMEOTCP_PDU_FLAGS, * PNVMEOTCP_PDU_FLAGS;

#pragma pack(push, 1)
/**
 * typedef struct _NVMEoTCP_PDU_HDR - NVMEoTCP PDU Header
 *
 * ucType         : Specifies the type of PDU
 * ucFlags        : PDU-TYPE specific flags
 * ucHeaderLen    : Length of PDU header (not including the Header Digest) in bytes
 * ucPduDataOffset: PDU Data Offset from the start of the PDU in bytes
 * uPduLen        : Total length of PDU (includes CH, PSH, HDGST, PAD, DATA, and DDGST) in bytes
 */
typedef struct _NVMEoTCP_PDU_HDR {
	UCHAR	ucType;
	UCHAR	ucFlags;
	UCHAR	ucHeaderLen;
	UCHAR	ucPduDataOffset;
	ULONG	ulPduLen;
} NVMEoTCP_PDU_HDR, * PNVMEoTCP_PDU_HDR;

/**
 * typedef struct _NVMEOF_TCP_ICREQ_PDU - NVMEoTCP Initialize Connection Request PDU
 *
 * Hdr		      : TCP PDU header
 * usPDUFmtVersion: PDU Format Version
 * ucHPDA		  : Host PDU Data Alignment (This value is 0’s based value in
 *  										 units of dwords and must be a value
 *											 in the range 0 to 31)
 * uMaxNrOSR2T    : Host PDU header and Data digest enable option flags
 * sMaxNrOSR2T    :	Maximum Number of Outstanding R2T (MAXR2T)
 */
typedef struct _NVMEoTCP_ICREQ_PDU {
	NVMEoTCP_PDU_HDR	sHdr;
	USHORT				usPDUFmtVersion;
	UCHAR				ucHPDA;
	UCHAR				ucUseDigest;
	ULONG				ulMaxNrOSR2T;
	UCHAR				ucaReserved[112];
} NVMEoTCP_ICREQ_PDU, * PNVMEoTCP_ICREQ_PDU;

/**
 * typedef struct _NVMEoTCP_ICRESP_PDU - Initialize Connection Response PDU
 *
 * @sHdr		    : TCP PDU header
 * @sPDUFmtVersion  : PDU Format Version
 * @ucCPDA:         :  Controller PDU Data Alignment (This value is 0’s based value in
 *   										 units of dwords and must be a value
 *											 in the range 0 to 31)
 * @ucDigestEnabeled:          digest types enabled
 * @uMaxH2CDataLen  :       maximum data capsules per r2t supported
 */
typedef struct _NVMEoTCP_ICRESP_PDU {
	NVMEoTCP_PDU_HDR	sHdr;
	USHORT				usPDUFmtVersion;
	UCHAR				ucCPDA;
	UCHAR				ucDigestEnabeled;
	ULONG			    ulMaxH2CDataLen;
	UCHAR				ucaReserved[112];
} NVMEoTCP_ICRESP_PDU, * PNVMEoTCP_ICRESP_PDU;

/**
 * typedef struct _NVMEoTCP_TERM_CONN_REQ_PDU - Host to Controller and vice vera Terminate Connection Request PDU
 *
 * Hdr		   : TCP PDU header
 * FatalErrSts : Fatal Error Status
 * FatalErrInfo: Fatal Error Information
 */
typedef struct _NVMEoTCP_TERM_CONN_REQ_PDU {
	NVMEoTCP_PDU_HDR	sHdr;
	USHORT				usFatalErrSts;
	ULONG				ulFatalErrInfo;
	UCHAR				ucaReserved[8];
	UCHAR				ucaData[0];
} NVMEoTCP_TERM_CONN_REQ_PDU, * PNVMEoTCP_TERM_CONN_REQ_PDU;

/**
 * typedef struct _NVMEoTCP_CMD_PDU - NVME over TCP Command Capsule PDU
 *
 * Hdr: TCP PDU header
 * CMD: Core NVMe Command
 */
typedef struct _NVMEoTCP_CMD_PDU {
	NVMEoTCP_PDU_HDR	sHdr;
	NVME_CMD			sCMD;
} NVMEoTCP_CMD_PDU, * PNVMEoTCP_CMD_PDU;

/**
 * typedef struct _NVMEoTCP_RESP_PDU - NVME over TCP Response Capsule PDU
 *
 * Hdr: TCP PDU header
 * CQE: NVME Completion Queue Entry
 */
typedef struct _NVMEoTCP_RESP_PDU {
	NVMEoTCP_PDU_HDR		    sHdr;
	NVME_COMPLETION_QUEUE_ENTRY	sCQE;
} NVMEoTCP_RESP_PDU, * PNVMEoTCP_RESP_PDU;

/**
 * typedef struct _NVMEoTCP_READY2XFER_PDU - NVME over TCP Ready to Transfer PDU
 *
 * Hdr		  : TCP PDU header
 * usCmdCCID  : Command Capsule CID
 * usXferTag  : Transfer Tag (controller generated)
 * uReqDataLen: Byte offset from the start of the host-resident data
 * uReqDataLen: Number of bytes of command data buffer requested by the controller
 */
typedef struct _NVMEoTCP_READY2XFER_PDU {
	NVMEoTCP_PDU_HDR	sHdr;
	USHORT				usCommandID;
	USHORT				usXferTag;
	ULONG				uReqDataOff;
	ULONG				uReqDataLen;
	UCHAR				ucReserved[4];
} NVMEoTCP_READY2XFER_PDU, * PNVMEoTCP_READY2XFER_PDU;

/**
 * typedef struct _NVMEoTCP_DATA_PDU - Host To Controller and vice versa Data Transfer PDU (H2CData/C2HData)
 *
 * @sHdr		: TCP PDU header
 * @usCmdCCID	: Command Capsule CID
 * @usXferTag	: Transfer Tag (controller generated)
 * @uDataLen	: Byte offset from start of Command data buffer
 * @uDataLen	: PDU DATA field length in bytes
 */
typedef struct _NVMEoTCP_DATA_PDU {
	NVMEoTCP_PDU_HDR	sHdr;
	USHORT				usCommandID;
	USHORT				usXferTag;
	ULONG				uDataOff;
	ULONG				uDataLen;
	UCHAR				ucaReserved[4];
	UCHAR				ucaData[0];
} NVMEoTCP_DATA_PDU, * PNVMEoTCP_DATA_PDU;

typedef union _NVMEoTCP_PDU {
	NVMEoTCP_ICREQ_PDU	       sICReq;
	NVMEoTCP_ICRESP_PDU	       sICResp;
	NVMEoTCP_CMD_PDU           sCmd;
	NVMEoTCP_RESP_PDU	       sResp;
	NVMEoTCP_READY2XFER_PDU    sRdy2xfr;
	NVMEoTCP_DATA_PDU          sData;
	NVMEoTCP_TERM_CONN_REQ_PDU sConnTerm;
} NVMEoTCP_PDU, * PNVMEoTCP_PDU;

#pragma pack(pop)