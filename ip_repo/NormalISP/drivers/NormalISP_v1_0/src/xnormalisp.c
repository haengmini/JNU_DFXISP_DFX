// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.1 (64-bit)
// Tool Version Limit: 2024.05
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xnormalisp.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XNormalisp_CfgInitialize(XNormalisp *InstancePtr, XNormalisp_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XNormalisp_Set_raw_bayer(XNormalisp *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XNormalisp_WriteReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RAW_BAYER_DATA, (u32)(Data));
    XNormalisp_WriteReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RAW_BAYER_DATA + 4, (u32)(Data >> 32));
}

u64 XNormalisp_Get_raw_bayer(XNormalisp *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RAW_BAYER_DATA);
    Data += (u64)XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RAW_BAYER_DATA + 4) << 32;
    return Data;
}

void XNormalisp_Set_rgb_out(XNormalisp *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XNormalisp_WriteReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RGB_OUT_DATA, (u32)(Data));
    XNormalisp_WriteReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RGB_OUT_DATA + 4, (u32)(Data >> 32));
}

u64 XNormalisp_Get_rgb_out(XNormalisp *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RGB_OUT_DATA);
    Data += (u64)XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_RGB_OUT_DATA + 4) << 32;
    return Data;
}

void XNormalisp_Set_width(XNormalisp *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XNormalisp_WriteReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_WIDTH_DATA, Data);
}

u32 XNormalisp_Get_width(XNormalisp *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_WIDTH_DATA);
    return Data;
}

void XNormalisp_Set_height(XNormalisp *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XNormalisp_WriteReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_HEIGHT_DATA, Data);
}

u32 XNormalisp_Get_height(XNormalisp *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_HEIGHT_DATA);
    return Data;
}

u32 XNormalisp_Get_out_width(XNormalisp *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_OUT_WIDTH_DATA);
    return Data;
}

u32 XNormalisp_Get_out_width_vld(XNormalisp *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_OUT_WIDTH_CTRL);
    return Data & 0x1;
}

u32 XNormalisp_Get_out_height(XNormalisp *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_OUT_HEIGHT_DATA);
    return Data;
}

u32 XNormalisp_Get_out_height_vld(XNormalisp *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XNormalisp_ReadReg(InstancePtr->Control_BaseAddress, XNORMALISP_CONTROL_ADDR_OUT_HEIGHT_CTRL);
    return Data & 0x1;
}

