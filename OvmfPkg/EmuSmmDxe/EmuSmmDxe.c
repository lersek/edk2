/** @file

  This module emulates SMRAM by providing EFI_SMM_ACCESS2_PROTOCOL and
  EFI_SMM_CONTROL2_PROTOCOL.

  Copyright (C) 2013, Red Hat, Inc.<BR>

  Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Protocol/SmmAccess2.h>
#include <Protocol/SmmControl2.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>


STATIC EFI_SMM_ACCESS2_PROTOCOL mAccess2;


/**
  Opens the SMRAM area to be accessible by a boot-service driver.

  This function "opens" SMRAM so that it is visible while not inside of SMM.
  The function should return EFI_UNSUPPORTED if the hardware does not support
  hiding of SMRAM. The function should return EFI_DEVICE_ERROR if the SMRAM
  configuration is locked.

  @param[in] This           The EFI_SMM_ACCESS2_PROTOCOL instance.

  @retval EFI_SUCCESS       The operation was successful.
  @retval EFI_UNSUPPORTED   The system does not support opening and closing of
                            SMRAM.
  @retval EFI_DEVICE_ERROR  SMRAM cannot be opened, perhaps because it is
                            locked.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmAccess2Open (
  IN EFI_SMM_ACCESS2_PROTOCOL  *This
  )
{
  //
  // Normally we'd like to return EFI_UNSUPPORTED here (the PI spec allows it
  // and our SMRAM is always visible), but the EFI_SMM_COMMUNICATION_PROTOCOL
  // implementation in "MdeModulePkg/Core/PiSmmCore/PiSmmIpl.c" really wants
  // this function to succeed.
  //
  if (mAccess2.LockState) {
    return EFI_DEVICE_ERROR;
  }
  mAccess2.OpenState = TRUE;
  return EFI_SUCCESS;
}


/**
  Inhibits access to the SMRAM.

  This function "closes" SMRAM so that it is not visible while outside of SMM.
  The function should return EFI_UNSUPPORTED if the hardware does not support
  hiding of SMRAM.

  @param[in] This           The EFI_SMM_ACCESS2_PROTOCOL instance.

  @retval EFI_SUCCESS       The operation was successful.
  @retval EFI_UNSUPPORTED   The system does not support opening and closing of
                            SMRAM.
  @retval EFI_DEVICE_ERROR  SMRAM cannot be closed.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmAccess2Close (
  IN EFI_SMM_ACCESS2_PROTOCOL  *This
  )
{
  mAccess2.OpenState = FALSE;
  return EFI_SUCCESS;
}


/**
  Inhibits access to the SMRAM.

  This function prohibits access to the SMRAM region.  This function is usually
  implemented such that it is a write-once operation.

  @param[in] This          The EFI_SMM_ACCESS2_PROTOCOL instance.

  @retval EFI_SUCCESS      The device was successfully locked.
  @retval EFI_UNSUPPORTED  The system does not support locking of SMRAM.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmAccess2Lock (
  IN EFI_SMM_ACCESS2_PROTOCOL  *This
  )
{
  mAccess2.LockState = TRUE;
  mAccess2.OpenState = FALSE;
  return EFI_SUCCESS;
}


/**
  Queries the memory controller for the possible regions that will support
  SMRAM.

  @param[in]     This           The EFI_SMM_ACCESS2_PROTOCOL instance.
  @param[in,out] SmramMapSize   A pointer to the size, in bytes, of the
                                SmramMemoryMap buffer.
  @param[in,out] SmramMap       A pointer to the buffer in which firmware
                                places the current memory map.

  @retval EFI_SUCCESS           The chipset supported the given resource.
  @retval EFI_BUFFER_TOO_SMALL  The SmramMap parameter was too small.  The
                                current buffer size needed to hold the memory
                                map is returned in SmramMapSize.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmAccess2GetCapabilities (
  IN CONST EFI_SMM_ACCESS2_PROTOCOL  *This,
  IN OUT UINTN                       *SmramMapSize,
  IN OUT EFI_SMRAM_DESCRIPTOR        *SmramMap
  )
{
  UINTN OrigSize;

  OrigSize      = *SmramMapSize;
  *SmramMapSize = sizeof *SmramMap; // 1 record
  if (OrigSize < *SmramMapSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  SmramMap->PhysicalStart = PcdGet64 (PcdEmuSmmAreaBase);
  SmramMap->CpuStart      = SmramMap->PhysicalStart;
  SmramMap->PhysicalSize  = PcdGet32 (PcdEmuSmmAreaSize);
  SmramMap->RegionState   = mAccess2.LockState ? EFI_SMRAM_LOCKED :
                            mAccess2.OpenState ? EFI_SMRAM_OPEN :
                            EFI_SMRAM_CLOSED;

  ASSERT (SmramMap->PhysicalStart != 0);
  DEBUG ((EFI_D_INFO, "EmuSmmAccess2GetCapabilities: "
    "SMRAM Start=0x%Lx Size=0x%Lx\n",
    SmramMap->PhysicalStart, SmramMap->PhysicalSize));
  return EFI_SUCCESS;
}


STATIC EFI_SMM_ACCESS2_PROTOCOL mAccess2 = {
  &EmuSmmAccess2Open,
  &EmuSmmAccess2Close,
  &EmuSmmAccess2Lock,
  &EmuSmmAccess2GetCapabilities,
  FALSE,                         // initial LockState
  TRUE                           // initial OpenState
};


/**
  Invokes SMI activation from either the preboot or runtime environment.

  This function generates an SMI.

  @param[in]     This                The EFI_SMM_CONTROL2_PROTOCOL instance.
  @param[in,out] CommandPort         The value written to the command port.
  @param[in,out] DataPort            The value written to the data port.
  @param[in]     Periodic            Optional mechanism to engender a periodic
                                     stream.
  @param[in]     ActivationInterval  Optional parameter to repeat at this
                                     period one time or, if the Periodic
                                     Boolean is set, periodically.

  @retval EFI_SUCCESS            The SMI/PMI has been engendered.
  @retval EFI_DEVICE_ERROR       The timing is unsupported.
  @retval EFI_INVALID_PARAMETER  The activation period is unsupported.
  @retval EFI_INVALID_PARAMETER  The last periodic activation has not been
                                 cleared.
  @retval EFI_NOT_STARTED        The SMM base service has not been initialized.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmControl2Trigger (
  IN CONST EFI_SMM_CONTROL2_PROTOCOL  *This,
  IN OUT UINT8                        *CommandPort       OPTIONAL,
  IN OUT UINT8                        *DataPort          OPTIONAL,
  IN BOOLEAN                          Periodic           OPTIONAL,
  IN UINTN                            ActivationInterval OPTIONAL
  )
{
  //
  // The CommandPort and DataPort parameters allow the caller to trigger
  // (dispatch) a specific SMI handler. We don't have anything like that in
  // OvmfPkg.
  //
  // The only call to this function is made in SmmCommunicationCommunicate()
  // [MdeModulePkg/Core/PiSmmCore/PiSmmIpl.c], ie.
  // EFI_SMM_COMMUNICATION_PROTOCOL.Communicate(), to enter SMM and get access
  // to SMRAM. Since our emulated SMRAM is always visible (doesn't support
  // closing or locking), we don't need to do anything here.
  //
  return EFI_SUCCESS;
}


/**
  Clears any system state that was created in response to the Trigger() call.

  This function acknowledges and causes the deassertion of the SMI activation
  source.

  @param[in] This                The EFI_SMM_CONTROL2_PROTOCOL instance.
  @param[in] Periodic            Optional parameter to repeat at this period
                                 one time

  @retval EFI_SUCCESS            The SMI/PMI has been engendered.
  @retval EFI_DEVICE_ERROR       The source could not be cleared.
  @retval EFI_INVALID_PARAMETER  The service did not support the Periodic input
                                 argument.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmControl2Clear (
  IN CONST EFI_SMM_CONTROL2_PROTOCOL  *This,
  IN BOOLEAN                          Periodic OPTIONAL
  )
{
  return EFI_SUCCESS;
}


STATIC EFI_SMM_CONTROL2_PROTOCOL mControl2 = {
 &EmuSmmControl2Trigger,
 &EmuSmmControl2Clear,
 100*1000                // MinimumTriggerPeriod, unit is 10ns; unused
};


//
// Entry point of this driver.
//
EFI_STATUS
EFIAPI
EmuSmmDxeEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_HANDLE Handle;

  if (PcdGet32 (PcdEmuSmmAreaSize) == 0) {
    return EFI_UNSUPPORTED;
  }

  Handle = NULL;
  return gBS->InstallMultipleProtocolInterfaces (&Handle,
                &gEfiSmmAccess2ProtocolGuid, &mAccess2,
                &gEfiSmmControl2ProtocolGuid, &mControl2,
                NULL);
}
