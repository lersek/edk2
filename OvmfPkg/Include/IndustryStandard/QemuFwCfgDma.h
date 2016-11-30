/** @file
  Macro and type definitions related to QEMU's DMA-like fw_cfg access method,
  based on "docs/specs/fw_cfg.txt" in the QEMU tree.

  Copyright (C) 2016, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/


#ifndef __FW_CFG_DMA__
#define __FW_CFG_DMA__

#include <Base.h>

//
// If the following bit is set in the UINT32 fw_cfg revision / feature bitmap
// -- read from key 0x0001 with the basic IO Port or MMIO method --, then the
// DMA interface is available.
//
#define FW_CFG_F_DMA BIT1

//
// Communication structure for the DMA access method. All fields are encoded in
// big endian.
//
#pragma pack (1)
typedef struct {
  UINT32 Control;
  UINT32 Length;
  UINT64 Address;
} FW_CFG_DMA_ACCESS;
#pragma pack ()

//
// Macros for the FW_CFG_DMA_ACCESS.Control bitmap (in native encoding).
//
#define FW_CFG_DMA_CTL_ERROR  BIT0
#define FW_CFG_DMA_CTL_READ   BIT1
#define FW_CFG_DMA_CTL_SKIP   BIT2
#define FW_CFG_DMA_CTL_SELECT BIT3
#define FW_CFG_DMA_CTL_WRITE  BIT4

#endif
