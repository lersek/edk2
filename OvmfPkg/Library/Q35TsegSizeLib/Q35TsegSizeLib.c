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
STATIC UINT16  mExtendedTsegMbytes;

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

  //
  // Check if QEMU offers an extended TSEG.
  //
  // This can be seen from writing MCH_EXT_TSEG_MB_QUERY to the MCH_EXT_TSEG_MB
  // register, and reading back the register.
  //
  // On a QEMU machine type that does not offer an extended TSEG, the initial
  // write overwrites whatever value a malicious guest OS may have placed in
  // the (unimplemented) register, before entering S3 or rebooting.
  // Subsequently, the read returns MCH_EXT_TSEG_MB_QUERY unchanged.
  //
  // On a QEMU machine type that offers an extended TSEG, the initial write
  // triggers an update to the register. Subsequently, the value read back
  // (which is guaranteed to differ from MCH_EXT_TSEG_MB_QUERY) tells us the
  // number of megabytes.
  //
  PciWrite16 (DRAMC_REGISTER_Q35 (MCH_EXT_TSEG_MB), MCH_EXT_TSEG_MB_QUERY);
  mExtendedTsegMbytes = PciRead16 (DRAMC_REGISTER_Q35 (MCH_EXT_TSEG_MB));
  if (mExtendedTsegMbytes != MCH_EXT_TSEG_MB_QUERY) {
    DEBUG ((
      DEBUG_INFO,
      "%a: %a: QEMU offers an extended TSEG (%d MB)\n",
      gEfiCallerBaseName,
      __FUNCTION__,
      mExtendedTsegMbytes
      ));

    mPreferredEsmramcTsegSzMask = MCH_ESMRAMC_TSEG_EXT;
    return;
  }

  //
  // Fall back to the default TSEG size otherwise.
  //
  switch (FixedPcdGet8 (PcdQ35TsegDefaultMbytes)) {
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
  case MCH_ESMRAMC_TSEG_EXT:
    if (mExtendedTsegMbytes != MCH_EXT_TSEG_MB_QUERY) {
      Mbytes = mExtendedTsegMbytes;
      break;
    }
    //
    // Fall through otherwise -- QEMU didn't offer an extended TSEG, so this
    // should never happen.
    //
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
