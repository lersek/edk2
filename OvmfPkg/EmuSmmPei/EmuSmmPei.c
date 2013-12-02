/** @file

  PEI module emulating SMRAM access by producing PEI_SMM_ACCESS_PPI and
  EFI_PEI_SMM_COMMUNICATION_PPI.

  Copyright (C) 2013, Red Hat, Inc.<BR>

  Copyright (c) 2010, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Guid/AcpiS3Context.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>
#include <Ppi/SmmAccess.h>
#include <Ppi/SmmCommunication.h>

/**
  Communicates with a registered handler.

  This function provides a service to send and receive messages from a
  registered UEFI service.

  @param[in] This                The EFI_PEI_SMM_COMMUNICATION_PPI instance.
  @param[in] CommBuffer          A pointer to the buffer to convey into SMRAM.
  @param[in] CommSize            The size of the data buffer being passed in.On
                                 exit, the size of data being returned. Zero if
                                 the handler does not wish to reply with any
                                 data.

  @retval EFI_SUCCESS            The message was successfully posted.
  @retval EFI_INVALID_PARAMETER  The CommBuffer was NULL.
**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmCommunicationCommunicate (
  IN CONST EFI_PEI_SMM_COMMUNICATION_PPI   *This,
  IN OUT VOID                              *CommBuffer,
  IN OUT UINTN                             *CommSize
  )
{
  return EFI_NOT_STARTED;
}


STATIC EFI_PEI_SMM_COMMUNICATION_PPI mCommunication = {
  &EmuSmmCommunicationCommunicate
};


STATIC PEI_SMM_ACCESS_PPI mAccess;


/**
  Opens the SMRAM area to be accessible by a PEIM driver.

  This function "opens" SMRAM so that it is visible while not inside of SMM.
  The function should return EFI_UNSUPPORTED if the hardware does not support
  hiding of SMRAM. The function should return EFI_DEVICE_ERROR if the SMRAM
  configuration is locked.

  @param  PeiServices            General purpose services available to every
                                 PEIM.
  @param  This                   The pointer to the SMM Access Interface.
  @param  DescriptorIndex        The region of SMRAM to Open.

  @retval EFI_SUCCESS            The region was successfully opened.
  @retval EFI_DEVICE_ERROR       The region could not be opened because locked
                                 by chipset.
  @retval EFI_INVALID_PARAMETER  The descriptor index was out of bounds.

**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmPeiAccessOpen (
  IN EFI_PEI_SERVICES                **PeiServices,
  IN PEI_SMM_ACCESS_PPI              *This,
  IN UINTN                           DescriptorIndex
  )
{
  if (DescriptorIndex > 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (mAccess.LockState) {
    return EFI_DEVICE_ERROR;
  }
  mAccess.OpenState = TRUE;
  return EFI_SUCCESS;
}


/**
  Inhibits access to the SMRAM.

  This function "closes" SMRAM so that it is not visible while outside of SMM.
  The function should return EFI_UNSUPPORTED if the hardware does not support
  hiding of SMRAM.

  @param  PeiServices              General purpose services available to every
                                   PEIM.
  @param  This                     The pointer to the SMM Access Interface.
  @param  DescriptorIndex          The region of SMRAM to Close.

  @retval EFI_SUCCESS              The region was successfully closed.
  @retval EFI_DEVICE_ERROR         The region could not be closed because
                                   locked by chipset.
  @retval EFI_INVALID_PARAMETER    The descriptor index was out of bounds.

**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmPeiAccessClose (
  IN EFI_PEI_SERVICES                **PeiServices,
  IN PEI_SMM_ACCESS_PPI              *This,
  IN UINTN                           DescriptorIndex
  )
{
  if (DescriptorIndex > 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (mAccess.LockState) {
    return EFI_DEVICE_ERROR;
  }
  mAccess.OpenState = FALSE;
  return EFI_SUCCESS;
}


/**
  Inhibits access to the SMRAM.

  This function prohibits access to the SMRAM region.  This function is usually
  implemented such that it is a write-once operation.

  @param  PeiServices              General purpose services available to every
                                   PEIM.
  @param  This                     The pointer to the SMM Access Interface.
  @param  DescriptorIndex          The region of SMRAM to Close.

  @retval EFI_SUCCESS            The region was successfully locked.
  @retval EFI_DEVICE_ERROR       The region could not be locked because at
                                 least one range is still open.
  @retval EFI_INVALID_PARAMETER  The descriptor index was out of bounds.

**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmPeiAccessLock (
  IN EFI_PEI_SERVICES                **PeiServices,
  IN PEI_SMM_ACCESS_PPI              *This,
  IN UINTN                           DescriptorIndex
  )
{
  if (DescriptorIndex > 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (mAccess.OpenState) {
    return EFI_DEVICE_ERROR;
  }
  mAccess.LockState = TRUE;
  return EFI_SUCCESS;
}


/**
  Queries the memory controller for the possible regions that will support
  SMRAM.

  @param  PeiServices           General purpose services available to every
                                PEIM.
  @param This                   The pointer to the SmmAccessPpi Interface.
  @param SmramMapSize           The pointer to the variable containing size of
                                the buffer to contain the description
                                information.
  @param SmramMap               The buffer containing the data describing the
                                Smram region descriptors.

  @retval EFI_BUFFER_TOO_SMALL  The user did not provide a sufficient buffer.
  @retval EFI_SUCCESS           The user provided a sufficiently-sized buffer.

**/
STATIC
EFI_STATUS
EFIAPI
EmuSmmPeiAccessGetCapabilities (
  IN EFI_PEI_SERVICES                **PeiServices,
  IN PEI_SMM_ACCESS_PPI              *This,
  IN OUT UINTN                       *SmramMapSize,
  IN OUT EFI_SMRAM_DESCRIPTOR        *SmramMap
  )
{
  UINTN OrigSize;

  OrigSize      = *SmramMapSize;
  *SmramMapSize = sizeof *SmramMap; // 1 record
  if (OrigSize < *SmramMapSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  SmramMap->PhysicalStart = PcdGet64 (PcdEmuSmmAreaBase);
  SmramMap->CpuStart      = SmramMap->PhysicalStart;
  SmramMap->PhysicalSize  = PcdGet32 (PcdEmuSmmAreaSize);
  SmramMap->RegionState   = mAccess.LockState ? EFI_SMRAM_LOCKED :
                            mAccess.OpenState ? EFI_SMRAM_OPEN :
                            EFI_SMRAM_CLOSED;

  ASSERT (SmramMap->PhysicalStart != 0);
  DEBUG ((EFI_D_INFO, "EmuSmmPeiAccessGetCapabilities: "
    "SMRAM Start=0x%Lx Size=0x%Lx\n",
    SmramMap->PhysicalStart, SmramMap->PhysicalSize));
  return EFI_SUCCESS;
}


STATIC PEI_SMM_ACCESS_PPI mAccess = {
  &EmuSmmPeiAccessOpen,
  &EmuSmmPeiAccessClose,
  &EmuSmmPeiAccessLock,
  &EmuSmmPeiAccessGetCapabilities,
  FALSE,                           // initial LockState
  TRUE                             // initial OpenState
};


STATIC EFI_PEI_PPI_DESCRIPTOR mPpiSmmCommunication[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI,
    &gEfiPeiSmmCommunicationPpiGuid, &mCommunication
  },
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
    &gPeiSmmAccessPpiGuid, &mAccess
  }
};


STATIC SMM_S3_RESUME_STATE mResumeState;


EFI_STATUS
EFIAPI
EmuSmmPeiEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  UINT64               *DiscloseSmstPtrPtr;
  EFI_SMRAM_DESCRIPTOR *SmramDesc;

  if (PcdGet32 (PcdEmuSmmAreaSize) == 0) {
    return EFI_UNSUPPORTED;
  }

  DiscloseSmstPtrPtr = (UINT64 *)(UINTN) PcdGet64 (PcdDiscloseSmstPtrPtr);
  if (DiscloseSmstPtrPtr == NULL) {
    return EFI_UNSUPPORTED;
  }

  mResumeState.Smst = (EFI_PHYSICAL_ADDRESS) *DiscloseSmstPtrPtr;

  SmramDesc = (EFI_SMRAM_DESCRIPTOR *) BuildGuidHob (&gEfiAcpiVariableGuid,
                                         sizeof (EFI_SMRAM_DESCRIPTOR));
  if (SmramDesc == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem (SmramDesc, sizeof *SmramDesc);
  SmramDesc->CpuStart = (EFI_PHYSICAL_ADDRESS)(UINTN) &mResumeState;

  return PeiServicesInstallPpi (mPpiSmmCommunication);
}
