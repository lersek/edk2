/** @file

  Common functions for CPU DXE module.

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

  Module Name:  MpCommon.c

**/

#include "MpService.h"
#include "Cpu.h"

UINTN                 mStartupVectorSize;
EFI_PHYSICAL_ADDRESS  mApMachineCheckHandlerBase;
UINT32                mApMachineCheckHandlerSize;

/**
  Allocates startup vector for APs.

  This function allocates Startup vector for APs.

  @param  Size  The size of startup vector.

**/
VOID
AllocateStartupVector (
  UINTN   Size
  )
{
  EFI_STATUS                            Status;
  EFI_PHYSICAL_ADDRESS                  StartAddress;

  mStartupVectorSize = Size;
  Status = EFI_NOT_FOUND;

  //
  // Allocate wakeup buffer below 640K. Don't touch legacy region.
  // Leave some room immediately below 640K in for CSM module.
  // PcdEbdaReservedMemorySize has been required to be a multiple of 4KB.
  //
  StartAddress = 0xA0000 - PcdGet32 (PcdEbdaReservedMemorySize) -
                 (EFI_SIZE_TO_PAGES (Size) * EFI_PAGE_SIZE);
  for (mStartupVector = StartAddress;
       mStartupVector >= 0x2000;
       mStartupVector -= (EFI_SIZE_TO_PAGES (Size) * EFI_PAGE_SIZE)) {

    //
    // If finally no CSM in the platform, this wakeup buffer is to be used in
    // S3 boot path.
    //
    Status = gBS->AllocatePages (
                    AllocateAddress,
                    EfiReservedMemoryType,
                    EFI_SIZE_TO_PAGES (Size),
                    &mStartupVector
                    );

    if (!EFI_ERROR (Status)) {
      break;
    }
  }

  ASSERT_EFI_ERROR (Status);
}

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
  )
{
  EFI_LEGACY_BIOS_PROTOCOL   *LegacyBios;
  VOID                       *LegacyRegion;
  EFI_STATUS                 Status;

  Status = gBS->LocateProtocol (&gEfiLegacyBiosProtocolGuid, NULL,
                  (VOID **)&LegacyBios);
  if (EFI_ERROR (Status)) {
    return ;
  }

  //
  // Allocate 4K aligned bytes from either 0xE0000 or 0xF0000.
  // Note some CSM16 does not satisfy alignment request, so allocate a buffer
  // of 2*4K and adjust the base address myself.
  //
  Status = LegacyBios->GetLegacyRegion (
                         LegacyBios,
                         0x2000,
                         0,
                         0x1000,
                         &LegacyRegion
                         );
  ASSERT_EFI_ERROR (Status);

  if ((((UINTN)LegacyRegion) & 0xfff) != 0) {
    LegacyRegion =
      (VOID *)(UINTN)((((UINTN)LegacyRegion) + 0xfff) & ~((UINTN)0xfff));
  }

  //
  // Free original wakeup buffer
  //
  FreePages ((VOID *)(UINTN)mStartupVector,
    EFI_SIZE_TO_PAGES (mStartupVectorSize));

  mStartupVector = (EFI_PHYSICAL_ADDRESS)(UINTN)LegacyRegion;
  mAcpiCpuData->StartupVector = mStartupVector;
}

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
  )
{
  UINTN       PointerValue;
  VOID        *Pointer;

  Pointer = AllocateAcpiNvsMemoryBelow4G (Size + Alignment - 1);

  PointerValue  = (UINTN) Pointer;
  PointerValue  = (PointerValue + Alignment - 1) / Alignment * Alignment;

  Pointer      = (VOID *) PointerValue;

  return Pointer;
}

/**
  Allocate EfiACPIMemoryNVS below 4G memory address.

  This function allocates EfiACPIMemoryNVS below 4G memory address.

  @param  Size         Size of memory to allocate.

  @return Allocated address for output.

**/
VOID*
AllocateAcpiNvsMemoryBelow4G (
  IN   UINTN   Size
  )
{
  UINTN                 Pages;
  EFI_PHYSICAL_ADDRESS  Address;
  EFI_STATUS            Status;
  VOID*                 Buffer;

  Pages = EFI_SIZE_TO_PAGES (Size);
  Address = 0xffffffff;

  Status  = gBS->AllocatePages (
                   AllocateMaxAddress,
                   EfiACPIMemoryNVS,
                   Pages,
                   &Address
                   );
  ASSERT_EFI_ERROR (Status);

  Buffer = (VOID *) (UINTN) Address;
  ZeroMem (Buffer, Size);

  return Buffer;
}

/**
  Creates a copy of GDT and IDT for all APs.

  This function creates a copy of GDT and IDT for all APs.

  @param  Gdtr   Base and limit of GDT for AP
  @param  Idtr   Base and limit of IDT for AP

**/
VOID
PrepareGdtIdtForAP (
  OUT IA32_DESCRIPTOR          *Gdtr,
  OUT IA32_DESCRIPTOR          *Idtr
  )
{
  SEGMENT_DESCRIPTOR        *GdtForAP;
  INTERRUPT_GATE_DESCRIPTOR *IdtForAP;
  IA32_DESCRIPTOR           GdtrForBSP;
  IA32_DESCRIPTOR           IdtrForBSP;
  VOID                      *MachineCheckHandlerBuffer;

  //
  // Get the BSP's data of GDT and IDT
  //
  AsmReadGdtr ((IA32_DESCRIPTOR *) &GdtrForBSP);
  AsmReadIdtr ((IA32_DESCRIPTOR *) &IdtrForBSP);

  //
  // Allocate ACPI NVS memory for GDT, IDT, and machine check handler.
  // Combine allocation for ACPI NVS memory under 4G to save memory.
  //
  GdtForAP = AllocateAlignedAcpiNvsMemory (
               ((GdtrForBSP.Limit + 1) +
                (IdtrForBSP.Limit + 1) +
                (UINTN) ApMachineCheckHandlerEnd -
                (UINTN) ApMachineCheckHandler),
               8
               );

  //
  // GDT base is 8-bype aligned, and its size is multiple of 8-bype, so IDT
  // base here is also 8-bype aligned.
  //
  IdtForAP = (INTERRUPT_GATE_DESCRIPTOR *)
               ((UINTN) GdtForAP + GdtrForBSP.Limit + 1);
  MachineCheckHandlerBuffer = (VOID *) ((UINTN) GdtForAP +
                                        (GdtrForBSP.Limit + 1) +
                                        (IdtrForBSP.Limit + 1));
  //
  // Make copy for APs' GDT & IDT
  //
  CopyMem (GdtForAP, (VOID *) GdtrForBSP.Base, GdtrForBSP.Limit + 1);
  CopyMem (IdtForAP, (VOID *) IdtrForBSP.Base, IdtrForBSP.Limit + 1);

  //
  // Copy code for AP's machine check handler to ACPI NVS memory, and register
  // in IDT
  //
  CopyMem (
    MachineCheckHandlerBuffer,
    (VOID *) (UINTN) ApMachineCheckHandler,
    (UINTN) ApMachineCheckHandlerEnd - (UINTN) ApMachineCheckHandler
    );
  SetIdtEntry ((UINTN) MachineCheckHandlerBuffer,
    &IdtForAP[INTERRUPT_HANDLER_MACHINE_CHECK]);

  //
  // Set AP's profile for GDTR and IDTR
  //
  Gdtr->Base  = (UINTN) GdtForAP;
  Gdtr->Limit = GdtrForBSP.Limit;

  Idtr->Base  = (UINTN) IdtForAP;
  Idtr->Limit = IdtrForBSP.Limit;

  //
  // Save the AP's machine check handler information
  //
  mApMachineCheckHandlerBase =
    (EFI_PHYSICAL_ADDRESS) (UINTN) MachineCheckHandlerBuffer;
  mApMachineCheckHandlerSize =
    (UINT32) ((UINTN)ApMachineCheckHandlerEnd - (UINTN)ApMachineCheckHandler);
}
