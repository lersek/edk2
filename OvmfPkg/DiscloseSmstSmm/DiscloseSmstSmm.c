/** @file
  An SMM driver that publishes the address of the SMST.

  Copyright (C) 2013, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/DebugLib.h>
#include <Library/EmuNvramLib.h>
#include <Library/SmmServicesTableLib.h>

EFI_STATUS
EFIAPI
DiscloseSmstSmmEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  ASSERT (EmuNvramSmstPtrSize () == sizeof (EFI_PHYSICAL_ADDRESS));
  *(EFI_PHYSICAL_ADDRESS *)(UINTN) EmuNvramSmstPtrBase () =
    (EFI_PHYSICAL_ADDRESS)(UINTN) gSmst;

  //
  // No need to keep this in memory after saving the pointer at a known
  // address.
  //
  return EFI_ABORTED;
}
