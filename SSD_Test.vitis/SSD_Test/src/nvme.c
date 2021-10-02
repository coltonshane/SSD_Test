/*
Lightweight NVMe Driver

Copyright (C) 2021 by Shane W. Colton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// Include Headers -----------------------------------------------------------------------------------------------------

#include "nvme.h"
#include "nvme_priv.h"
// #include "xil_cache.h"
// #include "xil_mmu.h"
#include "xil_io.h"
#include "xtime_l.h"

// Private Pre-Processor Definitions -----------------------------------------------------------------------------------

#define ASQ_SIZE 0xF                // Admin Submission Queue Size: 16 Entries (0's Based)
#define ACQ_SIZE 0xF                // Admin Completion Queue Size: 16 Entries (0's Based)
#define IOSQ_SIZE 0x3F              // I/O Submission Queue Size: 64 Entries (0's Based)
#define IOCQ_SIZE 0x3F              // I/O Completion Queue Size: 64 Entries (0's Based)

// 4KiB Page < (2^1 Bank Groups * 2^2 Banks * 2^10 Columns * 64b)
#define DDR_PAGE_EXP 12
#define DDR_PAGE_SIZE (1 << DDR_PAGE_EXP)
#define DDR_PAGE_MASK (DDR_PAGE_SIZE - 1)

#define WORKLOAD_SEQUENTIAL 0x2     // Workload Hint for NVMe Controller

// Private Type Definitions --------------------------------------------------------------------------------------------

// Private Function Prototypes -----------------------------------------------------------------------------------------

int nvmeInitBridge(void);
void nvmeInitAdminQueue(void);
int nvmeInitController(u32 tTimeout_ms);
int nvmeIdentifyController(u32 tTimeout_ms);
int nvmeIdentifyNamespace(u32 tTimeout_ms);
int nvmeSetPowerState(u8 PS, u8 WH, u32 tTimeout_ms);
int nvmeCreateIOQueues(u32 tTimeout_ms);
int nvmeGetSMARTHealth(void);

void nvmeParsePowerStates();

int nvmeAdminCommand(const sqe_prp_type * sqe, cqe_type * cqe, u32 tTimeout_ms);
void nvmeSubmitAdminCommand(const sqe_prp_type * sqe);
int nvmeCompleteAdminCommand(cqe_type * cqe, u32 tTimeout_ms);

void nvmeSubmitIOCommand(const sqe_prp_type * sqe);
int nvmeCompleteIOCommands(cqe_type * cqe, u16 maxCompletions);

int nvmeCheckTimeout(XTime tStart, u32 tTimeout_ms);

// Public Global Variables ---------------------------------------------------------------------------------------------

// Private Global Variables --------------------------------------------------------------------------------------------

// AXI/PCIE Bridge and Device Registers
u32 * regPhyStatusControl =      (u32 *)(0x500000144);
u32 * regRootPortStatusControl = (u32 *)(0x500000148);
u32 * regDeviceClassCode = 		 (u32 *)(0x500100008);	// This u32 includes Class Code (31:8) and Revision ID (7:0).

// NVME Controller Registers, via AXI BAR
// Root Port Bridge must be enabled through regRootPortStatusControl for R/W access.
u64 * regCAP =     (u64 *)(0xB0000000);					// Controller Capabilities
u32 * regCC =      (u32 *)(0xB0000014);					// Controller Configuration
u32 * regCSTS =    (u32 *)(0xB000001C);					// Controller Status
u32 * regAQA =     (u32 *)(0xB0000024);					// Admin Queue Attributes
u64 * regASQ =     (u64 *)(0xB0000028);					// Admin Submission Queue Base Address
u64 * regACQ =     (u64 *)(0xB0000030);					// Admin Completion Queue Base Address
u32 * regSQ0TDBL = (u32 *)(0xB0001000);					// Admin Submission Queue Tail Doorbell
u32 * regCQ0HDBL = (u32 *)(0xB0001004);					// Admin Completion Queue Head Doorbell
u32 * regSQ1TDBL = (u32 *)(0xB0001008);					// I/O Submission Queue Tail Doorbell
u32 * regCQ1HDBL = (u32 *)(0xB000100C);					// I/O Completion Queue Head Doorbell

// Submission and Completion Queues
// Must be page-aligned at least large enough to fit the queue sizes defined above.
sqe_prp_type * asq =  (sqe_prp_type *)(0x10000000);		// Admin Submission Queue
cqe_type * acq =          (cqe_type *)(0x10001000);		// Admin Completion Queue
sqe_prp_type * iosq = (sqe_prp_type *)(0x10002000);		// I/O Submission Queue
cqe_type * iocq =         (cqe_type *)(0x10003000);		// I/O Completion Queue

// Identify Structures
idController_type * idController = (idController_type *)(0x10004000);
idNamespace_type * idNamespace = (idNamespace_type *)(0x10005000);
logSMARTHealth_type * logSMARTHealth = (logSMARTHealth_type *)(0x10006000);

// Dataset Management Ranges (256 * 16B = 4096B)
dsmRange_type * dsmRange = (dsmRange_type *)(0x10007000);

// Heap space for PRP lists for IO Transfers.
// Heap size is (IOSQ_SIZE + 1) * DDR_PAGE_SIZE.
u64 * prpListHeap = (u64 *)(0x10008000);

descPowerState_type descPowerState[32];

u16 asq_tail_local = 0;
u16 acq_head_local = 0;
u8 acq_phase = 0;
u16 iosq_tail_local = 0;
u16 iocq_head_local = 0;
u8 iocq_phase = 0;

int nvmeStatus = NVME_NOINIT;
u32 nsid = 1;
u8 lba_exp = 9;
u8 ps_idle = 0;
u32 lba_size = 512;
u16 admin_cid = 0;
u16 io_cid = 0;
u16 io_cid_last_completed = 0xFFFF;

// Interrupt Handlers --------------------------------------------------------------------------------------------------

// Public Function Definitions -----------------------------------------------------------------------------------------

int nvmeInit(void)
{
	nvmeStatus = NVME_OK;

	nvmeStatus |= nvmeInitBridge();
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	nvmeInitAdminQueue();

	nvmeStatus |= nvmeInitController(1000);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	nvmeStatus |= nvmeIdentifyController(10);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	nvmeStatus |= nvmeIdentifyNamespace(10);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	// nvmeStatus |= nvmeSetPowerState(0, WORKLOAD_SEQUENTIAL, 1000);
	// if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	nvmeStatus |= nvmeCreateIOQueues(10);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	nvmeGetMetrics();

	return nvmeStatus;
}

int nvmeGetStatus(void)
{
	return nvmeStatus;
}

u64 nvmeGetLBACount(void)
{
	if(nvmeStatus == NVME_OK)
	{ return idNamespace->NSZE; }
	else
	{ return 0; }
}

u16 nvmeGetLBASize(void)
{
	if(nvmeStatus == NVME_OK)
	{ return (1 << lba_exp); }
	else
	{ return 0; }
}

int nvmeGetMetrics(void)
{
	return nvmeGetSMARTHealth();
}

float nvmeGetTemp(void)
{
	// TO-DO: Move to calibration.
	static float nvmeDN0 = 0.0f;
	static float nvmeT0 = -273.15f;
	static float nvmeTSlope = 1.0f;

	static float nvmeTf = -100.0f;

	float nvmeT = (logSMARTHealth->Composite_Temperature - nvmeDN0) * nvmeTSlope + nvmeT0;
	if(nvmeTf == -100.0f)
	{
		nvmeTf = nvmeT;
	}
	else
	{
		nvmeTf = 0.95f * nvmeTf + 0.05f * nvmeT;
	}

	return nvmeTf;
}

int nvmeWrite(const u8 * srcByte, u64 destLBA, u32 numLBA)
{
	sqe_prp_type sqe;
	int nLBA = numLBA;
	int nPRP;
	int offset;
	u64 * prpList = prpListHeap + ((io_cid & IOSQ_SIZE) * (DDR_PAGE_SIZE >> 3));

	if ((u64) srcByte & 0x3) { return 1; } 	// Must be DWORD-aligned!

	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = io_cid;
	sqe.OPC = 0x01;
	sqe.NSID = nsid;
	sqe.PRP1 = (u64) srcByte;
	sqe.CDW10 = destLBA & 0xFFFFFFFF;
	sqe.CDW11 = (destLBA >> 32) & 0XFFFFFFFF;
	sqe.CDW12 = numLBA - 1; // 0's Based

	// Subtract off the integer number of LBAs covered by the first PRP.
	offset = (u64) srcByte & DDR_PAGE_MASK;
	nLBA -= (DDR_PAGE_SIZE - offset) >> lba_exp;

	// If there is more data to transfer...
	if(nLBA > 0)
	{
		// Move the source pointer to its page boundary.
		srcByte -= (u64) offset;

		nPRP = ((nLBA - 1) >> (DDR_PAGE_EXP - lba_exp)) + 1;
		if(nPRP > 1)
		{
			// 2 or more PRPs remaining, use a list.
			sqe.PRP2 = (u64) prpList;
			for(int p = 1; p <= nPRP; p++)
			{
				prpList[p-1] = (u64)(srcByte + (p << DDR_PAGE_EXP));
			}
		}
		else
		{
			// 1 PRP remaining, fits in the command itself.
			sqe.PRP2 = (u64) (srcByte + (1 << DDR_PAGE_EXP));
		}
	}

	nvmeSubmitIOCommand(&sqe);

	return 0;
}

int nvmeFlush()
{
	sqe_prp_type sqe;
	XTime tStart;
	XTime_GetTime(&tStart);

	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = io_cid;
	sqe.OPC = 0x00;
	sqe.NSID = nsid;

	nvmeSubmitIOCommand(&sqe);

	return 0;
}

int nvmeRead(u8 * destByte, u64 srcLBA, u32 numLBA)
{
	sqe_prp_type sqe;
	int nLBA = numLBA;
	int nPRP;
	int offset;
	u64 * prpList = prpListHeap + ((io_cid & IOSQ_SIZE) * (DDR_PAGE_SIZE >> 3));

	if ((u64) destByte & 0x3) { return 1; } 	// Must be DWORD-aligned!

	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = io_cid;
	sqe.OPC = 0x02;
	sqe.NSID = nsid;
	sqe.PRP1 = (u64) destByte;
	sqe.CDW10 = srcLBA & 0xFFFFFFFF;
	sqe.CDW11 = (srcLBA >> 32) & 0XFFFFFFFF;
	sqe.CDW12 = numLBA - 1; // 0's Based

	// Subtract off the integer number of LBAs covered by the first PRP.
	offset = (u64) destByte & DDR_PAGE_MASK;
	nLBA -= (DDR_PAGE_SIZE - offset) >> lba_exp;

	// If there is more data to transfer...
	if(nLBA > 0)
	{
		// Move the destination pointer to its page boundary.
		destByte -= (u64) offset;

		nPRP = ((nLBA - 1) >> (DDR_PAGE_EXP - lba_exp)) + 1;
		if(nPRP > 1)
		{
			// 2 or more PRPs remaining, use a list.
			sqe.PRP2 = (u64) prpList;
			for(int p = 1; p <= nPRP; p++)
			{
				prpList[p-1] = (u64)(destByte + (p << DDR_PAGE_EXP));
			}
		}
		else
		{
			// 1 PRP remaining, fits in the command itself.
			sqe.PRP2 = (u64) (destByte + (1 << DDR_PAGE_EXP));
		}
	}

	nvmeSubmitIOCommand(&sqe);

	return 0;
}

int nvmeServiceIOCompletions(u16 maxCompletions)
{
	u16 numCompletions;
	cqe_type cqeLastCompleted;

	numCompletions = nvmeCompleteIOCommands(&cqeLastCompleted, maxCompletions);

	return numCompletions;
}

u16 nvmeGetIOSlip(void)
{
	return (u16)(io_cid - io_cid_last_completed - 1);
}

int nvmeTrim(u64 startLBA, u32 numLBA)
{
	sqe_prp_type sqe;

	// Use a single range.
	memset(dsmRange, 0, 4096);
	dsmRange[0].contextAttributes = 0x00000000;
	dsmRange[0].start = startLBA;
	dsmRange[0].length = numLBA;

	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = io_cid;
	sqe.OPC = 0x09;
	sqe.NSID = nsid;
	sqe.PRP1 = (u64) dsmRange;
	sqe.CDW10 = 0;   // 0's Based
	sqe.CDW11 = 0x4; // Deallocate (AD) flag.

	nvmeSubmitIOCommand(&sqe);

	return 0;
}

// Private Function Definitions ----------------------------------------------------------------------------------------

int nvmeInitBridge(void)
{
	if(*regPhyStatusControl != PHY_OK) { return NVME_ERROR_PHY; }
	if((*regDeviceClassCode >> 8) != CLASS_CODE_OK) { return NVME_ERROR_DEV_CLASS; }

	// TO-DO: Additional device-level checks, e.g. that we are in Power State D0.
	// TO-DO: BAR setup. Note that we have AXI 0xA0000000-0xAFFFFFFF mapped to PCIe 0x00000000-0x0FFFFFFF.

	*regRootPortStatusControl |= BRIDGE_ENABLE;

	return NVME_OK;
}

void nvmeInitAdminQueue(void)
{
	*regAQA = (ACQ_SIZE << 16) | ASQ_SIZE;
	*regASQ = (u64) asq;
	*regACQ = (u64) acq;
}

int nvmeInitController(u32 tTimeout_ms)
{
	XTime tStart;
	u64 capability;

	// I/O Completion Queue Entry Size
	// TO-DO: This is fixed at 4 in NVMe v1.4, but should be pulled from Identify Controller CQES.
	*regCC &= ~REG_CC_IOCQES_Msk;
	*regCC |= (0x4) << REG_CC_IOCQES_Pos;

	// I/O Submission Queue Entry Size
	// TO-DO: This is fixed at 6 in NVMe v1.4, but should be pulled from Identify Controller SQES.
	*regCC &= ~REG_CC_IOSQES_Msk;
	*regCC |= (0x6) << REG_CC_IOSQES_Pos;

	// Arbitration Mechanism: Round Robin
	*regCC &= ~REG_CC_AMS_Msk;
	*regCC |= (0x0) << REG_CC_AMS_Pos;

	// Memory Page Size: 4KiB (Minimum)
	capability = (*regCAP & REG_CAP_MPSMIN_Msk) >> REG_CAP_MPSMIN_Pos;
	if(capability > 0) { return NVME_ERROR_MIN_PAGE_SIZE; }
	*regCC &= ~REG_CC_MPS_Msk;
	*regCC |= (0x0) << REG_CC_MPS_Pos;

	// I/O Command Set: NVM Command Set
	capability = (*regCAP & REG_CAP_CCS_Msk) >> REG_CAP_CCS_Pos;
	if((capability & 0x1) == 0) { return NVME_ERROR_COMMAND_SET;}
	*regCC &= ~REG_CC_CSS_Msk;
	*regCC |= (0x0) << REG_CC_CCS_Pos;

	// Doorbell Stride: Realign Pointers if Necessary
	capability = (*regCAP & REG_CAP_DSTRD_Msk) >> REG_CAP_DSTRD_Pos;
	if(capability > 0)
	{
		regCQ0HDBL = regSQ0TDBL + 1 * (1 << (2 + capability));
		regSQ1TDBL = regSQ0TDBL + 2 * (1 << (2 + capability));
		regCQ1HDBL = regSQ0TDBL + 3 * (1 << (2 + capability));
	}

	// Initialize all queue memory to zeros.
	memset(asq, 0, (ASQ_SIZE + 1) * sizeof(sqe_prp_type));
	memset(acq, 0, (ACQ_SIZE + 1) * sizeof(cqe_type));
	memset(iosq, 0, (IOSQ_SIZE + 1) * sizeof(sqe_prp_type));
	memset(iocq, 0, (IOCQ_SIZE + 1) * sizeof(cqe_type));

	// Enable Controller
	*regCC |= REG_CC_EN;

	// Wait for Controller Ready with Timeout
	XTime_GetTime(&tStart);
	do
	{
		if(nvmeCheckTimeout(tStart, tTimeout_ms)) { return NVME_ERROR_CSTS_RDY_TIMEOUT; }
	}
	while((*regCSTS & REG_CSTS_RDY) == 0);

	return NVME_OK;
}

int nvmeIdentifyController(u32 tTimeout_ms)
{
	u32 nvmeStatus = NVME_OK;
	sqe_prp_type sqe;
	cqe_type cqe;

	// Identify Controller
	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x06;
	sqe.PRP1 = (u64) idController;
	sqe.CDW10 = 0x00000001;
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, tTimeout_ms);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	if (idController->SQES != 0x66) { return NVME_ERROR_QUEUE_TYPE; }
	if (idController->CQES != 0x44) { return NVME_ERROR_QUEUE_TYPE; }

	nvmeParsePowerStates();

	return NVME_OK;
}

int nvmeIdentifyNamespace(u32 tTimeout_ms)
{
	u32 nvmeStatus = NVME_OK;
	sqe_prp_type sqe;
	cqe_type cqe;

	// First, get a list of all Active NSIDs.
	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x06;
	sqe.PRP1 = (u64) idNamespace;
	sqe.CDW10 = 0x00000002;
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, tTimeout_ms);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	// Take the first NSID on the list, just in case it's not NSID 1.
	nsid = *(u32 *) ((u64) idNamespace);

	// Now, fill in the Identify Namespace struct for that NSID.
	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x06;
	sqe.NSID = nsid;
	sqe.PRP1 = (u64) idNamespace;
	sqe.CDW10 = 0x00000000;
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, tTimeout_ms);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	lba_exp = (idNamespace->LBAF[idNamespace->FLBAS]) >> 16;
	if((lba_exp < 9) || (lba_exp > 12)) { return NVME_ERROR_LBA_SIZE; }
	lba_size = (1 << lba_exp);

	return NVME_OK;
}

int nvmeSetPowerState(u8 PS, u8 WH, u32 tTimeout_ms)
{
	u32 nvmeStatus = NVME_OK;
	sqe_prp_type sqe;
	cqe_type cqe;

	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x09;
	sqe.NSID = nsid;
	sqe.CDW10 = 0x02;
	sqe.CDW11 = (PS & 0x1F);
	if(PS == 0)
	{
		// Supply a workload hint only for PS0. TO-DO: Supply for all operational power states.
		sqe.CDW11 |= (WH & 0x7) << 5;
	}
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, tTimeout_ms);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	// Not checking for Power State in the CQE because it may indicate the current state rather than the target state.

	return NVME_OK;
}

int nvmeCreateIOQueues(u32 tTimeout_ms)
{
	u32 nvmeStatus = NVME_OK;
	sqe_prp_type sqe;
	cqe_type cqe;

	// Create I/O Completion Queue
	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x05;
	sqe.PRP1 = (u64) iocq;
	sqe.CDW10 = (IOCQ_SIZE << 16) | 0x0001;
	sqe.CDW11 = 0x00000001;
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, tTimeout_ms);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }
	if(cqe.SF_P >> 1) { return NVME_ERROR_QUEUE_CREATION; }

	// Create I/O Submission Queue
	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x01;
	sqe.PRP1 = (u64) iosq;
	sqe.CDW10 = (IOSQ_SIZE << 16) | 0x0001;
	sqe.CDW11 = 0x00010001;
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, tTimeout_ms);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }
	if(cqe.SF_P >> 1) { return NVME_ERROR_QUEUE_CREATION; }

	return NVME_OK;
}

int nvmeGetSMARTHealth(void)
{
	u32 nvmeStatus = NVME_OK;
	sqe_prp_type sqe;
	cqe_type cqe;

	// Get Log Page 02: SMART / Health Information
	memset(&sqe, 0, sizeof(sqe_prp_type));
	sqe.CID = admin_cid;
	sqe.OPC = 0x02;
	sqe.NSID = 0xFFFFFFFF;	// Scope: Controller
	sqe.PRP1 = (u64) logSMARTHealth;
	sqe.CDW10 = 0x007F0002;	// 128DWORD (512B) of Log Identifier 0x02
	nvmeStatus = nvmeAdminCommand(&sqe, &cqe, 0);
	if(nvmeStatus != NVME_OK) { return nvmeStatus; }

	return NVME_OK;
}

void nvmeParsePowerStates(void)
{
	u32 powerScale;
	u64 psBaseAddress;

	for(int i = 0; i <= idController->NPSS; i++)
	{
		psBaseAddress = (u64)(idController->PSD0) + 32*i;

		// Max Power
		descPowerState[i].pMax = (float)(*(u16 *)(psBaseAddress + PSD_MXPS_Offset));
		powerScale = ((*(u32 *)(psBaseAddress + PSD_MXPS_Offset)) & PSD_MXPS_Msk) >> PSD_MXPS_Pos;
		switch(powerScale)
		{
			case 1: descPowerState[i].pMax *= 0.0001f; break;
			default: descPowerState[i].pMax *= 0.01f; break;
		}

		// Idle Power
		descPowerState[i].pIdle = (float)(*(u16 *)(psBaseAddress + PSD_IPS_Offset));
		powerScale = ((*(u32 *)(psBaseAddress + PSD_IPS_Offset)) & PSD_IPS_Msk) >> PSD_IPS_Pos;
		switch(powerScale)
		{
		case 1: descPowerState[i].pIdle *= 0.0001f; break;
		case 2: descPowerState[i].pIdle *= 0.01f; break;
		default: descPowerState[i].pIdle *= 0.0f; break;
		}

		// Active Power
		descPowerState[i].pActive = (float)(*(u16 *)(psBaseAddress + PSD_APS_Offset));
		powerScale = ((*(u32 *)(psBaseAddress + PSD_APS_Offset)) & PSD_IPS_Msk) >> PSD_IPS_Pos;
		switch(powerScale)
		{
		case 1: descPowerState[i].pActive *= 0.0001f; break;
		case 2: descPowerState[i].pActive *= 0.01f; break;
		default: descPowerState[i].pActive *= 0.0f; break;
		}

		// Entry and Exit Times
		descPowerState[i].tEnter_us = *(u32 *)(psBaseAddress + PSD_ENLAT_Offset);
		descPowerState[i].tExit_us = *(u32 *)(psBaseAddress + PSD_EXLAT_Offset);

		// Other Fields
		descPowerState[i].NOPS = ((*(u32 *)(psBaseAddress + PSD_NOPS_Offset)) & PSD_NOPS_Msk) >> PSD_NOPS_Pos;
		descPowerState[i].RRT = ((*(u32 *)(psBaseAddress + PSD_RXX_Offset)) & PSD_RRT_Msk) >> PSD_RRT_Pos;
		descPowerState[i].RRL = ((*(u32 *)(psBaseAddress + PSD_RXX_Offset)) & PSD_RRL_Msk) >> PSD_RRL_Pos;
		descPowerState[i].RWT = ((*(u32 *)(psBaseAddress + PSD_RXX_Offset)) & PSD_RWT_Msk) >> PSD_RWT_Pos;
		descPowerState[i].RWL = ((*(u32 *)(psBaseAddress + PSD_RXX_Offset)) & PSD_RWL_Msk) >> PSD_RWL_Pos;
		descPowerState[i].APW = ((*(u32 *)(psBaseAddress + PSD_APW_Offset)) & PSD_APW_Msk) >> PSD_APW_Pos;

		// Set the idle power state to the first one to indicate NOPS.
		if((descPowerState[i].NOPS) && (ps_idle == 0))
		{
			ps_idle = i;
		}
	}
}

int nvmeAdminCommand(const sqe_prp_type * sqe, cqe_type * cqe, u32 tTimeout_ms)
{
	u16 admin_cid_wait = admin_cid;
	u32 nvmeStatus = NVME_OK;
	XTime tStart;
	XTime_GetTime(&tStart);

	nvmeSubmitAdminCommand(sqe);

	// Don't wait for completion if tTimeout_ms == 0.
	if(tTimeout_ms == 0) { return NVME_OK; }

	do
	{
		nvmeStatus |= nvmeCompleteAdminCommand(cqe, tTimeout_ms);
		if(nvmeStatus != NVME_OK) { return nvmeStatus; }

		if(nvmeCheckTimeout(tStart, tTimeout_ms)) { return NVME_ERROR_ADMIN_COMMAND_TIMEOUT; }
	} while (cqe->CID != admin_cid_wait);


	return NVME_OK;
}

void nvmeSubmitAdminCommand(const sqe_prp_type * sqe)
{
	u64 asq_offset = asq_tail_local * sizeof(sqe_prp_type);
	memcpy((void *)((u64)asq + asq_offset), sqe, sizeof(sqe_prp_type));
	asq_tail_local = (asq_tail_local + 1) & ASQ_SIZE;
	admin_cid++;

	isb(); dsb(); // Xil_DCacheFlush();
	*regSQ0TDBL = asq_tail_local;
}

// Blocking Admin Command Completion
int nvmeCompleteAdminCommand(cqe_type * cqe, u32 tTimeout_ms)
{
	XTime tStart;
	cqe_type * cqeTemp;
	u64 acq_offset = acq_head_local * sizeof(cqe_type);

	XTime_GetTime(&tStart);
	do
	{
		isb(); dsb(); // Xil_DCacheInvalidate();
		cqeTemp = (cqe_type *)((u64)acq + acq_offset);

		if(nvmeCheckTimeout(tStart, tTimeout_ms)) { return NVME_ERROR_ACQ_TIMEOUT; }
	} while((cqeTemp->SF_P & 0x0001) == acq_phase);

	acq_head_local = (acq_head_local + 1) & ACQ_SIZE;
	if(acq_head_local == 0) { acq_phase ^= 0x01; }

	isb(); dsb(); // Xil_DCacheFlush();
	*regCQ0HDBL = acq_head_local;

	*cqe = *cqeTemp;

	return NVME_OK;
}

void nvmeSubmitIOCommand(const sqe_prp_type * sqe)
{
	u64 iosq_offset = iosq_tail_local * sizeof(sqe_prp_type);
	memcpy((void *)((u64)iosq + iosq_offset), sqe, sizeof(sqe_prp_type));
	iosq_tail_local = (iosq_tail_local + 1) & IOSQ_SIZE;
	io_cid++;

	isb(); dsb(); // Xil_DCacheFlush();
	*regSQ1TDBL = iosq_tail_local;
}

// Non-Blocking IO Command Completion
int nvmeCompleteIOCommands(cqe_type * cqe, u16 nCompletionsMax)
{
	u32 nCompletions = 0;
	cqe_type * cqeTemp;
	u64 iocq_offset;

	for(nCompletions = 0; nCompletions < nCompletionsMax; nCompletions++)
	{
		iocq_offset = iocq_head_local * sizeof(cqe_type);

		isb(); dsb(); // Xil_DCacheInvalidate();
		cqeTemp = (cqe_type *)((u64)iocq + iocq_offset);

		if((cqeTemp->SF_P & 0x0001) == iocq_phase) { break; }

		io_cid_last_completed = cqeTemp->CID;

		iocq_head_local = (iocq_head_local + 1) & IOCQ_SIZE;
		if(iocq_head_local == 0) { iocq_phase ^= 0x01; }
	}

	if(nCompletions > 0)
	{
		isb(); dsb(); // Xil_DCacheFlush();
		*regCQ1HDBL = iocq_head_local;
	}

	*cqe = *cqeTemp;

	return nCompletions;
}

int nvmeCheckTimeout(XTime tStart, u32 tTimeout_ms)
{
	XTime tNow;
	u64 tElapsed_ms;

	XTime_GetTime(&tNow);
	tElapsed_ms = (tNow - tStart) / (COUNTS_PER_SECOND / 1000);

	return (tElapsed_ms >= tTimeout_ms);
}

