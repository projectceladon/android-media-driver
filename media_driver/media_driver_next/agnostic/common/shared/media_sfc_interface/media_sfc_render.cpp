/*
* Copyright (c) 2020, Intel Corporation
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
//! \file     meida_sfc_render.cpp
//! \brief    Common interface for sfc
//! \details  Common interface for sfc
//!
#include "media_sfc_interface.h"
#include "media_sfc_render.h"
#include "vp_feature_manager.h"
#include "mhw_vebox.h"
#include "vphal_common.h"
#include "vp_platform_interface.h"
#include "vp_pipeline.h"
#include "media_vdbox_sfc_render.h"
#include "media_interfaces_vphal.h"
#include "mos_os.h"
#include "renderhal.h"

using namespace vp;

typedef MediaInterfacesFactory<VphalDevice> VphalFactory;

MediaSfcRender::MediaSfcRender(PMOS_INTERFACE osInterface, MEDIA_SFC_INTERFACE_MODE mode) : m_osInterface(osInterface), m_mode(mode)
{
}

MediaSfcRender::~MediaSfcRender()
{
    Destroy();
}

void MediaSfcRender::Destroy()
{
    MOS_STATUS status = MOS_STATUS_SUCCESS;
    MOS_Delete(m_vdboxSfcRender);
    MOS_Delete(m_vpPipeline);
    MOS_Delete(m_vpPlatformInterface);
    MOS_Delete(m_vpMhwinterface);

    if (m_renderHal)
    {
        if (m_renderHal->pfnDestroy)
        {
            status = m_renderHal->pfnDestroy(m_renderHal);
            if (MOS_STATUS_SUCCESS != status)
            {
                VP_PUBLIC_ASSERTMESSAGE("Failed to destroy RenderHal, eStatus:%d.\n", status);
            }
        }
        MOS_FreeMemory(m_renderHal);
    }

    Delete_MhwCpInterface(m_cpInterface);
    m_cpInterface = nullptr;
    MOS_Delete(m_sfcInterface);

    if (m_veboxInterface)
    {
        status = m_veboxInterface->DestroyHeap();
        if (MOS_STATUS_SUCCESS != status)
        {
            VP_PUBLIC_ASSERTMESSAGE("Failed to destroy vebox heap, eStatus:%d.\n", status);
        }
        MOS_Delete(m_veboxInterface);
    }

    MOS_Delete(m_statusTable);
}

MOS_STATUS MediaSfcRender::Render(VEBOX_SFC_PARAMS &sfcParam)
{
    if (!m_initialized || !m_mode.veboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_UNINITIALIZED);
    }

    VP_PUBLIC_CHK_STATUS_RETURN(IsParameterSupported(sfcParam));

    VP_PARAMS params = {};
    params.type = PIPELINE_PARAM_TYPE_MEDIA_SFC_INTERFACE;
    params.sfcParams = &sfcParam;
    VP_PUBLIC_CHK_STATUS_RETURN(m_vpPipeline->Prepare(&params));
    VP_PUBLIC_CHK_STATUS_RETURN(m_vpPipeline->Execute());
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaSfcRender::Render(MOS_COMMAND_BUFFER *cmdBuffer, VDBOX_SFC_PARAMS &param)
{
    if (!m_initialized || !m_mode.vdboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_UNINITIALIZED);
    }

    VP_PUBLIC_CHK_NULL_RETURN(m_vdboxSfcRender);
    VP_PUBLIC_CHK_NULL_RETURN(cmdBuffer);
    VP_PUBLIC_CHK_STATUS_RETURN(IsParameterSupported(param));

    VP_PUBLIC_CHK_STATUS_RETURN(m_vdboxSfcRender->AddSfcStates(cmdBuffer, param));
    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    MediaSfcInterface initialize
//! \details  Initialize the BltState, create BLT context.
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS MediaSfcRender::Initialize()
{
    if (m_initialized)
    {
        return MOS_STATUS_SUCCESS;
    }

    VphalDevice         *vphalDevice = nullptr;
    PLATFORM            platform = {};
    MOS_STATUS          status = MOS_STATUS_SUCCESS;
    MEDIA_FEATURE_TABLE *skuTable = nullptr;
    MEDIA_WA_TABLE      *waTable = nullptr;

    VP_PUBLIC_CHK_NULL_RETURN(m_osInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_osInterface->pfnGetPlatform);
    VP_PUBLIC_CHK_NULL_RETURN(m_osInterface->pfnGetSkuTable);
    VP_PUBLIC_CHK_NULL_RETURN(m_osInterface->pfnGetWaTable);

    skuTable = m_osInterface->pfnGetSkuTable(m_osInterface);
    waTable = m_osInterface->pfnGetWaTable(m_osInterface);

    VP_PUBLIC_CHK_NULL_RETURN(skuTable);
    VP_PUBLIC_CHK_NULL_RETURN(waTable);

    // Check whether SFC supported.
    if (!MEDIA_IS_SKU(skuTable, FtrSFCPipe))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Clean the garbage data if any.
    Destroy();

    m_statusTable = MOS_New(VPHAL_STATUS_TABLE);
    VP_PUBLIC_CHK_NULL_RETURN(m_statusTable);

    // Create platform interface and vp pipeline by vphalDevice.
    m_osInterface->pfnGetPlatform(m_osInterface, &platform);
    vphalDevice = VphalFactory::CreateHal(platform.eProductFamily);
    VP_PUBLIC_CHK_NULL_RETURN(vphalDevice);

    if (vphalDevice->Initialize(m_osInterface, m_osInterface->pOsContext, false, &status) != MOS_STATUS_SUCCESS)
    {
        vphalDevice->Destroy();
        MOS_Delete(vphalDevice);
        return status;
    }

    if (nullptr == vphalDevice->m_vpPipeline || nullptr == vphalDevice->m_vpPlatformInterface)
    {
        vphalDevice->Destroy();
        MOS_Delete(vphalDevice);
        return status;
    }

    m_vpPipeline = vphalDevice->m_vpPipeline;
    m_vpPlatformInterface = vphalDevice->m_vpPlatformInterface;
    MOS_Delete(vphalDevice);

    // Create mhw interfaces.
    MhwInterfaces::CreateParams params      = {};
    params.Flags.m_sfc                      = MEDIA_IS_SKU(skuTable, FtrSFCPipe);
    params.Flags.m_vebox                    = MEDIA_IS_SKU(skuTable, FtrVERing);
    MhwInterfaces *mhwInterfaces            = MhwInterfaces::CreateFactory(params, m_osInterface);
    VP_PUBLIC_CHK_NULL_RETURN(mhwInterfaces);

    m_sfcInterface                          = mhwInterfaces->m_sfcInterface;
    m_veboxInterface                        = mhwInterfaces->m_veboxInterface;

    // mi interface and cp interface will always be created during MhwInterfaces::CreateFactory.
    // Delete them here since they will also be created by RenderHal_InitInterface.
    MOS_Delete(mhwInterfaces->m_miInterface);
    Delete_MhwCpInterface(mhwInterfaces->m_cpInterface);
    MOS_Delete(mhwInterfaces);

    if (m_veboxInterface &&
        m_veboxInterface->m_veboxSettings.uiNumInstances > 0 &&
        m_veboxInterface->m_veboxHeap == nullptr)
    {
        // Allocate VEBOX Heap
        VP_PUBLIC_CHK_STATUS_RETURN(m_veboxInterface->CreateHeap());
    }

    // Initialize render hal.
    m_renderHal = (PRENDERHAL_INTERFACE)MOS_AllocAndZeroMemory(sizeof(RENDERHAL_INTERFACE));
    VP_PUBLIC_CHK_NULL_RETURN(m_renderHal);
    VP_PUBLIC_CHK_STATUS_RETURN(RenderHal_InitInterface(
        m_renderHal,
        &m_cpInterface,
        m_osInterface));
    RENDERHAL_SETTINGS  RenderHalSettings = {};
    RenderHalSettings.iMediaStates = 32; // Init MEdia state values
    VP_PUBLIC_CHK_STATUS_RETURN(m_renderHal->pfnInitialize(m_renderHal, &RenderHalSettings));

    // Initialize vpPipeline.
    m_vpMhwinterface = MOS_New(VP_MHWINTERFACE);
    VP_PUBLIC_CHK_NULL_RETURN(m_vpMhwinterface);
    MOS_ZeroMemory(m_vpMhwinterface, sizeof(VP_MHWINTERFACE));
    m_osInterface->pfnGetPlatform(m_osInterface, &m_vpMhwinterface->m_platform);
    m_vpMhwinterface->m_waTable                = waTable;
    m_vpMhwinterface->m_skuTable               = skuTable;
    m_vpMhwinterface->m_osInterface            = m_osInterface;
    m_vpMhwinterface->m_renderHal              = m_renderHal;
    m_vpMhwinterface->m_veboxInterface         = m_veboxInterface;
    m_vpMhwinterface->m_sfcInterface           = m_sfcInterface;
    m_vpMhwinterface->m_renderer               = nullptr;
    m_vpMhwinterface->m_cpInterface            = m_cpInterface;
    m_vpMhwinterface->m_mhwMiInterface         = m_renderHal->pMhwMiInterface;
    m_vpMhwinterface->m_statusTable            = m_statusTable;
    m_vpMhwinterface->m_vpPlatformInterface    = m_vpPlatformInterface;

    if (m_mode.veboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(m_vpPipeline->Init(m_vpMhwinterface));
    }
    else
    {
        MOS_Delete(m_vpPipeline);
    }

    if (m_mode.vdboxSfcEnabled)
    {
        m_vdboxSfcRender = MOS_New(MediaVdboxSfcRender);
        VP_PUBLIC_CHK_NULL_RETURN(m_vdboxSfcRender);
        VP_PUBLIC_CHK_STATUS_RETURN(m_vdboxSfcRender->Initialize(*m_vpMhwinterface));
    }

    m_initialized = true;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaSfcRender::InitScalingParams(FeatureParamScaling &scalingParams, VDBOX_SFC_PARAMS &sfcParam)
{
    if (!m_mode.vdboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_UNINITIALIZED);
    }

    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.output.surface);

    RECT                rcSrcInput          = {0, 0, (int32_t)sfcParam.input.width,             (int32_t)sfcParam.input.height              };
    RECT                rcOutput            = {0, 0, (int32_t)sfcParam.output.surface->dwWidth, (int32_t)sfcParam.output.surface->dwHeight  };

    scalingParams.type                      = FeatureTypeScalingOnSfc;
    scalingParams.formatInput               = sfcParam.input.format;
    scalingParams.formatOutput              = sfcParam.output.surface->Format;
    scalingParams.scalingMode               = VPHAL_SCALING_AVS;
    scalingParams.scalingPreference         = VPHAL_SCALING_PREFER_SFC;              //!< DDI indicate Scaling preference
    scalingParams.bDirectionalScalar        = false;                                 //!< Vebox Directional Scalar
    scalingParams.rcSrcInput                = rcSrcInput;                            //!< No input crop support for VD mode. rcSrcInput must have same width/height of input image.
    scalingParams.rcDstInput                = sfcParam.output.rcDst;
    scalingParams.rcMaxSrcInput             = rcSrcInput;
    scalingParams.dwWidthInput              = sfcParam.input.width;
    scalingParams.dwHeightInput             = sfcParam.input.height;
    scalingParams.rcSrcOutput               = rcOutput;
    scalingParams.rcDstOutput               = rcOutput;
    scalingParams.rcMaxSrcOutput            = rcOutput;
    scalingParams.dwWidthOutput             = sfcParam.output.surface->dwWidth;
    scalingParams.dwHeightOutput            = sfcParam.output.surface->dwHeight;
    scalingParams.pColorFillParams          = nullptr;
    scalingParams.pCompAlpha                = nullptr;
    scalingParams.colorSpaceOutput          = sfcParam.output.colorSpace;
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaSfcRender::InitScalingParams(FeatureParamScaling &scalingParams, VEBOX_SFC_PARAMS &sfcParam)
{
    if (!m_mode.veboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_UNINITIALIZED);
    }

    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.input.surface);
    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.output.surface);

    scalingParams.scalingMode            = VPHAL_SCALING_AVS;
    scalingParams.scalingPreference      = VPHAL_SCALING_PREFER_SFC;
    scalingParams.bDirectionalScalar     = false;
    scalingParams.formatInput            = sfcParam.input.surface->Format;
    scalingParams.rcSrcInput             = sfcParam.input.rcSrc;
    scalingParams.rcMaxSrcInput          = sfcParam.input.rcSrc;
    scalingParams.dwWidthInput           = sfcParam.input.surface->dwWidth;
    scalingParams.dwHeightInput          = sfcParam.input.surface->dwHeight;
    scalingParams.formatOutput           = sfcParam.output.surface->Format;
    scalingParams.colorSpaceOutput       = sfcParam.output.colorSpace;
    scalingParams.pColorFillParams       = nullptr;
    scalingParams.pCompAlpha             = nullptr;

    RECT recOutput = {0, 0, (int32_t)sfcParam.output.surface->dwWidth, (int32_t)sfcParam.output.surface->dwHeight};

    if (sfcParam.input.rotation == (MEDIA_ROTATION)VPHAL_ROTATION_IDENTITY    ||
        sfcParam.input.rotation == (MEDIA_ROTATION)VPHAL_ROTATION_180         ||
        sfcParam.input.rotation == (MEDIA_ROTATION)VPHAL_MIRROR_HORIZONTAL    ||
        sfcParam.input.rotation == (MEDIA_ROTATION)VPHAL_MIRROR_VERTICAL)
    {
        scalingParams.dwWidthOutput  = sfcParam.output.surface->dwWidth;
        scalingParams.dwHeightOutput = sfcParam.output.surface->dwHeight;

        scalingParams.rcDstInput     = sfcParam.output.rcDst;
        scalingParams.rcSrcOutput    = recOutput;
        scalingParams.rcDstOutput    = recOutput;
        scalingParams.rcMaxSrcOutput = recOutput;
    }
    else
    {
        scalingParams.dwWidthOutput      = sfcParam.output.surface->dwHeight;
        scalingParams.dwHeightOutput     = sfcParam.output.surface->dwWidth;

        RECT_ROTATE(scalingParams.rcDstInput, sfcParam.output.rcDst);
        RECT_ROTATE(scalingParams.rcSrcOutput, recOutput);
        RECT_ROTATE(scalingParams.rcDstOutput, recOutput);
        RECT_ROTATE(scalingParams.rcMaxSrcOutput, recOutput);
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaSfcRender::IsParameterSupported(
    VDBOX_SFC_PARAMS                    &sfcParam)
{
    if (!m_mode.vdboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_UNINITIALIZED);
    }

    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.output.surface);
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_vdboxSfcRender);

    VpScalingFilter scalingFilter(m_vpMhwinterface);
    FeatureParamScaling scalingParams = {};
    VP_PUBLIC_CHK_STATUS_RETURN(InitScalingParams(scalingParams, sfcParam));

    VP_EXECUTE_CAPS vpExecuteCaps   = {};
    vpExecuteCaps.bSFC              = 1;
    vpExecuteCaps.bSfcCsc           = 1;
    vpExecuteCaps.bSfcScaling       = 1;
    vpExecuteCaps.bSfcRotMir        = 1;

    VP_PUBLIC_CHK_STATUS_RETURN(scalingFilter.Init(sfcParam.videoParams.codecStandard, sfcParam.videoParams.jpeg.jpegChromaType));
    VP_PUBLIC_CHK_STATUS_RETURN(scalingFilter.SetExecuteEngineCaps(scalingParams, vpExecuteCaps));
    VP_PUBLIC_CHK_STATUS_RETURN(scalingFilter.CalculateEngineParams());

    SFC_SCALING_PARAMS *params = scalingFilter.GetSfcParams();
    VP_PUBLIC_CHK_NULL_RETURN(params);

    // Check original input size (for JPEG)
    if (!MOS_WITHIN_RANGE(sfcParam.input.width, m_sfcInterface->m_minWidth, m_sfcInterface->m_maxWidth) ||
        !MOS_WITHIN_RANGE(sfcParam.input.height, m_sfcInterface->m_minHeight, m_sfcInterface->m_maxHeight))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check input size
    if (!MOS_WITHIN_RANGE(params->dwInputFrameWidth, m_sfcInterface->m_minWidth, m_sfcInterface->m_maxWidth) ||
        !MOS_WITHIN_RANGE(params->dwInputFrameHeight, m_sfcInterface->m_minHeight, m_sfcInterface->m_maxHeight))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check output size
    if (!MOS_WITHIN_RANGE(params->dwOutputFrameWidth, m_sfcInterface->m_minWidth, m_sfcInterface->m_maxWidth) ||
        !MOS_WITHIN_RANGE(params->dwOutputFrameHeight, m_sfcInterface->m_minHeight, m_sfcInterface->m_maxHeight))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check output region rectangles
    if ((scalingParams.rcDstInput.bottom - scalingParams.rcDstInput.top > (int32_t)scalingParams.dwHeightOutput) ||
        (scalingParams.rcDstInput.right - scalingParams.rcDstInput.left > (int32_t)scalingParams.dwWidthOutput))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check scaling ratio
    if (!MOS_WITHIN_RANGE(params->fAVSXScalingRatio, m_sfcInterface->m_minScalingRatio, m_sfcInterface->m_maxScalingRatio) ||
        !MOS_WITHIN_RANGE(params->fAVSYScalingRatio, m_sfcInterface->m_minScalingRatio, m_sfcInterface->m_maxScalingRatio))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check input and output format (limited only to current decode processing usage)
    if (!m_vdboxSfcRender->IsVdboxSfcFormatSupported(sfcParam.videoParams.codecStandard, sfcParam.input.format, sfcParam.output.surface->Format))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaSfcRender::IsParameterSupported(
    VEBOX_SFC_PARAMS                    &sfcParam)
{
    if (!m_mode.veboxSfcEnabled)
    {
        VP_PUBLIC_CHK_STATUS_RETURN(MOS_STATUS_UNINITIALIZED);
    }

    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.input.surface);
    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.output.surface);
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcInterface);

    VpScalingFilter scalingFilter(m_vpMhwinterface);
    FeatureParamScaling scalingParams       = {};

    VP_PUBLIC_CHK_STATUS_RETURN(InitScalingParams(scalingParams, sfcParam));

    VP_EXECUTE_CAPS vpExecuteCaps   = {};
    vpExecuteCaps.bSFC              = 1;
    vpExecuteCaps.bSfcCsc           = 1;
    vpExecuteCaps.bSfcScaling       = 1;
    vpExecuteCaps.bSfcRotMir        = 1;

    VP_PUBLIC_CHK_STATUS_RETURN(scalingFilter.Init());
    VP_PUBLIC_CHK_STATUS_RETURN(scalingFilter.SetExecuteEngineCaps(scalingParams, vpExecuteCaps));
    VP_PUBLIC_CHK_STATUS_RETURN(scalingFilter.CalculateEngineParams());

    SFC_SCALING_PARAMS *params = scalingFilter.GetSfcParams();
    VP_PUBLIC_CHK_NULL_RETURN(params);

    // Check input size
    if (!MOS_WITHIN_RANGE(params->dwInputFrameWidth, m_sfcInterface->m_minWidth, m_sfcInterface->m_maxWidth) ||
        !MOS_WITHIN_RANGE(params->dwInputFrameHeight, m_sfcInterface->m_minHeight, m_sfcInterface->m_maxHeight))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check output size
    if (!MOS_WITHIN_RANGE(params->dwOutputFrameWidth, m_sfcInterface->m_minWidth, m_sfcInterface->m_maxWidth) ||
        !MOS_WITHIN_RANGE(params->dwOutputFrameHeight, m_sfcInterface->m_minHeight, m_sfcInterface->m_maxHeight))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check input region rectangles
    if ((scalingParams.rcSrcInput.bottom - scalingParams.rcSrcInput.top > (int32_t)scalingParams.dwHeightInput) ||
        (scalingParams.rcSrcInput.right - scalingParams.rcSrcInput.left > (int32_t)scalingParams.dwWidthInput))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check output region rectangles
    if ((scalingParams.rcDstInput.bottom - scalingParams.rcDstInput.top > (int32_t)scalingParams.dwHeightOutput) ||
        (scalingParams.rcDstInput.right - scalingParams.rcDstInput.left > (int32_t)scalingParams.dwWidthOutput))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check scaling ratio
    if (!MOS_WITHIN_RANGE(params->fAVSXScalingRatio, m_sfcInterface->m_minScalingRatio, m_sfcInterface->m_maxScalingRatio) ||
        !MOS_WITHIN_RANGE(params->fAVSYScalingRatio, m_sfcInterface->m_minScalingRatio, m_sfcInterface->m_maxScalingRatio))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    // Check input and output format
    if (!m_vpPipeline->IsVeboxSfcFormatSupported(sfcParam.input.surface->Format, sfcParam.output.surface->Format))
    {
        return MOS_STATUS_PLATFORM_NOT_SUPPORTED;
    }

    return MOS_STATUS_SUCCESS;
}