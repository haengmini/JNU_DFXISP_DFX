// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.1 (64-bit)
// Tool Version Limit: 2024.05
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#ifdef SDT
#include "xparameters.h"
#endif
#include "xlowlightisp.h"

extern XLowlightisp_Config XLowlightisp_ConfigTable[];

#ifdef SDT
XLowlightisp_Config *XLowlightisp_LookupConfig(UINTPTR BaseAddress) {
	XLowlightisp_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XLowlightisp_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XLowlightisp_ConfigTable[Index].Control_BaseAddress == BaseAddress) {
			ConfigPtr = &XLowlightisp_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XLowlightisp_Initialize(XLowlightisp *InstancePtr, UINTPTR BaseAddress) {
	XLowlightisp_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XLowlightisp_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XLowlightisp_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XLowlightisp_Config *XLowlightisp_LookupConfig(u16 DeviceId) {
	XLowlightisp_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XLOWLIGHTISP_NUM_INSTANCES; Index++) {
		if (XLowlightisp_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XLowlightisp_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XLowlightisp_Initialize(XLowlightisp *InstancePtr, u16 DeviceId) {
	XLowlightisp_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XLowlightisp_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XLowlightisp_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

