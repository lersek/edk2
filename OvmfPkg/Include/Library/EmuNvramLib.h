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

#ifndef __EMU_NVRAM_LIB__
#define __EMU_NVRAM_LIB__

#include <Uefi/UefiBaseType.h>

//
// Please see the documentation in the C source file.
//

UINT32 EFIAPI EmuNvramBase             (VOID);
UINT32 EFIAPI EmuNvramSize             (VOID);
UINT32 EFIAPI EmuNvramSmramBase        (VOID);
UINT32 EFIAPI EmuNvramSmramSize        (VOID);
UINT32 EFIAPI EmuNvramSmstPtrBase      (VOID);
UINT32 EFIAPI EmuNvramSmstPtrSize      (VOID);
UINT32 EFIAPI EmuNvramS3ResumePoolBase (VOID);
UINT32 EFIAPI EmuNvramS3ResumePoolSize (VOID);
#endif
