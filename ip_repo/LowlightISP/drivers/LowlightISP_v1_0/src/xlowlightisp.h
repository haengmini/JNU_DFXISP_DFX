// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.1 (64-bit)
// Tool Version Limit: 2024.05
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XLOWLIGHTISP_H
#define XLOWLIGHTISP_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xlowlightisp_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
#ifdef SDT
    char *Name;
#else
    u16 DeviceId;
#endif
    u64 Control_BaseAddress;
} XLowlightisp_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u32 IsReady;
} XLowlightisp;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XLowlightisp_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XLowlightisp_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XLowlightisp_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XLowlightisp_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
#ifdef SDT
int XLowlightisp_Initialize(XLowlightisp *InstancePtr, UINTPTR BaseAddress);
XLowlightisp_Config* XLowlightisp_LookupConfig(UINTPTR BaseAddress);
#else
int XLowlightisp_Initialize(XLowlightisp *InstancePtr, u16 DeviceId);
XLowlightisp_Config* XLowlightisp_LookupConfig(u16 DeviceId);
#endif
int XLowlightisp_CfgInitialize(XLowlightisp *InstancePtr, XLowlightisp_Config *ConfigPtr);
#else
int XLowlightisp_Initialize(XLowlightisp *InstancePtr, const char* InstanceName);
int XLowlightisp_Release(XLowlightisp *InstancePtr);
#endif


void XLowlightisp_Set_raw_bayer(XLowlightisp *InstancePtr, u64 Data);
u64 XLowlightisp_Get_raw_bayer(XLowlightisp *InstancePtr);
void XLowlightisp_Set_rgb_out(XLowlightisp *InstancePtr, u64 Data);
u64 XLowlightisp_Get_rgb_out(XLowlightisp *InstancePtr);
void XLowlightisp_Set_width(XLowlightisp *InstancePtr, u32 Data);
u32 XLowlightisp_Get_width(XLowlightisp *InstancePtr);
void XLowlightisp_Set_height(XLowlightisp *InstancePtr, u32 Data);
u32 XLowlightisp_Get_height(XLowlightisp *InstancePtr);
u32 XLowlightisp_Get_out_width(XLowlightisp *InstancePtr);
u32 XLowlightisp_Get_out_width_vld(XLowlightisp *InstancePtr);
u32 XLowlightisp_Get_out_height(XLowlightisp *InstancePtr);
u32 XLowlightisp_Get_out_height_vld(XLowlightisp *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
