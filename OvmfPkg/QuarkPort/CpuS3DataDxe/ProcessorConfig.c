/** @file
  Code for processor configuration.

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

  Module Name:  ProcessorConfig.c

**/

#include "MpService.h"
#include "Cpu.h"

EFI_PHYSICAL_ADDRESS                mStartupVector;
ACPI_CPU_DATA                       *mAcpiCpuData;
EFI_EVENT                           mSmmConfigurationNotificationEvent;
EFI_HANDLE                          mImageHandle;

/**
  Event notification that is fired every time a gEfiSmmConfigurationProtocol
  installs.

  This function configures all logical processors with three-phase
  architecture.

  @param  Event                 The Event that is being processed, not used.
  @param  Context               Event Context, not used.

**/
VOID
EFIAPI
SmmConfigurationEventNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS    Status;
  VOID  *Registration;
  EFI_SMM_CONFIGURATION_PROTOCOL  *SmmConfiguration;

  //
  // Make sure this notification is for this handler
  //
  Status = gBS->LocateProtocol (&gEfiSmmConfigurationProtocolGuid, NULL,
                  (VOID **)&SmmConfiguration);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Save CPU S3 data
  //
  SaveCpuS3Data (mImageHandle);

  //
  // Setup notification on Legacy BIOS Protocol to reallocate AP wakeup
  //
  EfiCreateProtocolNotifyEvent (
    &gEfiLegacyBiosProtocolGuid,
    TPL_CALLBACK,
    ReAllocateMemoryForAP,
    NULL,
    &Registration
    );
}

/**
  First phase MP initialization before SMM initialization.
  
  @retval EFI_SUCCESS      First phase MP initialization was done successfully.
  @retval EFI_UNSUPPORTED  There is legacy APIC ID conflict and can't be
                           rsolved in xAPIC mode.

**/
EFI_STATUS
ProcessorConfiguration (
  VOID
  )
{
  //
  // Wakeup APs for the first time, BSP stalls for arbitrary
  // time for APs' completion. BSP then collects the number
  // and BIST information of APs.
  //
  WakeupAPAndCollectBist ();

  return EFI_SUCCESS;
}

/**
  Entrypoint of CpuS3DataDxe module.
  
  This function is the entrypoint of CpuS3DataDxe module.
  It populates ACPI_CPU_DATA for PiSmmCpuDxeSmm.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.
  
  @retval EFI_SUCCESS   The entrypoint always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
CpuS3DataInitialize (
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *Registration;

  mImageHandle = ImageHandle;
  //
  // Configure processors with three-phase architecture
  //
  Status = ProcessorConfiguration ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Install notification callback on SMM Configuration Protocol
  //
  mSmmConfigurationNotificationEvent = EfiCreateProtocolNotifyEvent (
                                         &gEfiSmmConfigurationProtocolGuid,
                                         TPL_CALLBACK,
                                         SmmConfigurationEventNotify,
                                         NULL,
                                         &Registration
                                         );

  return EFI_SUCCESS;
}

/**
  Wakes up APs for the first time to count their number and collect BIST data.

  This function wakes up APs for the first time to count their number and
  collect BIST data.

**/
VOID
WakeupAPAndCollectBist (
  VOID
  )
{
  //
  // Prepare code and data for APs' startup vector
  //
  PrepareAPStartupVector ();
}

/**
  Prepare ACPI NVS memory below 4G memory for use of S3 resume.
  
  This function allocates ACPI NVS memory below 4G memory for use of S3 resume,
  and saves data into the memory region.

  @param  Context   The Context save the info.

**/
VOID
SaveCpuS3Data (
  VOID    *Context
  )
{
  MP_CPU_SAVED_DATA       *MpCpuSavedData;

  //
  // Allocate ACPI NVS memory below 4G memory for use of S3 resume.
  //
  MpCpuSavedData = AllocateAcpiNvsMemoryBelow4G (sizeof (MP_CPU_SAVED_DATA));

  //
  // Set the value for CPU data
  //
  mAcpiCpuData                 = &(MpCpuSavedData->AcpiCpuData);
  mAcpiCpuData->StartupVector  = mStartupVector;

  //
  // Set the base address of CPU S3 data to PcdCpuS3DataAddress
  //
  PcdSet64 (PcdCpuS3DataAddress, (UINT64)(UINTN)mAcpiCpuData); 
}
