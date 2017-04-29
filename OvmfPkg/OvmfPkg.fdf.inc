## @file
#  FDF include file that defines the main macros and sets the dependent PCDs.
#
#  Copyright (C) 2014, Red Hat, Inc.
#  Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
#
#  This program and the accompanying materials are licensed and made available
#  under the terms and conditions of the BSD License which accompanies this
#  distribution. The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
#  IMPLIED.
#
##

#
# Default flash size is 2MB.
#
# Defining FD_SIZE_1MB on the build command line can override this.
#
# A firmware binary built for the default 2MB flash size, and a firmware binary
# built with FD_SIZE_1MB defined, use the same variable store layout.
#
# Defining FD_SIZE_4MB results in a different (much larger) variable store
# structure that is incompatible with both the default and the FD_SIZE_1MB
# firmware binaries.
#

DEFINE BLOCK_SIZE        = 0x1000

!ifdef $(FD_SIZE_4MB)

DEFINE VARS_SIZE         = 0x80000
DEFINE VARS_BLOCKS       = 0x80
DEFINE VARS_LIVE_SIZE    = 0x3E000
DEFINE VARS_SPARE_SIZE   = 0x40000

DEFINE FW_BASE_ADDRESS   = 0xFFC00000
DEFINE FW_SIZE           = 0x00400000
DEFINE FW_BLOCKS         = 0x400
DEFINE CODE_BASE_ADDRESS = 0xFFC80000
DEFINE CODE_SIZE         = 0x00380000
DEFINE CODE_BLOCKS       = 0x380
DEFINE FVMAIN_SIZE       = 0x0034C000
DEFINE SECFV_OFFSET      = 0x003CC000
DEFINE SECFV_SIZE        = 0x34000

!else

DEFINE VARS_SIZE         = 0x20000
DEFINE VARS_BLOCKS       = 0x20
DEFINE VARS_LIVE_SIZE    = 0xE000
DEFINE VARS_SPARE_SIZE   = 0x10000

!ifdef $(FD_SIZE_1MB)

DEFINE FW_BASE_ADDRESS   = 0xFFF00000
DEFINE FW_SIZE           = 0x00100000
DEFINE FW_BLOCKS         = 0x100
DEFINE CODE_BASE_ADDRESS = 0xFFF20000
DEFINE CODE_SIZE         = 0x000E0000
DEFINE CODE_BLOCKS       = 0xE0
DEFINE FVMAIN_SIZE       = 0x000CC000
DEFINE SECFV_OFFSET      = 0x000EC000
DEFINE SECFV_SIZE        = 0x14000

!else

DEFINE FW_BASE_ADDRESS   = 0xFFE00000
DEFINE FW_SIZE           = 0x00200000
DEFINE FW_BLOCKS         = 0x200
DEFINE CODE_BASE_ADDRESS = 0xFFE20000
DEFINE CODE_SIZE         = 0x001E0000
DEFINE CODE_BLOCKS       = 0x1E0
DEFINE FVMAIN_SIZE       = 0x001AC000
DEFINE SECFV_OFFSET      = 0x001CC000
DEFINE SECFV_SIZE        = 0x34000

!endif

!endif

SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFdBaseAddress     = $(FW_BASE_ADDRESS)
SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFirmwareFdSize    = $(FW_SIZE)
SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFirmwareBlockSize = $(BLOCK_SIZE)

SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageVariableBase = $(FW_BASE_ADDRESS)
SET gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize = $(VARS_LIVE_SIZE)

SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageEventLogBase = gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageVariableBase + gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize
SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageEventLogSize = $(BLOCK_SIZE)

SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageFtwWorkingBase = gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageEventLogBase + gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageEventLogSize
SET gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingSize = $(BLOCK_SIZE)

SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageFtwSpareBase = gUefiOvmfPkgTokenSpaceGuid.PcdOvmfFlashNvStorageFtwWorkingBase + gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwWorkingSize
SET gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageFtwSpareSize = $(VARS_SPARE_SIZE)

DEFINE MEMFD_BASE_ADDRESS = 0x800000
