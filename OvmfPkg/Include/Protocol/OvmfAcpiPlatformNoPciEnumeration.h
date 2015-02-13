/** @file
  Platform-specific drivers can install this protocol to allow
  OvmfPkg/AcpiPlatformDxe to proceed with ACPI table installation without
  waiting for PCI enumeration completion.

  A new, standalone handle is recommended to carry the protocol. The interface
  associated with the protocol is supposed to be NULL.

  Copyright (C) 2015, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __OVMF_ACPI_PLATFORM_NO_PCI_ENUMERATION_H__
#define __OVMF_ACPI_PLATFORM_NO_PCI_ENUMERATION_H__

#define OVMF_ACPI_PLATFORM_NO_PCI_ENUMERATION_PROTOCOL_GUID \
{0x2e96f1b1, 0xc880, 0x4893, {0xb3, 0xeb, 0x64, 0x2c, 0xb4, 0x99, 0x1c, 0x5f}}

extern EFI_GUID gOvmfAcpiPlatformNoPciEnumerationProtocolGuid;

#endif
