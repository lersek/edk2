/** @file

  A hook-in library for variable-related PEIMs, in order to set
  - gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableBase64,
  - gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingBase,
  - gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareBase,
  from their gUefiOvmfPkgTokenSpaceGuid counterparts, just before those PEIMs
  consume them.

  Copyright (C) 2017, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/PcdLib.h>

RETURN_STATUS
EFIAPI
SetFlashNvStorageAddresses (
  VOID
  )
{
  RETURN_STATUS PcdStatus;

  PcdStatus = PcdSet64S (
                PcdFlashNvStorageVariableBase64,
                PcdGet32 (PcdOvmfFlashNvStorageVariableBase)
                );
  if (RETURN_ERROR (PcdStatus)) {
    return PcdStatus;
  }

  PcdStatus = PcdSet32S (
                PcdFlashNvStorageFtwWorkingBase,
                PcdGet32 (PcdOvmfFlashNvStorageFtwWorkingBase)
                );
  if (RETURN_ERROR (PcdStatus)) {
    return PcdStatus;
  }

  PcdStatus = PcdSet32S (
                PcdFlashNvStorageFtwSpareBase,
                PcdGet32 (PcdOvmfFlashNvStorageFtwSpareBase)
                );
  return PcdStatus;
}
