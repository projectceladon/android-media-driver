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
//! \file     meida_vdbox_sfc_render.cpp
//! \brief    Common interface for sfc
//! \details  Common interface for sfc
//!
#include "vp_feature_manager.h"
#include "media_sfc_interface.h"
#include "media_vdbox_sfc_render.h"
#include "mos_os.h"
#include "vp_render_sfc_base.h"
#include "vp_render_ief.h"
#include "vp_mem_compression.h"

using namespace vp;

MediaVdboxSfcRender::MediaVdboxSfcRender()
{
}

MediaVdboxSfcRender::~MediaVdboxSfcRender()
{
    Destroy();
}

void MediaVdboxSfcRender::Destroy()
{
    MOS_Delete(m_sfcRender);
    MOS_Delete(m_cscFilter);
    MOS_Delete(m_scalingFilter);
    MOS_Delete(m_rotMirFilter);
    MOS_Delete(m_allocator);
    MOS_Delete(m_mmc);
}

//!
//! \brief    MediaSfcInterface initialize
//! \details  Initialize the BltState, create BLT context.
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS MediaVdboxSfcRender::Initialize(VP_MHWINTERFACE &vpMhwinterface)
{
    VP_PUBLIC_CHK_NULL_RETURN(vpMhwinterface.m_vpPlatformInterface);
    VP_PUBLIC_CHK_NULL_RETURN(vpMhwinterface.m_osInterface);

    m_vpMhwInterface    = vpMhwinterface;
    m_osInterface       = m_vpMhwInterface.m_osInterface;
    m_mmc               = MOS_New(VPMediaMemComp, m_osInterface, &m_vpMhwInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_mmc);
    m_allocator         = MOS_New(VpAllocator, m_osInterface, m_mmc);
    VP_PUBLIC_CHK_NULL_RETURN(m_allocator);
    m_cscFilter         = MOS_New(VpCscFilter, &m_vpMhwInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_cscFilter);
    m_scalingFilter     = MOS_New(VpScalingFilter, &m_vpMhwInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_scalingFilter);
    m_rotMirFilter      = MOS_New(VpRotMirFilter, &m_vpMhwInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_rotMirFilter);
    VP_PUBLIC_CHK_STATUS_RETURN(m_vpMhwInterface.m_vpPlatformInterface->CreateSfcRender(m_sfcRender, m_vpMhwInterface, m_allocator));
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaVdboxSfcRender::SetCSCParams(VDBOX_SFC_PARAMS &sfcParam, VP_EXECUTE_CAPS &vpExecuteCaps)
{
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
    VP_PUBLIC_CHK_NULL_RETURN(m_cscFilter);
    FeatureParamCsc cscParams       = {};
    cscParams.type                  = FeatureTypeCscOnSfc;
    cscParams.formatInput           = sfcParam.input.format;
    cscParams.formatOutput          = sfcParam.output.surface->Format;
    cscParams.colorSpaceInput       = sfcParam.input.colorSpace;
    cscParams.colorSpaceOutput      = sfcParam.output.colorSpace;
    cscParams.chromaSitingInput     = sfcParam.input.chromaSiting;
    cscParams.chromaSitingOutput    = sfcParam.output.chromaSiting;

    m_cscFilter->Init();
    m_cscFilter->SetExecuteEngineCaps(cscParams, vpExecuteCaps);
    m_cscFilter->CalculateEngineParams();

    return m_sfcRender->SetCSCParams(m_cscFilter->GetSfcParams());
}

MOS_STATUS MediaVdboxSfcRender::SetScalingParams(VDBOX_SFC_PARAMS &sfcParam, VP_EXECUTE_CAPS &vpExecuteCaps)
{
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
    VP_PUBLIC_CHK_NULL_RETURN(m_scalingFilter);

    RECT                rcSrcInput          = {0, 0, (int32_t)sfcParam.input.width,             (int32_t)sfcParam.input.height              };
    RECT                rcOutput            = {0, 0, (int32_t)sfcParam.output.surface->dwWidth, (int32_t)sfcParam.output.surface->dwHeight  };
    FeatureParamScaling scalingParams       = {};
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

    m_scalingFilter->Init(sfcParam.videoParams.codecStandard, sfcParam.videoParams.jpeg.jpegChromaType);
    m_scalingFilter->SetExecuteEngineCaps(scalingParams, vpExecuteCaps);
    m_scalingFilter->CalculateEngineParams();

    return m_sfcRender->SetScalingParams(m_scalingFilter->GetSfcParams());
}

MOS_STATUS MediaVdboxSfcRender::SetRotMirParams(VDBOX_SFC_PARAMS &sfcParam, VP_EXECUTE_CAPS &vpExecuteCaps)
{
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
    VP_PUBLIC_CHK_NULL_RETURN(m_rotMirFilter);
    FeatureParamRotMir rotMirParams     = {};
    rotMirParams.type                   = FeatureTypeRotMirOnSfc;
    rotMirParams.formatInput            = sfcParam.input.format;
    rotMirParams.formatOutput           = sfcParam.output.surface->Format;
    rotMirParams.rotation               = sfcParam.input.mirrorEnabled ? VPHAL_MIRROR_HORIZONTAL : VPHAL_ROTATION_IDENTITY;
    rotMirParams.tileOutput             = sfcParam.output.surface->TileType;

    m_rotMirFilter->Init();
    m_rotMirFilter->SetExecuteEngineCaps(rotMirParams, vpExecuteCaps);
    m_rotMirFilter->CalculateEngineParams();

    return m_sfcRender->SetRotMirParams(m_rotMirFilter->GetSfcParams());
}

MOS_STATUS MediaVdboxSfcRender::AddSfcStates(MOS_COMMAND_BUFFER *cmdBuffer, VDBOX_SFC_PARAMS &sfcParam)
{
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
    VP_PUBLIC_CHK_NULL_RETURN(sfcParam.output.surface);
    VP_PUBLIC_CHK_NULL_RETURN(cmdBuffer);

    VP_EXECUTE_CAPS vpExecuteCaps   = {};
    vpExecuteCaps.bSFC              = 1;
    vpExecuteCaps.bSfcCsc           = 1;
    vpExecuteCaps.bSfcScaling       = 1;
    vpExecuteCaps.bSfcRotMir        = 1;

    VP_PUBLIC_CHK_STATUS_RETURN(m_sfcRender->Init(sfcParam.videoParams));
    VP_PUBLIC_CHK_STATUS_RETURN(SetCSCParams(sfcParam, vpExecuteCaps));
    VP_PUBLIC_CHK_STATUS_RETURN(SetScalingParams(sfcParam, vpExecuteCaps));
    VP_PUBLIC_CHK_STATUS_RETURN(SetRotMirParams(sfcParam, vpExecuteCaps));

    RECT        rcOutput        = {0, 0, (int32_t)sfcParam.output.surface->dwWidth, (int32_t)sfcParam.output.surface->dwHeight};
    // The value of plane offset are different between vp and codec. updatePlaneOffset need be set to true when create vp surface
    // with mos surface from codec hal.
    VP_SURFACE  *renderTarget   = m_allocator->AllocateVpSurface(*sfcParam.output.surface,
                                                            sfcParam.output.colorSpace,
                                                            sfcParam.output.chromaSiting,
                                                            rcOutput,
                                                            rcOutput,
                                                            SURF_OUT_RENDERTARGET,
                                                            true);

    //---------------------------------
    // Send CMD: SFC pipe commands
    //---------------------------------

    VP_RENDER_CHK_STATUS_RETURN(m_sfcRender->SetupSfcState(renderTarget));
    VP_RENDER_CHK_STATUS_RETURN(m_sfcRender->SendSfcCmd(
                            CODECHAL_JPEG != sfcParam.videoParams.codecStandard,
                            cmdBuffer));

    m_allocator->DestroyVpSurface(renderTarget);

    return MOS_STATUS_SUCCESS;
}

bool MediaVdboxSfcRender::IsVdboxSfcFormatSupported(
    CODECHAL_STANDARD           codecStandard,
    MOS_FORMAT                  inputFormat,
    MOS_FORMAT                  outputFormat)
{
    if (nullptr == m_sfcRender)
    {
        return false;
    }
    return m_sfcRender->IsVdboxSfcFormatSupported(codecStandard, inputFormat, outputFormat);
}
