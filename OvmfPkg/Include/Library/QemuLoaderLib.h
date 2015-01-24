/** @file
  Client library for the QEMU FwCfg table loader interface.

  Copyright (C) 2014-2015, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __QEMU_LOADER_LIB_H__
#define __QEMU_LOADER_LIB_H__

#include <Protocol/AcpiTable.h>

/**
  Download, process, and install ACPI table data from the QEMU loader
  interface.

  @param[in] AcpiProtocol  The ACPI table protocol used to install tables.

  @retval  EFI_UNSUPPORTED       Firmware configuration is unavailable, or QEMU
                                 loader command with unsupported parameters
                                 has been found.

  @retval  EFI_NOT_FOUND         The host doesn't export the required fw_cfg
                                 files.

  @retval  EFI_OUT_OF_RESOURCES  Memory allocation failed, or more than
                                 INSTALLED_TABLES_MAX tables found.

  @retval  EFI_PROTOCOL_ERROR    Found invalid fw_cfg contents.

  @return                        Status codes returned by
                                 AcpiProtocol->InstallAcpiTable().

**/
EFI_STATUS
EFIAPI
InstallAllQemuLinkedTables (
  IN   EFI_ACPI_TABLE_PROTOCOL       *AcpiProtocol
  );

#endif
