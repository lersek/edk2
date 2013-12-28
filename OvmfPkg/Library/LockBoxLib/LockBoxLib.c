/** @file

  Library implementing the LockBox interface on top of OVMF's emulated NVRAM.

  Copyright (C) 2013, Red Hat, Inc.
  Copyright (c) 2010 - 2011, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/EmuNvramLib.h>
#include <Library/LockBoxLib.h>

#pragma pack(1)
typedef struct {
  EFI_GUID             Guid;
  EFI_PHYSICAL_ADDRESS OrigAddress;
  UINT32               Size;        // including this header
  UINT64               Attributes;
  //
  // Positive length data follows here. It's empty only for the terminator
  // header.
  //
} LOCK_BOX_HEADER;
#pragma pack()


RETURN_STATUS
EFIAPI
LockBoxLibInitialize (
  VOID
  )
{
  LOCK_BOX_HEADER *Header;

  //
  // The LockBox storage can be disabled, or it must be able to hold the
  // terminator entry.
  //
  if (EmuNvramLockBoxSize () == 0) {
    return RETURN_SUCCESS;
  }
  if (EmuNvramLockBoxSize () < sizeof (LOCK_BOX_HEADER)) {
    return RETURN_UNSUPPORTED;
  }

  //
  // If the executable including this library is the first one to look at the
  // lockbox after cold boot, then prepare the terminator element.
  //
  Header = (LOCK_BOX_HEADER *)(UINTN) EmuNvramLockBoxBase ();
  ASSERT (Header->Size == 0 || Header->Size >= sizeof *Header);
  if (Header->Size == 0) {
    Header->Size = sizeof *Header;
  }
  return RETURN_SUCCESS;
}


/**
  Find LockBox entry based on GUID.

  This function may only be called after EmuNvramLockBoxSize() returns nonzero.

  @param[in] Guid  The GUID to search for.

  @return  Address of the LOCK_BOX_HEADER found. If LOCK_BOX_HEADER.Size is
           greater than sizeof(LOCK_BOX_HEADER), then the returned header
           contains the Guid of interest. Otherwise, the Guid was not found and
           the terminator header is returned (whose size is exactly
           sizeof(LOCK_BOX_HEADER)).
**/
STATIC
LOCK_BOX_HEADER *
EFIAPI
FindHeaderByGuid (
  IN CONST EFI_GUID *Guid
  )
{
  LOCK_BOX_HEADER *Header;

  Header = (LOCK_BOX_HEADER *)(UINTN) EmuNvramLockBoxBase ();
  while (Header->Size > sizeof *Header && !CompareGuid (Guid, &Header->Guid)) {
    Header = (LOCK_BOX_HEADER *) ((UINT8 *) Header + Header->Size);
  }
  ASSERT (Header->Size >= sizeof *Header);
  return Header;
}


/**
  This function will save confidential information to lockbox.

  @param Guid       the guid to identify the confidential information
  @param Buffer     the address of the confidential information
  @param Length     the length of the confidential information

  @retval RETURN_SUCCESS            the information is saved successfully.
  @retval RETURN_INVALID_PARAMETER  the Guid is NULL, or Buffer is NULL, or
                                    Length is 0
  @retval RETURN_ALREADY_STARTED    the requested GUID already exist.
  @retval RETURN_OUT_OF_RESOURCES   no enough resource to save the information.
  @retval RETURN_ACCESS_DENIED      it is too late to invoke this interface
  @retval RETURN_NOT_STARTED        it is too early to invoke this interface
  @retval RETURN_UNSUPPORTED        the service is not supported by
                                    implementaion.
**/
RETURN_STATUS
EFIAPI
SaveLockBox (
  IN  GUID                        *Guid,
  IN  VOID                        *Buffer,
  IN  UINTN                       Length
  )
{
  UINT32          Size;
  UINTN           Free;
  LOCK_BOX_HEADER *Header;

  if (Guid == NULL || Buffer == NULL || Length == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Length > 0xFFFFFFFF - sizeof *Header) {
    return RETURN_OUT_OF_RESOURCES;
  }
  Size = (UINT32) (sizeof *Header + Length);

  Free = EmuNvramLockBoxSize ();
  if (Free == 0) {
    return RETURN_UNSUPPORTED;
  }

  Header = FindHeaderByGuid (Guid);
  if (Header->Size > sizeof *Header) {
    return RETURN_ALREADY_STARTED;
  }
  Free -= (UINTN) Header - EmuNvramLockBoxBase();

  ASSERT (Free >= sizeof *Header);
  Free -= sizeof *Header;

  if (Free < Size) {
    return RETURN_OUT_OF_RESOURCES;
  }

  //
  // overwrite the current terminator header with new metadata
  //
  CopyGuid (&Header->Guid, Guid);
  Header->OrigAddress = (UINTN) Buffer;
  Header->Size        = Size;
  Header->Attributes  = 0;

  //
  // copy contents
  //
  CopyMem (Header + 1, Buffer, Length);

  //
  // add new terminator
  //
  Header = (LOCK_BOX_HEADER *) ((UINT8 *) Header + Header->Size);
  Header->Size = sizeof *Header;

  DEBUG ((DEBUG_VERBOSE, "%a: Guid=%g Buffer=%p Length=0x%x\n", __FUNCTION__,
    Guid, Buffer, (UINT32) Length));
  return RETURN_SUCCESS;
}


/**
  This function will set lockbox attributes.

  @param Guid       the guid to identify the confidential information
  @param Attributes the attributes of the lockbox

  @retval RETURN_SUCCESS            the information is saved successfully.
  @retval RETURN_INVALID_PARAMETER  attributes is invalid.
  @retval RETURN_NOT_FOUND          the requested GUID not found.
  @retval RETURN_ACCESS_DENIED      it is too late to invoke this interface
  @retval RETURN_NOT_STARTED        it is too early to invoke this interface
  @retval RETURN_UNSUPPORTED        the service is not supported by
                                    implementaion.
**/
RETURN_STATUS
EFIAPI
SetLockBoxAttributes (
  IN  GUID                        *Guid,
  IN  UINT64                      Attributes
  )
{
  LOCK_BOX_HEADER *Header;

  if (Guid == NULL) {
    return RETURN_INVALID_PARAMETER;
  }
  if (EmuNvramLockBoxSize () == 0) {
    return RETURN_UNSUPPORTED;
  }

  Header = FindHeaderByGuid (Guid);
  if (Header->Size == sizeof *Header) {
    return RETURN_NOT_FOUND;
  }
  Header->Attributes = Attributes;

  DEBUG ((DEBUG_VERBOSE, "%a: Guid=%g Attributes=0x%Lx\n", __FUNCTION__, Guid,
    Attributes));
  return RETURN_SUCCESS;
}


/**
  This function will update confidential information to lockbox.

  @param Guid   the guid to identify the original confidential information
  @param Offset the offset of the original confidential information
  @param Buffer the address of the updated confidential information
  @param Length the length of the updated confidential information

  @retval RETURN_SUCCESS            the information is saved successfully.
  @retval RETURN_INVALID_PARAMETER  the Guid is NULL, or Buffer is NULL, or
                                    Length is 0.
  @retval RETURN_NOT_FOUND          the requested GUID not found.
  @retval RETURN_BUFFER_TOO_SMALL   the original buffer to too small to hold
                                    new information.
  @retval RETURN_ACCESS_DENIED      it is too late to invoke this interface
  @retval RETURN_NOT_STARTED        it is too early to invoke this interface
  @retval RETURN_UNSUPPORTED        the service is not supported by
                                    implementaion.
**/
RETURN_STATUS
EFIAPI
UpdateLockBox (
  IN  GUID                        *Guid,
  IN  UINTN                       Offset,
  IN  VOID                        *Buffer,
  IN  UINTN                       Length
  )
{
  LOCK_BOX_HEADER *Header;

  if (Guid == NULL || Buffer == NULL || Length == 0) {
    return RETURN_INVALID_PARAMETER;
  }
  if (EmuNvramLockBoxSize () == 0) {
    return RETURN_UNSUPPORTED;
  }

  Header = FindHeaderByGuid (Guid);
  if (Header->Size == sizeof *Header) {
    return RETURN_NOT_FOUND;
  }

  if (Header->Size - sizeof *Header < Offset ||
      Header->Size - sizeof *Header - Offset < Length) {
    return RETURN_BUFFER_TOO_SMALL;
  }
  CopyMem ((UINT8 *) (Header + 1) + Offset, Buffer, Length);

  DEBUG ((DEBUG_VERBOSE, "%a: Guid=%g Offset=0x%x Length=0x%x\n", __FUNCTION__,
    Guid, (UINT32) Offset, (UINT32) Length));
  return RETURN_SUCCESS;
}


/**
  This function will restore confidential information from lockbox.

  @param Guid   the guid to identify the confidential information
  @param Buffer the address of the restored confidential information
                NULL means restored to original address, Length MUST be NULL at
                same time.
  @param Length the length of the restored confidential information

  @retval RETURN_SUCCESS            the information is restored successfully.
  @retval RETURN_INVALID_PARAMETER  the Guid is NULL, or one of Buffer and
                                    Length is NULL.
  @retval RETURN_WRITE_PROTECTED    Buffer and Length are NULL, but the LockBox
                                    has no LOCK_BOX_ATTRIBUTE_RESTORE_IN_PLACE
                                    attribute.
  @retval RETURN_BUFFER_TOO_SMALL   the Length is too small to hold the
                                    confidential information.
  @retval RETURN_NOT_FOUND          the requested GUID not found.
  @retval RETURN_NOT_STARTED        it is too early to invoke this interface
  @retval RETURN_ACCESS_DENIED      not allow to restore to the address
  @retval RETURN_UNSUPPORTED        the service is not supported by
                                    implementaion.
**/
RETURN_STATUS
EFIAPI
RestoreLockBox (
  IN  GUID                        *Guid,
  IN  VOID                        *Buffer, OPTIONAL
  IN  OUT UINTN                   *Length  OPTIONAL
  )
{
  LOCK_BOX_HEADER *Header;

  if (Guid == NULL || (Buffer == NULL) ^ (*Length == 0)) {
    return RETURN_INVALID_PARAMETER;
  }
  if (EmuNvramLockBoxSize () == 0) {
    return RETURN_UNSUPPORTED;
  }

  Header = FindHeaderByGuid (Guid);
  if (Header->Size == sizeof *Header) {
    return RETURN_NOT_FOUND;
  }

  if (Buffer == NULL) {
    if (!(Header->Attributes & LOCK_BOX_ATTRIBUTE_RESTORE_IN_PLACE)) {
      return RETURN_WRITE_PROTECTED;
    }
    if ((UINTN) Header->OrigAddress != Header->OrigAddress) {
      return RETURN_UNSUPPORTED;
    }
    Buffer = (VOID *)(UINTN) Header->OrigAddress;
  } else {
    UINTN OrigLength;

    OrigLength = *Length;
    *Length = Header->Size - sizeof *Header;

    if (OrigLength < *Length) {
      return RETURN_BUFFER_TOO_SMALL;
    }
  }
  CopyMem (Buffer, Header + 1, Header->Size - sizeof *Header);

  DEBUG ((DEBUG_VERBOSE, "%a: Guid=%g Buffer=%p\n", __FUNCTION__, Guid,
    Buffer));
  return RETURN_SUCCESS;
}


/**
  This function will restore confidential information from all lockbox which
  have RestoreInPlace attribute.

  @retval RETURN_SUCCESS            the information is restored successfully.
  @retval RETURN_NOT_STARTED        it is too early to invoke this interface
  @retval RETURN_UNSUPPORTED        the service is not supported by
                                    implementaion.
**/
RETURN_STATUS
EFIAPI
RestoreAllLockBoxInPlace (
  VOID
  )
{
  LOCK_BOX_HEADER *Header;

  if (EmuNvramLockBoxSize () == 0) {
    return RETURN_UNSUPPORTED;
  }

  Header = (LOCK_BOX_HEADER *)(UINTN) EmuNvramLockBoxBase ();
  while (Header->Size > sizeof *Header) {
    if (Header->Attributes & LOCK_BOX_ATTRIBUTE_RESTORE_IN_PLACE) {
      VOID *Buffer;

      if ((UINTN) Header->OrigAddress != Header->OrigAddress) {
        return RETURN_UNSUPPORTED;
      }
      Buffer = (VOID *)(UINTN) Header->OrigAddress;
      CopyMem (Buffer, Header + 1, Header->Size - sizeof *Header);
      DEBUG ((DEBUG_VERBOSE, "%a: Guid=%g Buffer=%p\n", __FUNCTION__,
        Header->Guid, Buffer));
    }

    Header = (LOCK_BOX_HEADER *) ((UINT8 *) Header + Header->Size);
  }
  return RETURN_SUCCESS;
}
