/** @file
  Library exposing OVMF's emulated NVRAM.

  Copyright (C) 2013, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/EmuNvramLib.h>

/**
  Return the size of the NVRAM portion used for SMRAM emulation.

  @retval  0 if SMRAM emulation inside the NVRAM is disabled.
  @return  Size otherwise.
*/
UINT32
EFIAPI
EmuNvramSmramSize (
  VOID
  )
{
  return PcdGet32 (PcdEmuNvramSmramSize);
}

/**
  Return the size of the NVRAM portion that stores the pointer to the SMST
  (system management system table).

  @retval  0                            if the NVRAM doesn't provide such
                                        storage,
  @retval  sizeof(EFI_PHYSICAL_ADDRESS) if the storage is provided.
*/
UINT32
EFIAPI
EmuNvramSmstPtrSize (
  VOID
  )
{
  UINT32 Size;

  Size = PcdGet32 (PcdEmuNvramSmstPtrSize);
  ASSERT (Size == 0 || Size == sizeof (EFI_PHYSICAL_ADDRESS));
  return Size;
}

/**
  Return the size of the NVRAM portion used for S3 Resume Pool emulation.

  @retval  0 if S3 Resume Pool emulation inside the NVRAM is disabled.
  @return  Size otherwise.
*/
UINT32
EFIAPI
EmuNvramS3ResumePoolSize (
  VOID
  )
{
  return PcdGet32 (PcdEmuNvramS3ResumePoolSize);
}

/**
  Return the full (cumulative) size of the emulated NVRAM.

  @retval  0 if NVRAM emulation is disbled.
  @return  Size otherwise.
**/
UINT32
EFIAPI
EmuNvramSize (
  VOID
  )
{
  return ALIGN_VALUE (EmuNvramSmramSize (), EFI_PAGE_SIZE) +
         ALIGN_VALUE (EmuNvramSmstPtrSize (), EFI_PAGE_SIZE) +
         ALIGN_VALUE (EmuNvramS3ResumePoolSize (), EFI_PAGE_SIZE);
}

/**
  Return the base address of the emulated NVRAM.
**/
UINT32
EFIAPI
EmuNvramBase (
  VOID
  )
{
  return PcdGet32 (PcdOvmfMemFvBase) + PcdGet32 (PcdOvmfMemFvSize) + SIZE_4MB;
}

/**
  Return the base address of the NVRAM portion used for SMRAM emulation.
*/
UINT32
EFIAPI
EmuNvramSmramBase (
  VOID
  )
{
  return EmuNvramBase ();
}

/**
  Return the base address of the NVRAM portion that stores the pointer to the
  SMST (system management system table).
*/
UINT32
EFIAPI
EmuNvramSmstPtrBase (
  VOID
  )
{
  return EmuNvramSmramBase () +
    ALIGN_VALUE (EmuNvramSmramSize (), EFI_PAGE_SIZE);
}


/**
  Return the base address of the NVRAM portion used for S3 Resume Pool
  emulation.
*/
UINT32
EFIAPI
EmuNvramS3ResumePoolBase (
  VOID
  )
{
  return EmuNvramSmstPtrBase () +
    ALIGN_VALUE (EmuNvramSmstPtrSize (), EFI_PAGE_SIZE);
}
