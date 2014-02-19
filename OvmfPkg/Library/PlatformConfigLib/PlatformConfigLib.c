/** @file

  Library for serializing (persistently storing) and deserializing OVMF's
  platform configuration.

  Copyright (C) 2014, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/PlatformConfigLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/OvmfPlatformConfig.h>


//
// Name of the UEFI variable that we use for persistent storage.
//
STATIC CHAR16 mVariableName[] = L"PlatformConfig";


/**
  Serialize and persistently save platform configuration.

  @param[in] PlatformConfig  The platform configuration to serialize and save.

  @return  Status codes returned by gRT->SetVariable().
**/
EFI_STATUS
EFIAPI
PlatformConfigSave (
  IN PLATFORM_CONFIG *PlatformConfig
  )
{
  EFI_STATUS Status;

  //
  // We could implement any kind of translation here, as part of serialization.
  // For example, we could expose the platform configuration in separate
  // variables with human-readable contents, allowing other tools to access
  // them more easily. For now, just save a binary dump.
  //
  Status = gRT->SetVariable (mVariableName, &gOvmfPlatformConfigGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                    EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof *PlatformConfig, PlatformConfig);
  return Status;
}


/**
  Load and deserialize platform configuration.

  When the function fails, output parameters are indeterminate.

  @param[out] PlatformConfig    The platform configuration to receive the
                                loaded data.

  @param[out] OptionalElements  This bitmap describes the presence of optional
                                configuration elements.

  @retval  EFI_SUCCESS         Loading & deserialization successful.
  @retval  EFI_PROTOCOL_ERROR  Invalid contents in persistent store.
  @return                      Error codes returned by gRT->GetVariable().
**/
EFI_STATUS
EFIAPI
PlatformConfigLoad (
  OUT PLATFORM_CONFIG *PlatformConfig,
  OUT UINT64          *OptionalElements
  )
{
  UINTN      DataSize;
  EFI_STATUS Status;

  //
  // Any translation done in PlatformConfigSave() would have to be mirrored
  // here. For now, just load the binary dump.
  //
  // Versioning of the binary wire format can be implemented later on, based on
  // size (only incremental changes, ie. new fields), and on GUID.
  // (Incompatible changes require a GUID change.)
  //
  DataSize = sizeof *PlatformConfig;
  Status = gRT->GetVariable (mVariableName, &gOvmfPlatformConfigGuid,
                  NULL /* Attributes */, &DataSize, PlatformConfig);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // No optional configuration elements are supported for the time being.
  //
  if (DataSize < sizeof *PlatformConfig) {
    return EFI_PROTOCOL_ERROR;
  }
  *OptionalElements = 0;
  return EFI_SUCCESS;
}
