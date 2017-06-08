/** @file
  Utility library to query TSEG size-related quantities on Q35.

  Copyright (C) 2017, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __Q35_TSEG_SIZE_LIB__
#define __Q35_TSEG_SIZE_LIB__

/**
  Query the preferred size of TSEG, in megabytes.

  The caller is responsible for calling this function only on the Q35 board. If
  the function is called on another board, the function logs an informative
  error message and does not return.

  @return  The preferred size of TSEG, expressed in megabytes.
**/
UINT16
EFIAPI
Q35TsegSizeGetPreferredMbytes (
  VOID
  );

/**
  Query the ESMRAMC.TSEG_SZ bit-field value that corresponds to the preferred
  TSEG size.

  The caller is responsible for calling this function only on the Q35 board. If
  the function is called on another board, the function logs an informative
  error message and does not return.

  @return  The ESMRAMC.TSEG_SZ bit-field value that corresponds to the
           preferred TSEG size. The return value is a subset of
           MCH_ESMRAMC_TSEG_MASK, defined in <IndustryStandard/Q35MchIch9.h>.
**/
UINT8
EFIAPI
Q35TsegSizeGetPreferredEsmramcTsegSzMask (
  VOID
  );

/**
  Extract the TSEG_SZ bit-field from the passed in ESMRAMC register value, and
  return the number of megabytes that it represents.

  The caller is responsible for calling this function only on the Q35 board. If
  the function is called on another board, the function logs an informative
  error message and does not return.

  @param[in] EsmramcVal  The ESMRAMC register value to extract the TSEG_SZ
                         bit-field from, using MCH_ESMRAMC_TSEG_MASK from
                         <IndustryStandard/Q35MchIch9.h>. If the extracted
                         bit-field cannot be mapped to a MB count, the function
                         logs an error message and does not return.

  @return  The number of megabytes that the extracted TSEG_SZ bit-field
           represents.
**/
UINT16
EFIAPI
Q35TsegSizeConvertEsmramcValToMbytes (
  IN UINT8 EsmramcVal
  );

#endif
