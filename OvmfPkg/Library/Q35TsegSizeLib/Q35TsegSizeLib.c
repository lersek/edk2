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

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PciLib.h>
#include <Library/Q35TsegSizeLib.h>
#include <OvmfPlatforms.h>

STATIC BOOLEAN mPreferencesInitialized;
STATIC UINT8   mPreferredEsmramcTsegSzMask;

/**
  Fetch the preferences into static variables that are going to be used by the
  public functions of this library instance.

  The Q35 board requirement documented on those interfaces is commonly enforced
  here.
**/
STATIC
VOID
Q35TsegSizeGetPreferences (
  VOID
  )
{
  UINT16 HostBridgeDevId;

  if (mPreferencesInitialized) {
    return;
  }

  //
  // This function should only be reached if SMRAM support is required.
  //
  ASSERT (FeaturePcdGet (PcdSmmSmramRequire));

  HostBridgeDevId = PciRead16 (OVMF_HOSTBRIDGE_DID);
  if (HostBridgeDevId != INTEL_Q35_MCH_DEVICE_ID) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %a: no TSEG (SMRAM) on host bridge DID=0x%04x; "
      "only DID=0x%04x (Q35) is supported\n",
      gEfiCallerBaseName,
      __FUNCTION__,
      HostBridgeDevId,
      INTEL_Q35_MCH_DEVICE_ID
      ));
    ASSERT (FALSE);
    CpuDeadLoop ();
  }

  mPreferencesInitialized = TRUE;

  switch (FixedPcdGet8 (PcdQ35TsegMbytes)) {
  case 1:
    mPreferredEsmramcTsegSzMask = MCH_ESMRAMC_TSEG_1MB;
    break;
  case 2:
    mPreferredEsmramcTsegSzMask = MCH_ESMRAMC_TSEG_2MB;
    break;
  case 8:
    mPreferredEsmramcTsegSzMask = MCH_ESMRAMC_TSEG_8MB;
    break;
  default:
    ASSERT (FALSE);
  }
}


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
  )
{
  //
  // Query the ESMRAMC.TSEG_SZ preference and convert it to megabytes.
  //
  return Q35TsegSizeConvertEsmramcValToMbytes (
           Q35TsegSizeGetPreferredEsmramcTsegSzMask ()
           );
}


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
  )
{
  Q35TsegSizeGetPreferences ();
  return mPreferredEsmramcTsegSzMask;
}


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
  )
{
  UINT8  TsegSizeBits;
  UINT16 Mbytes;

  Q35TsegSizeGetPreferences ();

  TsegSizeBits = EsmramcVal & MCH_ESMRAMC_TSEG_MASK;
  switch (TsegSizeBits) {
  case MCH_ESMRAMC_TSEG_1MB:
    Mbytes = 1;
    break;
  case MCH_ESMRAMC_TSEG_2MB:
    Mbytes = 2;
    break;
  case MCH_ESMRAMC_TSEG_8MB:
    Mbytes = 8;
    break;
  default:
    DEBUG ((
      DEBUG_ERROR,
      "%a: %a: unknown TsegSizeBits=0x%02x\n",
      gEfiCallerBaseName,
      __FUNCTION__,
      TsegSizeBits
      ));
    ASSERT (FALSE);
    CpuDeadLoop ();

    //
    // Keep compilers happy.
    //
    Mbytes = 0;
  }

  return Mbytes;
}
