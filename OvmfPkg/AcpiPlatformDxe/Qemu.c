/** @file
  OVMF ACPI QEMU support

  Copyright (c) 2008 - 2012, Intel Corporation. All rights reserved.<BR>

  Copyright (C) 2012-2014, Red Hat, Inc.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "AcpiPlatform.h"
#include "QemuLoader.h"
#include <Library/BaseMemoryLib.h>        // CopyMem()
#include <Library/MemoryAllocationLib.h>  // AllocatePool()
#include <Library/QemuFwCfgLib.h>         // QemuFwCfgFindFile()
#include <Library/DxeServicesTableLib.h>  // gDS
#include <Library/PcdLib.h>               // PcdGet16()
#include <Library/UefiLib.h>              // EfiGetSystemConfigurationTable()
#include <Library/OrderedCollectionLib.h> // OrderedCollectionInit()
#include <IndustryStandard/Acpi.h>        // EFI_ACPI_DESCRIPTION_HEADER
#include <Guid/Acpi.h>                    // gEfiAcpi10TableGuid

BOOLEAN
QemuDetected (
  VOID
  )
{
  if (!QemuFwCfgIsAvailable ()) {
    return FALSE;
  }

  return TRUE;
}


STATIC
UINTN
CountBits16 (
  UINT16 Mask
  )
{
  //
  // For all N >= 1, N bits are enough to represent the number of bits set
  // among N bits. It's true for N == 1. When adding a new bit (N := N+1),
  // the maximum number of possibly set bits increases by one, while the
  // representable maximum doubles.
  //
  Mask = ((Mask & 0xAAAA) >> 1) + (Mask & 0x5555);
  Mask = ((Mask & 0xCCCC) >> 2) + (Mask & 0x3333);
  Mask = ((Mask & 0xF0F0) >> 4) + (Mask & 0x0F0F);
  Mask = ((Mask & 0xFF00) >> 8) + (Mask & 0x00FF);

  return Mask;
}


STATIC
EFI_STATUS
EFIAPI
QemuInstallAcpiMadtTable (
  IN   EFI_ACPI_TABLE_PROTOCOL       *AcpiProtocol,
  IN   VOID                          *AcpiTableBuffer,
  IN   UINTN                         AcpiTableBufferSize,
  OUT  UINTN                         *TableKey
  )
{
  UINTN                                               CpuCount;
  UINTN                                               PciLinkIsoCount;
  UINTN                                               NewBufferSize;
  EFI_ACPI_1_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *Madt;
  EFI_ACPI_1_0_PROCESSOR_LOCAL_APIC_STRUCTURE         *LocalApic;
  EFI_ACPI_1_0_IO_APIC_STRUCTURE                      *IoApic;
  EFI_ACPI_1_0_INTERRUPT_SOURCE_OVERRIDE_STRUCTURE    *Iso;
  EFI_ACPI_1_0_LOCAL_APIC_NMI_STRUCTURE               *LocalApicNmi;
  VOID                                                *Ptr;
  UINTN                                               Loop;
  EFI_STATUS                                          Status;

  ASSERT (AcpiTableBufferSize >= sizeof (EFI_ACPI_DESCRIPTION_HEADER));

  QemuFwCfgSelectItem (QemuFwCfgItemSmpCpuCount);
  CpuCount = QemuFwCfgRead16 ();
  ASSERT (CpuCount >= 1);

  //
  // Set Level-tiggered, Active High for these identity mapped IRQs. The bitset
  // corresponds to the union of all possible interrupt assignments for the LNKA,
  // LNKB, LNKC, LNKD PCI interrupt lines. See the DSDT.
  //
  PciLinkIsoCount = CountBits16 (PcdGet16 (Pcd8259LegacyModeEdgeLevel));

  NewBufferSize = 1                     * sizeof (*Madt) +
                  CpuCount              * sizeof (*LocalApic) +
                  1                     * sizeof (*IoApic) +
                  (1 + PciLinkIsoCount) * sizeof (*Iso) +
                  1                     * sizeof (*LocalApicNmi);

  Madt = AllocatePool (NewBufferSize);
  if (Madt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (&(Madt->Header), AcpiTableBuffer, sizeof (EFI_ACPI_DESCRIPTION_HEADER));
  Madt->Header.Length    = (UINT32) NewBufferSize;
  Madt->LocalApicAddress = PcdGet32 (PcdCpuLocalApicBaseAddress);
  Madt->Flags            = EFI_ACPI_1_0_PCAT_COMPAT;
  Ptr = Madt + 1;

  LocalApic = Ptr;
  for (Loop = 0; Loop < CpuCount; ++Loop) {
    LocalApic->Type            = EFI_ACPI_1_0_PROCESSOR_LOCAL_APIC;
    LocalApic->Length          = sizeof (*LocalApic);
    LocalApic->AcpiProcessorId = (UINT8) Loop;
    LocalApic->ApicId          = (UINT8) Loop;
    LocalApic->Flags           = 1; // enabled
    ++LocalApic;
  }
  Ptr = LocalApic;

  IoApic = Ptr;
  IoApic->Type             = EFI_ACPI_1_0_IO_APIC;
  IoApic->Length           = sizeof (*IoApic);
  IoApic->IoApicId         = (UINT8) CpuCount;
  IoApic->Reserved         = EFI_ACPI_RESERVED_BYTE;
  IoApic->IoApicAddress    = 0xFEC00000;
  IoApic->SystemVectorBase = 0x00000000;
  Ptr = IoApic + 1;

  //
  // IRQ0 (8254 Timer) => IRQ2 (PIC) Interrupt Source Override Structure
  //
  Iso = Ptr;
  Iso->Type                        = EFI_ACPI_1_0_INTERRUPT_SOURCE_OVERRIDE;
  Iso->Length                      = sizeof (*Iso);
  Iso->Bus                         = 0x00; // ISA
  Iso->Source                      = 0x00; // IRQ0
  Iso->GlobalSystemInterruptVector = 0x00000002;
  Iso->Flags                       = 0x0000; // Conforms to specs of the bus
  ++Iso;

  //
  // Set Level-tiggered, Active High for all possible PCI link targets.
  //
  for (Loop = 0; Loop < 16; ++Loop) {
    if ((PcdGet16 (Pcd8259LegacyModeEdgeLevel) & (1 << Loop)) == 0) {
      continue;
    }
    Iso->Type                        = EFI_ACPI_1_0_INTERRUPT_SOURCE_OVERRIDE;
    Iso->Length                      = sizeof (*Iso);
    Iso->Bus                         = 0x00; // ISA
    Iso->Source                      = (UINT8) Loop;
    Iso->GlobalSystemInterruptVector = (UINT32) Loop;
    Iso->Flags                       = 0x000D; // Level-tiggered, Active High
    ++Iso;
  }
  ASSERT (
    (UINTN) (Iso - (EFI_ACPI_1_0_INTERRUPT_SOURCE_OVERRIDE_STRUCTURE *)Ptr) ==
      1 + PciLinkIsoCount
    );
  Ptr = Iso;

  LocalApicNmi = Ptr;
  LocalApicNmi->Type            = EFI_ACPI_1_0_LOCAL_APIC_NMI;
  LocalApicNmi->Length          = sizeof (*LocalApicNmi);
  LocalApicNmi->AcpiProcessorId = 0xFF; // applies to all processors
  //
  // polarity and trigger mode of the APIC I/O input signals conform to the
  // specifications of the bus
  //
  LocalApicNmi->Flags           = 0x0000;
  //
  // Local APIC interrupt input LINTn to which NMI is connected.
  //
  LocalApicNmi->LocalApicInti   = 0x01;
  Ptr = LocalApicNmi + 1;

  ASSERT ((UINTN) ((UINT8 *)Ptr - (UINT8 *)Madt) == NewBufferSize);
  Status = InstallAcpiTable (AcpiProtocol, Madt, NewBufferSize, TableKey);

  FreePool (Madt);

  return Status;
}


#pragma pack(1)

typedef struct {
  UINT64 Base;
  UINT64 End;
  UINT64 Length;
} PCI_WINDOW;

typedef struct {
  PCI_WINDOW PciWindow32;
  PCI_WINDOW PciWindow64;
} FIRMWARE_DATA;

typedef struct {
  UINT8 BytePrefix;
  UINT8 ByteValue;
} AML_BYTE;

typedef struct {
  UINT8    NameOp;
  UINT8    RootChar;
  UINT8    NameChar[4];
  UINT8    PackageOp;
  UINT8    PkgLength;
  UINT8    NumElements;
  AML_BYTE Pm1aCntSlpTyp;
  AML_BYTE Pm1bCntSlpTyp;
  AML_BYTE Reserved[2];
} SYSTEM_STATE_PACKAGE;

#pragma pack()


STATIC
EFI_STATUS
EFIAPI
PopulateFwData(
  OUT  FIRMWARE_DATA *FwData
  )
{
  EFI_STATUS                      Status;
  UINTN                           NumDesc;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR *AllDesc;

  Status = gDS->GetMemorySpaceMap (&NumDesc, &AllDesc);
  if (Status == EFI_SUCCESS) {
    UINT64 NonMmio32MaxExclTop;
    UINT64 Mmio32MinBase;
    UINT64 Mmio32MaxExclTop;
    UINTN CurDesc;

    Status = EFI_UNSUPPORTED;

    NonMmio32MaxExclTop = 0;
    Mmio32MinBase = BASE_4GB;
    Mmio32MaxExclTop = 0;

    for (CurDesc = 0; CurDesc < NumDesc; ++CurDesc) {
      CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Desc;
      UINT64 ExclTop;

      Desc = &AllDesc[CurDesc];
      ExclTop = Desc->BaseAddress + Desc->Length;

      if (ExclTop <= (UINT64) PcdGet32 (PcdOvmfFdBaseAddress)) {
        switch (Desc->GcdMemoryType) {
          case EfiGcdMemoryTypeNonExistent:
            break;

          case EfiGcdMemoryTypeReserved:
          case EfiGcdMemoryTypeSystemMemory:
            if (NonMmio32MaxExclTop < ExclTop) {
              NonMmio32MaxExclTop = ExclTop;
            }
            break;

          case EfiGcdMemoryTypeMemoryMappedIo:
            if (Mmio32MinBase > Desc->BaseAddress) {
              Mmio32MinBase = Desc->BaseAddress;
            }
            if (Mmio32MaxExclTop < ExclTop) {
              Mmio32MaxExclTop = ExclTop;
            }
            break;

          default:
            ASSERT(0);
        }
      }
    }

    if (Mmio32MinBase < NonMmio32MaxExclTop) {
      Mmio32MinBase = NonMmio32MaxExclTop;
    }

    if (Mmio32MinBase < Mmio32MaxExclTop) {
      FwData->PciWindow32.Base   = Mmio32MinBase;
      FwData->PciWindow32.End    = Mmio32MaxExclTop - 1;
      FwData->PciWindow32.Length = Mmio32MaxExclTop - Mmio32MinBase;

      FwData->PciWindow64.Base   = 0;
      FwData->PciWindow64.End    = 0;
      FwData->PciWindow64.Length = 0;

      Status = EFI_SUCCESS;
    }

    FreePool (AllDesc);
  }

  DEBUG ((
    DEBUG_INFO,
    "ACPI PciWindow32: Base=0x%08lx End=0x%08lx Length=0x%08lx\n",
    FwData->PciWindow32.Base,
    FwData->PciWindow32.End,
    FwData->PciWindow32.Length
    ));
  DEBUG ((
    DEBUG_INFO,
    "ACPI PciWindow64: Base=0x%08lx End=0x%08lx Length=0x%08lx\n",
    FwData->PciWindow64.Base,
    FwData->PciWindow64.End,
    FwData->PciWindow64.Length
    ));

  return Status;
}


STATIC
VOID
EFIAPI
GetSuspendStates (
  UINTN                *SuspendToRamSize,
  SYSTEM_STATE_PACKAGE *SuspendToRam,
  UINTN                *SuspendToDiskSize,
  SYSTEM_STATE_PACKAGE *SuspendToDisk
  )
{
  STATIC CONST SYSTEM_STATE_PACKAGE Template = {
    0x08,                   // NameOp
    '\\',                   // RootChar
    { '_', 'S', 'x', '_' }, // NameChar[4]
    0x12,                   // PackageOp
    0x0A,                   // PkgLength
    0x04,                   // NumElements
    { 0x0A, 0x00 },         // Pm1aCntSlpTyp
    { 0x0A, 0x00 },         // Pm1bCntSlpTyp -- we don't support it
    {                       // Reserved[2]
      { 0x0A, 0x00 },
      { 0x0A, 0x00 }
    }
  };
  RETURN_STATUS                     Status;
  FIRMWARE_CONFIG_ITEM              FwCfgItem;
  UINTN                             FwCfgSize;
  UINT8                             SystemStates[6];

  //
  // configure defaults
  //
  *SuspendToRamSize = sizeof Template;
  CopyMem (SuspendToRam, &Template, sizeof Template);
  SuspendToRam->NameChar[2]             = '3'; // S3
  SuspendToRam->Pm1aCntSlpTyp.ByteValue = 1;   // PIIX4: STR

  *SuspendToDiskSize = sizeof Template;
  CopyMem (SuspendToDisk, &Template, sizeof Template);
  SuspendToDisk->NameChar[2]             = '4'; // S4
  SuspendToDisk->Pm1aCntSlpTyp.ByteValue = 2;   // PIIX4: POSCL

  //
  // check for overrides
  //
  Status = QemuFwCfgFindFile ("etc/system-states", &FwCfgItem, &FwCfgSize);
  if (Status != RETURN_SUCCESS || FwCfgSize != sizeof SystemStates) {
    DEBUG ((DEBUG_INFO, "ACPI using S3/S4 defaults\n"));
    return;
  }
  QemuFwCfgSelectItem (FwCfgItem);
  QemuFwCfgReadBytes (sizeof SystemStates, SystemStates);

  //
  // Each byte corresponds to a system state. In each byte, the MSB tells us
  // whether the given state is enabled. If so, the three LSBs specify the
  // value to be written to the PM control register's SUS_TYP bits.
  //
  if (SystemStates[3] & BIT7) {
    SuspendToRam->Pm1aCntSlpTyp.ByteValue =
        SystemStates[3] & (BIT2 | BIT1 | BIT0);
    DEBUG ((DEBUG_INFO, "ACPI S3 value: %d\n",
            SuspendToRam->Pm1aCntSlpTyp.ByteValue));
  } else {
    *SuspendToRamSize = 0;
    DEBUG ((DEBUG_INFO, "ACPI S3 disabled\n"));
  }

  if (SystemStates[4] & BIT7) {
    SuspendToDisk->Pm1aCntSlpTyp.ByteValue =
        SystemStates[4] & (BIT2 | BIT1 | BIT0);
    DEBUG ((DEBUG_INFO, "ACPI S4 value: %d\n",
            SuspendToDisk->Pm1aCntSlpTyp.ByteValue));
  } else {
    *SuspendToDiskSize = 0;
    DEBUG ((DEBUG_INFO, "ACPI S4 disabled\n"));
  }
}


STATIC
EFI_STATUS
EFIAPI
QemuInstallAcpiSsdtTable (
  IN   EFI_ACPI_TABLE_PROTOCOL       *AcpiProtocol,
  IN   VOID                          *AcpiTableBuffer,
  IN   UINTN                         AcpiTableBufferSize,
  OUT  UINTN                         *TableKey
  )
{
  EFI_STATUS    Status;
  FIRMWARE_DATA *FwData;

  Status = EFI_OUT_OF_RESOURCES;

  FwData = AllocateReservedPool (sizeof (*FwData));
  if (FwData != NULL) {
    UINTN                SuspendToRamSize;
    SYSTEM_STATE_PACKAGE SuspendToRam;
    UINTN                SuspendToDiskSize;
    SYSTEM_STATE_PACKAGE SuspendToDisk;
    UINTN                SsdtSize;
    UINT8                *Ssdt;

    GetSuspendStates (&SuspendToRamSize,  &SuspendToRam,
                      &SuspendToDiskSize, &SuspendToDisk);
    SsdtSize = AcpiTableBufferSize + 17 + SuspendToRamSize + SuspendToDiskSize;
    Ssdt = AllocatePool (SsdtSize);

    if (Ssdt != NULL) {
      Status = PopulateFwData (FwData);

      if (Status == EFI_SUCCESS) {
        UINT8 *SsdtPtr;

        SsdtPtr = Ssdt;

        CopyMem (SsdtPtr, AcpiTableBuffer, AcpiTableBufferSize);
        SsdtPtr += AcpiTableBufferSize;

        //
        // build "OperationRegion(FWDT, SystemMemory, 0x12345678, 0x87654321)"
        //
        *(SsdtPtr++) = 0x5B; // ExtOpPrefix
        *(SsdtPtr++) = 0x80; // OpRegionOp
        *(SsdtPtr++) = 'F';
        *(SsdtPtr++) = 'W';
        *(SsdtPtr++) = 'D';
        *(SsdtPtr++) = 'T';
        *(SsdtPtr++) = 0x00; // SystemMemory
        *(SsdtPtr++) = 0x0C; // DWordPrefix

        //
        // no virtual addressing yet, take the four least significant bytes
        //
        CopyMem(SsdtPtr, &FwData, 4);
        SsdtPtr += 4;

        *(SsdtPtr++) = 0x0C; // DWordPrefix

        *(UINT32*) SsdtPtr = sizeof (*FwData);
        SsdtPtr += 4;

        //
        // add suspend system states
        //
        CopyMem (SsdtPtr, &SuspendToRam, SuspendToRamSize);
        SsdtPtr += SuspendToRamSize;
        CopyMem (SsdtPtr, &SuspendToDisk, SuspendToDiskSize);
        SsdtPtr += SuspendToDiskSize;

        ASSERT((UINTN) (SsdtPtr - Ssdt) == SsdtSize);
        ((EFI_ACPI_DESCRIPTION_HEADER *) Ssdt)->Length = (UINT32) SsdtSize;
        Status = InstallAcpiTable (AcpiProtocol, Ssdt, SsdtSize, TableKey);
      }

      FreePool(Ssdt);
    }

    if (Status != EFI_SUCCESS) {
      FreePool(FwData);
    }
  }

  return Status;
}


EFI_STATUS
EFIAPI
QemuInstallAcpiTable (
  IN   EFI_ACPI_TABLE_PROTOCOL       *AcpiProtocol,
  IN   VOID                          *AcpiTableBuffer,
  IN   UINTN                         AcpiTableBufferSize,
  OUT  UINTN                         *TableKey
  )
{
  EFI_ACPI_DESCRIPTION_HEADER        *Hdr;
  EFI_ACPI_TABLE_INSTALL_ACPI_TABLE  TableInstallFunction;

  Hdr = (EFI_ACPI_DESCRIPTION_HEADER*) AcpiTableBuffer;
  switch (Hdr->Signature) {
  case EFI_ACPI_1_0_APIC_SIGNATURE:
    TableInstallFunction = QemuInstallAcpiMadtTable;
    break;
  case EFI_ACPI_1_0_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE:
    TableInstallFunction = QemuInstallAcpiSsdtTable;
    break;
  default:
    TableInstallFunction = InstallAcpiTable;
  }

  return TableInstallFunction (
           AcpiProtocol,
           AcpiTableBuffer,
           AcpiTableBufferSize,
           TableKey
           );
}


/**
  Check if an array of bytes starts with an RSD PTR structure, and if so,
  return the EFI ACPI table GUID that corresponds to its version.

  Checksum is ignored.

  @param[in] Buffer     The array to check.

  @param[in] Size       Number of bytes in Buffer.

  @param[out] AcpiTableGuid  On successful exit, pointer to the EFI ACPI table
                             GUID in statically allocated storage that
                             corresponds to the detected RSD PTR version.

  @retval  EFI_SUCCESS         RSD PTR structure detected at the beginning of
                               Buffer, and its advertised size does not exceed
                               Size.

  @retval  EFI_PROTOCOL_ERROR  RSD PTR structure detected at the beginning of
                               Buffer, but it has inconsistent size.

  @retval  EFI_NOT_FOUND       RSD PTR structure not found.

**/

STATIC
EFI_STATUS
CheckRsdp (
  IN  CONST VOID *Buffer,
  IN  UINTN      Size,
  OUT EFI_GUID   **AcpiTableGuid
  )
{
  CONST UINT64                                       *Signature;
  CONST EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp1;
  CONST EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp2;

  if (Size < sizeof *Signature) {
    return EFI_NOT_FOUND;
  }
  Signature = Buffer;

  if (*Signature != EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE) {
    return EFI_NOT_FOUND;
  }

  //
  // Signature found -- from this point on we can only report
  // EFI_PROTOCOL_ERROR or EFI_SUCCESS.
  //
  if (Size < sizeof *Rsdp1) {
    return EFI_PROTOCOL_ERROR;
  }
  Rsdp1 = Buffer;

  if (Rsdp1->Reserved == 0) {
    //
    // ACPI 1.0 doesn't include the Length field
    //
    *AcpiTableGuid = &gEfiAcpi10TableGuid;
    return EFI_SUCCESS;
  }

  if (Size < sizeof *Rsdp2) {
    return EFI_PROTOCOL_ERROR;
  }
  Rsdp2 = Buffer;

  if (Size < Rsdp2->Length || Rsdp2->Length < sizeof *Rsdp2) {
    return EFI_PROTOCOL_ERROR;
  }

  *AcpiTableGuid = &gEfiAcpi20TableGuid;
  return EFI_SUCCESS;
}

//
// The user structure for the ordered collection that will track the fw_cfg
// blobs under processing.
//
typedef struct {
  UINT8 File[QEMU_LOADER_FNAME_SIZE]; // NUL-terminated name of the fw_cfg
                                      // blob. This is the ordering / search
                                      // key.
  UINTN Size;                         // The number of bytes in this blob.
  UINT8 *Base;                        // Pointer to the blob data.
} BLOB;

/**
  Compare a standalone key against a user structure containing an embedded key.

  @param[in] StandaloneKey  Pointer to the bare key.

  @param[in] UserStruct     Pointer to the user structure with the embedded
                            key.

  @retval <0  If StandaloneKey compares less than UserStruct's key.

  @retval  0  If StandaloneKey compares equal to UserStruct's key.

  @retval >0  If StandaloneKey compares greater than UserStruct's key.
**/
STATIC
INTN
EFIAPI
BlobKeyCompare (
  IN CONST VOID *StandaloneKey,
  IN CONST VOID *UserStruct
  )
{
  CONST BLOB *Blob;

  Blob = UserStruct;
  return AsciiStrCmp (StandaloneKey, (CONST CHAR8 *)Blob->File);
}

/**
  Comparator function for two user structures.

  @param[in] UserStruct1  Pointer to the first user structure.

  @param[in] UserStruct2  Pointer to the second user structure.

  @retval <0  If UserStruct1 compares less than UserStruct2.

  @retval  0  If UserStruct1 compares equal to UserStruct2.

  @retval >0  If UserStruct1 compares greater than UserStruct2.
**/
STATIC
INTN
EFIAPI
BlobCompare (
  IN CONST VOID *UserStruct1,
  IN CONST VOID *UserStruct2
  )
{
  CONST BLOB *Blob1;

  Blob1 = UserStruct1;
  return BlobKeyCompare (Blob1->File, UserStruct2);
}

/**
  Download, process, and install ACPI table data from the QEMU loader
  interface.

  @retval  EFI_UNSUPPORTED       Firmware configuration is unavailable, or QEMU
                                 loader command with unsupported parameters
                                 has been found.

  @retval  EFI_NOT_FOUND         The host doesn't export the required fw_cfg
                                 files.

  @retval  EFI_OUT_OF_RESOURCES  Memory allocation failed.

  @retval  EFI_PROTOCOL_ERROR    Found invalid fw_cfg contents.

  @retval  EFI_ALREADY_STARTED   One of the ACPI TABLE GUIDs has been found in
                                 the EFI Configuration Table, indicating the
                                 presence of a preexistent RSD PTR table, and
                                 therefore that of another module installing
                                 ACPI tables.

  @retval  EFI_SUCCESS           Installation of ACPI tables succeeded.

**/

EFI_STATUS
EFIAPI
InstallAllQemuLinkedTables (
  VOID
  )
{
  EFI_STATUS               Status;
  FIRMWARE_CONFIG_ITEM     FwCfgItem;
  UINTN                    FwCfgSize;
  VOID                     *Rsdp;
  UINTN                    RsdpBufferSize;
  UINT8                    *Loader;
  CONST QEMU_LOADER_ENTRY  *LoaderEntry, *LoaderEnd;
  ORDERED_COLLECTION       *Tracker;
  ORDERED_COLLECTION_ENTRY *TrackerEntry, *TrackerEntry2;
  BLOB                     *Blob;
  EFI_GUID                 *AcpiTableGuid;

  //
  // This function allocates memory on four levels. From lowest to highest:
  // - Areas consisting of whole pages, of type EfiACPIMemoryNVS, for
  //   (processed) ACPI payload,
  // - BLOB structures referencing the above, tracking their names, sizes, and
  //   addresses,
  // - ORDERED_COLLECTION_ENTRY objects internal to OrderedCollectionLib,
  //   linking the BLOB structures,
  // - an ORDERED_COLLECTION organizing the ORDERED_COLLECTION_ENTRY entries.
  //
  // On exit, the last three levels are torn down unconditionally. If we exit
  // with success, then the first (lowest) level is left in place, constituting
  // the ACPI tables for the guest. If we exit with error, then even the first
  // (lowest) level is torn down.
  //

  Status = QemuFwCfgFindFile ("etc/table-loader", &FwCfgItem, &FwCfgSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  if (FwCfgSize % sizeof *LoaderEntry != 0) {
    DEBUG ((EFI_D_ERROR, "%a: \"etc/table-loader\" has invalid size 0x%Lx\n",
      __FUNCTION__, (UINT64)FwCfgSize));
    return EFI_PROTOCOL_ERROR;
  }

  if (!EFI_ERROR (EfiGetSystemConfigurationTable (
                    &gEfiAcpi10TableGuid, &Rsdp)) ||
      !EFI_ERROR (EfiGetSystemConfigurationTable (
                    &gEfiAcpi20TableGuid, &Rsdp))) {
    DEBUG ((EFI_D_ERROR, "%a: RSD PTR already present\n", __FUNCTION__));
    return EFI_ALREADY_STARTED;
  }

  Loader = AllocatePool (FwCfgSize);
  if (Loader == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  QemuFwCfgSelectItem (FwCfgItem);
  QemuFwCfgReadBytes (FwCfgSize, Loader);

  Tracker = OrderedCollectionInit (BlobCompare, BlobKeyCompare);
  if (Tracker == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeLoader;
  }

  Rsdp           = NULL;
  RsdpBufferSize = 0;

  LoaderEntry = (QEMU_LOADER_ENTRY *)Loader;
  LoaderEnd   = (QEMU_LOADER_ENTRY *)(Loader + FwCfgSize);
  while (LoaderEntry < LoaderEnd) {
    switch (LoaderEntry->Type) {
    case QemuLoaderCmdAllocate: {
      CONST QEMU_LOADER_ALLOCATE *Allocate;
      UINTN                      NumPages;
      EFI_PHYSICAL_ADDRESS       Address;

      Allocate = &LoaderEntry->Command.Allocate;
      if (Allocate->File[QEMU_LOADER_FNAME_SIZE - 1] != '\0') {
        DEBUG ((EFI_D_ERROR, "%a: malformed file name in Allocate command\n",
          __FUNCTION__));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }
      if (Allocate->Alignment > EFI_PAGE_SIZE) {
        DEBUG ((EFI_D_ERROR, "%a: unsupported alignment 0x%x in Allocate "
          "command\n", __FUNCTION__, Allocate->Alignment));
        Status = EFI_UNSUPPORTED;
        goto FreeTracker;
      }
      Status = QemuFwCfgFindFile ((CHAR8 *)Allocate->File, &FwCfgItem,
                 &FwCfgSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "%a: nonexistent file \"%a\" in Allocate "
          "command\n", __FUNCTION__, Allocate->File));
        goto FreeTracker;
      }

      NumPages = EFI_SIZE_TO_PAGES (FwCfgSize);
      Address = 0xFFFFFFFF;
      Status = gBS->AllocatePages (AllocateMaxAddress, EfiACPIMemoryNVS,
                      NumPages, &Address);
      if (EFI_ERROR (Status)) {
        goto FreeTracker;
      }

      Blob = AllocatePool (sizeof *Blob);
      if (Blob == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto FreePages;
      }
      CopyMem (Blob->File, Allocate->File, QEMU_LOADER_FNAME_SIZE);
      Blob->Size = FwCfgSize;
      Blob->Base = (VOID *)(UINTN)Address;

      if (Allocate->Zone == QemuLoaderAllocFSeg) {
        if (Rsdp == NULL) {
          Rsdp = Blob->Base;
          RsdpBufferSize = Blob->Size;
        } else {
          DEBUG ((EFI_D_ERROR, "%a: duplicate RSD PTR candidate in Allocate "
            "command\n", __FUNCTION__));
          Status = EFI_PROTOCOL_ERROR;
          goto FreeBlob;
        }
      }

      Status = OrderedCollectionInsert (Tracker, NULL, Blob);
      if (Status == RETURN_ALREADY_STARTED) {
        DEBUG ((EFI_D_ERROR, "%a: duplicated file \"%a\" in Allocate "
          "command\n", __FUNCTION__, Allocate->File));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeBlob;
      }
      if (EFI_ERROR (Status)) {
        goto FreeBlob;
      }

      QemuFwCfgSelectItem (FwCfgItem);
      QemuFwCfgReadBytes (FwCfgSize, Blob->Base);
      ZeroMem (Blob->Base + Blob->Size,
        EFI_PAGES_TO_SIZE (NumPages) - Blob->Size);

      DEBUG ((EFI_D_VERBOSE, "%a: Allocate: File=\"%a\" Alignment=0x%x "
        "Zone=%d Size=0x%Lx Address=0x%Lx\n", __FUNCTION__, Allocate->File,
        Allocate->Alignment, Allocate->Zone, (UINT64)Blob->Size,
        (UINT64)(UINTN)Blob->Base));
      break;

FreeBlob:
      FreePool (Blob);
FreePages:
      gBS->FreePages (Address, NumPages);
      goto FreeTracker;
    }

    case QemuLoaderCmdAddPointer: {
      CONST QEMU_LOADER_ADD_POINTER *AddPointer;
      CONST BLOB                    *Blob2;
      UINT8                         *PointerField;

      AddPointer = &LoaderEntry->Command.AddPointer;
      if (AddPointer->PointerFile[QEMU_LOADER_FNAME_SIZE - 1] != '\0' ||
          AddPointer->PointeeFile[QEMU_LOADER_FNAME_SIZE - 1] != '\0') {
        DEBUG ((EFI_D_ERROR, "%a: malformed file name in AddPointer command\n",
          __FUNCTION__));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }

      TrackerEntry = OrderedCollectionFind (Tracker, AddPointer->PointerFile);
      TrackerEntry2 = OrderedCollectionFind (Tracker, AddPointer->PointeeFile);
      if (TrackerEntry == NULL || TrackerEntry2 == NULL) {
        DEBUG ((EFI_D_ERROR, "%a: invalid blob reference(s) \"%a\" / \"%a\" "
          "in AddPointer command\n", __FUNCTION__, AddPointer->PointerFile,
          AddPointer->PointeeFile));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }

      Blob = OrderedCollectionUserStruct (TrackerEntry);
      Blob2 = OrderedCollectionUserStruct (TrackerEntry2);
      if ((AddPointer->PointerSize != 1 && AddPointer->PointerSize != 2 &&
           AddPointer->PointerSize != 4 && AddPointer->PointerSize != 8) ||
          Blob->Size < AddPointer->PointerSize ||
          Blob->Size - AddPointer->PointerSize < AddPointer->PointerOffset) {
        DEBUG ((EFI_D_ERROR, "%a: invalid pointer location in \"%a\" in "
          "AddPointer command\n", __FUNCTION__, AddPointer->PointerFile));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }

      PointerField = Blob->Base + AddPointer->PointerOffset;
      switch (AddPointer->PointerSize) {
      case 1:
        *PointerField += (UINT8)(UINTN)Blob2->Base;
        break;

      case 2:
        *(UINT16 *)PointerField += (UINT16)(UINTN)Blob2->Base;
        break;

      case 4:
        *(UINT32 *)PointerField += (UINT32)(UINTN)Blob2->Base;
        break;

      case 8:
        *(UINT64 *)PointerField += (UINT64)(UINTN)Blob2->Base;
        break;
      }

      DEBUG ((EFI_D_VERBOSE, "%a: AddPointer: PointerFile=\"%a\" "
        "PointeeFile=\"%a\" PointerOffset=0x%x PointerSize=%d\n", __FUNCTION__,
        AddPointer->PointerFile, AddPointer->PointeeFile,
        AddPointer->PointerOffset, AddPointer->PointerSize));
      break;
    }

    case QemuLoaderCmdAddChecksum: {
      CONST QEMU_LOADER_ADD_CHECKSUM *AddChecksum;

      AddChecksum = &LoaderEntry->Command.AddChecksum;
      if (AddChecksum->File[QEMU_LOADER_FNAME_SIZE - 1] != '\0') {
        DEBUG ((EFI_D_ERROR, "%a: malformed file name in AddChecksum "
          "command\n", __FUNCTION__));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }

      TrackerEntry = OrderedCollectionFind (Tracker, AddChecksum->File);
      if (TrackerEntry == NULL) {
        DEBUG ((EFI_D_ERROR, "%a: invalid blob reference \"%a\" in "
          "AddChecksum command\n", __FUNCTION__, AddChecksum->File));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }

      Blob = OrderedCollectionUserStruct (TrackerEntry);
      if (Blob->Size <= AddChecksum->ResultOffset ||
          Blob->Size < AddChecksum->Length ||
          Blob->Size - AddChecksum->Length < AddChecksum->Start) {
        DEBUG ((EFI_D_ERROR, "%a: invalid checksum location or range in "
          "\"%a\" in AddChecksum command\n", __FUNCTION__, AddChecksum->File));
        Status = EFI_PROTOCOL_ERROR;
        goto FreeTracker;
      }

      Blob->Base[AddChecksum->ResultOffset] = CalculateCheckSum8 (
                                               Blob->Base + AddChecksum->Start,
                                               AddChecksum->Length
                                               );
      DEBUG ((EFI_D_VERBOSE, "%a: AddChecksum: File=\"%a\" ResultOffset=0x%x "
        "Start=0x%x Length=0x%x\n", __FUNCTION__, AddChecksum->File,
        AddChecksum->ResultOffset, AddChecksum->Start, AddChecksum->Length));
      break;
    }

    default:
      DEBUG ((EFI_D_VERBOSE, "%a: unknown loader command: 0x%x\n",
        __FUNCTION__, LoaderEntry->Type));
      break;
    }

    ++LoaderEntry;
  }

  if (Rsdp == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: no RSD PTR candidate\n", __FUNCTION__));
    Status = EFI_PROTOCOL_ERROR;
    goto FreeTracker;
  }

  AcpiTableGuid = NULL;
  if (EFI_ERROR (CheckRsdp (Rsdp, RsdpBufferSize, &AcpiTableGuid))) {
    DEBUG ((EFI_D_ERROR, "%a: RSD PTR not found in candidate\n",
      __FUNCTION__));
    Status = EFI_PROTOCOL_ERROR;
    goto FreeTracker;
  }

  Status = gBS->InstallConfigurationTable (AcpiTableGuid, Rsdp);

FreeTracker:
  //
  // Tear down the tracker structure, and if we're exiting with an error, the
  // pages holding the blob data (ie. the processed ACPI payload) as well.
  //
  for (TrackerEntry = OrderedCollectionMin (Tracker); TrackerEntry != NULL;
       TrackerEntry = TrackerEntry2) {
    VOID *UserStruct;

    TrackerEntry2 = OrderedCollectionNext (TrackerEntry);
    OrderedCollectionDelete (Tracker, TrackerEntry, &UserStruct);
    if (EFI_ERROR (Status)) {
      Blob = UserStruct;
      gBS->FreePages ((UINTN)Blob->Base, EFI_SIZE_TO_PAGES (Blob->Size));
    }
    FreePool (UserStruct);
  }
  OrderedCollectionUninit (Tracker);

FreeLoader:
  FreePool (Loader);
  return Status;
}
