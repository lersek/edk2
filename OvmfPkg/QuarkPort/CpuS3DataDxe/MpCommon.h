/** @file

  Include files of common functions for CPU DXE module.

  Copyright (C) 2015, Red Hat, Inc.
  Copyright (c) 2013-2015 Intel Corporation.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.
  * Neither the name of Intel Corporation nor the names of its
  contributors may be used to endorse or promote products derived
  from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Module Name:  MpCommon.h

**/

#ifndef _MP_COMMON_H_
#define _MP_COMMON_H_

#include <Library/BaseLib.h>

#include "ArchSpecificDef.h"
#include <AcpiCpuData.h>

#include <Protocol/LegacyBios.h>
#include <Protocol/SmmConfiguration.h>

#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define INTERRUPT_HANDLER_MACHINE_CHECK       0x12

#pragma pack(1)

typedef  struct {
  UINT16  LimitLow;
  UINT16  BaseLow;
  UINT8   BaseMiddle;
  UINT8   Attributes1;
  UINT8	  Attributes2;	
  UINT8   BaseHigh;
} SEGMENT_DESCRIPTOR;

#pragma pack()

typedef struct {
  VOID              *StackStart;
  IA32_DESCRIPTOR   GdtrProfile;
  IA32_DESCRIPTOR   IdtrProfile;
} MP_CPU_EXCHANGE_INFO;

extern MP_CPU_EXCHANGE_INFO        *mExchangeInfo;
extern EFI_PHYSICAL_ADDRESS        mApMachineCheckHandlerBase;
extern UINT32                      mApMachineCheckHandlerSize;

/**
  Allocates startup vector for APs.

  This function allocates Startup vector for APs.

  @param  Size  The size of startup vector.

**/
VOID
AllocateStartupVector (
  UINTN   Size
  );

/**
  Creates a copy of GDT and IDT for all APs.

  This function creates a copy of GDT and IDT for all APs.

  @param  Gdtr   Base and limit of GDT for AP
  @param  Idtr   Base and limit of IDT for AP

**/
VOID
PrepareGdtIdtForAP (
  OUT IA32_DESCRIPTOR  *Gdtr,
  OUT IA32_DESCRIPTOR  *Idtr
  );

/**
  Allocate aligned ACPI NVS memory below 4G.
  
  This function allocates aligned ACPI NVS memory below 4G.

  @param  Size       Size of memory region to allocate
  @param  Alignment  Alignment in bytes
  
  @return Base address of the allocated region

**/
VOID*
AllocateAlignedAcpiNvsMemory (
  IN  UINTN         Size,
  IN  UINTN         Alignment
  );

/**
  Prepares Startup Vector for APs.

  This function prepares Startup Vector for APs.

**/
VOID
PrepareAPStartupVector (
  VOID
  );

/**
  Sets specified IDT entry with given function pointer.

  This function sets specified IDT entry with given function pointer.

  @param  FunctionPointer  Function pointer for IDT entry.
  @param  IdtEntry         The IDT entry to update.

  @return The original IDT entry value.

**/
UINTN
SetIdtEntry (
  IN  UINTN                       FunctionPointer,
  OUT INTERRUPT_GATE_DESCRIPTOR   *IdtEntry
  );

/**
  Allocate EfiACPIMemoryNVS below 4G memory address.

  This function allocates EfiACPIMemoryNVS below 4G memory address.

  @param  Size         Size of memory to allocate.
  
  @return Allocated address for output.

**/
VOID*
AllocateAcpiNvsMemoryBelow4G (
  IN   UINTN   Size
  );

/**
  Protocol notification that is fired when LegacyBios protocol is installed.

  Re-allocate a wakeup buffer from E/F segment because the previous wakeup
  buffer under 640K won't be preserved by the legacy OS.

  @param  Event                 The triggered event.
  @param  Context               Context for this event.
**/
VOID
EFIAPI
ReAllocateMemoryForAP (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

#endif
