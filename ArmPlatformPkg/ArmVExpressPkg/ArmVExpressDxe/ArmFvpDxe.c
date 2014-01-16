/** @file

  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2014, Red Hat, Inc.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/VirtioMmioDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH                  Vendor;
  EFI_PHYSICAL_ADDRESS                TransportBase;
  EFI_DEVICE_PATH_PROTOCOL            End;
} ARM_FVP_VIRTIO_TRANSPORT_PATH;
#pragma pack()

STATIC CONST ARM_FVP_VIRTIO_TRANSPORT_PATH mTransportPath =
{
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8) (OFFSET_OF (ARM_FVP_VIRTIO_TRANSPORT_PATH, End)     ),
        (UINT8) (OFFSET_OF (ARM_FVP_VIRTIO_TRANSPORT_PATH, End) >> 8)
      }
    },
    EFI_CALLER_ID_GUID,
  },
  0,
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      sizeof (EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};


typedef struct {
  EFI_HANDLE                    Handle;
  ARM_FVP_VIRTIO_TRANSPORT_PATH Path;
} ARM_FVP_VIRTIO_TRANSPORT;

STATIC ARM_FVP_VIRTIO_TRANSPORT *mTransports;
STATIC UINT32                   mCount;


EFI_STATUS
EFIAPI
ArmFvpInitialise (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS              Status;
  UINT32                  Idx;
  EFI_PHYSICAL_ADDRESS    Base;

  mTransports = AllocateZeroPool (sizeof *mTransports *
                                  PcdGet32 (PcdVirtioTransportCount));
  if (mTransports == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EFI_SUCCESS;
  Idx = 0;
  Base = PcdGet64 (PcdVirtioTransportFirst);

  while (!EFI_ERROR (Status) && Idx < PcdGet32 (PcdVirtioTransportCount)) {
    //
    // Prepare device path for the transport.
    //
    CopyMem (&mTransports[Idx].Path, &mTransportPath,
      sizeof (ARM_FVP_VIRTIO_TRANSPORT_PATH));
    mTransports[Idx].Path.TransportBase = Base;

    //
    // Allocate a fresh handle and install the device path on it.
    //
    Status = gBS->InstallProtocolInterface (&mTransports[Idx].Handle,
                    &gEfiDevicePathProtocolGuid, EFI_NATIVE_INTERFACE,
                    &mTransports[Idx].Path);
    if (!EFI_ERROR (Status)) {
      //
      // Install the VirtIo protocol with the MMIO backend on the handle.
      //
      Status = VirtioMmioInstallDevice (Base, mTransports[Idx].Handle);
    }

    ++Idx;
    if (PcdGetBool (PcdVirtioTransportDownward)) {
      Base -= PcdGet32 (PcdVirtioTransportSize);
    } else {
      Base += PcdGet32 (PcdVirtioTransportSize);
    }
  }

  if (EFI_ERROR (Status)) {
    //
    // Roll back the transports. For the most recently accessed transport,
    // Handle may be NULL (if device path installation failed), or the VirtIo
    // protocol may be missing (if only the device path installation succeded).
    // The code below copes with those cases. TransportBase is always set.
    //
    ASSERT (Idx > 0);
    DEBUG ((EFI_D_ERROR, "%a: failed to set up device at 0x%Lx: %r\n",
      __FUNCTION__,  mTransports[Idx - 1].Path.TransportBase, Status));

    do {
      --Idx;
      VirtioMmioUninstallDevice (mTransports[Idx].Handle);
      gBS->UninstallProtocolInterface (mTransports[Idx].Handle,
             &gEfiDevicePathProtocolGuid, &mTransports[Idx].Path);
    } while (PcdGetBool (PcdVirtioTransportAllRequired) && Idx > 0);

    if (Idx > 0) {
      Status = EFI_SUCCESS;
    } else {
      FreePool (mTransports);
    }
  }

  DEBUG ((EFI_D_INFO, "%a: keeping %Ld devices (%r)\n", __FUNCTION__,
    (INT64) Idx, Status));
  mCount = Idx;
  return Status;
}
