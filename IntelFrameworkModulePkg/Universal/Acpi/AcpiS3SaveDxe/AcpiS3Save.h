/** @file
  This is an implementation of the ACPI S3 Save protocol.  This is defined in 
  S3 boot path specification 0.9.

Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions
of the BSD License which accompanies this distribution.  The
full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _ACPI_S3_SAVE_H_
#define _ACPI_S3_SAVE_H_

/**
  Prepares all information that is needed in the S3 resume boot path.
  
  Allocate the resources or prepare informations and save in ACPI variable set for S3 resume boot path  
  
  @retval EFI_SUCCESS           All information was saved successfully.
**/
EFI_STATUS
EFIAPI
S3Ready (
  VOID
  );
#endif
