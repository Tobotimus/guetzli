/*
 * Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Integer implementation of the Discrete Cosine Transform (DCT)
//
// Note! DCT output is kept scaled by 16, to retain maximum 16bit precision

#include "guetzli/fdct.h"
#include "xdct.h"
#include "xaxidma.h"

XDct dct;
XAxiDma axiDMA;
XAxiDma_Config *axiDMA_cfg;
#ifndef __linux__
XDct_Config *dct_cfg;
#endif

#define MEM_BASE_ADDR  0x01000000
#define TX_BUFFER_BASE (MEM_BASE_ADDR + 0x00100000)
#define RX_BUFFER_BASE (MEM_BASE_ADDR + 0x00300000)

// Pointers to DMA TX/RX addresses
UINTPTR m_dma_buffer_TX = TX_BUFFER_BASE;
UINTPTR m_dma_buffer_RX = RX_BUFFER_BASE;

typedef union {
    uint32_t asInt;
    float asFloat;
} floatConverter;

namespace guetzli {

int32_t inStreamData[kDCTBlockSize];

void InitHardware() {
    printf("Initializing Dct\n");
#   ifndef __linux__
    dct_cfg = XDct_LookupConfig(XPAR_DCT_0_DEVICE_ID);
    if (dct_cfg) {
        int status = XDct_CfgInitialize(&dct, dct_cfg);
        if (status != XST_SUCCESS) {
            fprintf(stderr, "Error initializing Dct core\n");
        }
    }
#   else
    int status = XDct_Initialize(&dct, "dct_0");
    if (status != XST_SUCCESS) {
        fprintf(stderr, "Error initializing Dct core\n");
    }
#   endif

    printf("Initializing AxiDMA\n");
    axiDMA_cfg = XAxiDma_LookupConfig(XPAR_AXIDMA_0_DEVICE_ID);
    if (axiDMA_cfg) {
        int status = XAxiDma_CfgInitialize(&axiDMA, axiDMA_cfg);
        if (status != XST_SUCCESS)
        {
            fprintf(stderr, "Error initializing AxiDMA core\n");
        }
    }
    // Disable interrupts
    XAxiDma_IntrDisable(&axiDMA, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrDisable(&axiDMA, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
}


void ComputeBlockDCT(coeff_t* coeffs) {
    XDct_Start(&dct);

    for (int i = 0; i < kDCTBlockSize; i++) {
        inStreamData[i] = (int)coeffs[i];
    }

    // Flush the cache of the buffers
    Xil_DCacheFlushRange((uint32_t)inStreamData, kDCTBlockSize*sizeof(int32_t));
    Xil_DCacheFlushRange(m_dma_buffer_RX, kDCTBlockSize*sizeof(uint32_t));

    XAxiDma_SimpleTransfer(&axiDMA, (uint32_t)inStreamData, kDCTBlockSize*sizeof(int32_t), XAXIDMA_DMA_TO_DEVICE);

    XAxiDma_SimpleTransfer(&axiDMA, m_dma_buffer_RX, kDCTBlockSize*sizeof(uint32_t), XAXIDMA_DEVICE_TO_DMA);
    while (XAxiDma_Busy(&axiDMA, XAXIDMA_DEVICE_TO_DMA));

    // Invalidate the cache to avoid reading garbage
    Xil_DCacheInvalidateRange(m_dma_buffer_RX, kDCTBlockSize*sizeof(uint32_t));

    while (!XDct_IsDone(&dct));

    for (int i = 0; i < kDCTBlockSize; i++) {
        floatConverter val;
        val.asInt = reinterpret_cast<uint32_t *>(m_dma_buffer_RX)[i];
        coeffs[i] = (coeff_t)val.asFloat;
    }

}

}  // namespace guetzli
