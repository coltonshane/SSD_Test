/*
NVMe SSD Test Application Main
*/

#include "platform.h"
#include "pcie.h"
#include "nvme.h"
#include "ff.h"
#include "diskio.h"
#include "xtime_l.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xgpiops.h"
#include "xuartps.h"
#include "sleep.h"
#include <stdio.h>

#define US_PER_COUNT 1000 / (COUNTS_PER_SECOND / 1000)

// Test Configuration
#define TRIM_FIRST          1           // 0: Don't TRIM, 1: TRIM before test
#define TRIM_DELAY          0           // Extra wait time after TRIM in [min]. 0 = Wait for keypress.
#define USE_FS              0           // 0: Raw Disk Test, 1: File System Test
#define TEST_READ           0           // 0: Write, 1: Read (Raw Disk Test Only)
#define TOTAL_WRITE         1999        // Total write size in [GB].
#define TARGET_WRITE_RATE   4000        // Target write speed in [MB/s].
#define TOTAL_READ          1999        // Total read size in [GB]. (Raw Disk Test Only)
#define TARGET_READ_RATE    4000        // Target read speed in [MB/s]. (Raw Disk Test Only)
#define BLOCK_SIZE          (1 << 16)   // Block size in [B] as a power of 2.
#define BLOCKS_PER_FILE     (1 << 18)   // Blocks written per file in FS mode. (File System Test Only)
#define FS_AU_SIZE          (1 << 20)   // File system AU size in [B] as a power of 2. (File System Test Only)
#define NVME_SLIP_ALLOWED   16          // Amount of NVMe commands allowed to be in flight.

void trimWait(u32 waitMin);
void diskWriteTest();
void diskReadTest();
void fsWriteTest();

// Large data buffer in RAM for write source and read destination.
u8 * const data = (u8 * const) (0x20000000);

// GPIO Global Variables
XGpioPs Gpio;
XGpioPs_Config *gpioConfig;

int main()
{
    // Init
	init_platform();
    Xil_DCacheDisable();
    xil_printf("NVMe SSD test application started.\r\n");
    xil_printf("Initializing PCIe and NVMe driver...\r\n");

    // Configure PERST# pin and write it low then high to reset SSD.
    gpioConfig = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);
    XGpioPs_CfgInitialize(&Gpio, gpioConfig, gpioConfig->BaseAddr);
    XGpioPs_SetDirectionPin(&Gpio, 78, 1);	// 78 = EMIO 0 = PERST#
    XGpioPs_SetOutputEnablePin(&Gpio, 78, 1);
    XGpioPs_WritePin(&Gpio, 78, 0);
    usleep(10000);
    XGpioPs_WritePin(&Gpio, 78, 1);

    // Wait 100ms then initialize PCIe.
    usleep(100000);
    pcieInit();
    usleep(10000);

    // Start NVMe Driver
	u32 nvmeStatus;
	char strResult[128];
    nvmeStatus = nvmeInit();
    if (nvmeStatus == NVME_OK)
    {
    	xil_printf("NVMe initialization successful. PCIe link is Gen3 x4.\r\n");
    }
    else
    {
    	sprintf(strResult, "NVMe driver failed to initialize. Error Code: %8x\r\n", nvmeStatus);
    	xil_printf(strResult);
    	if(nvmeStatus == NVME_ERROR_PHY) {
   			xil_printf("PCIe link must be Gen3 x4 for this example.\r\n");
   		}
   		return 0;
   	}

    usleep(10000);

    // TRIM if indicated.
    if (TRIM_FIRST)
    {
    	trimWait(TRIM_DELAY);
    }

    // Select and run test.
    if (USE_FS)
    {
    	fsWriteTest();
    }
    else if (TEST_READ)
    {
    	diskReadTest();
    }
    else
    {
    	diskWriteTest();
    }

    // Deinit
    pcieDeinit();
    xil_printf("NVMe SSD test application finished.\r\n");
    cleanup_platform();
    return 0;
}

// TRIM and Wait for Garbage Collection
void trimWait(u32 trimDelay)
{
	char strWorking[128];

	xil_printf("Deallocating SSD (TRIM)...\r\n");
	u32 nLBATrimmed = 0;
	u32 nLBAToTrim = nvmeGetLBACount();	// Trim the entire drive...
	u32 nLBAPerTrim = (1 << 17);		// ...in these increments.

	XTime tStart, tNow;
	u32 sElapsed = 0;
	float progress = 0.0f;

	XTime_GetTime(&tStart);

	// TRIM loop.
	while(nLBATrimmed < nLBAToTrim)
	{
		if((nLBAToTrim - nLBATrimmed) >= nLBAPerTrim)
		{
			nvmeTrim(nLBATrimmed, nLBAPerTrim);
			while(nvmeGetIOSlip() > 0)
			{ nvmeServiceIOCompletions(16); }
			nLBATrimmed += nLBAPerTrim;
		}
		else
		{
			// Finishing pass, if nLBAToTrim is not a multiple of nLBAPerTrim.
			nvmeTrim(nLBATrimmed, (nLBAToTrim - nLBATrimmed));
			while(nvmeGetIOSlip() > 0)
			{ nvmeServiceIOCompletions(16); }
			nLBATrimmed = nLBAToTrim;
		}

		// 1Hz progress update.
		XTime_GetTime(&tNow);
		if((tNow - tStart) / COUNTS_PER_SECOND > sElapsed)
		{
			sElapsed = (tNow - tStart) / COUNTS_PER_SECOND;

			progress = (float)nLBATrimmed / (float)nLBAToTrim * 100.0f;

			sprintf(strWorking, "TRIM Progress: %3.0f percent...\r\n", progress);
			xil_printf(strWorking);
		}
	}

	xil_printf("Finished deallocating SSD.\r\n");

	if(trimDelay == 0)
	{
		xil_printf("Enter S to start test...\r\n");
		do
		{
			while(!XUartPs_IsReceiveData(XUartPs_LookupConfig(XPAR_XUARTPS_0_DEVICE_ID)->BaseAddress));
		} while (XUartPs_RecvByte(XUartPs_LookupConfig(XPAR_XUARTPS_0_DEVICE_ID)->BaseAddress) != 'S');
	}

	while(trimDelay)
	{
		sprintf(strWorking, "Waiting after TRIM, %d minutes remaining...\r\n", trimDelay);
		xil_printf(strWorking);

		trimDelay--;
		usleep(60000000);
	}
}

// Raw Disk Write Test
void diskWriteTest()
{
	char strWorking[128];

	xil_printf("Raw disk write test started.\r\n");

	// Setup for write test.
	u64 bytesToWrite = (u64)TOTAL_WRITE * 1000000000ULL;
	u32 blocksToWrite = bytesToWrite / BLOCK_SIZE;
	u32 blocksWritten = 0;
	u32 blocksWrittenPrev = 0;
	u32 lbaPerBlock = BLOCK_SIZE / 512;
	u32 lbaDest = 0;
	u64 countsPerBlock = (u64)BLOCK_SIZE * (u64)COUNTS_PER_SECOND / (u64)(TARGET_WRITE_RATE * 1000000ULL);

	XTime tStart, tPrev, tNow;
	u32 sElapsed = 0;
	float rate = 0.0f;
	float totalWrittenGB = 0.0f;

	XTime_GetTime(&tStart);
	tPrev = tStart;

	xil_printf("Time [s], Rate [MB/s], Total [GB]\r\n");

	// Block writing loop.
	while(blocksWritten < blocksToWrite)
	{
		// Target data rate limiter.
		do { XTime_GetTime(&tNow); }
		while (tNow - tPrev < countsPerBlock);
		tPrev = tNow;

		// 1Hz progress update.
		if((tNow - tStart) / COUNTS_PER_SECOND > sElapsed)
		{
			sElapsed = (tNow - tStart) / COUNTS_PER_SECOND;

			rate = (float)((blocksWritten - blocksWrittenPrev) * BLOCK_SIZE) * 1e-6f;
			blocksWrittenPrev = blocksWritten;

			totalWrittenGB = (float)((u64)blocksWritten * (u64)BLOCK_SIZE) * 1e-9f;

			sprintf(strWorking, "%8d,%12.3f,%11.3f\r\n", sElapsed, rate, totalWrittenGB);
			xil_printf(strWorking);
		}

		// Add marker to data.
		*(u32 *) data = blocksWritten;

		// Write block.
		nvmeWrite(data, (u64) lbaDest, lbaPerBlock);
		while(nvmeGetIOSlip() > NVME_SLIP_ALLOWED)
		{ nvmeServiceIOCompletions(16); }
		blocksWritten++;
		lbaDest += lbaPerBlock;
	}

	xil_printf("Raw disk write test finished.\r\n");
}

// Raw Disk Read Test
void diskReadTest()
{
	char strWorking[128];

	xil_printf("Raw disk I/O read test started.\r\n");

	// Setup for read test.
	u64 bytesToRead = (u64)TOTAL_READ * 1000000000ULL;
	u32 blocksToRead = bytesToRead / BLOCK_SIZE;
	u32 blocksRead = 0;
	u32 blocksReadPrev = 0;
	u32 lbaPerBlock = BLOCK_SIZE / 512;
	u32 lbaSrc = 0;
	u64 countsPerBlock = (u64)BLOCK_SIZE * (u64)COUNTS_PER_SECOND / (u64)(TARGET_READ_RATE * 1000000ULL);

	XTime tStart, tPrev, tNow;
	u32 sElapsed = 0;
	float rate = 0.0f;
	float totalReadGB = 0.0f;

	XTime_GetTime(&tStart);
	tPrev = tStart;

	xil_printf("Time [s], Rate [MB/s], Total [GB]\r\n");

	// Block reading loop.
	while(blocksRead < blocksToRead)
	{
		// Target data rate limiter.
		do { XTime_GetTime(&tNow); }
		while (tNow - tPrev < countsPerBlock);
		tPrev = tNow;

		// 1Hz progress update.
		if((tNow - tStart) / COUNTS_PER_SECOND > sElapsed)
		{
			sElapsed = (tNow - tStart) / COUNTS_PER_SECOND;

			rate = (float)((blocksRead - blocksReadPrev) * BLOCK_SIZE) * 1e-6f;
			blocksReadPrev = blocksRead;

			totalReadGB = (float)((u64)blocksRead * (u64)BLOCK_SIZE) * 1e-9f;

			sprintf(strWorking, "%8d,%12.3f,%11.3f\r\n", sElapsed, rate, totalReadGB);
			xil_printf(strWorking);
		}

		// Read block.
		nvmeRead(data, (u64) lbaSrc, lbaPerBlock);
		while(nvmeGetIOSlip() > NVME_SLIP_ALLOWED)
		{ nvmeServiceIOCompletions(16); }
		blocksRead++;
		lbaSrc += lbaPerBlock;
	}

	xil_printf("Raw disk I/O read test finished.\r\n");
}

// File System Write Test
void fsWriteTest()
{
	char strWorking[128];

	xil_printf("File system write test started.\r\n");
	xil_printf("Formatting disk...\r\n");

	// Set up the file system and format the drive.
	FATFS fs;
	MKFS_PARM opt;
	FIL fil;
	FRESULT res;
	UINT bw;
	BYTE work[FF_MAX_SS];
	opt.fmt = FM_EXFAT;
	opt.au_size = FS_AU_SIZE;
	opt.align = 1;
	opt.n_fat = 1;
	opt.n_root = 512;
	res = f_mkfs("", &opt, work, sizeof work);
	if(res)
	{
		xil_printf("Failed to create file system on disk.\r\n");
		return;
	}

	// Mount the drive.
	res = f_mount(&fs, "", 0);
	if(res)
	{
		xil_printf("Failed to mount disk.\r\n");
		return;
	}
	xil_printf("Disk formatted and mounted successfully.\r\n");

	// Create first file.
	u32 nFile = 0;
	sprintf(strWorking, "f%06d.bin\n", nFile);
	f_open(&fil, strWorking, FA_CREATE_NEW | FA_WRITE);

	// Setup for write test.
	u64 bytesToWrite = (u64)TOTAL_WRITE * 1000000000ULL;
	u32 blocksToWrite = bytesToWrite / BLOCK_SIZE;
	u32 blocksWritten = 0;
	u32 blocksWrittenPrev = 0;
	u32 lbaPerBlock = BLOCK_SIZE / 512;
	u32 lbaDest = 0;
	u64 countsPerBlock = (u64)BLOCK_SIZE * (u64)COUNTS_PER_SECOND / (u64)(TARGET_WRITE_RATE * 1000000ULL);

	XTime tStart, tPrev, tNow;
	u32 sElapsed = 0;
	float rate = 0.0f;
	float totalWrittenGB = 0.0f;

	XTime_GetTime(&tStart);
	tPrev = tStart;

	xil_printf("Time [s], Rate [MB/s], Total [GB]\r\n");

	// Block writing loop.
	while(blocksWritten < blocksToWrite)
	{
		// Target data rate limiter.
		do { XTime_GetTime(&tNow); }
		while (tNow - tPrev < countsPerBlock);
		tPrev = tNow;

		// 1Hz progress update.
		if((tNow - tStart) / COUNTS_PER_SECOND > sElapsed)
		{
			sElapsed = (tNow - tStart) / COUNTS_PER_SECOND;

			rate = (float)((blocksWritten - blocksWrittenPrev) * BLOCK_SIZE) * 1e-6f;
			blocksWrittenPrev = blocksWritten;

			totalWrittenGB = (float)((u64)blocksWritten * (u64)BLOCK_SIZE) * 1e-9f;

			sprintf(strWorking, "%8d,%12.3f,%11.3f\r\n", sElapsed, rate, totalWrittenGB);
			xil_printf(strWorking);
		}

		// Add marker to data.
		*(u32 *) data = blocksWritten;

		// Write block.
		f_write(&fil, data, BLOCK_SIZE, &bw);
		blocksWritten++;
		lbaDest += lbaPerBlock;

		// Create new files as-needed.
		if((blocksWritten % BLOCKS_PER_FILE) == 0)
		{
			f_close(&fil);
			nFile++;
			sprintf(strWorking, "f%06d.bin\n", nFile);
			f_open(&fil, strWorking, FA_CREATE_NEW | FA_WRITE);
		}
	}

	// Clean up file system.
	f_close(&fil);
	f_mount(0, "", 0);

	xil_printf("File system write test finished.\r\n");
}
