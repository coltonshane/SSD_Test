/*
Lightweight NVMe Driver Includes

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

#ifndef __NVME_INCLUDE__
#define __NVME_INCLUDE__

// Include Headers -----------------------------------------------------------------------------------------------------

#include "xil_types.h"

// Public Pre-Processor Definitions ------------------------------------------------------------------------------------

#define NVME_NOINIT                        0xFFFFFFFF
#define NVME_OK                            0x00000000
#define NVME_ERROR_PHY                     0x00000001
#define NVME_ERROR_DEV_CLASS               0x00000002
#define NVME_ERROR_MIN_PAGE_SIZE           0x00000004
#define NVME_ERROR_COMMAND_SET             0x00000008
#define NVME_ERROR_CSTS_RDY_TIMEOUT        0x00000010
#define NVME_ERROR_ACQ_TIMEOUT             0x00000020
#define NVME_ERROR_ADMIN_COMMAND_TIMEOUT   0x00000040
#define NVME_ERROR_QUEUE_TYPE              0x00000080
#define NVME_ERROR_MAX_TRANSFER_SIZE	   0x00000100
#define NVME_ERROR_LBA_SIZE                0x00000200
#define NVME_ERROR_POWER_STATE_TRANSITION  0x00000400
#define NVME_ERROR_QUEUE_CREATION          0x00000800

#define NVME_RW_OK                         0x00000000
#define NVME_RW_BAD_ALIGNMENT              0x00000001

// Public Type Definitions ---------------------------------------------------------------------------------------------

// Public Function Prototypes ------------------------------------------------------------------------------------------

int nvmeInit(void);
int nvmeGetStatus(void);
u64 nvmeGetLBACount(void);
u16 nvmeGetLBASize(void);
int nvmeGetMetrics(void);
float nvmeGetTemp(void);

int nvmeWrite(const u8 * srcByte, u64 destLBA, u32 numLBA);
int nvmeFlush();
int nvmeRead(u8 * destByte, u64 srcLBA, u32 numLBA);
int nvmeServiceIOCompletions(u16 maxCompletions);
u16 nvmeGetIOSlip(void);
int nvmeTrim(u64 startLBA, u32 numLBA);

// Externed Public Global Variables ------------------------------------------------------------------------------------

extern u8 lba_exp;

#endif
