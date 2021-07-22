/*
* Copyright (c) 2018-2021, Intel Corporation
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
//! \file     vp_resource_manager.cpp
//! \brief    The source file of the base class of vp resource manager
//! \details  all the vp resources will be traced here for usages using intermeida 
//!           surfaces.
//!
#include "vp_resource_manager.h"
#include "vp_vebox_cmd_packet.h"
#include "sw_filter_pipe.h"
#include "vp_utils.h"

using namespace std;
namespace vp
{

#define VP_SAME_SAMPLE_THRESHOLD 0

inline bool IsInterleaveFirstField(VPHAL_SAMPLE_TYPE sampleType)
{
    return ((sampleType == SAMPLE_INTERLEAVED_ODD_FIRST_BOTTOM_FIELD)   ||
            (sampleType == SAMPLE_INTERLEAVED_EVEN_FIRST_TOP_FIELD)     ||
            (sampleType == SAMPLE_SINGLE_TOP_FIELD));
}

extern const VEBOX_SPATIAL_ATTRIBUTES_CONFIGURATION g_cInit_VEBOX_SPATIAL_ATTRIBUTES_CONFIGURATIONS =
{
    // DWORD 0
    {
        {NOISE_BLF_RANGE_THRESHOLD_S0_DEFAULT},           // RangeThrStart0
    },

    // DWORD 1
    {
        {NOISE_BLF_RANGE_THRESHOLD_S1_DEFAULT},           // RangeThrStart1
    },

    // DWORD 2
    {
        {NOISE_BLF_RANGE_THRESHOLD_S2_DEFAULT},           // RangeThrStart2
    },

    // DWORD 3
    {
        {NOISE_BLF_RANGE_THRESHOLD_S3_DEFAULT},           // RangeThrStart3
    },

    // DWORD 4
    {
        {NOISE_BLF_RANGE_THRESHOLD_S4_DEFAULT},           // RangeThrStart4
    },

    // DWORD 5
    {
        {NOISE_BLF_RANGE_THRESHOLD_S5_DEFAULT},           // RangeThrStart5
    },

    // DWORD 6
    {
        {0},                                              // Reserved
    },

    // DWORD 7
    {
        {0},                                              // Reserved
    },

    // DWORD 8
    {
        {NOISE_BLF_RANGE_WGTS0_DEFAULT},                  // RangeWgt0
    },

    // DWORD 9
    {
        {NOISE_BLF_RANGE_WGTS1_DEFAULT},                  // RangeWgt1
    },

    // DWORD 10
    {
        {NOISE_BLF_RANGE_WGTS2_DEFAULT},                  // RangeWgt2
    },

    // DWORD 11
    {
        {NOISE_BLF_RANGE_WGTS3_DEFAULT},                  // RangeWgt3
    },

    // DWORD 12
    {
        {NOISE_BLF_RANGE_WGTS4_DEFAULT},                  // RangeWgt4
    },

    // DWORD 13
    {
        {NOISE_BLF_RANGE_WGTS5_DEFAULT},                  // RangeWgt5
    },

    // DWORD 14
    {
        {0},                                              // Reserved
    },

    // DWORD 15
    {
        {0},                                              // Reserved
    },

    // DWORD 16 - 41: DistWgt[5][5]
    {
        {NOISE_BLF_DISTANCE_WGTS00_DEFAULT, NOISE_BLF_DISTANCE_WGTS01_DEFAULT, NOISE_BLF_DISTANCE_WGTS02_DEFAULT, NOISE_BLF_DISTANCE_WGTS01_DEFAULT, NOISE_BLF_DISTANCE_WGTS00_DEFAULT},
        {NOISE_BLF_DISTANCE_WGTS10_DEFAULT, NOISE_BLF_DISTANCE_WGTS11_DEFAULT, NOISE_BLF_DISTANCE_WGTS12_DEFAULT, NOISE_BLF_DISTANCE_WGTS11_DEFAULT, NOISE_BLF_DISTANCE_WGTS10_DEFAULT},
        {NOISE_BLF_DISTANCE_WGTS20_DEFAULT, NOISE_BLF_DISTANCE_WGTS21_DEFAULT, NOISE_BLF_DISTANCE_WGTS22_DEFAULT, NOISE_BLF_DISTANCE_WGTS21_DEFAULT, NOISE_BLF_DISTANCE_WGTS20_DEFAULT},
        {NOISE_BLF_DISTANCE_WGTS10_DEFAULT, NOISE_BLF_DISTANCE_WGTS11_DEFAULT, NOISE_BLF_DISTANCE_WGTS12_DEFAULT, NOISE_BLF_DISTANCE_WGTS11_DEFAULT, NOISE_BLF_DISTANCE_WGTS10_DEFAULT},
        {NOISE_BLF_DISTANCE_WGTS00_DEFAULT, NOISE_BLF_DISTANCE_WGTS01_DEFAULT, NOISE_BLF_DISTANCE_WGTS02_DEFAULT, NOISE_BLF_DISTANCE_WGTS01_DEFAULT, NOISE_BLF_DISTANCE_WGTS00_DEFAULT},
    },

    // Padding
    {
        0,                                      // Padding
        0,                                      // Padding
        0,                                      // Padding
        0,                                      // Padding
        0,                                      // Padding
        0,                                      // Padding
        0,                                      // Padding
    }
};

VpResourceManager::VpResourceManager(MOS_INTERFACE &osInterface, VpAllocator &allocator, VphalFeatureReport &reporting)
    : m_osInterface(osInterface), m_allocator(allocator), m_reporting(reporting)
{
    InitSurfaceConfigMap();
}

VpResourceManager::~VpResourceManager()
{
    // Clean all intermedia Resource
    DestoryVeboxOutputSurface();
    DestoryVeboxDenoiseOutputSurface();

    for (uint32_t i = 0; i < VP_NUM_STMM_SURFACES; i++)
    {
        if (m_veboxSTMMSurface[i])
        {
            m_allocator.DestroyVpSurface(m_veboxSTMMSurface[i]);
        }
    }

    if (m_veboxStatisticsSurface)
    {
        m_allocator.DestroyVpSurface(m_veboxStatisticsSurface);
    }

    if (m_veboxRgbHistogram)
    {
        m_allocator.DestroyVpSurface(m_veboxRgbHistogram);
    }

    if (m_veboxDNTempSurface)
    {
        m_allocator.DestroyVpSurface(m_veboxDNTempSurface);
    }

    if (m_veboxDNSpatialConfigSurface)
    {
        m_allocator.DestroyVpSurface(m_veboxDNSpatialConfigSurface);
    }

    if (m_vebox3DLookUpTables)
    {
        m_allocator.DestroyVpSurface(m_vebox3DLookUpTables);
    }

    while (!m_intermediaSurfaces.empty())
    {
        VP_SURFACE * surf = m_intermediaSurfaces.back();
        m_allocator.DestroyVpSurface(surf);
        m_intermediaSurfaces.pop_back();
    }

    m_allocator.CleanRecycler();
}

void VpResourceManager::CleanTempSurfaces()
{
    VP_FUNC_CALL();

    while (!m_tempSurface.empty())
    {
        auto it = m_tempSurface.begin();
        m_allocator.DestroyVpSurface(it->second);
        m_tempSurface.erase(it);
    }
}

MOS_STATUS VpResourceManager::OnNewFrameProcessStart(SwFilterPipe &pipe)
{
    VP_FUNC_CALL();

    VP_SURFACE *inputSurface    = pipe.GetSurface(true, 0);
    VP_SURFACE *outputSurface   = pipe.GetSurface(false, 0);
    SwFilter   *diFilter        = pipe.GetSwFilter(true, 0, FeatureTypeDi);

    if (nullptr == inputSurface && nullptr == outputSurface)
    {
        VP_PUBLIC_ASSERTMESSAGE("Both input and output surface being nullptr!");
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_INVALID_PARAMETER);
    }

    if (0 != m_currentPipeIndex)
    {
        VP_PUBLIC_ASSERTMESSAGE("m_currentPipeIndex(%d) is not 0. May caused by OnNewFrameProcessEnd not paired with OnNewFrameProcessStart!");
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_INVALID_PARAMETER);
    }

    VP_SURFACE *pastSurface   = pipe.GetPastSurface(0);
    VP_SURFACE *futureSurface = pipe.GetFutureSurface(0);

    int32_t currentFrameId = inputSurface ? inputSurface->FrameID : (outputSurface ? outputSurface->FrameID : 0);
    int32_t pastFrameId = pastSurface ? pastSurface->FrameID : 0;
    int32_t futureFrameId = futureSurface ? futureSurface->FrameID : 0;

    m_currentFrameIds.valid                     = true;
    m_currentFrameIds.diEnabled                 = nullptr != diFilter;
    m_currentFrameIds.currentFrameId            = currentFrameId;
    m_currentFrameIds.pastFrameId               = pastFrameId;
    m_currentFrameIds.futureFrameId             = futureFrameId;
    m_currentFrameIds.pastFrameAvailable        = pastSurface ? true : false;
    m_currentFrameIds.futureFrameAvailable      = futureSurface ? true : false;

    // Only set sameSamples flag DI enabled frames.
    if (m_pastFrameIds.valid && m_currentFrameIds.pastFrameAvailable &&
        m_pastFrameIds.diEnabled && m_currentFrameIds.diEnabled)
    {
        m_sameSamples   =
               WITHIN_BOUNDS(
                      m_currentFrameIds.currentFrameId - m_pastFrameIds.currentFrameId,
                      -VP_SAME_SAMPLE_THRESHOLD,
                      VP_SAME_SAMPLE_THRESHOLD) &&
               WITHIN_BOUNDS(
                      m_currentFrameIds.pastFrameId - m_pastFrameIds.pastFrameId,
                      -VP_SAME_SAMPLE_THRESHOLD,
                      VP_SAME_SAMPLE_THRESHOLD);

        if (m_sameSamples)
        {
            m_outOfBound = false;
        }
        else
        {
            m_outOfBound =
                OUT_OF_BOUNDS(
                        m_currentFrameIds.pastFrameId - m_pastFrameIds.currentFrameId,
                        -VP_SAME_SAMPLE_THRESHOLD,
                        VP_SAME_SAMPLE_THRESHOLD);
        }
    }
    // bSameSamples flag also needs to be set for no reference case
    else if (m_pastFrameIds.valid && !m_currentFrameIds.pastFrameAvailable &&
        m_pastFrameIds.diEnabled && m_currentFrameIds.diEnabled)
    {
        m_sameSamples   =
               WITHIN_BOUNDS(
                      m_currentFrameIds.currentFrameId - m_pastFrameIds.currentFrameId,
                      -VP_SAME_SAMPLE_THRESHOLD,
                      VP_SAME_SAMPLE_THRESHOLD);
        m_outOfBound    = false;
    }
    else
    {
        m_sameSamples   = false;
        m_outOfBound    = false;
    }

    if (inputSurface)
    {
        m_maxSrcRect.right  = MOS_MAX(m_maxSrcRect.right, inputSurface->rcSrc.right);
        m_maxSrcRect.bottom = MOS_MAX(m_maxSrcRect.bottom, inputSurface->rcSrc.bottom);
    }

    // Swap buffers for next iteration
    if (!m_sameSamples)
    {
        m_currentDnOutput   = (m_currentDnOutput + 1) & 1;
        m_currentStmmIndex  = (m_currentStmmIndex + 1) & 1;
    }

    m_pastFrameIds = m_currentFrameIds;

    return MOS_STATUS_SUCCESS;
}

void VpResourceManager::OnNewFrameProcessEnd()
{
    m_allocator.CleanRecycler();
    m_currentPipeIndex = 0;
    CleanTempSurfaces();
}

void VpResourceManager::InitSurfaceConfigMap()
{
    ///*             _b64DI
    //               |      _sfcEnable
    //               |      |      _sameSample
    //               |      |      |      _outOfBound
    //               |      |      |      |      _pastRefAvailable
    //               |      |      |      |      |      _futureRefAvailable
    //               |      |      |      |      |      |      _firstDiField
    //               |      |      |      |      |      |      |      _currentInputSurface
    //               |      |      |      |      |      |      |      |                     _pastInputSurface
    //               |      |      |      |      |      |      |      |                     |                      _currentOutputSurface
    //               |      |      |      |      |      |      |      |                     |                      |                     _pastOutputSurface*/
    //               |      |      |      |      |      |      |      |                     |                      |                     |                     */
    AddSurfaceConfig(true,  true,  false, false, true,  false, true,  VEBOX_SURFACE_INPUT,  VEBOX_SURFACE_PAST_REF, VEBOX_SURFACE_FRAME1, VEBOX_SURFACE_FRAME0);
    AddSurfaceConfig(true,  true,  true,  false, true,  false, false, VEBOX_SURFACE_FRAME1, VEBOX_SURFACE_NULL,    VEBOX_SURFACE_NULL,   VEBOX_SURFACE_NULL);
    AddSurfaceConfig(true,  true,  false, false, false, false, true,  VEBOX_SURFACE_INPUT,  VEBOX_SURFACE_NULL,    VEBOX_SURFACE_FRAME1, VEBOX_SURFACE_NULL);
    AddSurfaceConfig(true,  true,  false, false, false, false, false, VEBOX_SURFACE_INPUT,  VEBOX_SURFACE_NULL,    VEBOX_SURFACE_FRAME1, VEBOX_SURFACE_NULL);
    AddSurfaceConfig(true,  true,  true,  false, false, false, true,  VEBOX_SURFACE_INPUT,  VEBOX_SURFACE_NULL,    VEBOX_SURFACE_FRAME1, VEBOX_SURFACE_NULL);
    AddSurfaceConfig(true,  true,  true,  false, false, false, false, VEBOX_SURFACE_INPUT,  VEBOX_SURFACE_NULL,    VEBOX_SURFACE_FRAME1, VEBOX_SURFACE_NULL);
}

uint32_t VpResourceManager::GetHistogramSurfaceSize(VP_EXECUTE_CAPS& caps, uint32_t inputWidth, uint32_t inputHeight)
{
    // Allocate Rgb Histogram surface----------------------------------------------
    // Size of RGB histograms, 1 set for each slice. For single slice, other set will be 0
    uint32_t dwSize = VP_VEBOX_RGB_HISTOGRAM_SIZE;
    dwSize += VP_VEBOX_RGB_ACE_HISTOGRAM_SIZE_RESERVED;
    // Size of ACE histograms, 1 set for each slice. For single slice, other set will be 0
    dwSize += VP_VEBOX_ACE_HISTOGRAM_SIZE_PER_FRAME_PER_SLICE *         // Ace histogram size per slice
        VP_NUM_FRAME_PREVIOUS_CURRENT *                                 // Ace for Prev + Curr
        VP_VEBOX_HISTOGRAM_SLICES_COUNT;                                // Total number of slices
    return dwSize;
}

MOS_STATUS VpResourceManager::GetResourceHint(std::vector<FeatureType> &featurePool, SwFilterPipe& executedFilters, RESOURCE_ASSIGNMENT_HINT &hint)
{
    uint32_t    index      = 0;
    SwFilterSubPipe *inputPipe = executedFilters.GetSwFilterPrimaryPipe(index);

    // only process Primary surface
    VP_PUBLIC_CHK_NULL_RETURN(inputPipe);
    for (auto filterID : featurePool)
    {
        SwFilter* feature = (SwFilter*)inputPipe->GetSwFilter(FeatureType(filterID));
        if (feature)
        {
            VP_PUBLIC_CHK_STATUS_RETURN(feature->SetResourceAssignmentHint(hint));
        }
    }
    return MOS_STATUS_SUCCESS;
}

struct VP_SURFACE_PARAMS
{
    uint32_t                width;
    uint32_t                height;
    MOS_FORMAT              format;
    MOS_TILE_TYPE           tileType;
    MOS_RESOURCE_MMC_MODE   surfCompressionMode = MOS_MMC_DISABLED;
    bool                    surfCompressible   = false;
    VPHAL_CSPACE            colorSpace;
    RECT                    rcSrc;              //!< Source rectangle
    RECT                    rcDst;              //!< Destination rectangle
    RECT                    rcMaxSrc;           //!< Max source rectangle
    VPHAL_SAMPLE_TYPE       sampleType;
};

MOS_STATUS VpResourceManager::GetIntermediaOutputSurfaceParams(VP_SURFACE_PARAMS &params, SwFilterPipe &executedFilters)
{
    SwFilterCsc *csc = dynamic_cast<SwFilterCsc *>(executedFilters.GetSwFilter(true, 0, FeatureType::FeatureTypeCsc));
    SwFilterScaling *scaling = dynamic_cast<SwFilterScaling *>(executedFilters.GetSwFilter(true, 0, FeatureType::FeatureTypeScaling));
    SwFilterRotMir *rotMir = dynamic_cast<SwFilterRotMir *>(executedFilters.GetSwFilter(true, 0, FeatureType::FeatureTypeRotMir));
    SwFilterDeinterlace *di = dynamic_cast<SwFilterDeinterlace *>(executedFilters.GetSwFilter(true, 0, FeatureType::FeatureTypeDi));
    VP_SURFACE *inputSurface = executedFilters.GetSurface(true, 0);

    VP_PUBLIC_CHK_NULL_RETURN(inputSurface);

    if (scaling)
    {
        params.width = scaling->GetSwFilterParams().output.dwWidth;
        params.height = scaling->GetSwFilterParams().output.dwHeight;
        params.sampleType = scaling->GetSwFilterParams().output.sampleType;
        params.rcSrc = scaling->GetSwFilterParams().output.rcSrc;
        params.rcDst = scaling->GetSwFilterParams().output.rcDst;
        params.rcMaxSrc = scaling->GetSwFilterParams().output.rcMaxSrc;
    }
    else
    {
        params.width = inputSurface->osSurface->dwWidth;
        params.height = inputSurface->osSurface->dwHeight;
        params.sampleType = di ? SAMPLE_PROGRESSIVE : inputSurface->SampleType;
        params.rcSrc = inputSurface->rcSrc;
        params.rcDst = inputSurface->rcDst;
        params.rcMaxSrc = inputSurface->rcMaxSrc;
    }

    // Do not use rotatoin flag in scaling swfilter as it has not been initialized here.
    // It will be initialized during pipe update after resource being assigned.
    if (rotMir &&
        (rotMir->GetSwFilterParams().rotation == VPHAL_ROTATION_90 ||
        rotMir->GetSwFilterParams().rotation == VPHAL_ROTATION_270 ||
        rotMir->GetSwFilterParams().rotation == VPHAL_ROTATE_90_MIRROR_VERTICAL ||
        rotMir->GetSwFilterParams().rotation == VPHAL_ROTATE_90_MIRROR_HORIZONTAL))
    {
        swap(params.width, params.height);
        RECT tmp = params.rcSrc;
        RECT_ROTATE(params.rcSrc, tmp);
        tmp = params.rcDst;
        RECT_ROTATE(params.rcDst, tmp);
        tmp = params.rcMaxSrc;
        RECT_ROTATE(params.rcMaxSrc, tmp);
    }

    if (csc)
    {
        params.format = csc->GetSwFilterParams().formatOutput;
        params.colorSpace = csc->GetSwFilterParams().output.colorSpace;
    }
    else
    {
        params.format = inputSurface->osSurface->Format;
        params.colorSpace = inputSurface->ColorSpace;
    }
    params.tileType = MOS_TILE_Y;
    params.surfCompressionMode = MOS_MMC_DISABLED;
    params.surfCompressible   = false;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpResourceManager::AssignIntermediaSurface(SwFilterPipe &executedFilters)
{
    VP_SURFACE* outputSurface = executedFilters.GetSurface(false, 0);
    if (outputSurface)
    {
        // No need intermedia surface.
        return MOS_STATUS_SUCCESS;
    }

    while (m_currentPipeIndex >= m_intermediaSurfaces.size())
    {
        m_intermediaSurfaces.push_back(nullptr);
    }
    VP_SURFACE_PARAMS params = {};
    bool allocated = false;
    // Get surface parameter.
    GetIntermediaOutputSurfaceParams(params, executedFilters);

    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
        m_intermediaSurfaces[m_currentPipeIndex],
        "IntermediaSurface",
        params.format,
        MOS_GFXRES_2D,
        params.tileType,
        params.width,
        params.height,
        params.surfCompressible,
        params.surfCompressionMode,
        allocated,
        false,
        IsDeferredResourceDestroyNeeded(),
        MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_RENDER));

    VP_PUBLIC_CHK_NULL_RETURN(m_intermediaSurfaces[m_currentPipeIndex]);

    m_intermediaSurfaces[m_currentPipeIndex]->ColorSpace = params.colorSpace;
    m_intermediaSurfaces[m_currentPipeIndex]->rcDst      = params.rcDst;
    m_intermediaSurfaces[m_currentPipeIndex]->rcSrc      = params.rcSrc;
    m_intermediaSurfaces[m_currentPipeIndex]->rcMaxSrc   = params.rcMaxSrc;
    m_intermediaSurfaces[m_currentPipeIndex]->SampleType = params.sampleType;

    VP_SURFACE *output = m_allocator.AllocateVpSurface(*m_intermediaSurfaces[m_currentPipeIndex]);
    VP_PUBLIC_CHK_NULL_RETURN(output);

    executedFilters.AddSurface(output, false, 0);

    return MOS_STATUS_SUCCESS;
}

VP_SURFACE * VpResourceManager::GetCopyInstOfExtSurface(VP_SURFACE* surf)
{
    VP_FUNC_CALL();

    if (nullptr == surf || 0 == surf->GetAllocationHandle())
    {
        return nullptr;
    }
    auto it = m_tempSurface.find(surf->GetAllocationHandle());
    if (it != m_tempSurface.end())
    {
        return it->second;
    }
    VP_SURFACE *surface = m_allocator.AllocateVpSurface(*surf);
    if (surface)
    {
        m_tempSurface.insert(make_pair(surf->GetAllocationHandle(), surface));
    }
    else
    {
        VP_PUBLIC_ASSERTMESSAGE("Allocate temp surface faild!");
    }
    return surface;
}

MOS_STATUS VpResourceManager::AssignExecuteResource(std::vector<FeatureType> &featurePool, VP_EXECUTE_CAPS& caps, SwFilterPipe &executedFilters)
{
    VP_FUNC_CALL();

    VP_SURFACE                  *inputSurface   = GetCopyInstOfExtSurface(executedFilters.GetSurface(true, 0));
    VP_SURFACE                  *outputSurface  = GetCopyInstOfExtSurface(executedFilters.GetSurface(false, 0));
    VP_SURFACE                  *pastSurface    = GetCopyInstOfExtSurface(executedFilters.GetPastSurface(0));
    VP_SURFACE                  *futureSurface  = GetCopyInstOfExtSurface(executedFilters.GetFutureSurface(0));

    RESOURCE_ASSIGNMENT_HINT    resHint         = {};

    VP_PUBLIC_CHK_STATUS_RETURN(GetResourceHint(featurePool, executedFilters, resHint));

    if (nullptr == outputSurface)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(AssignIntermediaSurface(executedFilters));
        outputSurface  = executedFilters.GetSurface(false, 0);
        VP_PUBLIC_CHK_NULL_RETURN(outputSurface);
    }

    VP_PUBLIC_CHK_STATUS_RETURN(AssignExecuteResource(caps, inputSurface, outputSurface,
        pastSurface, futureSurface, resHint, executedFilters.GetSurfacesSetting()));
    ++m_currentPipeIndex;
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpResourceManager::AssignExecuteResource(VP_EXECUTE_CAPS& caps, VP_SURFACE *inputSurface, VP_SURFACE *outputSurface,
    VP_SURFACE *pastSurface, VP_SURFACE *futureSurface, RESOURCE_ASSIGNMENT_HINT resHint, VP_SURFACE_SETTING &surfSetting)
{
    surfSetting.Clean();

    if (caps.bVebox || caps.bDnKernelUpdate)
    {
        // Create Vebox Resources
        VP_PUBLIC_CHK_STATUS_RETURN(AssignVeboxResource(caps, inputSurface, outputSurface, pastSurface, futureSurface, resHint, surfSetting));
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS GetVeboxOutputParams(VP_EXECUTE_CAPS &executeCaps, MOS_FORMAT inputFormat, MOS_TILE_TYPE inputTileType, MOS_FORMAT outputFormat,
                                MOS_FORMAT &veboxOutputFormat, MOS_TILE_TYPE &veboxOutputTileType)
{
    // Vebox Chroma Co-Sited downsampleing is part of VEO. It only affects format of vebox output surface, but not
    // affect sfc input format, that's why different logic between GetSfcInputFormat and GetVeboxOutputParams.
    // Check DI first and downsampling to NV12 if possible to save bandwidth no matter IECP enabled or not.
    if (executeCaps.bDI || executeCaps.bDiProcess2ndField)
    {
        // NV12 will be used if target output is not YUV2 to save bandwidth.
        if (outputFormat == Format_YUY2)
        {
            veboxOutputFormat = Format_YUY2;
        }
        else
        {
            veboxOutputFormat = Format_NV12;
        }
        veboxOutputTileType = MOS_TILE_Y;
    }
    else if (executeCaps.bIECP)
    {
        // Upsampling to yuv444 for IECP input/output.
        // To align with legacy path, need to check whether inputFormat can also be used for IECP case,
        // in which case IECP down sampling will be applied.
        veboxOutputFormat = Format_AYUV;
        veboxOutputTileType = inputTileType;
    }
    else
    {
        veboxOutputFormat = inputFormat;
        veboxOutputTileType = inputTileType;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_FORMAT GetSfcInputFormat(VP_EXECUTE_CAPS &executeCaps, MOS_FORMAT inputFormat, VPHAL_CSPACE colorSpaceOutput)
{
    // Vebox Chroma Co-Sited downsampling is part of VEO. It only affects format of vebox output surface, but not
    // affect sfc input format, that's why different logic between GetSfcInputFormat and GetVeboxOutputParams.
    // Check IECP first here, since IECP is done after DI, and the vebox downsampling not affect the vebox input.
    if (executeCaps.bIECP)
    {
        // Upsampling to yuv444 for IECP input/output.
        // To align with legacy path, need to check whether inputFormat can also be used for IECP case,
        // in which case IECP down sampling will be applied.
        return Format_AYUV;
    }
    else if (executeCaps.bHDR3DLUT)
    {
        return IS_COLOR_SPACE_BT2020(colorSpaceOutput) ? Format_R10G10B10A2 : Format_A8B8G8R8;
    }
    else if (executeCaps.bDI)
    {
        // If the input is 4:2:0, then chroma data is doubled vertically to 4:2:2
        // For executeCaps.bDiProcess2ndField, no DI enabled in vebox, so no need
        // set to YUY2 here.
        return Format_YUY2;
    }

    return inputFormat;
}

MOS_STATUS VpResourceManager::ReAllocateVeboxOutputSurface(VP_EXECUTE_CAPS& caps, VP_SURFACE *inputSurface, VP_SURFACE *outputSurface,  bool &allocated)
{
    MOS_RESOURCE_MMC_MODE           surfCompressionMode = MOS_MMC_DISABLED;
    bool                            bSurfCompressible   = false;
    uint32_t                        i                   = 0;

    VP_PUBLIC_CHK_NULL_RETURN(inputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface->osSurface);
    VP_PUBLIC_CHK_NULL_RETURN(outputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(outputSurface->osSurface);

    MOS_FORMAT      veboxOutputFormat                   = inputSurface->osSurface->Format;
    MOS_TILE_TYPE   veboxOutputTileType                 = inputSurface->osSurface->TileType;

    VP_PUBLIC_CHK_STATUS_RETURN(GetVeboxOutputParams(caps, inputSurface->osSurface->Format, inputSurface->osSurface->TileType,
                                            outputSurface->osSurface->Format, veboxOutputFormat, veboxOutputTileType));

    allocated = false;
    if (IS_VP_VEBOX_DN_ONLY(caps))
    {
        bSurfCompressible = inputSurface->osSurface->bCompressible;
        surfCompressionMode = inputSurface->osSurface->CompressionMode;
    }
    else
    {
        bSurfCompressible = true;
        surfCompressionMode = MOS_MMC_MC;
    }

    if (m_currentFrameIds.pastFrameAvailable && m_currentFrameIds.futureFrameAvailable)
    {
        // Not switch back to 2 after being set to 4.
        m_veboxOutputCount = 4;
    }

    for (i = 0; i < m_veboxOutputCount; i++)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
            m_veboxOutput[i],
            "VeboxSurfaceOutput",
            veboxOutputFormat,
            MOS_GFXRES_2D,
            veboxOutputTileType,
            inputSurface->osSurface->dwWidth,
            inputSurface->osSurface->dwHeight,
            bSurfCompressible,
            surfCompressionMode,
            allocated,
            false,
            IsDeferredResourceDestroyNeeded(),
            MOS_HW_RESOURCE_USAGE_VP_OUTPUT_PICTURE_FF));

        m_veboxOutput[i]->ColorSpace = inputSurface->ColorSpace;
        m_veboxOutput[i]->rcDst      = inputSurface->rcDst;
        m_veboxOutput[i]->rcSrc      = inputSurface->rcSrc;
        m_veboxOutput[i]->rcMaxSrc   = inputSurface->rcMaxSrc;

        m_veboxOutput[i]->SampleType = SAMPLE_PROGRESSIVE;
    }

    if (allocated)
    {
        // Report Compress Status
        if (m_veboxOutput[0]->osSurface)
        {
            m_reporting.FFDICompressible = m_veboxOutput[0]->osSurface->bIsCompressed;
            m_reporting.FFDICompressMode = (uint8_t)m_veboxOutput[0]->osSurface->CompressionMode;
        }
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpResourceManager::ReAllocateVeboxDenoiseOutputSurface(VP_EXECUTE_CAPS& caps, VP_SURFACE *inputSurface, bool &allocated)
{
    MOS_RESOURCE_MMC_MODE           surfCompressionMode = MOS_MMC_DISABLED;
    bool                            bSurfCompressible   = false;
    MOS_TILE_MODE_GMM               tileModeByForce     = MOS_TILE_UNSET_GMM;
    auto *                          pSkuTable            = m_osInterface.pfnGetSkuTable(&m_osInterface);

    VP_PUBLIC_CHK_NULL_RETURN(inputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface->osSurface);
    VP_PUBLIC_CHK_NULL_RETURN(pSkuTable);

    if (MEDIA_IS_SKU(pSkuTable, FtrMediaTile64))
    {
        tileModeByForce = MOS_TILE_64_GMM;
    }

    allocated = false;
    if (IS_VP_VEBOX_DN_ONLY(caps))
    {
        bSurfCompressible = inputSurface->osSurface->bCompressible;
        surfCompressionMode = inputSurface->osSurface->CompressionMode;
    }
    else
    {
        bSurfCompressible = true;
        surfCompressionMode = MOS_MMC_MC;
    }

    for (uint32_t i = 0; i < VP_NUM_DN_SURFACES; i++)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
            m_veboxDenoiseOutput[i],
            "VeboxFFDNSurface",
            inputSurface->osSurface->Format,
            MOS_GFXRES_2D,
            inputSurface->osSurface->TileType,
            inputSurface->osSurface->dwWidth,
            inputSurface->osSurface->dwHeight,
            bSurfCompressible,
            surfCompressionMode,
            allocated,
            false,
            IsDeferredResourceDestroyNeeded(),
            MOS_HW_RESOURCE_USAGE_VP_INPUT_REFERENCE_FF,
            tileModeByForce));

        // if allocated, pVeboxState->PastSurface is not valid for DN reference.
        if (allocated)
        {
            // If DI is enabled, try to use app's reference if provided
            if (caps.bRefValid && caps.bDI)
            {
                //CopySurfaceValue(pVeboxState->m_previousSurface, pVeboxState->m_currentSurface->pBwdRef);
            }
            else
            {
                caps.bRefValid = false;
            }
            // Report Compress Status
            if (m_veboxDenoiseOutput[i]->osSurface)
            {
                m_reporting.FFDNCompressible = m_veboxDenoiseOutput[i]->osSurface->bIsCompressed;
                m_reporting.FFDNCompressMode = (uint8_t)m_veboxDenoiseOutput[i]->osSurface->CompressionMode;
            }
        }
        else
        {
            caps.bRefValid = true;
        }

        // DN's output format should be same to input
        m_veboxDenoiseOutput[i]->SampleType =
            inputSurface->SampleType;

        // Set Colorspace of FFDN
        m_veboxDenoiseOutput[i]->ColorSpace = inputSurface->ColorSpace;

        // Copy FrameID and parameters, as DN output will be used as next blt's current
        m_veboxDenoiseOutput[i]->FrameID = inputSurface->FrameID;

        // Place Holder to update report for debug purpose
    }
    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Vebox initialize STMM History
//! \details  Initialize STMM History surface
//! Description:
//!   This function is used by VEBox for initializing
//!   the STMM surface.  The STMM / Denoise history is a custom surface used 
//!   for both input and output. Each cache line contains data for 4 4x4s. 
//!   The STMM for each 4x4 is 8 bytes, while the denoise history is 1 byte 
//!   and the chroma denoise history is 1 byte for each U and V.
//!   Byte    Data\n
//!   0       STMM for 2 luma values at luma Y=0, X=0 to 1\n
//!   1       STMM for 2 luma values at luma Y=0, X=2 to 3\n
//!   2       Luma Denoise History for 4x4 at 0,0\n
//!   3       Not Used\n
//!   4-5     STMM for luma from X=4 to 7\n
//!   6       Luma Denoise History for 4x4 at 0,4\n
//!   7       Not Used\n
//!   8-15    Repeat for 4x4s at 0,8 and 0,12\n
//!   16      STMM for 2 luma values at luma Y=1,X=0 to 1\n
//!   17      STMM for 2 luma values at luma Y=1, X=2 to 3\n
//!   18      U Chroma Denoise History\n
//!   19      Not Used\n
//!   20-31   Repeat for 3 4x4s at 1,4, 1,8 and 1,12\n
//!   32      STMM for 2 luma values at luma Y=2,X=0 to 1\n
//!   33      STMM for 2 luma values at luma Y=2, X=2 to 3\n
//!   34      V Chroma Denoise History\n
//!   35      Not Used\n
//!   36-47   Repeat for 3 4x4s at 2,4, 2,8 and 2,12\n
//!   48      STMM for 2 luma values at luma Y=3,X=0 to 1\n
//!   49      STMM for 2 luma values at luma Y=3, X=2 to 3\n
//!   50-51   Not Used\n
//!   36-47   Repeat for 3 4x4s at 3,4, 3,8 and 3,12\n
//! \param    [in] iSurfaceIndex
//!           Index of STMM surface array
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS VpResourceManager::VeboxInitSTMMHistory(MOS_SURFACE *stmmSurface)
{
    uint32_t            dwSize = 0;
    int32_t             x = 0, y = 0;
    uint8_t*            pByte = nullptr;
    MOS_LOCK_PARAMS     LockFlags;

    VP_PUBLIC_CHK_NULL_RETURN(stmmSurface);
    MOS_ZeroMemory(&LockFlags, sizeof(MOS_LOCK_PARAMS));

    LockFlags.WriteOnly = 1;
    LockFlags.TiledAsTiled = 1; // Set TiledAsTiled flag for STMM surface initialization.

    // Lock the surface for writing
    pByte = (uint8_t*)m_allocator.Lock(
        &stmmSurface->OsResource,
        &LockFlags);
    VP_PUBLIC_CHK_NULL_RETURN(pByte);

    dwSize = stmmSurface->dwWidth >> 2;

    // Fill STMM surface with DN history init values.
    for (y = 0; y < (int32_t)stmmSurface->dwHeight; y++)
    {
        for (x = 0; x < (int32_t)dwSize; x++)
        {
            MOS_FillMemory(pByte, 2, DNDI_HISTORY_INITVALUE);
            // skip denosie history init.
            pByte += 4;
        }

        pByte += stmmSurface->dwPitch - stmmSurface->dwWidth;
    }

    // Unlock the surface
    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.UnLock(&stmmSurface->OsResource));
    return MOS_STATUS_SUCCESS;
}

// Allocate STMM (Spatial-Temporal Motion Measure) Surfaces
MOS_STATUS VpResourceManager::ReAllocateVeboxSTMMSurface(VP_EXECUTE_CAPS& caps, VP_SURFACE *inputSurface, bool &allocated)
{
    MOS_RESOURCE_MMC_MODE           surfCompressionMode = MOS_MMC_DISABLED;
    bool                            bSurfCompressible   = false;
    uint32_t                        i                   = 0;
    MOS_TILE_MODE_GMM               tileModeByForce     = MOS_TILE_UNSET_GMM;
    auto *                          pSkuTable            = m_osInterface.pfnGetSkuTable(&m_osInterface);

    VP_PUBLIC_CHK_NULL_RETURN(inputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface->osSurface);
    VP_PUBLIC_CHK_NULL_RETURN(pSkuTable);

    if (MEDIA_IS_SKU(pSkuTable, FtrMediaTile64))
    {
        tileModeByForce = MOS_TILE_64_GMM;
    }

    allocated = false;
    for (i = 0; i < VP_NUM_STMM_SURFACES; i++)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
            m_veboxSTMMSurface[i],
            "VeboxSTMMSurface",
            Format_STMM,
            MOS_GFXRES_2D,
            MOS_TILE_Y,
            inputSurface->osSurface->dwWidth,
            inputSurface->osSurface->dwHeight,
            bSurfCompressible,
            surfCompressionMode,
            allocated,
            false,
            IsDeferredResourceDestroyNeeded(),
            MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_FF,
            tileModeByForce));

        if (allocated)
        {
            VP_PUBLIC_CHK_NULL_RETURN(m_veboxSTMMSurface[i]);
            VP_PUBLIC_CHK_STATUS_RETURN(VeboxInitSTMMHistory(m_veboxSTMMSurface[i]->osSurface));
            // Report Compress Status
            m_reporting.STMMCompressible = bSurfCompressible;
            m_reporting.STMMCompressMode = (uint8_t)surfCompressionMode;
        }
    }
    return MOS_STATUS_SUCCESS;
}

void VpResourceManager::DestoryVeboxOutputSurface()
{
    for (uint32_t i = 0; i < VP_MAX_NUM_VEBOX_SURFACES; i++)
    {
        m_allocator.DestroyVpSurface(m_veboxOutput[i], IsDeferredResourceDestroyNeeded());
    }
}

void VpResourceManager::DestoryVeboxDenoiseOutputSurface()
{
    for (uint32_t i = 0; i < VP_NUM_DN_SURFACES; i++)
    {
        m_allocator.DestroyVpSurface(m_veboxDenoiseOutput[i], IsDeferredResourceDestroyNeeded());
    }
}

void VpResourceManager::DestoryVeboxSTMMSurface()
{
    // Free DI history buffers (STMM = Spatial-temporal motion measure)
    for (uint32_t i = 0; i < VP_NUM_STMM_SURFACES; i++)
    {
        m_allocator.DestroyVpSurface(m_veboxSTMMSurface[i], IsDeferredResourceDestroyNeeded());
    }
}

uint32_t VpResourceManager::Get3DLutSize()
{
    return VP_VEBOX_HDR_3DLUT65;
}

MOS_STATUS VpResourceManager::AllocateVeboxResource(VP_EXECUTE_CAPS& caps, VP_SURFACE *inputSurface, VP_SURFACE *outputSurface)
{
    VP_FUNC_CALL();
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface->osSurface);
    VP_PUBLIC_CHK_NULL_RETURN(outputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(outputSurface->osSurface);

    MOS_FORMAT                      format;
    MOS_TILE_TYPE                   TileType;
    uint32_t                        dwWidth;
    uint32_t                        dwHeight;
    uint32_t                        dwSize;
    uint32_t                        i;
    MOS_RESOURCE_MMC_MODE           surfCompressionMode = MOS_MMC_DISABLED;
    bool                            bSurfCompressible   = false;
    bool                            bAllocated          = false;
    uint8_t                         InitValue           = 0;

    // change the init value when null hw is enabled
    if (NullHW::IsEnabled())
    {
        InitValue = 0x80;
    }

    if (IS_VP_VEBOX_DN_ONLY(caps))
    {
        bSurfCompressible = inputSurface->osSurface->bCompressible;
        surfCompressionMode = inputSurface->osSurface->CompressionMode;
    }
    else
    {
        bSurfCompressible = true;
        surfCompressionMode = MOS_MMC_MC;
    }

    // Decide DN output surface
    if (VeboxOutputNeeded(caps))
    {
        VP_PUBLIC_CHK_STATUS_RETURN(ReAllocateVeboxOutputSurface(caps, inputSurface, outputSurface, bAllocated));
    }
    else
    {
        DestoryVeboxOutputSurface();
    }

    if (VeboxDenoiseOutputNeeded(caps))
    {
        VP_PUBLIC_CHK_STATUS_RETURN(ReAllocateVeboxDenoiseOutputSurface(caps, inputSurface, bAllocated));
        if (bAllocated)
        {
            m_currentDnOutput = 0;
            m_pastDnOutputValid = false;
        }
    }
    else
    {
        DestoryVeboxDenoiseOutputSurface();
        m_pastDnOutputValid = false;
    }

    if (VeboxSTMMNeeded(caps, false))
    {
        VP_PUBLIC_CHK_STATUS_RETURN(ReAllocateVeboxSTMMSurface(caps, inputSurface, bAllocated));
        if (bAllocated)
        {
            m_currentStmmIndex = 0;
        }
    }
    else
    {
        DestoryVeboxSTMMSurface();
    }

#if VEBOX_AUTO_DENOISE_SUPPORTED
    // Allocate Temp Surface for Vebox Update kernels----------------------------------------
    // the surface size is one Page
    dwSize = MHW_PAGE_SIZE;
    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
        m_veboxDNTempSurface,
        "VeboxDNTempSurface",
        Format_Buffer,
        MOS_GFXRES_BUFFER,
        MOS_TILE_LINEAR,
        dwSize,
        1,
        false,
        MOS_MMC_DISABLED,
        bAllocated,
        true,
        IsDeferredResourceDestroyNeeded(),
        MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_FF));

    // Allocate Spatial Attributes Configuration Surface for DN kernel Gen9+-----------
    dwSize = MHW_PAGE_SIZE;
    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
        m_veboxDNSpatialConfigSurface,
        "VeboxSpatialAttributesConfigurationSurface",
        Format_RAW,
        MOS_GFXRES_BUFFER,
        MOS_TILE_LINEAR,
        dwSize,
        1,
        false,
        MOS_MMC_DISABLED,
        bAllocated,
        false,
        IsDeferredResourceDestroyNeeded(),
        MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_FF));

    if (bAllocated)
    {
        // initialize Spatial Attributes Configuration Surface
        VP_PUBLIC_CHK_STATUS_RETURN(InitVeboxSpatialAttributesConfiguration());
    }

#endif

    dwSize = GetHistogramSurfaceSize(caps, inputSurface->osSurface->dwWidth, inputSurface->osSurface->dwHeight);

    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
        m_veboxRgbHistogram,
        "VeboxLaceAceRgbHistogram",
        Format_Buffer,
        MOS_GFXRES_BUFFER,
        MOS_TILE_LINEAR,
        dwSize,
        1,
        false,
        MOS_MMC_DISABLED,
        bAllocated,
        false,
        IsDeferredResourceDestroyNeeded(),
        MOS_HW_RESOURCE_USAGE_VP_INTERNAL_WRITE_FF));

    m_isHistogramReallocated = bAllocated;

    if (bAllocated && NullHW::IsEnabled())
    {
        // Initialize veboxRgbHistogram Surface
        VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.OsFillResource(
            &(m_veboxRgbHistogram->osSurface->OsResource),
            dwSize,
            InitValue));
    }

    // Allocate Statistics State Surface----------------------------------------
    // Width to be a aligned on 64 bytes and height is 1/4 the height
    // Per frame information written twice per frame for 2 slices
    // Surface to be a rectangle aligned with dwWidth to get proper dwSize
    // APG PAth need to make sure input surface width/height is what to processed width/Height
    dwWidth = MOS_ALIGN_CEIL(inputSurface->osSurface->dwWidth, 64);
    dwHeight = MOS_ROUNDUP_DIVIDE(inputSurface->osSurface->dwHeight, 4) +
        MOS_ROUNDUP_DIVIDE(VP_VEBOX_STATISTICS_SIZE * sizeof(uint32_t), dwWidth);

    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
        m_veboxStatisticsSurface,
        "VeboxStatisticsSurface",
        Format_Buffer,
        MOS_GFXRES_BUFFER,
        MOS_TILE_LINEAR,
        dwWidth,
        dwHeight,
        false,
        MOS_MMC_DISABLED,
        bAllocated,
        true,
        IsDeferredResourceDestroyNeeded(),
        MOS_HW_RESOURCE_USAGE_VP_INTERNAL_WRITE_FF));

    if (bAllocated && NullHW::IsEnabled())
    {
        // Initialize veboxStatisticsSurface Surface
        VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.OsFillResource(
            &(m_veboxStatisticsSurface->osSurface->OsResource),
            dwSize,
            InitValue));
    }

    if (caps.bHDR3DLUT)
    {
        // HDR
        dwSize = Get3DLutSize();
        VP_PUBLIC_CHK_STATUS_RETURN(m_allocator.ReAllocateSurface(
            m_vebox3DLookUpTables,
            "Vebox3DLutTableSurface",
            Format_Buffer,
            MOS_GFXRES_BUFFER,
            MOS_TILE_LINEAR,
            dwSize,
            1,
            false,
            MOS_MMC_DISABLED,
            bAllocated,
            false,
            IsDeferredResourceDestroyNeeded()));
    }
    // cappipe

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpResourceManager::AssignSurface(VP_EXECUTE_CAPS caps, VEBOX_SURFACE_ID &surfaceId, SurfaceType surfaceType, VP_SURFACE *inputSurface, VP_SURFACE *outputSurface, VP_SURFACE *pastSurface, VP_SURFACE *futureSurface, VP_SURFACE_GROUP &surfGroup)
{
    switch (surfaceId)
    {
    case VEBOX_SURFACE_INPUT:
        surfGroup.insert(std::make_pair(surfaceType, inputSurface));
        break;
    case VEBOX_SURFACE_OUTPUT:
        surfGroup.insert(std::make_pair(surfaceType, outputSurface));
        break;
    case VEBOX_SURFACE_PAST_REF:
        if (caps.bDN && m_pastDnOutputValid)
        {
            surfGroup.insert(std::make_pair(surfaceType, m_veboxDenoiseOutput[(m_currentDnOutput + 1) & 1]));
        }
        else
        {
            surfGroup.insert(std::make_pair(surfaceType, pastSurface));
        }
        break;
    case VEBOX_SURFACE_FUTURE_REF:
        surfGroup.insert(std::make_pair(surfaceType, futureSurface));
        break;
    case VEBOX_SURFACE_FRAME0:
        surfGroup.insert(std::make_pair(surfaceType, m_veboxOutput[(m_currentDnOutput + 0) % m_veboxOutputCount]));
        break;
    case VEBOX_SURFACE_FRAME1:
        surfGroup.insert(std::make_pair(surfaceType, m_veboxOutput[(m_currentDnOutput + 1) % m_veboxOutputCount]));
        break;
    case VEBOX_SURFACE_FRAME2:
        surfGroup.insert(std::make_pair(surfaceType, m_veboxOutput[(m_currentDnOutput + 2) % m_veboxOutputCount]));
        break;
    case VEBOX_SURFACE_FRAME3:
        surfGroup.insert(std::make_pair(surfaceType, m_veboxOutput[(m_currentDnOutput + 3) % m_veboxOutputCount]));
        break;
    default:
        break;
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpResourceManager::AssignVeboxResource(VP_EXECUTE_CAPS& caps, VP_SURFACE *inputSurface, VP_SURFACE *outputSurface,
    VP_SURFACE *pastSurface, VP_SURFACE *futureSurface, RESOURCE_ASSIGNMENT_HINT resHint, VP_SURFACE_SETTING &surfSetting)
{
    VP_FUNC_CALL();
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(inputSurface->osSurface);
    VP_PUBLIC_CHK_NULL_RETURN(outputSurface);
    VP_PUBLIC_CHK_NULL_RETURN(outputSurface->osSurface);

    MOS_FORMAT                      format;
    MOS_TILE_TYPE                   TileType;
    uint32_t                        dwWidth;
    uint32_t                        dwHeight;
    uint32_t                        dwSize;
    uint32_t                        i;
    auto&                           surfGroup = surfSetting.surfGroup;

    // Render case reuse vebox resource, and don`t need re-allocate.
    if (!caps.bRender)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(AllocateVeboxResource(caps, inputSurface, outputSurface));
    }

    if (caps.bDI || caps.bDiProcess2ndField)
    {
        bool b60fpsDi = resHint.b60fpsDi || caps.bDiProcess2ndField;
        VEBOX_SURFACES_CONFIG cfg(b60fpsDi, caps.bSFC, m_sameSamples, m_outOfBound, m_currentFrameIds.pastFrameAvailable,
            m_currentFrameIds.futureFrameAvailable, IsInterleaveFirstField(inputSurface->SampleType));
        auto it = m_veboxSurfaceConfigMap.find(cfg.value);
        if (m_veboxSurfaceConfigMap.end() == it)
        {
            VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_INVALID_PARAMETER);
        }
        auto surfaces = it->second;
        VP_PUBLIC_CHK_STATUS_RETURN(AssignSurface(caps, surfaces.currentInputSurface, SurfaceTypeVeboxInput, inputSurface, outputSurface, pastSurface, futureSurface, surfSetting.surfGroup));
        VP_PUBLIC_CHK_STATUS_RETURN(AssignSurface(caps, surfaces.pastInputSurface, SurfaceTypeVeboxPreviousInput, inputSurface, outputSurface, pastSurface, futureSurface, surfSetting.surfGroup));
        VP_PUBLIC_CHK_STATUS_RETURN(AssignSurface(caps, surfaces.currentOutputSurface, SurfaceTypeVeboxCurrentOutput, inputSurface, outputSurface, pastSurface, futureSurface, surfSetting.surfGroup));
        VP_PUBLIC_CHK_STATUS_RETURN(AssignSurface(caps, surfaces.pastOutputSurface, SurfaceTypeVeboxPreviousOutput, inputSurface, outputSurface, pastSurface, futureSurface, surfSetting.surfGroup));

        if (caps.bDN)
        {
            // Insert DN output surface
            surfGroup.insert(std::make_pair(SurfaceTypeDNOutput, m_veboxDenoiseOutput[m_currentDnOutput]));
        }

        caps.bRefValid = surfGroup.find(SurfaceTypeVeboxPreviousInput) != surfGroup.end();
    }
    else
    {
        surfGroup.insert(std::make_pair(SurfaceTypeVeboxInput, inputSurface));
        surfGroup.insert(std::make_pair(SurfaceTypeVeboxCurrentOutput, GetVeboxOutputSurface(caps, outputSurface)));

        if (caps.bDN)
        {
            // Insert DN output surface
            surfGroup.insert(std::make_pair(SurfaceTypeDNOutput, m_veboxDenoiseOutput[m_currentDnOutput]));
            // Insert DN Reference surface
            if (caps.bRefValid)
            {
                surfGroup.insert(std::make_pair(SurfaceTypeVeboxPreviousInput, m_veboxDenoiseOutput[(m_currentDnOutput + 1) & 1]));
            }
        }
    }

    if (VeboxSTMMNeeded(caps, true))
    {
        // Insert STMM input surface
        surfGroup.insert(std::make_pair(SurfaceTypeSTMMIn, m_veboxSTMMSurface[m_currentStmmIndex]));
        // Insert STMM output surface
        surfGroup.insert(std::make_pair(SurfaceTypeSTMMOut, m_veboxSTMMSurface[(m_currentStmmIndex + 1) & 1]));
    }

#if VEBOX_AUTO_DENOISE_SUPPORTED
    // Insert Vebox auto DN noise level surface
    surfGroup.insert(std::make_pair(SurfaceTypeAutoDNNoiseLevel, m_veboxDNTempSurface));
    // Insert Vebox auto DN spatial config surface/buffer
    surfGroup.insert(std::make_pair(SurfaceTypeAutoDNSpatialConfig, m_veboxDNSpatialConfigSurface));
#endif

    // Insert Vebox histogram surface
    surfGroup.insert(std::make_pair(SurfaceTypeLaceAceRGBHistogram, m_veboxRgbHistogram));

    // Insert Vebox statistics surface
    surfGroup.insert(std::make_pair(SurfaceTypeStatistics, m_veboxStatisticsSurface));

    // Insert Vebox 3Dlut surface
    surfGroup.insert(std::make_pair(SurfaceType3dLut, m_vebox3DLookUpTables));

    // Update previous Dn output flag for next frame to use.
    if (surfGroup.find(SurfaceTypeDNOutput) != surfGroup.end() || m_sameSamples && m_pastDnOutputValid)
    {
        m_pastDnOutputValid = true;
    }
    else
    {
        m_pastDnOutputValid = false;
    }

    return MOS_STATUS_SUCCESS;
}

VP_SURFACE* VpResourceManager::GetVeboxOutputSurface(VP_EXECUTE_CAPS& caps, VP_SURFACE *outputSurface)
{
    if (caps.bRender)
    {
        // Place Holder when enable DI
        return nullptr;
    }

    if (!caps.bSFC) // Vebox output directlly to output surface
    {
        // RenderTarget will be assigned in VpVeboxCmdPacket::GetSurface.
        return nullptr;
    }
    else if (caps.bDI && caps.bVebox) // Vebox DI enable
    {
        // Place Holder when enable DI
        return nullptr;
    }
    else if (caps.bIECP) // SFC + IECP enabled, output to internal surface
    {
        return m_veboxOutput[m_currentDnOutput];
    }
    else if (caps.bDN) // SFC + DN case
    {
        // DN + SFC scenario needs IECP implicitly, which need vebox output surface being assigned.
        // Use m_currentDnOutput to ensure m_veboxOutput surface paired with DN output surface.
        return m_veboxOutput[m_currentDnOutput];
    }
    else
    {
        // Write to SFC cases, Vebox output is not needed.
        VP_PUBLIC_NORMALMESSAGE("No need output for Vebox output");
        return nullptr;
    }
}

MOS_STATUS VpResourceManager::InitVeboxSpatialAttributesConfiguration()
{
    VP_FUNC_CALL();

    VP_PUBLIC_CHK_NULL_RETURN(m_veboxDNSpatialConfigSurface);
    VP_PUBLIC_CHK_NULL_RETURN(m_veboxDNSpatialConfigSurface->osSurface);

    uint8_t* data = (uint8_t*)& g_cInit_VEBOX_SPATIAL_ATTRIBUTES_CONFIGURATIONS;
    return m_allocator.Write1DSurface(m_veboxDNSpatialConfigSurface, data,
        (uint32_t)sizeof(VEBOX_SPATIAL_ATTRIBUTES_CONFIGURATION));
}

bool VpResourceManager::VeboxOutputNeeded(VP_EXECUTE_CAPS& caps)
{
    // If DN and/or Hotpixel are the only functions enabled then the only output is the Denoised Output
    // and no need vebox output.
    // For any other vebox features being enabled, vebox output surface is needed.
    if (caps.bDI                ||
        caps.bQueryVariance     ||
        caps.bDiProcess2ndField ||
        caps.bIECP              ||
        (caps.bDN && caps.bSFC))  // DN + SFC needs IECP implicitly and outputs to DI surface
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool VpResourceManager::VeboxDenoiseOutputNeeded(VP_EXECUTE_CAPS& caps)
{
    return caps.bDN;
}

// In some case, STMM should not be destroyed even when not being used by current workload to maintain data,
// e.g. DI second field case.
// If queryAssignment == true, query whether STMM needed by current workload.
// If queryAssignment == false, query whether STMM needed to be allocated.
bool VpResourceManager::VeboxSTMMNeeded(VP_EXECUTE_CAPS& caps, bool queryAssignment)
{
    if (queryAssignment)
    {
        return caps.bDI || caps.bDN;
    }
    else
    {
        return caps.bDI || caps.bDiProcess2ndField || caps.bDN;
    }
}

};
