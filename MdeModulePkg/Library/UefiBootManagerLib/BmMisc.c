/** @file
  Misc library functions.

Copyright (c) 2011 - 2017, Intel Corporation. All rights reserved.<BR>
(C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "InternalBm.h"

/**
  Delete the instance in Multi which matches partly with Single instance

  @param  Multi                 A pointer to a multi-instance device path data
                                structure.
  @param  Single                A pointer to a single-instance device path data
                                structure.

  @return This function will remove the device path instances in Multi which partly
          match with the Single, and return the result device path. If there is no
          remaining device path as a result, this function will return NULL.

**/
EFI_DEVICE_PATH_PROTOCOL *
BmDelPartMatchInstance (
  IN     EFI_DEVICE_PATH_PROTOCOL  *Multi,
  IN     EFI_DEVICE_PATH_PROTOCOL  *Single
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Instance;
  EFI_DEVICE_PATH_PROTOCOL  *NewDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *TempNewDevicePath;
  UINTN                     InstanceSize;
  UINTN                     SingleDpSize;

  NewDevicePath     = NULL;
  TempNewDevicePath = NULL;

  if (Multi == NULL || Single == NULL) {
    return Multi;
  }

  Instance        = GetNextDevicePathInstance (&Multi, &InstanceSize);
  SingleDpSize    = GetDevicePathSize (Single) - END_DEVICE_PATH_LENGTH;
  InstanceSize   -= END_DEVICE_PATH_LENGTH;

  while (Instance != NULL) {

    if (CompareMem (Instance, Single, MIN (SingleDpSize, InstanceSize)) != 0) {
      //
      // Append the device path instance which does not match with Single
      //
      TempNewDevicePath = NewDevicePath;
      NewDevicePath = AppendDevicePathInstance (NewDevicePath, Instance);
      if (TempNewDevicePath != NULL) {
        FreePool(TempNewDevicePath);
      }
    }
    FreePool(Instance);
    Instance      = GetNextDevicePathInstance (&Multi, &InstanceSize);
    InstanceSize -= END_DEVICE_PATH_LENGTH;
  }

  return NewDevicePath;
}

/**
  Function compares a device path data structure to that of all the nodes of a
  second device path instance.

  @param  Multi                 A pointer to a multi-instance device path data
                                structure.
  @param  Single                A pointer to a single-instance device path data
                                structure.

  @retval TRUE                  If the Single device path is contained within Multi device path.
  @retval FALSE                 The Single device path is not match within Multi device path.

**/
BOOLEAN
BmMatchDevicePaths (
  IN  EFI_DEVICE_PATH_PROTOCOL  *Multi,
  IN  EFI_DEVICE_PATH_PROTOCOL  *Single
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePathInst;
  UINTN                     Size;

  if (Multi == NULL || Single  == NULL) {
    return FALSE;
  }

  DevicePath     = Multi;
  DevicePathInst = GetNextDevicePathInstance (&DevicePath, &Size);

  //
  // Search for the match of 'Single' in 'Multi'
  //
  while (DevicePathInst != NULL) {
    //
    // If the single device path is found in multiple device paths,
    // return success
    //
    if (CompareMem (Single, DevicePathInst, Size) == 0) {
      FreePool (DevicePathInst);
      return TRUE;
    }

    FreePool (DevicePathInst);
    DevicePathInst = GetNextDevicePathInstance (&DevicePath, &Size);
  }

  return FALSE;
}

/**
  This routine adjust the memory information for different memory type and 
  save them into the variables for next boot. It resets the system when
  memory information is updated and the current boot option belongs to
  boot category instead of application category. It doesn't count the
  reserved memory occupied by RAM Disk.

  @param Boot               TRUE if current boot option belongs to boot
                            category instead of application category.
**/
VOID
BmSetMemoryTypeInformationVariable (
  IN BOOLEAN                    Boot
  )
{
  EFI_STATUS                   Status;
  EFI_MEMORY_TYPE_INFORMATION  *PreviousMemoryTypeInformation;
  EFI_MEMORY_TYPE_INFORMATION  *CurrentMemoryTypeInformation;
  UINTN                        VariableSize;
  UINTN                        Index;
  UINTN                        Index1;
  UINT32                       Previous;
  UINT32                       Current;
  UINT32                       Next;
  EFI_HOB_GUID_TYPE            *GuidHob;
  BOOLEAN                      MemoryTypeInformationModified;
  BOOLEAN                      MemoryTypeInformationVariableExists;
  EFI_BOOT_MODE                BootMode;

  MemoryTypeInformationModified       = FALSE;
  MemoryTypeInformationVariableExists = FALSE;


  BootMode = GetBootModeHob ();
  //
  // In BOOT_IN_RECOVERY_MODE, Variable region is not reliable.
  //
  if (BootMode == BOOT_IN_RECOVERY_MODE) {
    return;
  }

  //
  // Only check the the Memory Type Information variable in the boot mode 
  // other than BOOT_WITH_DEFAULT_SETTINGS because the Memory Type
  // Information is not valid in this boot mode.
  //
  if (BootMode != BOOT_WITH_DEFAULT_SETTINGS) {
    VariableSize = 0;
    Status = gRT->GetVariable (
                    EFI_MEMORY_TYPE_INFORMATION_VARIABLE_NAME,
                    &gEfiMemoryTypeInformationGuid,
                    NULL, 
                    &VariableSize, 
                    NULL
                    );
    if (Status == EFI_BUFFER_TOO_SMALL) {
      MemoryTypeInformationVariableExists = TRUE;
    }
  }

  //
  // Retrieve the current memory usage statistics.  If they are not found, then
  // no adjustments can be made to the Memory Type Information variable.
  //
  Status = EfiGetSystemConfigurationTable (
             &gEfiMemoryTypeInformationGuid,
             (VOID **) &CurrentMemoryTypeInformation
             );
  if (EFI_ERROR (Status) || CurrentMemoryTypeInformation == NULL) {
    return;
  }

  //
  // Get the Memory Type Information settings from Hob if they exist,
  // PEI is responsible for getting them from variable and build a Hob to save them.
  // If the previous Memory Type Information is not available, then set defaults
  //
  GuidHob = GetFirstGuidHob (&gEfiMemoryTypeInformationGuid);
  if (GuidHob == NULL) {
    //
    // If Platform has not built Memory Type Info into the Hob, just return.
    //
    return;
  }
  VariableSize                  = GET_GUID_HOB_DATA_SIZE (GuidHob);
  PreviousMemoryTypeInformation = AllocateCopyPool (VariableSize, GET_GUID_HOB_DATA (GuidHob));
  if (PreviousMemoryTypeInformation == NULL) {
    return;
  }

  //
  // Use a heuristic to adjust the Memory Type Information for the next boot
  //
  DEBUG ((EFI_D_INFO, "Memory  Previous  Current    Next   \n"));
  DEBUG ((EFI_D_INFO, " Type    Pages     Pages     Pages  \n"));
  DEBUG ((EFI_D_INFO, "======  ========  ========  ========\n"));

  for (Index = 0; PreviousMemoryTypeInformation[Index].Type != EfiMaxMemoryType; Index++) {

    for (Index1 = 0; CurrentMemoryTypeInformation[Index1].Type != EfiMaxMemoryType; Index1++) {
      if (PreviousMemoryTypeInformation[Index].Type == CurrentMemoryTypeInformation[Index1].Type) {
        break;
      }
    }
    if (CurrentMemoryTypeInformation[Index1].Type == EfiMaxMemoryType) {
      continue;
    }

    //
    // Previous is the number of pages pre-allocated
    // Current is the number of pages actually needed
    //
    Previous = PreviousMemoryTypeInformation[Index].NumberOfPages;
    Current  = CurrentMemoryTypeInformation[Index1].NumberOfPages;
    Next     = Previous;

    //
    // Inconsistent Memory Reserved across bootings may lead to S4 fail
    // Write next varible to 125% * current when the pre-allocated memory is:
    //  1. More than 150% of needed memory and boot mode is BOOT_WITH_DEFAULT_SETTING
    //  2. Less than the needed memory
    //
    if ((Current + (Current >> 1)) < Previous) {
      if (BootMode == BOOT_WITH_DEFAULT_SETTINGS) {
        Next = Current + (Current >> 2);
      }
    } else if (Current > Previous) {
      Next = Current + (Current >> 2);
    }
    if (Next > 0 && Next < 4) {
      Next = 4;
    }

    if (Next != Previous) {
      PreviousMemoryTypeInformation[Index].NumberOfPages = Next;
      MemoryTypeInformationModified = TRUE;
    }

    DEBUG ((EFI_D_INFO, "  %02x    %08x  %08x  %08x\n", PreviousMemoryTypeInformation[Index].Type, Previous, Current, Next));
  }

  //
  // If any changes were made to the Memory Type Information settings, then set the new variable value;
  // Or create the variable in first boot.
  //
  if (MemoryTypeInformationModified || !MemoryTypeInformationVariableExists) {
    Status = BmSetVariableAndReportStatusCodeOnError (
               EFI_MEMORY_TYPE_INFORMATION_VARIABLE_NAME,
               &gEfiMemoryTypeInformationGuid,
               EFI_VARIABLE_NON_VOLATILE  | EFI_VARIABLE_BOOTSERVICE_ACCESS,
               VariableSize,
               PreviousMemoryTypeInformation
               );

    if (!EFI_ERROR (Status)) {
      //
      // If the Memory Type Information settings have been modified and the boot option belongs to boot category,
      // then reset the platform so the new Memory Type Information setting will be used to guarantee that an S4
      // entry/resume cycle will not fail.
      //
      if (MemoryTypeInformationModified) {
        DEBUG ((EFI_D_INFO, "Memory Type Information settings change.\n"));
        if (Boot && PcdGetBool (PcdResetOnMemoryTypeInformationChange)) {
          DEBUG ((EFI_D_INFO, "...Warm Reset!!!\n"));
          gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
        }
      }
    } else {
      DEBUG ((EFI_D_ERROR, "Memory Type Information settings cannot be saved. OS S4 may fail!\n"));
    }
  }
  FreePool (PreviousMemoryTypeInformation);
}

/**
  Set the variable and report the error through status code upon failure.

  @param  VariableName           A Null-terminated string that is the name of the vendor's variable.
                                 Each VariableName is unique for each VendorGuid. VariableName must
                                 contain 1 or more characters. If VariableName is an empty string,
                                 then EFI_INVALID_PARAMETER is returned.
  @param  VendorGuid             A unique identifier for the vendor.
  @param  Attributes             Attributes bitmask to set for the variable.
  @param  DataSize               The size in bytes of the Data buffer. Unless the EFI_VARIABLE_APPEND_WRITE, 
                                 or EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute is set, a size of zero
                                 causes the variable to be deleted. When the EFI_VARIABLE_APPEND_WRITE attribute is 
                                 set, then a SetVariable() call with a DataSize of zero will not cause any change to 
                                 the variable value (the timestamp associated with the variable may be updated however 
                                 even if no new data value is provided,see the description of the 
                                 EFI_VARIABLE_AUTHENTICATION_2 descriptor below. In this case the DataSize will not 
                                 be zero since the EFI_VARIABLE_AUTHENTICATION_2 descriptor will be populated). 
  @param  Data                   The contents for the variable.

  @retval EFI_SUCCESS            The firmware has successfully stored the variable and its data as
                                 defined by the Attributes.
  @retval EFI_INVALID_PARAMETER  An invalid combination of attribute bits, name, and GUID was supplied, or the
                                 DataSize exceeds the maximum allowed.
  @retval EFI_INVALID_PARAMETER  VariableName is an empty string.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be retrieved due to a hardware error.
  @retval EFI_WRITE_PROTECTED    The variable in question is read-only.
  @retval EFI_WRITE_PROTECTED    The variable in question cannot be deleted.
  @retval EFI_SECURITY_VIOLATION The variable could not be written due to EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACESS
                                 being set, but the AuthInfo does NOT pass the validation check carried out by the firmware.

  @retval EFI_NOT_FOUND          The variable trying to be updated or deleted was not found.
**/
EFI_STATUS
BmSetVariableAndReportStatusCodeOnError (
  IN CHAR16     *VariableName,
  IN EFI_GUID   *VendorGuid,
  IN UINT32     Attributes,
  IN UINTN      DataSize,
  IN VOID       *Data
  )
{
  EFI_STATUS                 Status;
  EDKII_SET_VARIABLE_STATUS  *SetVariableStatus;
  UINTN                      NameSize;

  Status = gRT->SetVariable (
                  VariableName,
                  VendorGuid,
                  Attributes,
                  DataSize,
                  Data
                  );
  if (EFI_ERROR (Status)) {
    NameSize = StrSize (VariableName);
    SetVariableStatus = AllocatePool (sizeof (EDKII_SET_VARIABLE_STATUS) + NameSize + DataSize);
    if (SetVariableStatus != NULL) {
      CopyGuid (&SetVariableStatus->Guid, VendorGuid);
      SetVariableStatus->NameSize   = NameSize;
      SetVariableStatus->DataSize   = DataSize;
      SetVariableStatus->SetStatus  = Status;
      SetVariableStatus->Attributes = Attributes;
      CopyMem (SetVariableStatus + 1,                          VariableName, NameSize);
      CopyMem (((UINT8 *) (SetVariableStatus + 1)) + NameSize, Data,         DataSize);

      REPORT_STATUS_CODE_EX (
        EFI_ERROR_CODE,
        PcdGet32 (PcdErrorCodeSetVariable),
        0,
        NULL,
        &gEdkiiStatusCodeDataTypeVariableGuid,
        SetVariableStatus,
        sizeof (EDKII_SET_VARIABLE_STATUS) + NameSize + DataSize
        );

      FreePool (SetVariableStatus);
    }
  }

  return Status;
}


/**
  Dynamically allocate and initialize an EDKII_OS_LOADER_DETAIL status code
  payload.

  @param[in] BootOption           Capture the OptionNumber, FilePath and
                                  Description fields of BootOption in the
                                  EDKII_OS_LOADER_DETAIL payload.

  @param[out] OsLoaderDetail      On successful return, set to the
                                  EDKII_OS_LOADER_DETAIL object that has been
                                  allocated and initialized. On failure, not
                                  modified.

  @param[out] OsLoaderDetailSize  On successful return, set to the size of the
                                  EDKII_OS_LOADER_DETAIL object that has been
                                  allocated and initialized. On failure, not
                                  modified.

  @retval EFI_UNSUPPORTED        EFI_DEBUG_CODE reporting is disabled in the
                                 platform.

  @retval EFI_INVALID_PARAMETER  BootOption->OptionNumber is not in
                                 0x0000..0xFFFF, inclusive.

  @retval EFI_BAD_BUFFER_SIZE    BootOption->FilePath and/or
                                 BootOption->Description would exceed the
                                 UINT16 size limits presented by
                                 EDKII_OS_LOADER_DETAIL or
                                 EFI_STATUS_CODE_DATA.

  @retval EFI_OUT_OF_RESOURCES   Memory allocation failed.

  @retval EFI_SUCCESS            The EDKII_OS_LOADER_DETAIL object has been
                                 allocated and initialized. The caller is
                                 responsible for freeing the object with
                                 FreePool().
**/
EFI_STATUS
BmCreateOsLoaderDetail (
  IN  CONST EFI_BOOT_MANAGER_LOAD_OPTION *BootOption,
  OUT EDKII_OS_LOADER_DETAIL             **OsLoaderDetail,
  OUT UINTN                              *OsLoaderDetailSize
  )
{
  UINTN                  DescriptionSize;
  UINTN                  DevicePathSize;
  UINTN                  PayloadSize;
  EDKII_OS_LOADER_DETAIL *Payload;
  UINT8                  *VariableSizeData;

  if (!ReportDebugCodeEnabled ()) {
    return EFI_UNSUPPORTED;
  }

  if (BootOption->OptionNumber >= LoadOptionNumberMax) {
    return EFI_INVALID_PARAMETER;
  }

  DescriptionSize = (BootOption->Description == NULL) ?
                    0 :
                    StrSize (BootOption->Description);
  DevicePathSize = GetDevicePathSize (BootOption->FilePath);
  PayloadSize = sizeof *Payload + DescriptionSize + DevicePathSize;

  if (DescriptionSize > MAX_UINT16 ||
      DevicePathSize > MAX_UINT16 ||
      PayloadSize > MAX_UINT16) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Payload = AllocateZeroPool (PayloadSize);
  if (Payload == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  VariableSizeData = (UINT8 *)(Payload + 1);

  //
  // Populate the variable size fields at the end of the payload.
  //
  CopyMem (VariableSizeData, BootOption->Description, DescriptionSize);
  VariableSizeData += DescriptionSize;

  CopyMem (VariableSizeData, BootOption->FilePath, DevicePathSize);
  VariableSizeData += DevicePathSize;

  ASSERT (VariableSizeData - (UINT8 *)Payload == PayloadSize);

  //
  // Populate the fixed fields in the payload. Any members not listed below
  // remain zero-filled at this point.
  //
  Payload->BootOptionNumber = (UINT16)BootOption->OptionNumber;
  Payload->DescriptionSize  = (UINT16)DescriptionSize;
  Payload->DevicePathSize   = (UINT16)DevicePathSize;

  *OsLoaderDetail = Payload;
  *OsLoaderDetailSize = PayloadSize;
  return EFI_SUCCESS;
}


/**
  Report an EFI_DEBUG_CODE status code with EDKII_OS_LOADER_DETAIL as payload
  (i.e., extended data).

  @param[in,out] OsLoaderDetail  The EDKII_OS_LOADER_DETAIL payload previously
                                 created with BmCreateOsLoaderDetail(), and
                                 modified zero or more times by
                                 BmReportOsLoaderDetail(). If OsLoaderDetail is
                                 NULL, the function does nothing. Otherwise,
                                 the Type and Status fields are overwritten in
                                 OsLoaderDetail, and a status code is reported.

  @param[in] OsLoaderDetailSize  The size returned by BmCreateOsLoaderDetail().
                                 If OsLoaderDetail is NULL, OsLoaderDetailSize
                                 may be zero.

  @param[in] DetailType          OsLoaderDetail->Type is set to DetailType
                                 before reporting the status code. The caller
                                 is responsible for passing an
                                 EDKII_OS_LOADER_DETAIL_TYPE_* value.

  @param[in] DetailStatus        OsLoaderDetail->Status is set to DetailStatus
                                 before reporting the status code.

  @retval EFI_UNSUPPORTED  EFI_DEBUG_CODE reporting is disabled in the
                           platform.

  @retval EFI_ABORTED      OsLoaderDetail is NULL.

  @return                  Values propagated from REPORT_STATUS_CODE_EX().
**/
EFI_STATUS
BmReportOsLoaderDetail (
  IN OUT EDKII_OS_LOADER_DETAIL *OsLoaderDetail     OPTIONAL,
  IN     UINTN                  OsLoaderDetailSize,
  IN     UINT32                 DetailType,
  IN     EFI_STATUS             DetailStatus
  )
{
  EFI_STATUS Status;

  if (!ReportDebugCodeEnabled ()) {
    return EFI_UNSUPPORTED;
  }

  if (OsLoaderDetail == NULL) {
    return EFI_ABORTED;
  }

  OsLoaderDetail->Type = DetailType;
  OsLoaderDetail->Status = DetailStatus;

  Status = REPORT_STATUS_CODE_EX (
             EFI_DEBUG_CODE,                              // Type
             PcdGet32 (PcdDebugCodeOsLoaderDetail),       // Value
             0,                                           // Instance
             &gEfiCallerIdGuid,                           // CallerId
             &gEdkiiStatusCodeDataTypeOsLoaderDetailGuid, // ExtendedDataGuid
             OsLoaderDetail,                              // ExtendedData
             OsLoaderDetailSize                           // ExtendedDataSize
             );
  return Status;
}


/**
  Print the device path info.

  @param DevicePath           The device path need to print.
**/
VOID
BmPrintDp (
  EFI_DEVICE_PATH_PROTOCOL            *DevicePath
  )
{
  CHAR16                              *Str;

  Str = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
  DEBUG ((EFI_D_INFO, "%s", Str));
  if (Str != NULL) {
    FreePool (Str);
  }
}

/**
  Convert a single character to number.
  It assumes the input Char is in the scope of L'0' ~ L'9' and L'A' ~ L'F'

  @param    Char   The input char which need to convert to int.

  @return  The converted 8-bit number or (UINTN) -1 if conversion failed.
**/
UINTN
BmCharToUint (
  IN CHAR16                           Char
  )
{
  if ((Char >= L'0') && (Char <= L'9')) {
    return (Char - L'0');
  }

  if ((Char >= L'A') && (Char <= L'F')) {
    return (Char - L'A' + 0xA);
  }

  return (UINTN) -1;
}

/**
  Dispatch the deferred images that are returned from all DeferredImageLoad instances.

  @retval EFI_SUCCESS       At least one deferred image is loaded successfully and started.
  @retval EFI_NOT_FOUND     There is no deferred image.
  @retval EFI_ACCESS_DENIED There are deferred images but all of them are failed to load.
**/
EFI_STATUS
EFIAPI
EfiBootManagerDispatchDeferredImages (
  VOID
  )
{
  EFI_STATUS                         Status;
  EFI_DEFERRED_IMAGE_LOAD_PROTOCOL   *DeferredImage;
  UINTN                              HandleCount;
  EFI_HANDLE                         *Handles;
  UINTN                              Index;
  UINTN                              ImageIndex;
  EFI_DEVICE_PATH_PROTOCOL           *ImageDevicePath;
  VOID                               *Image;
  UINTN                              ImageSize;
  BOOLEAN                            BootOption;
  EFI_HANDLE                         ImageHandle;
  UINTN                              ExitDataSize;
  CHAR16                             *ExitData;
  UINTN                              ImageCount;
  UINTN                              LoadCount;

  //
  // Find all the deferred image load protocols.
  //
  HandleCount = 0;
  Handles = NULL;
  Status = gBS->LocateHandleBuffer (
    ByProtocol,
    &gEfiDeferredImageLoadProtocolGuid,
    NULL,
    &HandleCount,
    &Handles
  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  ImageCount = 0;
  LoadCount  = 0;
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiDeferredImageLoadProtocolGuid, (VOID **) &DeferredImage);
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (ImageIndex = 0; ;ImageIndex++) {
      //
      // Load all the deferred images in this protocol instance.
      //
      Status = DeferredImage->GetImageInfo (
                                DeferredImage,
                                ImageIndex,
                                &ImageDevicePath,
                                (VOID **) &Image,
                                &ImageSize,
                                &BootOption
                                );
      if (EFI_ERROR (Status)) {
        break;
      }
      ImageCount++;
      //
      // Load and start the image.
      //
      Status = gBS->LoadImage (
        BootOption,
        gImageHandle,
        ImageDevicePath,
        NULL,
        0,
        &ImageHandle
      );
      if (!EFI_ERROR (Status)) {
        LoadCount++;
        //
        // Before calling the image, enable the Watchdog Timer for
        // a 5 Minute period
        //
        gBS->SetWatchdogTimer (5 * 60, 0x0000, 0x00, NULL);
        Status = gBS->StartImage (ImageHandle, &ExitDataSize, &ExitData);
        if (ExitData != NULL) {
          FreePool (ExitData);
        }

        //
        // Clear the Watchdog Timer after the image returns.
        //
        gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);
      }
    }
  }
  if (Handles != NULL) {
    FreePool (Handles);
  }

  if (ImageCount == 0) {
    return EFI_NOT_FOUND;
  } else {
    if (LoadCount == 0) {
      return EFI_ACCESS_DENIED;
    } else {
      return EFI_SUCCESS;
    }
  }
}
