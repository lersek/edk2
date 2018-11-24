/** @file
  Expose the address(es) of the ACPI RSD PTR table(s) in a MB-aligned structure
  to the hypervisor.

  The hypervisor locates the MB-aligned structure based on the signature GUID
  that is at offset 0 in the structure. Once the RSD PTR address(es) are
  retrieved, the hypervisor may perform various ACPI checks.

  This feature is a development aid, for supporting ACPI table unit tests in
  hypervisors. Do not enable in production builds.

  Copyright (C) 2018, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License that accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __ACPI_TEST_SUPPORT_H__
#define __ACPI_TEST_SUPPORT_H__

#include <Uefi/UefiBaseType.h>

#define ACPI_TEST_SUPPORT_GUID                         \
  {                                                    \
    0x5478594e,                                        \
    0xdfcb,                                            \
    0x425f,                                            \
    { 0x8e, 0x42, 0xc8, 0xaf, 0xf8, 0x8a, 0x88, 0x7a } \
  }

extern EFI_GUID gAcpiTestSupportGuid;

//
// The following structure must be allocated in Boot Services Data type memory,
// aligned at a 1MB boundary.
//
#pragma pack (1)
typedef struct {
  //
  // The signature GUID is written to the MB-aligned structure from
  // gAcpiTestSupportGuid, but with all bits inverted. That's the actual GUID
  // value that the hypervisor should look for at each MB boundary, looping
  // over all guest RAM pages with that alignment, until a match is found. The
  // bit-flipping occurs in order not to store the actual GUID in any UEFI
  // executable, which might confuse guest memory analysis. Note that EFI_GUID
  // has little endian representation.
  //
  EFI_GUID             InverseSignatureGuid;
  //
  // The Rsdp10 and Rsdp20 fields may be read when the signature GUID matches.
  // Rsdp10 is the guest-physical address of the ACPI 1.0 specification RSD PTR
  // table, in 8-byte little endian representation. Rsdp20 is the same, for the
  // ACPI 2.0 or later specification RSD PTR table. Each of these fields may be
  // zero (independently of the other) if the UEFI System Table does not
  // provide the corresponding UEFI Configuration Table.
  //
  EFI_PHYSICAL_ADDRESS Rsdp10;
  EFI_PHYSICAL_ADDRESS Rsdp20;
} ACPI_TEST_SUPPORT;
#pragma pack ()

#endif // __ACPI_TEST_SUPPORT_H__
