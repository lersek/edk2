/** @file
  Define the GUID, macros, and the data structure for passing details of OS
  Loading from the UEFI Boot Manager to the Platform, as debug codes.

  Copyright (C) 2017, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License that accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _STATUS_CODE_DATA_TYPE_OS_LOADER_DETAIL_H_
#define _STATUS_CODE_DATA_TYPE_OS_LOADER_DETAIL_H_

#include <Protocol/DevicePath.h>
#include <Uefi/UefiBaseType.h>

//
// The GUID for EFI_STATUS_CODE_DATA.Type, in order to identify the trailing
// payload as EDKII_OS_LOADER_DETAIL.
//
// The EFI_STATUS_CODE_VALUE under which to report such data is
// PcdGet32(PcdDebugCodeOsLoaderDetail).
//
#define EDKII_STATUS_CODE_DATA_TYPE_OS_LOADER_DETAIL_GUID \
  {                                                       \
    0xBE4904DC,                                           \
    0x7EC4,                                               \
    0x4167,                                               \
    { 0x90, 0x52, 0x38, 0x4D, 0x0B, 0x81, 0x30, 0x0B }    \
  }

//
// The UEFI Boot Manager is about to call gBS->LoadImage() on the boot option.
//
#define EDKII_OS_LOADER_DETAIL_TYPE_LOAD        0x00000000
//
// gBS->LoadImage() failed on the boot option.
//
#define EDKII_OS_LOADER_DETAIL_TYPE_LOAD_ERROR  0x00000001
//
// The UEFI Boot Manager is about to call gBS->StartImage() on the boot option.
//
#define EDKII_OS_LOADER_DETAIL_TYPE_START       0x00000002
//
// gBS->StartImage() failed on the boot option.
//
#define EDKII_OS_LOADER_DETAIL_TYPE_START_ERROR 0x00000003

//
// Structure for passing details about the above actions and results. Currently
// a common structure is used for all of them.
//
// The structure can be extended compatibly by adding fields at the end. The
// presence of such fields can be deduced from the containing
// EFI_STATUS_CODE_DATA.Size field. Incompatible extensions require a new GUID
// for the containing EFI_STATUS_CODE_DATA.Type field.
//
#pragma pack (1)
typedef struct {
  //
  // EDKII_OS_LOADER_DETAIL_TYPE_* values.
  //
  UINT32                      Type;
  //
  // The number of the Boot#### UEFI variable from which the OS is being
  // loaded.
  //
  UINT16                      BootOptionNumber;
  //
  // The size of Description in bytes, including the terminating L'\0'
  // character. If DescriptionSize is 0, then Description is absent. The type
  // of this field is UINT16 because all of EDKII_OS_LOADER_DETAIL has to fit
  // into EFI_STATUS_CODE_DATA.Size, which has type UINT16.
  //
  UINT16                      DescriptionSize;
  //
  // The size of DevicePath in bytes, including the terminating end node. If
  // DevicePathSize is 0, then DevicePath is absent. The type of this field is
  // UINT16 because all of EDKII_OS_LOADER_DETAIL has to fit into
  // EFI_STATUS_CODE_DATA.Size, which has type UINT16.
  //
  //
  UINT16                      DevicePathSize;
  //
  // Used only for EDKII_OS_LOADER_DETAIL_TYPE_*_ERROR, the following field
  // reports the failure code.
  //
  EFI_STATUS                  Status;
  //
  // The human-readable description of the boot option for which the OS is
  // being loaded. Populated from EFI_LOAD_OPTION.Description.
  //
  // CHAR16                   Description[];
  //
  // Describes the device and location of the OS image being loaded. Populated
  // from the first element of the packed EFI_LOAD_OPTION.FilePathList array.
  //
  // EFI_DEVICE_PATH_PROTOCOL DevicePath;
} EDKII_OS_LOADER_DETAIL;
#pragma pack ()

extern EFI_GUID gEdkiiStatusCodeDataTypeOsLoaderDetailGuid;

#endif // _STATUS_CODE_DATA_TYPE_OS_LOADER_DETAIL_H_
