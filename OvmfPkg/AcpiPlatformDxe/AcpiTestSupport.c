/** @file
  Register a Ready-To-Boot callback for populating the ACPI_TEST_SUPPORT
  structure.

  Copyright (C) 2018, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "AcpiPlatform.h"

#include <Guid/Acpi.h>
#include <Guid/AcpiTestSupport.h>
#include <Guid/EventGroup.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

STATIC volatile ACPI_TEST_SUPPORT *mAcpiTestSupport;
STATIC EFI_EVENT                  mAcpiTestSupportEvent;

STATIC
VOID
EFIAPI
AcpiTestSupportOnReadyToBoot (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  VOID                          *Pages;
  CONST VOID                    *Rsdp10;
  CONST VOID                    *Rsdp20;
  CONST EFI_CONFIGURATION_TABLE *ConfigTable;
  CONST EFI_CONFIGURATION_TABLE *ConfigTablesEnd;
  volatile EFI_GUID             *InverseSignature;
  UINTN                         Idx;

  Pages = AllocateAlignedPages (EFI_SIZE_TO_PAGES (sizeof *mAcpiTestSupport),
            SIZE_1MB);
  if (Pages == NULL) {
    DEBUG ((DEBUG_WARN, "%a: AllocateAlignedPages() failed\n", __FUNCTION__));
    UnregisterAcpiTestSupport ();
    return;
  }

  //
  // Locate both gEfiAcpi10TableGuid and gEfiAcpi20TableGuid config tables in
  // one go.
  //
  Rsdp10 = NULL;
  Rsdp20 = NULL;
  ConfigTable = gST->ConfigurationTable;
  ConfigTablesEnd = gST->ConfigurationTable + gST->NumberOfTableEntries;
  while ((Rsdp10 == NULL || Rsdp20 == NULL) && ConfigTable < ConfigTablesEnd) {
    if (CompareGuid (&ConfigTable->VendorGuid, &gEfiAcpi10TableGuid)) {
      Rsdp10 = ConfigTable->VendorTable;
    } else if (CompareGuid (&ConfigTable->VendorGuid, &gEfiAcpi20TableGuid)) {
      Rsdp20 = ConfigTable->VendorTable;
    }
    ++ConfigTable;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: AcpiTestSupport=%p Rsdp10=%p Rsdp20=%p\n",
    __FUNCTION__, Pages, Rsdp10, Rsdp20));

  //
  // Store the RSD PTR address(es) first, then the signature second.
  //
  mAcpiTestSupport = Pages;
  mAcpiTestSupport->Rsdp10 = (UINTN)Rsdp10;
  mAcpiTestSupport->Rsdp20 = (UINTN)Rsdp20;

  MemoryFence();

  InverseSignature = &mAcpiTestSupport->InverseSignatureGuid;
  InverseSignature->Data1  = gAcpiTestSupportGuid.Data1;
  InverseSignature->Data1 ^= MAX_UINT32;
  InverseSignature->Data2  = gAcpiTestSupportGuid.Data2;
  InverseSignature->Data2 ^= MAX_UINT16;
  InverseSignature->Data3  = gAcpiTestSupportGuid.Data3;
  InverseSignature->Data3 ^= MAX_UINT16;
  for (Idx = 0; Idx < sizeof InverseSignature->Data4; ++Idx) {
    InverseSignature->Data4[Idx]  = gAcpiTestSupportGuid.Data4[Idx];
    InverseSignature->Data4[Idx] ^= MAX_UINT8;
  }

  UnregisterAcpiTestSupport ();
}

VOID
RegisterAcpiTestSupport (
  VOID
  )
{
  EFI_STATUS Status;

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                  AcpiTestSupportOnReadyToBoot, NULL /* Context */,
                  &gEfiEventReadyToBootGuid, &mAcpiTestSupportEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: CreateEventEx(): %r\n", __FUNCTION__, Status));
  }
}

VOID
UnregisterAcpiTestSupport (
  VOID
  )
{
  if (mAcpiTestSupportEvent != NULL) {
    gBS->CloseEvent (mAcpiTestSupportEvent);
    mAcpiTestSupportEvent = NULL;
  }
}
