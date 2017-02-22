/** @file
  Base Null library instance of the QemuFwCfgS3Lib class.

  This library instance returns constant FALSE from QemuFwCfgS3Enabled(), and
  all other library functions trigger assertion failures. It is suitable for
  QEMU targets and machine types that never enable S3.

  Copyright (C) 2017, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/DebugLib.h>
#include <Library/QemuFwCfgS3Lib.h>

/**
  Determine if S3 support is explicitly enabled.

  @retval  TRUE   If S3 support is explicitly enabled. Other functions in this
                  library may be called (subject to their individual
                  restrictions).

           FALSE  Otherwise. This includes unavailability of the firmware
                  configuration interface. No other function in this library
                  must be called.
**/
BOOLEAN
EFIAPI
QemuFwCfgS3Enabled (
  VOID
  )
{
  return FALSE;
}


/**
  Install the client module's FW_CFG_BOOT_SCRIPT_APPEND_FUNCTION callback for
  when the production of ACPI S3 Boot Script opcodes becomes possible.

  Take ownership of the client-provided Context, and pass it to the callback
  function, when the latter is invoked.

  Allocate scratch space for those ACPI S3 Boot Script opcodes to work upon
  that the client will produce in the callback function.

  @param[in] Append             FW_CFG_BOOT_SCRIPT_APPEND_FUNCTION to invoke
                                when the production of ACPI S3 Boot Script
                                opcodes becomes possible. Append() may be
                                called immediately from
                                QemuFwCfgS3TransferOwnership().

  @param[in,out] Context        Client-provided data structure for the Append()
                                callback function to consume.

                                If Context points to dynamically allocated
                                memory, then Append() must release it.

                                If Context points to dynamically allocated
                                memory, and QemuFwCfgS3TransferOwnership()
                                returns successfully, then the caller of
                                QemuFwCfgS3TransferOwnership() must neither
                                dereference nor even evaluate Context any
                                longer, as ownership of the referenced area has
                                been transferred to Append().

  @param[in] ScratchBufferSize  The size of the scratch buffer that will hold,
                                in reserved memory, all client data read,
                                written, and checked by the ACPI S3 Boot Script
                                opcodes produced by Append().

  @retval RETURN_UNSUPPORTED       The library instance does not support this
                                   function.

  @retval RETURN_NOT_FOUND         The fw_cfg DMA interface to QEMU is
                                   unavailable.

  @retval RETURN_BAD_BUFFER_SIZE   ScratchBufferSize is too large.

  @retval RETURN_OUT_OF_RESOURCES  Memory allocation failed.

  @retval RETURN_SUCCESS           Append() has been installed, and the
                                   ownership of Context has been transferred.
                                   Reserved memory has been allocated for the
                                   scratch buffer.

                                   A successful invocation of
                                   QemuFwCfgS3TransferOwnership() cannot be
                                   rolled back.

  @return                          Error codes from underlying functions.
**/
EFIAPI
RETURN_STATUS
QemuFwCfgS3TransferOwnership (
  IN     FW_CFG_BOOT_SCRIPT_APPEND_FUNCTION *Append,
  IN OUT VOID                               *Context,          OPTIONAL
  IN     UINTN                              ScratchBufferSize
  )
{
  ASSERT (FALSE);
  return RETURN_UNSUPPORTED;
}
