/*
Lightweight NVMe Private Driver Includes

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

#ifndef __NVME_PRIV_INCLUDE__
#define __NVME_PRIV_INCLUDE__

// Include Headers -----------------------------------------------------------------------------------------------------

// Private Pre-Processor Definitions -----------------------------------------------------------------------------------

// AXI/PCIe Bridge Register Constants
// ====================================================================================
#define PHY_OK 0x00001884			// Gen3 x4 link is up, in state L0.
#define BRIDGE_ENABLE 0x1			// Bridge Status/Control enable(d) bit.
#define CLASS_CODE_OK 0x00010802	// Mass Storage, Non-Volatile Memory, NVMe.
// ====================================================================================

// NVMe Controller Register Maps
// ====================================================================================
// Controller Capabilties
#define REG_CAP_CMBS         0x0200000000000000
#define REG_CAP_PMRS         0x0100000000000000
#define REG_CAP_MPSMAX_Msk   0x00F0000000000000
#define REG_CAP_MPSMAX_Pos                   52
#define REG_CAP_MPSMIN_Msk   0x000F000000000000
#define REG_CAP_MPSMIN_Pos                   48
#define REG_CAP_BPS          0x0000200000000000
#define REG_CAP_CCS_Msk      0x00001FE000000000
#define REG_CAP_CCS_Pos                      37
#define REG_CAP_NSSRS        0x0000001000000000
#define REG_CAP_DSTRD_Msk    0x0000000F00000000
#define REG_CAP_DSTRD_Pos                    32
#define REG_CAP_TO_Msk       0x000000000F000000
#define REG_CAP_TO_Pos                       24
#define REG_CAP_AMS_Msk      0x0000000000060000
#define REG_CAP_AMS_Pos                      17
#define REG_CAP_CQR          0x0000000000010000
#define REG_CAP_MQES_Msk     0x000000000000FFFF
#define REG_CAP_MQES_Pos                      0

// Controller Configuration
#define REG_CC_IOCQES_Msk            0x00F00000
#define REG_CC_IOCQES_Pos                    20
#define REG_CC_IOSQES_Msk            0x000F0000
#define REG_CC_IOSQES_Pos                    16
#define REG_CC_SHN_Msk               0x0000C000
#define REG_CC_SHN_Pos                       14
#define REG_CC_AMS_Msk               0x00003800
#define REG_CC_AMS_Pos                       11
#define REG_CC_MPS_Msk               0x00000780
#define REG_CC_MPS_Pos                        7
#define REG_CC_CSS_Msk               0x00000070
#define REG_CC_CCS_Pos                        4
#define REG_CC_EN                    0x00000001

// Controller Status
#define REG_CSTS_PP                  0x00000020
#define REG_CSTS_NSSRO               0x00000010
#define REG_CSTS_SHST_Msk            0x0000000C
#define REG_CSTS_SHST_Pos                     2
#define REG_CSTS_CFS                 0x00000002
#define REG_CSTS_RDY                 0x00000001
// ====================================================================================

// Power State Descriptor Bitfields
// ====================================================================================
#define PSD_MXPS_Offset                      0
#define PSD_MXPS_Msk                0x01000000
#define PSD_MXPS_Pos                        24

#define PSD_IPS_Offset                      16
#define PSD_IPS_Msk                 0x00C00000
#define PSD_IPS_Pos                         22

#define PSD_APS_Offset                      20
#define PSD_APS_Msk                 0x00C00000
#define PSD_APS_Pos                         22

#define PSD_ENLAT_Offset                     4
#define PSD_EXLAT_Offset                     8

#define PSD_NOPS_Offset                      0
#define PSD_NOPS_Msk                0x02000000
#define PSD_NOPS_Pos						25

#define PSD_RXX_Offset                      12
#define PSD_RRT_Msk                 0x0000001F
#define PSD_RRT_Pos                          0
#define PSD_RRL_Msk                 0x00001F00
#define PSD_RRL_Pos                          8
#define PSD_RWT_Msk                 0x001F0000
#define PSD_RWT_Pos                         16
#define PSD_RWL_Msk                 0x1F000000
#define PSD_RWL_Pos                         24

#define PSD_APW_Offset                      22
#define PSD_APW_Msk                 0x00000007
#define PSD_APW_Pos                          0
// ====================================================================================

// Private Type Definitions --------------------------------------------------------------------------------------------

// 64B Submission Queue Entry, PRP
typedef struct __attribute__((packed))
{
	u8 OPC;
	u8 PSDT_FUSE;
	u16 CID;
	u32 NSID;
	u64 reserved;
	u64 MPTR;
	u64 PRP1;
	u64 PRP2;
	u32 CDW10;
	u32 CDW11;
	u32 CDW12;
	u32 CDW13;
	u32 CDW14;
	u32 CDW15;
} sqe_prp_type;

// 16B Completion Queue Entry
typedef struct __attribute__((packed))
{
	u32 CDW0;
	u32 reserved;
	u16 SQHD;
	u16 SQID;
	u16 CID;
	u16 SF_P;
} cqe_type;

// Identify Controller
typedef struct __attribute__((packed))
{
	u16 VID;            // PCI Vendor ID
	u16 SSVID;          // PCI Subsystem Vendor ID
	char SN[20];        // Serial Number (ASCII)
	char MN[40];        // Model Number (ASCII)
	char FR[8];         // Firmware Revision (ASCII)
	u8 RAB;             // Recommended Arbitration Burst (2^N [Command])
	u8 IEEE[3];         // IEEE OUI Identifier
	u8 CMIC;            // Controller Multi-Path I/O and Namespace Sharing Capabilities
	u8 MDTS;            // Maximum Data Transfer Size (2^N [Minimum Page Size])
	u16 CNTLID;         // Controller ID
	u32 VER;            // Version
	u32 RTD3R;          // RTD3 Resume Latency
	u32 RTD3E;          // RTD3 Entry Latency
	u32 OAES;           // Optional Asynchronous Events Supported
	u32 CTRATT;         // Controller Attributes
	u16 RRLS;           // Read Recovery Levels Supported
	u8 reserved0[9];
	u8 CNTRLTYPE;       // Controller Type
	u64 FGUID_L;        // FRU Globally Unique Identifier, Low QWORD
	u64 FGUID_H;        // FRU Globally Unique Identifier, High QWORD
	u16 CRDT1;          // Command Retry Delay Time 1 in [100ms]
	u16 CRDT2;          // Command Retry Delay Time 2 in [100ms]
	u16 CRDT3;          // Command Retry Delay Time 3 in [100ms]
	u8 reserved1[122];
	u16 OACS;           // Optional Admin Command Support
	u8 ACL;             // Abort Command Limit
	u8 AERL;            // Asynchronous Event Request Limit
	u8 FRMW;            // Firmware Updates
	u8 LPA;             // Log Page Attributes
	u8 ELPE;            // Error Log Page Entries
	u8 NPSS;            // Number of Power States Support
	u8 AVSCC;           // Admin Vendor Specific Command Configuration
	u8 APSTA;           // Autonomous Power State Transition Attributes
	u16 WCTEMP;         // Warning Composite Temperature Threshold in [K]
	u16 CCTEMP;         // Critical Composite Temperature Threshold in [K]
	u16 MTFA;           // Maximum Time for Firmware Activation
	u32 HMPRE;          // Host Memory Buffer Preferred Size in [4KiB]
	u32 HMMIN;          // Host Memory Buffer Minimum Size in [4KiB]
	u64 TNVMCAP_L;      // Total NVM Capacity in [B], Low QWORD
	u64 TNVMCAP_H;      // Total NVM Capacity in [B], High QWRD
	u64 UNVMCAP_L;      // Unallocated NVM Capacity in [B], Low QWORD
	u64 UNVMCAP_H;      // Unallocated NVM Capacity in [B], High QWORD
	u32 RPMBS;          // Replay Protected Memory Block Support
	u16 EDSTT;          // Extended Device Self-test Time in [min]
	u8 DSTO;            // Device Self-test Options
	u8 FWUG;            // Firmware Update Granularity
	u16 KAS;            // Keep Alive Support in [100ms]
	u16 HCTMA;          // Host Controlled Thermal Management Attributes
	u16 MNTMT;          // Minimum Thermal Management Temperature in [K]
	u16 MXTMT;          // Maximum Thermal Management Temperature in [K]
	u32 SANICAP;        // Sanitize Capabilities
	u32 HMMINDS;        // Host Memory Buffer Minimum Descriptor Entry Size in [4KiB]
	u16 HMMAXD;         // Host Memory Maximum Descriptors Entries
	u16 NSETIDMAX;      // NVM Set Identifier Maximum
	u16 ENDGIDMAX;      // Endurance Group Identifier Maximum
	u8 ANATT;           // ANA Transition Time
	u8 ANACAP;          // Asymmetric Namespace Access Capabilities
	u32 ANAGRPMAX;      // ANA Group Identifier Maximum
	u32 NANAGRPID;      // Number of ANA Group Identifiers
	u32 PELS;           // Persistent Event Log Size in [64KiB].
	u8 reserved2[156];
	u8 SQES;            // Submission Queue Entry Size (2^N [B])
	u8 CQES;            // Completion Queue Entry Size (2^N [B])
	u16 MAXCMD;         // Maximum Outstanding Commands
	u32 NN;             // Number of Namespaces
	u16 ONCS;           // Optional NVM Command Support
	u16 FUSES;          // Fused Operation Support
	u8 FNA;             // Format NVM Attributes
	u8 VWC;             // Volatile Write Cache
	u16 AWUN;           // Atomic Write Unit Normal in 0's Based [LB]
	u16 AWUPF;          // Atomic Write Unit Power Fail in 0's Based [LB]
	u8 NVMSCC;          // NVM Vendor Specific Command Configuration
	u8 NWPC;            // Namespace Write Protection Capabilities
	u16 ACWU;           // Atomic Compare & Write Unit in 0's Based [LB]
	u8 reserved3[2];
	u32 SGLS;           // SGL Support
	u32 MNAN;           // Maximum Number of Allowed Namespaces
	u8 reserved4[224];
	char SUBNQN[256];   // NVM Subsystem NVMe Qualified Name
	u8 reserved5[1024];
	u8 PSD0[1024];		// Power State Descriptors
} idController_type;

// Identify Controller Power State Descriptor Expansion
typedef struct __attribute__((packed))
{
	float pMax;
	float pActive;
	float pIdle;
	u32 tEnter_us;
	u32 tExit_us;
	u8 NOPS;
	u8 APW;
	u8 RWL;
	u8 RWT;
	u8 RRL;
	u8 RRT;
	u8 reserved[2];
} descPowerState_type;

// Identify Namespace
typedef struct __attribute__((packed))
{
	u64 NSZE;          // Namespace Size in [LB]
	u64 NCAP;          // Namespace Capacity in [LB]
	u64 NUSE;          // Namespace Utilization in [LB]
	u8 NSFEAT;         // Namespace Features
	u8 NLBAF;          // Number of LBA Formats
	u8 FLBAS;          // Formatted LBA Size
	u8 MC;             // Metadata Capabilities
	u8 DPC;            // End-to-end Data Protection Capabilities
	u8 DPS;            // End-to-end Data Protection Type Settings
	u8 NMIC;           // Namespace Multi-path I/O and Namespace Sharing Capabilities (NMIC)
	u8 RESCAP;         // Reservation Capabilities
	u8 FPI;            // Format Progress Indicator
	u8 DLFEAT;         // Deallocate Logical Block Features
	u16 NAWUN;         // Namespace Atomic Write Unit Normal
	u16 NAWUPF;        // Namespace Atomic Write Unit Power Fail
	u16 NACWU;         // Namespace Atomic Compare & Write Unit
	u16 NABSN;         // Namespace Atomic Boundary Size Normal
	u16 NABO;          // Namespace Atomic Boundary Offset
	u16 NABSPF;        // Namespace Atomic Boundary Size Power Fail
	u16 NOIOB;         // Namespace Optimal I/O Boundary
	u64 NVMCAP_L;      // NVM Capacity in [B], Low QWORD
	u64 NVMCAP_H;      // NVM Capacity in [B], High QWORD
	u16 NPWG;          // Namespace Preferred Write Granularity in 0's Based [Minimum Page Size]
	u16 NPWA;          // Namespace Preferred Write Alignment in 0's Based [LB]
	u16 NPDG;          // Namespace Preferred Deallocate Granularity in 0s Based [NPDA]
	u16 NPDA;          // Namespace Preferred Deallocate Alignment in 0's Based [LB]
	u16 NOWS;          // Namespace Optimal Write Size in 0's Based [LB]
	u8 reserved0[18];
	u32 ANAGRPID;      // ANA Group Identifier
	u8 reserved1[3];
	u8 NSATTR;         // Namespace Attributes
	u16 NVMSETID;      // NVM Set Identifier
	u16 ENDGID;        // Endurance Group Identifier
	u64 NGUID_L;       // Namespace Globally Unique Identifier, Low QWORD
	u64 NGUID_H;       // Namespace Globally Unique Identifier, High QWORD
	u64 EUI64;         // IEEE Extended Unique Identifier
	u32 LBAF[16];      // LBA Format N Support
} idNamespace_type;

// Log Page 02: SMART / Health Information
typedef struct __attribute__((packed))
{
	u8 Critical_Warning;
	u16 Composite_Temperature;		// [K]
	u8 Available_Spare;				// [%]
	u8 Available_Spare_Threshold;	// [%]
	u8 Percentage_Used;				// [%]
	u8 Endurance_Group_Critical_Warning_Summary;
	u8 reserved1[25];
	u64 Data_Units_Read_L;			// [1000 * 512B]
	u64 Data_Units_Read_H;
	u64 Data_Units_Written_L;		// [1000 * 512B]
	u64 Data_Units_Written_H;
	u64 Host_Read_Commands_L;
	u64 Host_Read_Commands_H;
	u64 Host_Write_Commands_L;
	u64 Host_Write_Commands_H;
	u64 Controller_Busy_Time_L;		// [min]
	u64 Controller_Busy_Time_H;
	u64 Power_Cycles_L;
	u64 Power_Cycles_H;
	u64 Power_On_Hours_L;			// [hr]
	u64 Power_On_Hours_H;
	u64 Unsafe_Shutdowns_L;
	u64 Unsafe_Shutdowns_H;
	u64 Media_and_Data_Integrity_Errors_L;
	u64 Media_and_Data_Integrity_Errors_H;
	u64 Number_of_Error_Information_Log_Entries_L;
	u64 Number_of_Error_Information_Log_Entries_H;
	u32 Warning_Composite_Temperature_Time;			// [min]
	u32 Critical_Composite_Temperature_Time;		// [min]
	u16 Temperature_Sensor[8];
	u32 Thermal_Management_Temperature_1_Transition_Count;
	u32 Thermal_Management_Temperature_2_Transition_Count;
	u32 Total_Time_For_Thermal_Management_Temperature_1;		// [s]
	u32 Total_Time_For_Thermal_Management_Temperature_2;		// [s]
	u8 reserved2[280];
} logSMARTHealth_type;

// Range Definition for Dataset Management Commands
typedef struct __attribute__((packed))
{
	u32 contextAttributes;
	u32 length;					// [LBA]
	u64 start;					// [LBA]
} dsmRange_type;

#endif
