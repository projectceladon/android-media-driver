/*
* Copyright (c) 2019, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
//!
//! \file     mos_utilities_common.h
//! \brief    Common OS service across different platform
//! \details  Common OS service across different platform
//!
#ifndef __MOS_UTILITIES_COMMON_H__
#define __MOS_UTILITIES_COMMON_H__

#include "mos_defs.h"

#ifndef __MOS_USER_FEATURE_WA_
#define __MOS_USER_FEATURE_WA_
#endif
//------------------------------------------------------------------------------
// SECTION: Media User Feature Control
//
// ABSTRACT: Is an abstraction to read and write system level settings relating
//      to GEN media driver.
//------------------------------------------------------------------------------

//!
//! \brief ASSERT when failing to read user feature key or default user feature key value,
//!        according to MOS_UserFeature_ReadValue_ID.
//!
#define MOS_USER_FEATURE_INVALID_KEY_ASSERT(_expr) \
    if ((_expr) == MOS_STATUS_NULL_POINTER)        \
    {                                              \
        MOS_OS_ASSERT(false);                      \
    }

//!
//! \brief User Feature Type maximum and minimum data size
//!
#define MOS_USER_CONTROL_MIN_DATA_SIZE 128
#define MOS_USER_CONTROL_MAX_DATA_SIZE 2048
#define MOS_USER_MAX_STRING_COUNT 128

#define MOS_USER_FEATURE_MAX_UINT32_STR_VALUE "4294967295"

//! MOS User Feature
#define __NULL_USER_FEATURE_VALUE_WRITE_DATA__        \
    {                                                 \
        __MOS_USER_FEATURE_KEY_INVALID_ID, { {0}, 0 } \
    }
#ifdef __MOS_USER_FEATURE_WA_
#define __NULL_USER_FEATURE_VALUE__                                                                                                                                                                                                              \
    {                                                                                                                                                                                                                                            \
        __MOS_USER_FEATURE_KEY_INVALID_ID, nullptr, nullptr, nullptr, nullptr, MOS_USER_FEATURE_TYPE_INVALID, MOS_USER_FEATURE_VALUE_TYPE_INVALID, nullptr, nullptr, false, 0, nullptr, MOS_USER_FEATURE_EFFECT_ALWAYS, {0}, { {0}, 0 }          \
    }
#define MOS_DECLARE_UF_KEY(Id, ValueName, Readpath, Writepath, Group, Type, ValueType, DefaultValue, Description)                                                          \
    {                                                                                                                                                                      \
        Id, ValueName, Group, Readpath, Writepath, Type, ValueType, DefaultValue, Description, false, 1, nullptr, MOS_USER_FEATURE_EFFECT_ALWAYS, {0}, { {0}, 0 }          \
    }
// The MOS_DECLARE_UF_KEY_DBGONLY macro will make the user feature key read only return default value in release build without accessing user setting
// it is an alternative way for removing the key defintion entirely in release driver, and still provide an unified place for default values of the
// user feature key read request that is needed for release driver
#define MOS_DECLARE_UF_KEY_DBGONLY(Id, ValueName, Readpath, Writepath, Group, Type, ValueType, DefaultValue, Description)                                                     \
    {                                                                                                                                                                         \
        Id, ValueName, Group, Readpath, Writepath, Type, ValueType, DefaultValue, Description, false, 1, nullptr, MOS_USER_FEATURE_EFFECT_DEBUGONLY, {0}, { {0}, 0 }          \
    }
#else
#define __NULL_USER_FEATURE_VALUE__                                                                                                                                                                                                        \
    {                                                                                                                                                                                                                                      \
        __MOS_USER_FEATURE_KEY_INVALID_ID, nullptr, nullptr, nullptr, nullptr, MOS_USER_FEATURE_TYPE_INVALID, MOS_USER_FEATURE_VALUE_TYPE_INVALID, nullptr, nullptr, false, 0, nullptr, MOS_USER_FEATURE_EFFECT_ALWAYS, { {0}, 0 }         \
    }
#define MOS_DECLARE_UF_KEY(Id, ValueName, Readpath, Writepath, Group, Type, ValueType, DefaultValue, Description)                                                     \
    {                                                                                                                                                                 \
        Id, ValueName, Group, Readpath, Writepath, Type, ValueType, DefaultValue, Description, false, 1, nullptr, MOS_USER_FEATURE_EFFECT_ALWAYS, { {0}, 0 }          \
    }
#define MOS_DECLARE_UF_KEY_DBGONLY(Id, ValueName, Readpath, Writepath, Group, Type, ValueType, DefaultValue, Description)                                                \
    {                                                                                                                                                                    \
        Id, ValueName, Group, Readpath, Writepath, Type, ValueType, DefaultValue, Description, false, 1, nullptr, MOS_USER_FEATURE_EFFECT_DEBUGONLY, { {0}, 0 }          \
    }
#endif
#ifndef MAX_USER_FEATURE_FIELD_LENGTH
#define MAX_USER_FEATURE_FIELD_LENGTH 256
#endif

//!
//! \brief User Feature Value IDs
//!
typedef enum _MOS_USER_FEATURE_VALUE_ID
{
    __MOS_USER_FEATURE_KEY_INVALID_ID = 0,
    __MEDIA_USER_FEATURE_VALUE_MEDIA_RESET_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MEDIA_RESET_TH_ID,
    __MEDIA_USER_FEATURE_VALUE_SOFT_RESET_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_SIM_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_FORCE_VDBOX_ID,
    __MEDIA_USER_FEATURE_VALUE_LINUX_PERFORMANCETAG_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_FE_BE_TIMING,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_OUTPUT_FILE,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_BUFFER_SIZE,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_TIMER_REG,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_ENABLE_MULTI_PROCESS,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_1,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_2,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_3,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_4,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_5,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_6,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_7,
    __MEDIA_USER_FEATURE_VALUE_PERF_PROFILER_REGISTER_8,
    __MEDIA_USER_FEATURE_VALUE_DISABLE_KMD_WATCHDOG_ID,
    __MEDIA_USER_FEATURE_VALUE_SINGLE_TASK_PHASE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AUX_TABLE_16K_GRANULAR_ID,
    __MEDIA_USER_FEATURE_VALUE_MFE_MBENC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MFE_FIRST_BUFFER_SUBMIT_ID,
    __MEDIA_USER_FEATURE_VALUE_RC_PANIC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_SLICE_SHUTDOWN_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_FORCE_YFYS_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_LOCK_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_HW_WALKER_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_SUPPRESS_RECON_PIC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_ME_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_16xME_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_32xME_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_BRC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_RATECONTROL_METHOD_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_TARGET_USAGE_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_ENABLE_FRAME_TRACKING_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_USED_VDBOX_NUM_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_ENABLE_COMPUTE_CONTEXT_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_ENABLE_COMPUTE_CONTEXT_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ENCODE_ME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ENCODE_16xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ENCODE_32xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ENCODE_MULTIPRED_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ENCODE_INTRA_REFRESH_QP_THRESHOLD_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_FTQ_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_CAF_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_CAF_DISABLE_HD_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_MB_BRC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ROUNDING_INTER_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ROUNDING_INTER_P_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ROUNDING_INTER_B_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ROUNDING_INTER_BREF_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ADAPTIVE_ROUNDING_INTER_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_SKIP_BIAS_ADJUSTMENT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ADAPTIVE_INTRA_SCALING_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_OLD_MODE_COST_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_FORCE_TO_SKIP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_SLIDING_WINDOW_SIZE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_MB_SLICE_THRESHOLD_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_SLICE_THRESHOLD_TABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_TAIL_INSERTION_DELAY_COUNT_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_THRESHOLD_I_SLICE_SIZE_MINUS_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_THRESHOLD_P_SLICE_SIZE_MINUS_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_CRE_PREFETCH_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_TLB_PREFETCH_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_TLB_ALLOCATION_WA_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_PERMB_STREAMOUT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_SINGLE_PASS_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENC_BRC_MOTION_ADAPTIVE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMIO_MFX_LRA_0_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMIO_MFX_LRA_1_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMIO_MFX_LRA_2_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_FLATNESS_CHECK_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_ADAPTIVE_SEARCH_WINDOW_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_ADAPTIVE_TRANSFORM_DECISION_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_WEIGHTED_PREDICTION_L0_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_WEIGHTED_PREDICTION_L1_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_FBR_BYPASS_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_STATIC_FRAME_DETECTION_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_COLOR_BIT_SUPPORT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_GROUP_ID_SELECT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_BRC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_MULTIREF_QP_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_BRC_SOFTWARE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_BRC_VAR_COMPU_BYPASS_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_BRC_SOFTWARE_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_CNL_AVC_ENCODE_ARB_WA_ID,
    __MEDIA_USER_FEATURE_VALUE_HUC_DEMO_KERNEL_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_ME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_16xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_HUC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_MULTIPASS_BRC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_MULTIPASS_BRC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_ADAPTIVE_REPAK_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_ADAPTIVE_REPAK_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_ENCODE_SINGLE_PASS_DYS_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MEMNINJA_COUNTER_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_ENABLE_CMD_INIT_HUC_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_SECURE_INPUT_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_16xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_32xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_16xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_32xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_26Z_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_REGION_NUMBER_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_NUM_B_KERNEL_SPLIT,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_POWER_SAVING,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_NUM_8x8_INTRA_KERNEL_SPLIT,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_WP_SUPPORT_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_MEDIARESET_TEST_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_RDOQ_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_IFRAME_RDOQ_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_MULTIPASS_BRC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_MULTIPASS_BRC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_BRC_SOFTWARE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_BRC_SOFTWARE_PATH_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_BRC_SOFTWARE_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_ACQP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_VQI_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_FORCE_PAK_PASS_NUM_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_ROUNDING_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_PAKOBJCMD_STREAMOUT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_LBCONLY_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_PARTIAL_FRAME_UPDATE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_NUM_THREADS_PER_LCU_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_MDF_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_CODEC_MMC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_MMC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_MMC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_CODEC_MMC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_MMC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_MPEG2_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_VC1_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_AVC_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_JPEG_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_VP8_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_HEVC_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_VP9_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_HISTOGRAM_FROM_VEBOX_ID,
    __MEDIA_USER_FEATURE_VALUE_DECODE_EXTENDED_MMC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_MMC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_EXTENDED_MMC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMC_DEC_RT_COMPRESSIBLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMC_DEC_RT_COMPRESSMODE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMC_ENC_RECON_COMPRESSIBLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MMC_ENC_RECON_COMPRESSMODE_ID,
    __MEDIA_USER_FEATURE_VALUE_SSEU_SETTING_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_SLICE_SHUTDOWN_DEFAULT_STATE_ID,
    __MEDIA_USER_FEATURE_VALUE_SLICE_SHUTDOWN_REQUEST_STATE_ID,
    __MEDIA_USER_FEATURE_VALUE_SLICE_SHUTDOWN_RESOLUTION_THRESHOLD_ID,
    __MEDIA_USER_FEATURE_VALUE_SLICE_SHUTDOWN_TARGET_USAGE_THRESHOLD_ID,
    __MEDIA_USER_FEATURE_VALUE_SLICE_COUNT_SET_SUPPORT_ID,
    __MEDIA_USER_FEATURE_VALUE_DYNAMIC_SLICE_SHUTDOWN_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_VDBOX_BALANCING_ID,
    __MEDIA_USER_FEATURE_VALUE_MPEG2_SLICE_STATE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MPEG2_ENCODE_BRC_DISTORTION_BUFFER_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_NUMBER_OF_CODEC_DEVICES_ON_VDBOX1_ID,
    __MEDIA_USER_FEATURE_VALUE_NUMBER_OF_CODEC_DEVICES_ON_VDBOX2_ID,
    __MEDIA_USER_FEATURE_VALUE_VDI_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_MEDIA_WALKER_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_CSC_COEFF_PATCH_MODE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP8_HW_SCOREBOARD_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP8_ENCODE_ME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP8_ENCODE_16xME_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP8_ENCODE_REPAK_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP8_ENCODE_MULTIPASS_BRC_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP8_ENCODE_ADAPTIVE_REPAK_ENABLE_ID,
#if MOS_COMMAND_BUFFER_DUMP_SUPPORTED
    __MEDIA_USER_FEATURE_VALUE_DUMP_COMMAND_BUFFER_ENABLE_ID,
#endif  // MOS_COMMAND_BUFFER_DUMP_SUPPORTED
#if MOS_COMMAND_RESINFO_DUMP_SUPPORTED
    __MEDIA_USER_FEATURE_VALUE_DUMP_COMMAND_INFO_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_DUMP_COMMAND_INFO_PATH_ID,
#endif  // MOS_COMMAND_RESINFO_DUMP_SUPPORTED
#if (_DEBUG || _RELEASE_INTERNAL)
    __MEDIA_USER_FEATURE_VALUE_GROUP_ID_ID,
    __MEDIA_USER_FEATURE_VALUE_MEDIA_PREEMPTION_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_OVERRIDE_L3TCCNTRL_REG,
    __MEDIA_USER_FEATURE_VALUE_MDF_OVERRIDE_MOCS_INDEX,
    __MEDIA_USER_FEATURE_VALUE_MDF_OVERRIDE_L3ALLOC_REG,
    __MEDIA_USER_FEATURE_VALUE_MDF_FORCE_RAMODE,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_VFE_MAX_THREADS_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_VFE_MAX_THREADS_SCALING_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_FTQ_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_AVC_CAF_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_HW_WALKER_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_L3_CACHE_CNTLREG_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_L3_CACHE_CNTLREG2_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_L3_CACHE_CNTLREG3_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_L3_CACHE_SQCREG1_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_L3_CACHE_SQCREG4_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_L3_LRA_1_REG1_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_NULL_HW_ACCELERATION_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDBOX_ID_USED,
    __MEDIA_USER_FEATURE_VALUE_VDENC_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_CSC_METHOD_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_RAW_TILE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_RAW_FORMAT_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_CQM_QP_THRESHOLD_ID,
    __MEDIA_USER_FEATURE_VALUE_ISA_ASM_DEBUG_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_ISA_ASM_DEBUG_SURF_BTI_ID,
    __MEDIA_USER_FEATURE_VALUE_ROWSTORE_CACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_INTRAROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_DEBLOCKINGFILTERROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_BSDMPCROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MPRROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VDENCROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_SIM_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_IS_CODEC_ROW_STORE_CACHE_ENABLED_ID,
    __MEDIA_USER_FEATURE_VALUE_BREAK_IN_CODECHAL_CREATE_ID,
    __MEDIA_USER_FEATURE_VALUE_MEDIASOLO_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_STREAM_OUT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_DECOMPRESS_DECODE_OUTPUT_ID,
    __MEDIA_USER_FEATURE_VALUE_DECOMPRESS_DECODE_SFC_OUTPUT_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_DEBUG_OUTPUT_DIRECTORY_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_DUMP_OUTPUT_DIRECTORY_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_DEBUG_CFG_GENERATION_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_RDOQ_INTRA_TU_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_RDOQ_INTRA_TU_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_RDOQ_INTRA_TU_THRESHOLD_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_ENABLE_FAKE_HEADER_SIZE_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_FAKE_IFRAME_HEADER_SIZE_ID,
    __MEDIA_USER_FEATURE_VALUE_CODECHAL_FAKE_PBFRAME_HEADER_SIZE_ID,
    __MEDIA_USER_FEATURE_VALUE_COMMAND_OVERRIDE_INPUT_FILE_PATH_ID,
    __MEDIA_USER_FEATURE_VALUE_MHW_BASE_VDENC_INTERFACE_ID,
#endif  // (_DEBUG || _RELEASE_INTERNAL)
    __MEDIA_USER_FEATURE_VALUE_STATUS_REPORTING_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_SPLIT_SCREEN_DEMO_POSITION_ID,
    __MEDIA_USER_FEATURE_VALUE_SPLIT_SCREEN_DEMO_PARAMETERS_ID,
#if MOS_MESSAGES_ENABLED
    __MOS_USER_FEATURE_KEY_MESSAGE_HLT_ENABLED_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_HLT_OUTPUT_DIRECTORY_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_PRINT_ENABLED_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_OS_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_OS_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_OS_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_HW_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_HW_ID,
    //!
    //! \brief 63____________________________________________________________________________3__________0
    //!         |                                                                            |   All    |
    //!         |                    Reserved                                                |Asrt|level|
    //!         |____________________________________________________________________________|__________|
    //!
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_HW_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_CODEC_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_CODEC_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_CODEC_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_VP_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_VP_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_VP_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_CP_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_CP_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_CP_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_DDI_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_DDI_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_DDI_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_CM_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_CM_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_CM_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_SCALABILITY_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_SCALABILITY_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_SCALABILITY_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_MMC_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_MMC_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_MMC_TAG_ID,
    __MOS_USER_FEATURE_KEY_MESSAGE_BLT_TAG_ID,
    __MOS_USER_FEATURE_KEY_BY_SUB_COMPONENT_BLT_ID,
    __MOS_USER_FEATURE_KEY_SUB_COMPONENT_BLT_TAG_ID,
#endif  // MOS_MESSAGES_ENABLED
    __MEDIA_USER_FEATURE_VALUE_HEVC_SF_2_DMA_SUBMITS_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVCDATROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVCDFROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVCSAOROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_HVDROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_DATROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VP9_DFROWSTORECACHE_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_DDI_DUMP_DIRECTORY_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_DDI_DUMP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_CMD_DUMP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_ETW_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_LOG_LEVEL_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_UMD_ULT_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_CURBE_DUMP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_SURFACE_DUMP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_SURFACE_STATE_DUMP_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_CMD_DUMP_COUNTER_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_SURFACE_STATE_DUMP_COUNTER_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_INTERFACE_DESCRIPTOR_DATA_DUMP_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_INTERFACE_DESCRIPTOR_DATA_COUNTER_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_DUMPPATH_USER_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_EMU_MODE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_DEFAULT_CM_QUEUE_TYPE_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_CCS_USE_VE_INTERFACE,
    __MEDIA_USER_FEATURE_VALUE_MDF_CCS_USE_VE_DEBUG_OVERRIDE,
    __MEDIA_USER_FEATURE_VALUE_MDF_FORCE_EXECUTION_PATH_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_MAX_THREAD_NUM_ID,
    __MEDIA_USER_FEATURE_VALUE_MDF_FORCE_COHERENT_STATELESSBTI_ID,
    __MEDIA_USER_FEATURE_ENABLE_RENDER_ENGINE_MMC_ID,
    __MEDIA_USER_FEATURE_VALUE_DISABLE_MMC_ID,
    __VPHAL_VEBOX_OUTPUTPIPE_MODE_ID,
    __VPHAL_VEBOX_FEATURE_INUSE_ID,
    __VPHAL_RNDR_SSD_CONTROL_ID,
    __VPHAL_RNDR_SCOREBOARD_CONTROL_ID,
    __VPHAL_RNDR_CMFC_CONTROL_ID,
#if (_DEBUG || _RELEASE_INTERNAL)
    __VPHAL_DBG_SURF_DUMP_OUTFILE_KEY_NAME_ID,
    __VPHAL_DBG_SURF_DUMP_LOCATION_KEY_NAME_ID,
    __VPHAL_DBG_SURF_DUMP_MANUAL_TRIGGER_KEY_NAME_ID,
    __VPHAL_DBG_SURF_DUMP_START_FRAME_KEY_NAME_ID,
    __VPHAL_DBG_SURF_DUMP_END_FRAME_KEY_NAME_ID,
    __VPHAL_DBG_SURF_DUMPER_ENABLE_PLANE_DUMP,
    __VPHAL_DBG_SURF_DUMP_ENABLE_AUX_DUMP_ID,
    __VPHAL_DBG_SURF_DUMPER_RESOURCE_LOCK_ID,
    __VPHAL_DBG_STATE_DUMP_OUTFILE_KEY_NAME_ID,
    __VPHAL_DBG_STATE_DUMP_LOCATION_KEY_NAME_ID,
    __VPHAL_DBG_STATE_DUMP_START_FRAME_KEY_NAME_ID,
    __VPHAL_DBG_STATE_DUMP_END_FRAME_KEY_NAME_ID,
    __VPHAL_DBG_PARAM_DUMP_OUTFILE_KEY_NAME_ID,
    __VPHAL_DBG_PARAM_DUMP_START_FRAME_KEY_NAME_ID,
    __VPHAL_DBG_PARAM_DUMP_END_FRAME_KEY_NAME_ID,
    __VPHAL_DBG_DUMP_OUTPUT_DIRECTORY_ID,
#endif
    __VPHAL_SET_SINGLE_SLICE_VEBOX_ID,
    __VPHAL_BYPASS_COMPOSITION_ID,
    __VPHAL_VEBOX_DISABLE_SFC_ID,
    __VPHAL_ENABLE_MMC_ID,
    __VPHAL_ENABLE_MMC_IN_USE_ID,
    __VPHAL_PRIMARY_SURFACE_COMPRESS_MODE_ID,
    __VPHAL_PRIMARY_SURFACE_COMPRESSIBLE_ID,
    __VPHAL_RT_COMPRESS_MODE_ID,
    __VPHAL_RT_COMPRESSIBLE_ID,
    __VPHAL_ENABLE_VEBOX_MMC_DECOMPRESS_ID,
    __VPHAL_VEBOX_DISABLE_TEMPORAL_DENOISE_FILTER_ID,
    __VPHAL_ENABLE_SUPER_RESOLUTION_ID,
    __VPHAL_SUPER_RESOLUTION_MODE_ID,
    __VPHAL_ENABLE_SUPER_RESOLUTION_EDSR_ID,
    __VPHAL_SUPER_RESOLUTION_EDSR_MODE_ID,
#if (_DEBUG || _RELEASE_INTERNAL)
    __VPHAL_COMP_8TAP_ADAPTIVE_ENABLE_ID,
    __VPHAL_RNDR_FORCE_VP_DECOMPRESSED_OUTPUT_ID,
#endif
#if ((_DEBUG || _RELEASE_INTERNAL) && !EMUL)
    __VPHAL_RNDR_VEBOX_MODE_0_ID,
    __VPHAL_RNDR_VEBOX_MODE_0_TO_2_ID,
    __VPHAL_RNDR_VEBOX_MODE_2_ID,
    __VPHAL_RNDR_VEBOX_MODE_2_TO_0_ID,
#endif
#if (_DEBUG || _RELEASE_INTERNAL)
    __VPHAL_ENABLE_COMPUTE_CONTEXT_ID,
#endif
    __MOS_USER_FEATURE_KEY_VP_CAPS_FF_OVERRIDE_ID,
    __MOS_USER_FEATURE_KEY_XML_AUTOGEN_ID,
    __MOS_USER_FEATURE_KEY_XML_FILEPATH_ID,
    __MOS_USER_FEATURE_KEY_XML_DUMP_GROUPS_ID,
    __MEDIA_USER_FEATURE_VALUE_FORCE_VEBOX_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_VEBOX_SCALABILITY_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_VEBOX_SPLIT_RATIO_ID,
    __MEDIA_USER_FEATURE_VALUE_HCP_DECODE_MODE_SWITCH_THRESHOLD1_ID,
    __MEDIA_USER_FEATURE_VALUE_HCP_DECODE_MODE_SWITCH_THRESHOLD2_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_VE_DEBUG_OVERRIDE,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_HW_SEMAPHORE,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_VDBOX_HW_SEMAPHORE,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_ENABLE_HW_STITCH,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_SUBTHREAD_NUM_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_PAK_ONLY_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VME_ENCODE_SSE_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENCODE_DISABLE_SCALABILITY,
    __MEDIA_USER_FEATURE_VALUE_HEVC_ENCODE_RDOQ_PERF_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_WATCHDOG_TIMER_THRESHOLD,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_DECODE_VIRTUAL_ENGINE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_DECODE_VE_CTXSCHEDULING_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_LINUX_FRAME_SPLIT_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_ENCODE_VIRTUAL_ENGINE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_ENCODE_VE_CTXSCHEDULING_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_VE_DEBUG_OVERRIDE_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_HCP_SCALABILITY_DECODE_ID,
    __MEDIA_USER_FEATURE_VALUE_HCP_DECODE_ALWAYS_FRAME_SPLIT_ID,
    __MEDIA_USER_FEATURE_VALUE_SCALABILITY_OVERRIDE_SPLIT_WIDTH_IN_MINCB,
    __MEDIA_USER_FEATURE_VALUE_SCALABILITY_FE_SEPARATE_SUBMISSION_ENABLED_ID,
    __MEDIA_USER_FEATURE_VALUE_SCALABILITY_FE_SEPARATE_SUBMISSION_IN_USE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VME_BRC_LTR_DISABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VME_BRC_LTR_INTERVAL_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VME_FORCE_SCALABILITY_ID,
    __MEDIA_USER_FEATURE_VALUE_HEVC_VDENC_SEMA_RESET_DELAY_ID,
    __MEDIA_USER_FEATURE_VALUE_SET_CMD_DEFAULT_PARS_FROM_FILES_ID,
    __MEDIA_USER_FEATURE_VALUE_CMD_PARS_FILES_DIRECORY_ID,
    __MEDIA_USER_FEATURE_VALUE_APOGEIOS_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_VPP_APOGEIOS_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_SUPER_RESOLUTION_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_SUPER_RESOLUTION_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_EXTERNAL_COPY_SYNC_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_UMD_OCA_ID,
    __MEDIA_USER_FEATURE_VALUE_COUNT_FOR_OCA_BUFFER_LEAKED_ID,
    __MEDIA_USER_FEATURE_VALUE_COUNT_FOR_OCA_1ST_LEVEL_BB_END_MISSED_ID,
    __MEDIA_USER_FEATURE_VALUE_COUNT_FOR_ADDITIONAL_OCA_BUFFER_ALLOCATED_ID,
    __MEDIA_USER_FEATURE_VALUE_OCA_STATUS_ID,
    __MEDIA_USER_FEATURE_VALUE_OCA_ERROR_HINT_ID,
    __MEDIA_USER_FEATURE_VALUE_IS_INDIRECT_STATE_HEAP_INVALID_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_SW_BACK_ANNOTATION_ID,
    __MEDIA_USER_FEATURE_VALUE_ENABLE_CC_STITCHING_ID,
#if (_DEBUG || _RELEASE_INTERNAL)
    __MEDIA_USER_FEATURE_VALUE_ENABLE_SW_STITCHING_ID,
    __MEDIA_USER_FEATURE_VALUE_ALLOC_MEMORY_FAIL_SIMULATE_MODE_ID,
    __MEDIA_USER_FEATURE_VALUE_ALLOC_MEMORY_FAIL_SIMULATE_FREQ_ID,
    __MEDIA_USER_FEATURE_VALUE_ALLOC_MEMORY_FAIL_SIMULATE_HINT_ID,
#endif
    __MEDIA_USER_FEATURE_VALUE_PERF_UTILITY_TOOL_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_PERF_OUTPUT_DIRECTORY_ID,
    __MEDIA_USER_FEATURE_VALUE_APO_MOS_PATH_ENABLE_ID,
    __MEDIA_USER_FEATURE_VALUE_APOGEIOS_HEVCD_ENABLE_ID,
    __MOS_USER_FEATURE_KEY_MAX_ID,
} MOS_USER_FEATURE_VALUE_ID;

//!
//! \brief User Feature Type
//!
typedef enum
{
    MOS_USER_FEATURE_TYPE_INVALID,
    MOS_USER_FEATURE_TYPE_USER,
    MOS_USER_FEATURE_TYPE_SYSTEM,
} MOS_USER_FEATURE_TYPE,
    *PMOS_USER_FEATURE_TYPE;

//!
//! \brief User Feature Value type
//!
typedef enum
{
    MOS_USER_FEATURE_VALUE_TYPE_INVALID,
    MOS_USER_FEATURE_VALUE_TYPE_BINARY,
    MOS_USER_FEATURE_VALUE_TYPE_BOOL,
    MOS_USER_FEATURE_VALUE_TYPE_INT32,
    MOS_USER_FEATURE_VALUE_TYPE_INT64,
    MOS_USER_FEATURE_VALUE_TYPE_UINT32,
    MOS_USER_FEATURE_VALUE_TYPE_UINT64,
    MOS_USER_FEATURE_VALUE_TYPE_FLOAT,
    MOS_USER_FEATURE_VALUE_TYPE_STRING,
    MOS_USER_FEATURE_VALUE_TYPE_MULTI_STRING,
} MOS_USER_FEATURE_VALUE_TYPE,
    *PMOS_USER_FEATURE_VALUE_TYPE;

//!
//! \brief User Feature Notification type
//!
typedef enum
{
    MOS_USER_FEATURE_NOTIFY_TYPE_INVALID,
    MOS_USER_FEATURE_NOTIFY_TYPE_VALUE_CHANGE,
} MOS_USER_FEATURE_NOTIFY_TYPE,
    *PMOS_USER_FEATURE_NOTIFY_TYPE;

//!
//! \brief User Feature Data Operation type
//!         NONE_CUSTOM_DEFAULT_VALUE :     None Custom Default Value for Input Data
//!         CUSTOM_DEFAULT_VALUE_TYPE :     With Custom Default Value for Input Data
//!
typedef enum
{
    MOS_USER_FEATURE_VALUE_DATA_FLAG_NONE_CUSTOM_DEFAULT_VALUE_TYPE = 0,
    MOS_USER_FEATURE_VALUE_DATA_FLAG_CUSTOM_DEFAULT_VALUE_TYPE,
} MOS_USER_FEATURE_VALUE_DATA_FLAG_TYPE,
    *PMOS_USER_FEATURE_VALUE_DATA_FLAG_TYPE;

//!
//! \brief User Feature Key Effective Range type
//!         EFFECT_ALWALYS   :   Effective on all driver builds
//!         EFFECT_DEBUGONLY :   Effective on release-internal and debug driver only
//!
typedef enum
{
    MOS_USER_FEATURE_EFFECT_ALWAYS = 0,
    MOS_USER_FEATURE_EFFECT_DEBUGONLY,
} MOS_USER_FEATURE_EFFECTIVE_TYPE,
    *PMOS_USER_FEATURE_EFFECTIVE_TYPE;

//!
//! \brief User Feature String Data
//!
typedef struct
{
    char *   pStringData;
    uint32_t uMaxSize;
    uint32_t uSize;
} MOS_USER_FEATURE_VALUE_STRING, *PMOS_USER_FEATURE_VALUE_STRING;

//!
//! \brief User Feature Multi String Data
//!
typedef struct
{
    char *                         pMultStringData;
    uint32_t                       uMaxSize;
    uint32_t                       uSize;
    PMOS_USER_FEATURE_VALUE_STRING pStrings;
    uint32_t                       uCount;
} MOS_USER_FEATURE_VALUE_MULTI_STRING, *PMOS_USER_FEATURE_VALUE_MULTI_STRING;

//!
//! \brief User Feature Binary Data
//!
typedef struct
{
    uint8_t *pBinaryData;
    uint32_t uMaxSize;
    uint32_t uSize;
} MOS_USER_FEATURE_VALUE_BINARY, *PMOS_USER_FEATURE_VALUE_BINARY;

//!
//! \brief      User Feature Value Data
//! \details    union :         to store the user feature value
//!             i32DataFlag :   the input data valye type
//!                             refer to MOS_USER_FEATURE_VALUE_DATA_FLAG_TYPE
//!
//!
typedef struct _MOS_USER_FEATURE_VALUE_DATA
{
    union
    {
        int32_t                             bData;
        uint32_t                            u32Data;
        uint64_t                            u64Data;
        int32_t                             i32Data;
        int64_t                             i64Data;
        float                               fData;
        MOS_USER_FEATURE_VALUE_STRING       StringData;
        MOS_USER_FEATURE_VALUE_MULTI_STRING MultiStringData;
        MOS_USER_FEATURE_VALUE_BINARY       BinaryData;
    };
    int32_t i32DataFlag;
} MOS_USER_FEATURE_VALUE_DATA, *PMOS_USER_FEATURE_VALUE_DATA;

//!
//! \brief User Feature Value Information
//!
typedef struct _MOS_USER_FEATURE_VALUE_WRITE_DATA
{
    uint32_t                    ValueID;
    MOS_USER_FEATURE_VALUE_DATA Value;
} MOS_USER_FEATURE_VALUE_WRITE_DATA, *PMOS_USER_FEATURE_VALUE_WRITE_DATA;

//!
//! \brief User Feature Value Information
//!
typedef struct _MOS_USER_FEATURE_VALUE_INFO
{
    char *   pcName;  //store name for the bitmask/enum values
    uint32_t Value;
} MOS_USER_FEATURE_VALUE_INFO, *PMOS_USER_FEATURE_VALUE_INFO;

//!
//! \brief User Feature Data
//!
typedef struct
{
    uint32_t                     ValueID;
    const char *                 pValueName;
    const char *                 pcGroup;        //!< User feature key group - eg: MediaSolo, MOS, Codec
    const char *                 pcPath;         //!< User feature Key Read Path
    const char *                 pcWritePath;    //!< User feature Key Write Path
    MOS_USER_FEATURE_TYPE        Type;           //!< User feature Key User Feature type - eg: System, User
    MOS_USER_FEATURE_VALUE_TYPE  ValueType;      //!< User feature key type - eg: bool,dword
    const char *                 DefaultValue;   //!< User feature key value
    const char *                 pcDescription;  //!< User feature key description
    int32_t                      bExists;        //<! Set if the user feature key is defined in the user feature key manager
    uint32_t                     uiNumOfValues;  //<! Number of valid user feature key values. Useful for user feature keys of type bitmask and enum
    PMOS_USER_FEATURE_VALUE_INFO pValueInfo;     //<! Store information of all valid enum/bit mask values and names
    MOS_USER_FEATURE_EFFECTIVE_TYPE EffctiveRange;  //<! User feature key effect range, eg: Always effective / debug driver only
    // Temp WA for old user feature read/write
#ifdef __MOS_USER_FEATURE_WA_
    union
    {
        int32_t                             bData;
        uint32_t                            u32Data;
        uint64_t                            u64Data;
        int32_t                             i32Data;
        int64_t                             i64Data;
        float                               fData;
        MOS_USER_FEATURE_VALUE_STRING       StringData;
        MOS_USER_FEATURE_VALUE_MULTI_STRING MultiStringData;
        MOS_USER_FEATURE_VALUE_BINARY       BinaryData;
    };
#endif
    MOS_USER_FEATURE_VALUE_DATA Value;  //!< User feature key value
} MOS_USER_FEATURE_VALUE, *PMOS_USER_FEATURE_VALUE;

//!
//! \brief User Feature Value Information
//!
typedef struct
{
    PMOS_USER_FEATURE_VALUE pUserFeatureValue;
} MOS_USER_FEATURE_VALUE_MAP, *PMOS_USER_FEATURE_VALUE_MAP;

//!
//! \brief User Feature Notification Data
//!
typedef struct
{
    MOS_USER_FEATURE_TYPE        Type;        //!< User Feature Type
    char *                       pPath;       //!< User Feature Path
    MOS_USER_FEATURE_NOTIFY_TYPE NotifyType;  //!< Notification Type
    int32_t                      bTriggered;  //!< Notification is triggered or not
    void *                       pHandle;     //!< OS Specific Handle
} MOS_USER_FEATURE_NOTIFY_DATA, *PMOS_USER_FEATURE_NOTIFY_DATA;

//!
//! \brief User Feature Interface
//!
typedef struct
{
    MOS_USER_FEATURE_TYPE   Type;         //!< User Feature Type
    const char *            pPath;        //!< User Feature Path
    PMOS_USER_FEATURE_VALUE pValues;      //!< Array of User Feature Values
    uint32_t                uiNumValues;  //!< Number of User Feature Values
} MOS_USER_FEATURE, *PMOS_USER_FEATURE;

//!
//! \brief OS User Feature Interface
//!
typedef struct _MOS_USER_FEATURE_INTERFACE *PMOS_USER_FEATURE_INTERFACE;
typedef struct _MOS_USER_FEATURE_INTERFACE
{
    void *                  pOsInterface;              //!< Pointer to OS Interface
    int32_t                 bIsNotificationSupported;  //!< Whether Notification feature is supported

    MOS_STATUS (*pfnEnableNotification)
    (
        PMOS_USER_FEATURE_INTERFACE   pOsUserFeatureInterface,
        PMOS_USER_FEATURE_NOTIFY_DATA pNotification);

    MOS_STATUS (*pfnDisableNotification)
    (
        PMOS_USER_FEATURE_INTERFACE   pOsUserFeatureInterface,
        PMOS_USER_FEATURE_NOTIFY_DATA pNotification);

    MOS_STATUS (*pfnParsePath)
    (
        PMOS_USER_FEATURE_INTERFACE pOsUserFeatureInterface,
        char *const                 pInputPath,
        PMOS_USER_FEATURE_TYPE      pUserFeatureType,
        char **                     ppSubPath);

} MOS_USER_FEATURE_INTERFACE;

//!
//! \brief User Feature Notification Data Common
//!
typedef struct
{
    void *   UFKey;       //!< Handle to User Feature Key
    HANDLE   hEvent;      //!< Handle to User Feature Key Event
    PTP_WAIT hWaitEvent;  //!< Handle to User Feature Key Wait Event
} MOS_USER_FEATURE_NOTIFY_DATA_COMMON, *PMOS_USER_FEATURE_NOTIFY_DATA_COMMON;

//!
//! \brief User Feature Key Path Info
//!
typedef struct
{
    char *   Path;
    uint32_t Length;
    uint32_t RefCnt;
} MOS_USER_FEATURE_KEY_PATH_INFO, *PMOS_USER_FEATURE_KEY_PATH_INFO;

#ifdef __cplusplus
//Memory alloc fail simulatiion related defination
#if (_DEBUG || _RELEASE_INTERNAL)
#define NO_ALLOC_ALIGNMENT (1)
#endif  //(_DEBUG || _RELEASE_INTERNAL)
#endif
#endif  // __MOS_UTILITIES_COMMON_H__
