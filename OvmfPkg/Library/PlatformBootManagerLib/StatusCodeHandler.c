/** @file
  Register a status code handler for printing EDKII_OS_LOADER_DETAIL reports to
  the console.

  This feature enables users that are not accustomed to analyzing the OVMF
  debug log to glean some information about UEFI boot option processing
  (loading and starting).

  Copyright (C) 2017, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Guid/StatusCodeDataTypeOsLoaderDetail.h>
#include <Protocol/ReportStatusCodeHandler.h>

#include "BdsPlatform.h"


/**
  Handle status codes reported through ReportStatusCodeLib /
  EFI_STATUS_CODE_PROTOCOL.ReportStatusCode(). Format matching status codes to
  the system console.

  The highest TPL at which this handler can be registered with
  EFI_RSC_HANDLER_PROTOCOL.Register() is TPL_NOTIFY. That's because
  AsciiPrint() uses EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL internally.

  The parameter list of this function precisely matches that of
  EFI_STATUS_CODE_PROTOCOL.ReportStatusCode().

  The return status of this function is ignored by the caller, but the function
  still returns sensible codes:

  @retval EFI_SUCCESS            The status code has been processed; either as
                                 a no-op, due to filtering, or by formatting it
                                 to the system console.

  @retval EFI_INVALID_PARAMETER  Unknown or malformed contents have been
                                 detected in EFI_STATUS_CODE_DATA, or in the
                                 EDKII_OS_LOADER_DETAIL payload within
                                 EFI_STATUS_CODE_DATA.
**/
STATIC
EFI_STATUS
EFIAPI
HandleStatusCode (
  IN EFI_STATUS_CODE_TYPE  CodeType,
  IN EFI_STATUS_CODE_VALUE Value,
  IN UINT32                Instance,
  IN EFI_GUID              *CallerId,
  IN EFI_STATUS_CODE_DATA  *Data
  )
{
  EDKII_OS_LOADER_DETAIL   *OsLoaderDetail;
  UINT8                    *VariableSizeData;
  CHAR16                   *Description;
  EFI_DEVICE_PATH_PROTOCOL *DevicePath;
  BOOLEAN                  DevPathStringIsDynamic;
  CHAR16                   *DevPathString;
  EFI_STATUS               Status;

  //
  // Ignore all status codes other than OsLoaderDetail.
  //
  if (Value != PcdGet32 (PcdDebugCodeOsLoaderDetail)) {
    return EFI_SUCCESS;
  }

  //
  // The status codes we are interested in are emitted by UefiBootManagerLib.
  // UefiBootManagerLib is built into several drivers and applications, e.g.
  // BdsDxe and UiApp. Process (i.e., print to the console) only those status
  // codes that come from BdsDxe; that is, from the driver module that this
  // PlatformBootManagerLib instance is also built into.
  //
  if (!CompareGuid (CallerId, &gEfiCallerIdGuid)) {
    return EFI_SUCCESS;
  }

  //
  // Sanity checks -- now that Value has been validated, we have expectations
  // to enforce.
  //
  if ((Data == NULL) ||
      (Data->HeaderSize < sizeof *Data) ||
      (Data->Size < sizeof *OsLoaderDetail) ||
      (!CompareGuid (&Data->Type,
          &gEdkiiStatusCodeDataTypeOsLoaderDetailGuid))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: unknown or malformed data for status code 0x%x\n",
      __FUNCTION__,
      PcdGet32 (PcdDebugCodeOsLoaderDetail)
      ));
    return EFI_INVALID_PARAMETER;
  }

  OsLoaderDetail = (EDKII_OS_LOADER_DETAIL *)(
                     (UINT8 *)Data + Data->HeaderSize
                     );

  //
  // More sanity checks. The additions on the RHS are carried out in UINTN and
  // cannot overflow.
  //
  if (Data->Size < (sizeof *OsLoaderDetail +
                    OsLoaderDetail->DescriptionSize +
                    OsLoaderDetail->DevicePathSize)) {
    DEBUG ((DEBUG_ERROR, "%a: malformed EDKII_OS_LOADER_DETAIL\n",
      __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Extract the known variable size fields from the payload.
  //
  VariableSizeData = (UINT8 *)(OsLoaderDetail + 1);

  Description = (CHAR16 *)VariableSizeData;
  VariableSizeData += OsLoaderDetail->DescriptionSize;

  DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)VariableSizeData;
  VariableSizeData += OsLoaderDetail->DevicePathSize;

  ASSERT (VariableSizeData - (UINT8 *)OsLoaderDetail <= Data->Size);

  //
  // Prepare the extracted variable size fields for printing.
  //
  if (OsLoaderDetail->DescriptionSize == 0) {
    Description = L"<no description available>";
  }

  DevPathStringIsDynamic = FALSE;
  if (OsLoaderDetail->DevicePathSize == 0) {
    DevPathString = L"<no device path available>";
  } else {
    DevPathString = ConvertDevicePathToText (
                      DevicePath,
                      FALSE,      // DisplayOnly
                      FALSE       // AllowShortcuts
                      );
    if (DevPathString == NULL) {
      DevPathString = L"<out of memory while formatting device path>";
    } else {
      DevPathStringIsDynamic = TRUE;
    }
  }

  //
  // Print the message to the console.
  //
  switch (OsLoaderDetail->Type) {
  case EDKII_OS_LOADER_DETAIL_TYPE_LOAD:
  case EDKII_OS_LOADER_DETAIL_TYPE_START:
    AsciiPrint (
      "%a: %a Boot%04x \"%s\" from %s\n",
      gEfiCallerBaseName,
      (OsLoaderDetail->Type == EDKII_OS_LOADER_DETAIL_TYPE_LOAD ?
       "loading" :
       "starting"),
      OsLoaderDetail->BootOptionNumber,
      Description,
      DevPathString
      );
    break;

  case EDKII_OS_LOADER_DETAIL_TYPE_LOAD_ERROR:
  case EDKII_OS_LOADER_DETAIL_TYPE_START_ERROR:
    AsciiPrint (
      "%a: failed to %a Boot%04x \"%s\" from %s: %r\n",
      gEfiCallerBaseName,
      (OsLoaderDetail->Type == EDKII_OS_LOADER_DETAIL_TYPE_LOAD_ERROR ?
       "load" :
       "start"),
      OsLoaderDetail->BootOptionNumber,
      Description,
      DevPathString,
      OsLoaderDetail->Status
      );
    break;

  default:
    DEBUG ((DEBUG_ERROR, "%a: unknown EDKII_OS_LOADER_DETAIL.Type 0x%x\n",
      __FUNCTION__, OsLoaderDetail->Type));
    Status = EFI_INVALID_PARAMETER;
    goto ReleaseDevPathString;
  }

  Status = EFI_SUCCESS;

ReleaseDevPathString:
  if (DevPathStringIsDynamic) {
    FreePool (DevPathString);
  }
  return Status;
}


/**
  Unregister HandleStatusCode() at ExitBootServices().

  (See EFI_RSC_HANDLER_PROTOCOL in Volume 3 of the Platform Init spec.)

  @param[in] Event    Event whose notification function is being invoked.

  @param[in] Context  Pointer to EFI_RSC_HANDLER_PROTOCOL, originally looked up
                      when HandleStatusCode() was registered.
**/
STATIC
VOID
EFIAPI
UnregisterAtExitBootServices (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_RSC_HANDLER_PROTOCOL *StatusCodeRouter;

  StatusCodeRouter = Context;
  StatusCodeRouter->Unregister (HandleStatusCode);
}


/**
  Register a status code handler for printing EDKII_OS_LOADER_DETAIL reports to
  the console.

  @retval EFI_SUCCESS  The status code handler has been successfully
                       registered.

  @return              Error codes propagated from boot services and from
                       EFI_RSC_HANDLER_PROTOCOL.
**/
EFI_STATUS
RegisterStatusCodeHandler (
  VOID
  )
{
  EFI_STATUS               Status;
  EFI_RSC_HANDLER_PROTOCOL *StatusCodeRouter;
  EFI_EVENT                ExitBootEvent;

  Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid,
                  NULL /* Registration */, (VOID **)&StatusCodeRouter);
  //
  // This protocol is provided by the ReportStatusCodeRouterRuntimeDxe driver
  // that we build into the firmware image. Given that PlatformBootManagerLib
  // is used as part of BdsDxe, and BDS Entry occurs after all DXE drivers have
  // been dispatched, the EFI_RSC_HANDLER_PROTOCOL is available at this point.
  //
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Register the handler callback.
  //
  Status = StatusCodeRouter->Register (HandleStatusCode, TPL_CALLBACK);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to register status code handler: %r\n",
      __FUNCTION__, Status));
    return Status;
  }

  //
  // Status code reporting and routing/handling extend into OS runtime. Since
  // we don't want our handler to survive the BDS phase, we have to unregister
  // the callback at ExitBootServices(). (See EFI_RSC_HANDLER_PROTOCOL in
  // Volume 3 of the Platform Init spec.)
  //
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES, // Type
                  TPL_CALLBACK,                  // NotifyTpl
                  UnregisterAtExitBootServices,  // NotifyFunction
                  StatusCodeRouter,              // NotifyContext
                  &ExitBootEvent                 // Event
                  );
  if (EFI_ERROR (Status)) {
    //
    // We have to unregister the callback right now, and fail the function.
    //
    DEBUG ((DEBUG_ERROR, "%a: failed to create ExitBootServices() event: %r\n",
      __FUNCTION__, Status));
    StatusCodeRouter->Unregister (HandleStatusCode);
    return Status;
  }

  return EFI_SUCCESS;
}
