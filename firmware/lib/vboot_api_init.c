/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - entry points for init, firmware selection
 */

#include "gbb_header.h"
#include "load_firmware_fw.h"
#include "rollback_index.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "vboot_nvstorage.h"


VbError_t VbInit(VbCommonParams* cparams, VbInitParams* iparams) {
  VbSharedDataHeader* shared = (VbSharedDataHeader*)cparams->shared_data_blob;
  GoogleBinaryBlockHeader* gbb = (GoogleBinaryBlockHeader*)cparams->gbb_data;
  VbNvContext vnc;
  VbError_t retval = VBERROR_SUCCESS;
  uint32_t recovery = VBNV_RECOVERY_NOT_REQUESTED;
  int is_s3_resume = 0;
  uint32_t s3_debug_boot = 0;
  uint32_t require_official_os = 0;
  uint32_t tpm_version = 0;
  uint32_t tpm_status = 0;
  int hw_dev_sw = 1;
  int is_dev = 0;

  VBDEBUG(("VbInit() input flags 0x%x\n", iparams->flags));

  /* Initialize output flags */
  iparams->out_flags = 0;

  /* Set up NV storage */
  VbExNvStorageRead(vnc.raw);
  VbNvSetup(&vnc);

  /* Initialize shared data structure */
  if (0 != VbSharedDataInit(shared, cparams->shared_data_size)) {
    VBDEBUG(("Shared data init error\n"));
    return VBERROR_INIT_SHARED_DATA;
  }

  shared->timer_vb_init_enter = VbExGetTimer();

  /* Copy boot switch flags */
  shared->flags = 0;
  if (!(iparams->flags & VB_INIT_FLAG_VIRTUAL_DEV_SWITCH) &&
      (iparams->flags & VB_INIT_FLAG_DEV_SWITCH_ON))
    is_dev = 1;
  if (iparams->flags & VB_INIT_FLAG_REC_BUTTON_PRESSED)
    shared->flags |= VBSD_BOOT_REC_SWITCH_ON;
  if (iparams->flags & VB_INIT_FLAG_WP_ENABLED)
    shared->flags |= VBSD_BOOT_FIRMWARE_WP_ENABLED;
  if (iparams->flags & VB_INIT_FLAG_S3_RESUME)
    shared->flags |= VBSD_BOOT_S3_RESUME;
  if (iparams->flags & VB_INIT_FLAG_RO_NORMAL_SUPPORT)
    shared->flags |= VBSD_BOOT_RO_NORMAL_SUPPORT;

  is_s3_resume = (iparams->flags & VB_INIT_FLAG_S3_RESUME ? 1 : 0);

  /* Check if the OS is requesting a debug S3 reset */
  VbNvGet(&vnc, VBNV_DEBUG_RESET_MODE, &s3_debug_boot);
  if (s3_debug_boot) {
    if (is_s3_resume) {
      VBDEBUG(("VbInit() requesting S3 debug boot\n"));
      iparams->out_flags |= VB_INIT_OUT_S3_DEBUG_BOOT;
      is_s3_resume = 0;               /* Proceed as if this is a normal boot */
    }

    /* Clear the request even if this is a normal boot, since we don't
     * want the NEXT S3 resume to be a debug reset unless the OS
     * asserts the request again. */
    VbNvSet(&vnc, VBNV_DEBUG_RESET_MODE, 0);
  }

  /* If this isn't a S3 resume, read the current recovery request, then clear
   * it so we don't get stuck in recovery mode. */
  if (!is_s3_resume) {
    VbNvGet(&vnc, VBNV_RECOVERY_REQUEST, &recovery);
    if (VBNV_RECOVERY_NOT_REQUESTED != recovery)
      VbNvSet(&vnc, VBNV_RECOVERY_REQUEST, VBNV_RECOVERY_NOT_REQUESTED);
  }

  /* If the previous boot failed in the firmware somewhere outside of verified
   * boot, and recovery is not requested for our own reasons, request recovery
   * mode.  This gives the calling firmware a way to request recovery if it
   * finds something terribly wrong. */
  if (VBNV_RECOVERY_NOT_REQUESTED == recovery &&
      iparams->flags & VB_INIT_FLAG_PREVIOUS_BOOT_FAIL) {
    recovery = VBNV_RECOVERY_RO_FIRMWARE;
  }

  /* If recovery button is pressed, override recovery reason.  Note that we
   * do this in the S3 resume path also. */
  if (iparams->flags & VB_INIT_FLAG_REC_BUTTON_PRESSED)
    recovery = VBNV_RECOVERY_RO_MANUAL;

  /* Copy current recovery reason to shared data. If we fail later on, it
   * won't matter, since we'll just reboot. */
  shared->recovery_reason = (uint8_t)recovery;

  /* If this is a S3 resume, resume the TPM. */
  /* FIXME: I think U-Boot won't ever ask us to do this. Can we remove it? */
  if (is_s3_resume) {
    if (TPM_SUCCESS != RollbackS3Resume()) {
      /* If we can't resume, just do a full reboot.  No need to go to recovery
       * mode here, since if the TPM is really broken we'll catch it on the
       * next boot. */
      retval = VBERROR_TPM_S3_RESUME;
    }
  } else {

    /* We need to know about dev mode now */
    if (iparams->flags & VB_INIT_FLAG_VIRTUAL_DEV_SWITCH)
      hw_dev_sw = 0;
    else if (iparams->flags & VB_INIT_FLAG_DEV_SWITCH_ON)
      is_dev = 1;

    VBPERFSTART("VB_TPMI");
    /* Initialize the TPM. *is_dev is both an input and output. The only time
     * it should be 1 on input is when we have a hardware dev-switch and it's
     * enabled. The only time it's promoted from 0 to 1 on return is when we
     * have a virtual dev-switch and the TPM has a valid rollback space with
     * the virtual switch already enabled. If the TPM space is initialized by
     * this call, its virtual dev-switch will be disabled by default. */
    tpm_status = RollbackFirmwareSetup(recovery, hw_dev_sw,
                                       &is_dev, &tpm_version);
    VBPERFEND("VB_TPMI");
    if (0 != tpm_status) {
      VBDEBUG(("Unable to setup TPM and read firmware version.\n"));

      if (TPM_E_MUST_REBOOT == tpm_status) {
        /* TPM wants to reboot into the same mode we're in now */
        VBDEBUG(("TPM requires a reboot.\n"));
        if (!recovery) {
          /* Not recovery mode.  Just reboot (not into recovery). */
          retval = VBERROR_TPM_REBOOT_REQUIRED;
          goto VbInit_exit;
        } else if (VBNV_RECOVERY_RO_TPM_REBOOT != shared->recovery_reason) {
          /* In recovery mode now, and we haven't requested a TPM reboot yet,
           * so request one. */
          VbNvSet(&vnc, VBNV_RECOVERY_REQUEST, VBNV_RECOVERY_RO_TPM_REBOOT);
          retval = VBERROR_TPM_REBOOT_REQUIRED;
          goto VbInit_exit;
        }
      }

      if (!recovery) {
        VbNvSet(&vnc, VBNV_RECOVERY_REQUEST, VBNV_RECOVERY_RO_TPM_ERROR);
        retval = VBERROR_TPM_FIRMWARE_SETUP;
        goto VbInit_exit;
      }
    }
    shared->fw_version_tpm_start = tpm_version;
    shared->fw_version_tpm = tpm_version;
    if (is_dev)
      shared->flags |= VBSD_BOOT_DEV_SWITCH_ON;
  }

  /* FIXME: May need a GBB flag for initial value of virtual dev-switch */

  /* Allow BIOS to load arbitrary option ROMs? */
  if (gbb->flags & GBB_FLAG_LOAD_OPTION_ROMS)
    iparams->out_flags |= VB_INIT_OUT_ENABLE_OPROM;

  /* The factory may need to boot custom OSes whenever the dev-switch is on */
  if (is_dev && (gbb->flags & GBB_FLAG_ENABLE_ALTERNATE_OS))
    iparams->out_flags |= VB_INIT_OUT_ENABLE_ALTERNATE_OS;

  /* Set output flags */
  if (VBNV_RECOVERY_NOT_REQUESTED != recovery) {
    /* Requesting recovery mode */
    iparams->out_flags |= (VB_INIT_OUT_ENABLE_RECOVERY |
                           VB_INIT_OUT_CLEAR_RAM |
                           VB_INIT_OUT_ENABLE_DISPLAY |
                           VB_INIT_OUT_ENABLE_USB_STORAGE);
  }
  else if (is_dev) {
    /* Developer switch is on, so need to support dev mode */
    iparams->out_flags |= (VB_INIT_OUT_CLEAR_RAM |
                           VB_INIT_OUT_ENABLE_DISPLAY |
                           VB_INIT_OUT_ENABLE_USB_STORAGE);
    /* ... which may or may not include custom OSes */
    VbNvGet(&vnc, VBNV_DEV_BOOT_SIGNED_ONLY, &require_official_os);
    if (!require_official_os)
      iparams->out_flags |= VB_INIT_OUT_ENABLE_ALTERNATE_OS;
  } else {
    /* Normal mode, so disable dev_boot_* flags.  This ensures they will be
     * initially disabled if the user later transitions back into developer
     * mode. */
    VbNvSet(&vnc, VBNV_DEV_BOOT_USB, 0);
    VbNvSet(&vnc, VBNV_DEV_BOOT_SIGNED_ONLY, 0);
  }

VbInit_exit:

  /* Tear down NV storage */
  VbNvTeardown(&vnc);
  if (vnc.raw_changed)
    VbExNvStorageWrite(vnc.raw);

  VBDEBUG(("VbInit() output flags 0x%x\n", iparams->out_flags));

  shared->timer_vb_init_exit = VbExGetTimer();

  return retval;
}
