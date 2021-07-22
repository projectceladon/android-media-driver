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
//! \file     vp_vebox_cmd_packet.cpp
//! \brief    vebox packet which used in by mediapipline.
//! \details  vebox packet provide the structures and generate the cmd buffer which mediapipline will used.
//!

#include "vp_vebox_cmd_packet.h"
#include "vp_utils.h"
#include "mos_resource_defs.h"
#include "hal_oca_interface.h"
#include "vp_render_sfc_m12.h"
#include "vp_render_ief.h"
#include "vp_feature_caps.h"
#include "vp_platform_interface.h"

namespace vp {

#define INTERP(x0, x1, x, y0, y1)   ((uint32_t) floor(y0+(x-x0)*(y1-y0)/(double)(x1-x0)))

const uint32_t  VpVeboxCmdPacket::m_satP1Table[MHW_STE_FACTOR_MAX + 1] = {
    0x00000000, 0xfffffffe, 0xfffffffc, 0xfffffffa, 0xfffffff6, 0xfffffff4, 0xfffffff2, 0xfffffff0, 0xffffffee, 0xffffffec };

const uint32_t   VpVeboxCmdPacket::m_satS0Table[MHW_STE_FACTOR_MAX + 1] = {
    0x000000ef, 0x00000100, 0x00000113, 0x00000129, 0x0000017a, 0x000001a2, 0x000001d3, 0x00000211, 0x00000262, 0x000002d1 };

const uint32_t   VpVeboxCmdPacket::m_satS1Table[MHW_STE_FACTOR_MAX + 1] = {
    0x000000ab, 0x00000080, 0x00000066, 0x00000055, 0x000000c2, 0x000000b9, 0x000000b0, 0x000000a9, 0x000000a2, 0x0000009c };

void VpVeboxCmdPacket::SetupSurfaceStates(
    PVPHAL_VEBOX_SURFACE_STATE_CMD_PARAMS   pVeboxSurfaceStateCmdParams)
{
    VP_PUBLIC_CHK_NULL_NO_STATUS_RETURN(pVeboxSurfaceStateCmdParams);
    MOS_ZeroMemory(pVeboxSurfaceStateCmdParams, sizeof(VPHAL_VEBOX_SURFACE_STATE_CMD_PARAMS));
    pVeboxSurfaceStateCmdParams->pSurfInput    = m_veboxPacketSurface.pCurrInput;
    pVeboxSurfaceStateCmdParams->pSurfOutput   = m_veboxPacketSurface.pCurrOutput;
    pVeboxSurfaceStateCmdParams->pSurfSTMM     = m_veboxPacketSurface.pSTMMInput;
    pVeboxSurfaceStateCmdParams->pSurfDNOutput = m_veboxPacketSurface.pDenoisedCurrOutput;
    pVeboxSurfaceStateCmdParams->bDIEnable     = m_PacketCaps.bDI;
    pVeboxSurfaceStateCmdParams->b3DlutEnable  = m_PacketCaps.bHDR3DLUT;  // Need to consider cappipe
}

MOS_STATUS VpVeboxCmdPacket::SetupVeboxState(
    PMHW_VEBOX_STATE_CMD_PARAMS pVeboxStateCmdParams)
{
    PMHW_VEBOX_MODE         pVeboxMode   = nullptr;
    MOS_STATUS              eStatus      = MOS_STATUS_SUCCESS;

    VP_RENDER_CHK_NULL_RETURN(pVeboxStateCmdParams);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface);

    pVeboxMode = &pVeboxStateCmdParams->VeboxMode;
    VP_RENDER_CHK_NULL_RETURN(pVeboxMode);

    VpVeboxRenderData* pRenderData = GetLastExecRenderData();
    VP_RENDER_ASSERT(pRenderData);

    MOS_ZeroMemory(pVeboxStateCmdParams, sizeof(*pVeboxStateCmdParams));

    // Always enable the global iecp to align with the legacy path.
    // For next step, need to enable it only when necessary.
    pVeboxMode->GlobalIECPEnable = true;

    pVeboxMode->DIEnable = m_PacketCaps.bDI;

    pVeboxMode->SFCParallelWriteEnable = m_IsSfcUsed &&
                                            (m_PacketCaps.bDN || m_PacketCaps.bDI);
    pVeboxMode->DNEnable = m_PacketCaps.bDN;
    pVeboxMode->DNDIFirstFrame = m_DNDIFirstFrame;
    pVeboxMode->DIOutputFrames = m_DIOutputFrames;
    pVeboxMode->DisableEncoderStatistics = true;
    pVeboxMode->DisableTemporalDenoiseFilter = false;

    pVeboxMode->ColorGamutCompressionEnable = m_PacketCaps.bCGC;

    pVeboxStateCmdParams->bUseVeboxHeapKernelResource = UseKernelResource();

    //Set up Chroma Sampling
    pVeboxStateCmdParams->ChromaSampling = pRenderData->GetChromaSubSamplingParams();

    // Permanent program limitation that should go in all the configurations of SKLGT which have 2 VEBOXes (i.e. GT3 & GT4)
    // VEBOX1 should be disabled whenever there is an VE-SFC workload.
    // This is because we have only one SFC all the GT configurations and that SFC is tied to VEBOX0.Hence the programming restriction.
    if (m_IsSfcUsed)
    {
        pVeboxMode->SingleSliceVeboxEnable = 1;
    }
    else
    {
        pVeboxMode->SingleSliceVeboxEnable = 0;
    }
    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::InitCmdBufferWithVeParams(
    PRENDERHAL_INTERFACE pRenderHal,
    MOS_COMMAND_BUFFER & CmdBuffer,
    PRENDERHAL_GENERIC_PROLOG_PARAMS pGenericPrologParams)
{
    MOS_STATUS                              eStatus = MOS_STATUS_SUCCESS;
    RENDERHAL_GENERIC_PROLOG_PARAMS_G12     genericPrologParamsG12 = {};

    genericPrologParamsG12.bEnableMediaFrameTracking      = pGenericPrologParams->bEnableMediaFrameTracking;
    genericPrologParamsG12.bMmcEnabled                    = pGenericPrologParams->bMmcEnabled;
    genericPrologParamsG12.dwMediaFrameTrackingAddrOffset = pGenericPrologParams->dwMediaFrameTrackingAddrOffset;
    genericPrologParamsG12.dwMediaFrameTrackingTag        = pGenericPrologParams->dwMediaFrameTrackingTag;
    genericPrologParamsG12.presMediaFrameTrackingSurface  = pGenericPrologParams->presMediaFrameTrackingSurface;

    genericPrologParamsG12.VEngineHintParams.BatchBufferCount = 2;
    //genericPrologParamsG12.VEngineHintParams.resScalableBatchBufs[0] = CmdBuffer[0].OsResource;
    //genericPrologParamsG12.VEngineHintParams.resScalableBatchBufs[1] = CmdBuffer[1].OsResource;
    genericPrologParamsG12.VEngineHintParams.UsingFrameSplit = true;
    genericPrologParamsG12.VEngineHintParams.UsingSFC = false;
    genericPrologParamsG12.VEngineHintParams.EngineInstance[0] = 0;
    genericPrologParamsG12.VEngineHintParams.EngineInstance[1] = 1;
    genericPrologParamsG12.VEngineHintParams.NeedSyncWithPrevious = true;
    genericPrologParamsG12.VEngineHintParams.SameEngineAsLastSubmission = true;

    pRenderHal->pOsInterface->VEEnable = false;

    // Initialize command buffer and insert prolog
    VP_RENDER_CHK_STATUS_RETURN(pRenderHal->pfnInitCommandBuffer(
        pRenderHal,
        &CmdBuffer,
        (PRENDERHAL_GENERIC_PROLOG_PARAMS)&genericPrologParamsG12));

    return eStatus;
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
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS VpVeboxCmdPacket::InitSTMMHistory()
{
    MOS_STATUS          eStatus;
    uint32_t            dwSize;
    int32_t             x, y;
    uint8_t*            pByte;
    MOS_LOCK_PARAMS     LockFlags;
    PVP_SURFACE         stmmSurface = GetSurface(SurfaceTypeSTMMIn);
    eStatus         = MOS_STATUS_SUCCESS;

    VP_PUBLIC_CHK_NULL_RETURN(stmmSurface);
    VP_PUBLIC_CHK_NULL_RETURN(stmmSurface->osSurface);

    MOS_ZeroMemory(&LockFlags, sizeof(MOS_LOCK_PARAMS));

    LockFlags.WriteOnly    = 1;
    LockFlags.TiledAsTiled = 1; // Set TiledAsTiled flag for STMM surface initialization.

    // Lock the surface for writing
    pByte = (uint8_t*)m_allocator->Lock(
        &stmmSurface->osSurface->OsResource,
        &LockFlags);

    VPHAL_RENDER_CHK_NULL(pByte);

    dwSize = stmmSurface->osSurface->dwWidth >> 2;

    // Fill STMM surface with DN history init values.
    for (y = 0; y < (int32_t)stmmSurface->osSurface->dwHeight; y++)
    {
        for (x = 0; x < (int32_t)dwSize; x++)
        {
            MOS_FillMemory(pByte, 2, DNDI_HISTORY_INITVALUE);
            // skip denosie history init.
            pByte += 4;
        }

        pByte += stmmSurface->osSurface->dwPitch - stmmSurface->osSurface->dwWidth;
    }

    // Unlock the surface
    VPHAL_RENDER_CHK_STATUS(m_allocator->UnLock(&stmmSurface->osSurface->OsResource));

finish:
    return eStatus;
}

bool VpVeboxCmdPacket::IsFormatMMCSupported(MOS_FORMAT Format)
{
    // Check if Sample Format is supported
    if ((Format != Format_YUY2) &&
        (Format != Format_Y210) &&
        (Format != Format_Y410) &&
        (Format != Format_Y216) &&
        (Format != Format_Y416) &&
        (Format != Format_P010) &&
        (Format != Format_P016) &&
        (Format != Format_AYUV) &&
        (Format != Format_NV21) &&
        (Format != Format_NV12) &&
        (Format != Format_UYVY) &&
        (Format != Format_YUYV) &&
        (Format != Format_R10G10B10A2)   &&
        (Format != Format_B10G10R10A2)   &&
        (Format != Format_A8B8G8R8)      &&
        (Format != Format_X8B8G8R8)      &&
        (Format != Format_A8R8G8B8)      &&
        (Format != Format_X8R8G8B8)      &&
        (Format != Format_A16B16G16R16F) &&
        (Format != Format_A16R16G16B16F))
    {
      VP_RENDER_NORMALMESSAGE("Unsupported Format '0x%08x' for VEBOX MMC ouput.", Format);
      return false;
    }

    return true;
}

MOS_STATUS VpVeboxCmdPacket::SetSfcMmcParams()
{
    VP_FUNC_CALL();
    VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
    VP_PUBLIC_CHK_NULL_RETURN(m_renderTarget);
    VP_PUBLIC_CHK_NULL_RETURN(m_renderTarget->osSurface);
    VP_PUBLIC_CHK_NULL_RETURN(m_mmc);

    VP_PUBLIC_CHK_STATUS_RETURN(m_sfcRender->SetMmcParams(m_renderTarget->osSurface,
                                                        IsFormatMMCSupported(m_renderTarget->osSurface->Format),
                                                        m_mmc->IsMmcEnabled()));

    return MOS_STATUS_SUCCESS;
}

VP_SURFACE *VpVeboxCmdPacket::GetSurface(SurfaceType type)
{
    auto it = m_surfSetting.surfGroup.find(type);
    VP_SURFACE *surf = (m_surfSetting.surfGroup.end() != it) ? it->second : nullptr;
    if (SurfaceTypeVeboxCurrentOutput == type && nullptr == surf && !m_IsSfcUsed)
    {
        // Vebox output case.
        surf = m_renderTarget;
    }
    else if (SurfaceTypeVeboxInput == type && surf)
    {
        // The vp surface object for external surface will be destroyed by hw filter before packet submit.
        // Store the vp surface object inside packet.
        if (MOS_FAILED(m_allocator->CopyVpSurface(*m_currentSurface ,*surf)))
        {
            return nullptr;
        }
        m_currentSurface->rcMaxSrc = m_currentSurface->rcSrc;
        surf = m_currentSurface;
    }
    else if (SurfaceTypeVeboxPreviousInput == type && surf)
    {
        // The vp surface object for external surface will be destroyed by hw filter before packet submit.
        // Store the vp surface object inside packet.
        if (MOS_FAILED(m_allocator->CopyVpSurface(*m_previousSurface ,*surf)))
        {
            return nullptr;
        }
        surf = m_previousSurface;
    }
    return surf;
}

MOS_STATUS VpVeboxCmdPacket::SetScalingParams(PSFC_SCALING_PARAMS scalingParams)
{
    VP_PUBLIC_CHK_NULL_RETURN(scalingParams);
    // Scaing only can be apply to SFC path
    if (m_PacketCaps.bSFC)
    {
        VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
        VP_PUBLIC_CHK_STATUS_RETURN(m_sfcRender->SetScalingParams(scalingParams));

        //---------------------------------
        // Set SFC State:  mmc
        //---------------------------------
        SetSfcMmcParams();

        return MOS_STATUS_SUCCESS;
    }
    else
    {
        VP_RENDER_NORMALMESSAGE("Scaling is enabled in SFC, pls recheck the features enabling in SFC");
        return MOS_STATUS_INVALID_PARAMETER;
    }
}

MOS_STATUS VpVeboxCmdPacket::SetSfcCSCParams(PSFC_CSC_PARAMS cscParams)
{
    VP_PUBLIC_CHK_NULL_RETURN(cscParams);

    if (m_PacketCaps.bSFC)
    {
        VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
        VP_PUBLIC_CHK_STATUS_RETURN(m_sfcRender->SetCSCParams(cscParams));
        return MOS_STATUS_SUCCESS;
    }
    else
    {
        VP_RENDER_NORMALMESSAGE("CSC/IEF for Output is enabled in SFC, pls recheck the features enabling in SFC");
        return MOS_STATUS_INVALID_PARAMETER;
    }
}

MOS_STATUS VpVeboxCmdPacket::SetVeboxBeCSCParams(PVEBOX_CSC_PARAMS cscParams)
{
    VP_RENDER_CHK_NULL_RETURN(cscParams);

    VpVeboxRenderData* pRenderData = GetLastExecRenderData();
    VP_RENDER_ASSERT(pRenderData);

    pRenderData->IECP.BeCSC.bBeCSCEnabled = cscParams->bCSCEnabled;

    MHW_VEBOX_IECP_PARAMS& veboxIecpParams = pRenderData->GetIECPParams();

    if (m_CscInputCspace  != cscParams->inputColorSpace ||
        m_CscOutputCspace != cscParams->outputColorSpace)
    {
        VeboxGetBeCSCMatrix(
            cscParams->inputColorSpace,
            cscParams->outputColorSpace,
            cscParams->inputFormat);

        m_CscInputCspace = cscParams->inputColorSpace;
        m_CscOutputCspace = cscParams->outputColorSpace;
    }

    if (m_PacketCaps.bVebox &&
        m_PacketCaps.bBeCSC &&
        cscParams->bCSCEnabled)
    {
        veboxIecpParams.bCSCEnable     = true;
        veboxIecpParams.pfCscCoeff     = m_fCscCoeff;
        veboxIecpParams.pfCscInOffset  = m_fCscInOffset;
        veboxIecpParams.pfCscOutOffset = m_fCscOutOffset;
    }

    VP_RENDER_CHK_STATUS_RETURN(SetVeboxOutputAlphaParams(cscParams));
    VP_RENDER_CHK_STATUS_RETURN(SetVeboxChromasitingParams(cscParams));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetVeboxOutputAlphaParams(PVEBOX_CSC_PARAMS cscParams)
{
    VP_RENDER_CHK_NULL_RETURN(cscParams);

    VpVeboxRenderData* pRenderData = GetLastExecRenderData();
    VP_RENDER_ASSERT(pRenderData);

    MHW_VEBOX_IECP_PARAMS& veboxIecpParams = pRenderData->GetIECPParams();

    if (IS_ALPHA_FORMAT(cscParams->outputFormat))
    {
        veboxIecpParams.bAlphaEnable = true;
    }
    else
    {
        veboxIecpParams.bAlphaEnable = false;
        return MOS_STATUS_SUCCESS;
    }

    MOS_FORMAT outFormat = cscParams->outputFormat;

    if (cscParams->alphaParams != nullptr)
    {
        switch (cscParams->alphaParams->AlphaMode)
        {
        case VPHAL_ALPHA_FILL_MODE_NONE:
            if (outFormat == Format_A8R8G8B8)
            {
                veboxIecpParams.wAlphaValue =
                    (uint8_t)(0xff * cscParams->alphaParams->fAlpha);
            }
            else
            {
                veboxIecpParams.wAlphaValue = 0xff;
            }
            break;

            // VEBOX does not support Background Color
        case VPHAL_ALPHA_FILL_MODE_BACKGROUND:

            // VPHAL_ALPHA_FILL_MODE_SOURCE_STREAM case is hit when the
            // input does not have alpha
            // So we set Opaque alpha channel.
        case VPHAL_ALPHA_FILL_MODE_SOURCE_STREAM:
        case VPHAL_ALPHA_FILL_MODE_OPAQUE:
        default:
            veboxIecpParams.wAlphaValue = 0xff;
            break;
        }
    }
    else
    {
        veboxIecpParams.wAlphaValue = 0xff;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetVeboxChromasitingParams(PVEBOX_CSC_PARAMS cscParams)
{
    VP_RENDER_CHK_NULL_RETURN(cscParams);

    VpVeboxRenderData* pRenderData = GetLastExecRenderData();
    VP_RENDER_ASSERT(pRenderData);

    MHW_VEBOX_CHROMA_SAMPLING& veboxChromaSamplingParams = pRenderData->GetChromaSubSamplingParams();

    veboxChromaSamplingParams.BypassChromaDownsampling                  = cscParams->bypassCDS;
    veboxChromaSamplingParams.BypassChromaUpsampling                    = cscParams->bypassCUS;
    veboxChromaSamplingParams.ChromaDownsamplingCoSitedHorizontalOffset = cscParams->chromaDownSamplingHorizontalCoef;
    veboxChromaSamplingParams.ChromaDownsamplingCoSitedVerticalOffset   = cscParams->chromaDownSamplingVerticalCoef;
    veboxChromaSamplingParams.ChromaUpsamplingCoSitedHorizontalOffset   = cscParams->chromaUpSamplingHorizontalCoef;
    veboxChromaSamplingParams.ChromaUpsamplingCoSitedVerticalOffset     = cscParams->chromaUpSamplingVerticalCoef;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetSfcRotMirParams(PSFC_ROT_MIR_PARAMS rotMirParams)
{
    VP_PUBLIC_CHK_NULL_RETURN(rotMirParams);

    if (m_PacketCaps.bSFC)
    {
        VP_PUBLIC_CHK_NULL_RETURN(m_sfcRender);
        VP_PUBLIC_CHK_STATUS_RETURN(m_sfcRender->SetRotMirParams(rotMirParams));
        return MOS_STATUS_SUCCESS;
    }
    else
    {
        VP_RENDER_NORMALMESSAGE("CSC/IEF for Output is enabled in SFC, pls recheck the features enabling in SFC");
        return MOS_STATUS_INVALID_PARAMETER;
    }

}

//!
//! \brief    Vebox Populate VEBOX parameters
//! \details  Populate the Vebox VEBOX state parameters to VEBOX RenderData
//! \param    [in] bDnEnabled
//!           true if DN being enabled
//! \param    [in] pChromaParams
//!           true to Chroma DN being enabled
//! \param    [in] pLumaParams
//!           Pointer to Luma DN and DI parameter
//! \param    [in] pChromaParams
//!           Pointer to Chroma DN parameter
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS VpVeboxCmdPacket::ConfigDnLumaChromaParams(
    bool                            bDnEnabled,
    bool                            bChromaDenoise,
    PVP_SAMPLER_STATE_DN_PARAM      pLumaParams,
    PVPHAL_DNUV_PARAMS              pChromaParams
    )
{
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();

    VP_PUBLIC_CHK_NULL_RETURN(pRenderData);

    MHW_VEBOX_DNDI_PARAMS &veboxDNDIParams = pRenderData->GetDNDIParams();

    // Luma Denoise Params
    if (bDnEnabled && pLumaParams)
    {
        veboxDNDIParams.dwDenoiseASDThreshold     = pLumaParams->dwDenoiseASDThreshold;
        veboxDNDIParams.dwDenoiseHistoryDelta     = pLumaParams->dwDenoiseHistoryDelta;
        veboxDNDIParams.dwDenoiseMaximumHistory   = pLumaParams->dwDenoiseMaximumHistory;
        veboxDNDIParams.dwDenoiseSTADThreshold    = pLumaParams->dwDenoiseSTADThreshold;
        veboxDNDIParams.dwDenoiseSCMThreshold     = pLumaParams->dwDenoiseSCMThreshold;
        veboxDNDIParams.dwDenoiseMPThreshold      = pLumaParams->dwDenoiseMPThreshold;
        veboxDNDIParams.dwLTDThreshold            = pLumaParams->dwLTDThreshold;
        veboxDNDIParams.dwTDThreshold             = pLumaParams->dwTDThreshold;
        veboxDNDIParams.dwGoodNeighborThreshold   = pLumaParams->dwGoodNeighborThreshold;
    }

    // Chroma Denoise Params
    if (bChromaDenoise && pChromaParams)
    {
        veboxDNDIParams.dwChromaSTADThreshold     = pChromaParams->dwSTADThresholdU; // Use U threshold for now
        veboxDNDIParams.dwChromaLTDThreshold      = pChromaParams->dwLTDThresholdU;  // Use U threshold for now
        veboxDNDIParams.dwChromaTDThreshold       = pChromaParams->dwTDThresholdU;   // Use U threshold for now
    }

    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Configure FMD parameter
//! \details  Configure FMD parameters for DNDI State
//! \param    [in] bProgressive
//!           true if sample being progressive
//! \param    [in] bAutoDenoise
//!           true if auto denoise being enabled
//! \param    [out] pLumaParams
//!           Pointer to DNDI Param for set FMD parameters
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS VpVeboxCmdPacket::ConfigFMDParams(bool bProgressive, bool bAutoDenoise)
{
    MOS_STATUS                      eStatus = MOS_STATUS_SUCCESS;
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();

    VP_PUBLIC_CHK_NULL_RETURN(pRenderData);

#if VEBOX_AUTO_DENOISE_SUPPORTED
    if (bProgressive && bAutoDenoise)
    {
        // out1 = Cur1st + Cur2nd
        pRenderData->GetDNDIParams().dwFMDFirstFieldCurrFrame =
            MEDIASTATE_DNDI_FIELDCOPY_NEXT;
        // out2 = Prv1st + Prv2nd
        pRenderData->GetDNDIParams().dwFMDSecondFieldPrevFrame =
            MEDIASTATE_DNDI_FIELDCOPY_PREV;
    }
    else
#endif
    {
        pRenderData->GetDNDIParams().dwFMDFirstFieldCurrFrame =
            MEDIASTATE_DNDI_DEINTERLACE;
        pRenderData->GetDNDIParams().dwFMDSecondFieldPrevFrame =
            MEDIASTATE_DNDI_DEINTERLACE;
    }

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::SetDnParams(
    PVEBOX_DN_PARAMS                    pDnParams)
{
    MOS_STATUS                      eStatus = MOS_STATUS_SUCCESS;
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();
    VP_SAMPLER_STATE_DN_PARAM       lumaParams = {};
    VPHAL_DNUV_PARAMS               chromaParams = {};

    VP_RENDER_ASSERT(pDnParams);
    VP_RENDER_ASSERT(pRenderData);

    pRenderData->DN.bDnEnabled = pDnParams->bDnEnabled;
    pRenderData->DN.bAutoDetect = pDnParams->bAutoDetect;
    pRenderData->DN.bChromaDnEnabled = pDnParams->bChromaDenoise;

    pRenderData->GetDNDIParams().bChromaDNEnable = pDnParams->bChromaDenoise;
    pRenderData->GetDNDIParams().bProgressiveDN = pDnParams->bDnEnabled && pDnParams->bProgressive;

    GetDnLumaParams(pDnParams->bDnEnabled, pDnParams->bAutoDetect, pDnParams->fDenoiseFactor, m_PacketCaps.bRefValid, &lumaParams);
    GetDnChromaParams(pDnParams->bChromaDenoise, pDnParams->bAutoDetect, pDnParams->fDenoiseFactor, &chromaParams);

    // Setup Denoise Params
    ConfigLumaPixRange(pDnParams->bDnEnabled, pDnParams->bAutoDetect, pDnParams->fDenoiseFactor);
    ConfigChromaPixRange(pDnParams->bChromaDenoise, pDnParams->bAutoDetect, pDnParams->fDenoiseFactor);
    ConfigDnLumaChromaParams(pDnParams->bDnEnabled, pDnParams->bChromaDenoise, &lumaParams, &chromaParams);

    // bDNDITopFirst in DNDI parameters need be configured during SetDIParams.

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::SetSteParams(
    PVEBOX_STE_PARAMS                    pSteParams)
{
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();
    MHW_VEBOX_IECP_PARAMS&           mhwVeboxIecpParams = pRenderData->GetIECPParams();

    VP_RENDER_ASSERT(pSteParams);
    VP_RENDER_ASSERT(pRenderData);

    if (pSteParams->bEnableSTE)
    {
        pRenderData->IECP.STE.bSteEnabled = true;
        mhwVeboxIecpParams.ColorPipeParams.bActive = true;
        mhwVeboxIecpParams.ColorPipeParams.bEnableSTE = true;

        if (pSteParams->dwSTEFactor > MHW_STE_FACTOR_MAX)
        {
            mhwVeboxIecpParams.ColorPipeParams.SteParams.dwSTEFactor = MHW_STE_FACTOR_MAX;
            mhwVeboxIecpParams.ColorPipeParams.SteParams.satP1 = m_satP1Table[MHW_STE_FACTOR_MAX];
            mhwVeboxIecpParams.ColorPipeParams.SteParams.satS0 = m_satS0Table[MHW_STE_FACTOR_MAX];
            mhwVeboxIecpParams.ColorPipeParams.SteParams.satS1 = m_satS1Table[MHW_STE_FACTOR_MAX];
        }
        else
        {
            mhwVeboxIecpParams.ColorPipeParams.SteParams.dwSTEFactor = pSteParams->dwSTEFactor;
            mhwVeboxIecpParams.ColorPipeParams.SteParams.satP1 = m_satP1Table[pSteParams->dwSTEFactor];
            mhwVeboxIecpParams.ColorPipeParams.SteParams.satS0 = m_satS0Table[pSteParams->dwSTEFactor];
            mhwVeboxIecpParams.ColorPipeParams.SteParams.satS1 = m_satS1Table[pSteParams->dwSTEFactor];
        }
    }
    else
    {
        pRenderData->IECP.STE.bSteEnabled = false;
        mhwVeboxIecpParams.ColorPipeParams.bEnableSTE = false;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetTccParams(
    PVEBOX_TCC_PARAMS                    pTccParams)
{
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();
    MHW_VEBOX_IECP_PARAMS&           mhwVeboxIecpParams = pRenderData->GetIECPParams();

    VP_RENDER_ASSERT(pTccParams);
    VP_RENDER_ASSERT(pRenderData);

    if (pTccParams->bEnableTCC)
    {
        pRenderData->IECP.TCC.bTccEnabled = true;
        mhwVeboxIecpParams.ColorPipeParams.bActive = true;
        mhwVeboxIecpParams.ColorPipeParams.bEnableTCC = true;
        mhwVeboxIecpParams.ColorPipeParams.TccParams.Magenta = pTccParams->Magenta;
        mhwVeboxIecpParams.ColorPipeParams.TccParams.Red = pTccParams->Red;
        mhwVeboxIecpParams.ColorPipeParams.TccParams.Yellow = pTccParams->Yellow;
        mhwVeboxIecpParams.ColorPipeParams.TccParams.Green = pTccParams->Green;
        mhwVeboxIecpParams.ColorPipeParams.TccParams.Cyan = pTccParams->Cyan;
        mhwVeboxIecpParams.ColorPipeParams.TccParams.Blue = pTccParams->Blue;
    }
    else
    {
        pRenderData->IECP.TCC.bTccEnabled = false;
        mhwVeboxIecpParams.ColorPipeParams.bEnableTCC = false;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetProcampParams(
    PVEBOX_PROCAMP_PARAMS                    pProcampParams)
{
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();
    MHW_VEBOX_IECP_PARAMS&           mhwVeboxIecpParams = pRenderData->GetIECPParams();

    VP_RENDER_ASSERT(pProcampParams);
    VP_RENDER_ASSERT(pRenderData);

    if (pProcampParams->bEnableProcamp)
    {
        pRenderData->IECP.PROCAMP.bProcampEnabled = true;
        mhwVeboxIecpParams.ProcAmpParams.bActive = true;
        mhwVeboxIecpParams.ProcAmpParams.bEnabled = true;
        mhwVeboxIecpParams.ProcAmpParams.brightness = (uint32_t)MOS_F_ROUND(pProcampParams->fBrightness * 16.0F);  // S7.4
        mhwVeboxIecpParams.ProcAmpParams.contrast   = (uint32_t)MOS_UF_ROUND(pProcampParams->fContrast * 128.0F);  // U4.7
        mhwVeboxIecpParams.ProcAmpParams.sinCS      = (uint32_t)MOS_F_ROUND(sin(MHW_DEGREE_TO_RADIAN(pProcampParams->fHue)) *
                                                         pProcampParams->fContrast *
                                                         pProcampParams->fSaturation * 256.0F);  // S7.8
        mhwVeboxIecpParams.ProcAmpParams.cosCS      = (uint32_t)MOS_F_ROUND(cos(MHW_DEGREE_TO_RADIAN(pProcampParams->fHue)) *
                                                         pProcampParams->fContrast *
                                                         pProcampParams->fSaturation * 256.0F);  // S7.8
    }
    else
    {
        pRenderData->IECP.PROCAMP.bProcampEnabled = false;
        mhwVeboxIecpParams.ProcAmpParams.bActive = false;
        mhwVeboxIecpParams.ProcAmpParams.bEnabled = false;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetDiParams(PVEBOX_DI_PARAMS diParams)
{
    MOS_STATUS                      eStatus         = MOS_STATUS_SUCCESS;
    VpVeboxRenderData               *renderData     = GetLastExecRenderData();
    VP_SAMPLER_STATE_DN_PARAM       lumaParams      = {};
    VPHAL_DNUV_PARAMS               chromaParams    = {};

    VP_PUBLIC_CHK_NULL_RETURN(diParams);
    VP_PUBLIC_CHK_NULL_RETURN(renderData);

    renderData->DI.value            = 0;
    renderData->DI.bDeinterlace     = diParams->bDiEnabled;
    renderData->DI.bQueryVariance   = false;

    // for 30i->30fps + SFC
    if (m_PacketCaps.bSFC && !diParams->b60fpsDi)
    {
        // Set BLT1's Current DI Output as BLT2's input, it is always under Mode0
        // BLT1 output 1st field of current frame for the following cases:
        if (m_DNDIFirstFrame                                                            ||
            (diParams->sampleTypeInput == SAMPLE_INTERLEAVED_ODD_FIRST_BOTTOM_FIELD) ||
            (diParams->sampleTypeInput == SAMPLE_INTERLEAVED_EVEN_FIRST_TOP_FIELD)   ||
            (diParams->sampleTypeInput == SAMPLE_SINGLE_TOP_FIELD)                   ||
            (diParams->sampleTypeInput == SAMPLE_PROGRESSIVE))
        {
            m_DIOutputFrames = MEDIA_VEBOX_DI_OUTPUT_CURRENT;
        }
        else
        {
            // First sample output - 2nd field of the previous frame
            m_DIOutputFrames = MEDIA_VEBOX_DI_OUTPUT_PREVIOUS;
        }
    }
    // for 30i->60fps or other 30i->30fps cases
    else
    {
            m_DIOutputFrames = m_DNDIFirstFrame ?
                MEDIA_VEBOX_DI_OUTPUT_CURRENT :
                MEDIA_VEBOX_DI_OUTPUT_BOTH;
    }

    VP_PUBLIC_CHK_STATUS_RETURN(SetDiParams(diParams->bDiEnabled, diParams->bSCDEnabled, diParams->bHDContent, diParams->sampleTypeInput, renderData->GetDNDIParams()));

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::SetDiParams(bool bDiEnabled, bool bSCDEnabled, bool bHDContent, VPHAL_SAMPLE_TYPE sampleTypeInput, MHW_VEBOX_DNDI_PARAMS &param)
{
    if (!bDiEnabled)
    {
        return MOS_STATUS_SUCCESS;
    }

    param.bDNDITopFirst = (sampleTypeInput == SAMPLE_INTERLEAVED_EVEN_FIRST_TOP_FIELD) ||
                        (sampleTypeInput == SAMPLE_INTERLEAVED_ODD_FIRST_TOP_FIELD);
    param.dwLumaTDMWeight            = VPHAL_VEBOX_DI_LUMA_TDM_WEIGHT_NATUAL;
    param.dwChromaTDMWeight          = VPHAL_VEBOX_DI_CHROMA_TDM_WEIGHT_NATUAL;
    param.dwSHCMDelta                = VPHAL_VEBOX_DI_SHCM_DELTA_NATUAL;
    param.dwSHCMThreshold            = VPHAL_VEBOX_DI_SHCM_THRESHOLD_NATUAL;
    param.dwSVCMDelta                = VPHAL_VEBOX_DI_SVCM_DELTA_NATUAL;
    param.dwSVCMThreshold            = VPHAL_VEBOX_DI_SVCM_THRESHOLD_NATUAL;
    param.bFasterConvergence         = false;
    param.bTDMLumaSmallerWindow      = false;
    param.bTDMChromaSmallerWindow    = false;
    param.dwLumaTDMCoringThreshold   = VPHAL_VEBOX_DI_LUMA_TDM_CORING_THRESHOLD_NATUAL;
    param.dwChromaTDMCoringThreshold = VPHAL_VEBOX_DI_CHROMA_TDM_CORING_THRESHOLD_NATUAL;
    param.bBypassDeflickerFilter     = true;
    param.bUseSyntheticContentMedian = false;
    param.bLocalCheck                = true;
    param.bSyntheticContentCheck     = false;
    param.dwDirectionCheckThreshold  = VPHAL_VEBOX_DI_DIRECTION_CHECK_THRESHOLD_NATUAL;
    param.dwTearingLowThreshold      = VPHAL_VEBOX_DI_TEARING_LOW_THRESHOLD_NATUAL;
    param.dwTearingHighThreshold     = VPHAL_VEBOX_DI_TEARING_HIGH_THRESHOLD_NATUAL;
    param.dwDiffCheckSlackThreshold  = VPHAL_VEBOX_DI_DIFF_CHECK_SLACK_THRESHOLD_NATUAL;
    param.dwSADWT0                   = VPHAL_VEBOX_DI_SAD_WT0_NATUAL;
    param.dwSADWT1                   = VPHAL_VEBOX_DI_SAD_WT1_NATUAL;
    param.dwSADWT2                   = VPHAL_VEBOX_DI_SAD_WT2_NATUAL;
    param.dwSADWT3                   = VPHAL_VEBOX_DI_SAD_WT3_NATUAL;
    param.dwSADWT4                   = VPHAL_VEBOX_DI_SAD_WT4_NATUAL;
    param.dwSADWT6                   = VPHAL_VEBOX_DI_SAD_WT6_NATUAL;
    param.bSCDEnable                 = bSCDEnabled;

    if (bHDContent)
    {
        param.dwLPFWtLUT0 = VPHAL_VEBOX_DI_LPFWTLUT0_HD_NATUAL;
        param.dwLPFWtLUT1 = VPHAL_VEBOX_DI_LPFWTLUT1_HD_NATUAL;
        param.dwLPFWtLUT2 = VPHAL_VEBOX_DI_LPFWTLUT2_HD_NATUAL;
        param.dwLPFWtLUT3 = VPHAL_VEBOX_DI_LPFWTLUT3_HD_NATUAL;
        param.dwLPFWtLUT4 = VPHAL_VEBOX_DI_LPFWTLUT4_HD_NATUAL;
        param.dwLPFWtLUT5 = VPHAL_VEBOX_DI_LPFWTLUT5_HD_NATUAL;
        param.dwLPFWtLUT6 = VPHAL_VEBOX_DI_LPFWTLUT6_HD_NATUAL;
        param.dwLPFWtLUT7 = VPHAL_VEBOX_DI_LPFWTLUT7_HD_NATUAL;
    }
    else
    {
        param.dwLPFWtLUT0 = VPHAL_VEBOX_DI_LPFWTLUT0_SD_NATUAL;
        param.dwLPFWtLUT1 = VPHAL_VEBOX_DI_LPFWTLUT1_SD_NATUAL;
        param.dwLPFWtLUT2 = VPHAL_VEBOX_DI_LPFWTLUT2_SD_NATUAL;
        param.dwLPFWtLUT3 = VPHAL_VEBOX_DI_LPFWTLUT3_SD_NATUAL;
        param.dwLPFWtLUT4 = VPHAL_VEBOX_DI_LPFWTLUT4_SD_NATUAL;
        param.dwLPFWtLUT5 = VPHAL_VEBOX_DI_LPFWTLUT5_SD_NATUAL;
        param.dwLPFWtLUT6 = VPHAL_VEBOX_DI_LPFWTLUT6_SD_NATUAL;
        param.dwLPFWtLUT7 = VPHAL_VEBOX_DI_LPFWTLUT7_SD_NATUAL;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetupDiIecpState(
    bool                        bDiScdEnable,
    PMHW_VEBOX_DI_IECP_CMD_PARAMS   pVeboxDiIecpCmdParams)
{
    uint32_t                            dwWidth                 = 0;
    uint32_t                            dwHeight                = 0;
    MHW_VEBOX_SURFACE_PARAMS            MhwVeboxSurfaceParam    = {};
    PMHW_VEBOX_INTERFACE                pVeboxInterface         = nullptr;
    MHW_VEBOX_SURFACE_CNTL_PARAMS       VeboxSurfCntlParams     = {};
    PVP_SURFACE                         pSurface                = nullptr;
    MOS_STATUS                          eStatus                 = MOS_STATUS_SUCCESS;

    VP_RENDER_CHK_NULL_RETURN(m_hwInterface);
    pVeboxInterface = m_hwInterface->m_veboxInterface;

    VP_RENDER_CHK_NULL_RETURN(pVeboxInterface);
    VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pCurrInput);
    VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pCurrInput->osSurface);
    VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pStatisticsOutput);
    VP_RENDER_CHK_NULL_RETURN(m_surfMemCacheCtl);

    MOS_ZeroMemory(pVeboxDiIecpCmdParams, sizeof(*pVeboxDiIecpCmdParams));

    // Align dwEndingX with surface state
    VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceParams(
                                    m_veboxPacketSurface.pCurrInput, &MhwVeboxSurfaceParam));
    VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->VeboxAdjustBoundary(
                                    &MhwVeboxSurfaceParam,
                                    &dwWidth,
                                    &dwHeight,
                                    m_PacketCaps.bDI));

    pVeboxDiIecpCmdParams->dwStartingX = 0;
    pVeboxDiIecpCmdParams->dwEndingX   = dwWidth - 1;

    pVeboxDiIecpCmdParams->pOsResCurrInput         = &m_veboxPacketSurface.pCurrInput->osSurface->OsResource;
    pVeboxDiIecpCmdParams->dwCurrInputSurfOffset   = m_veboxPacketSurface.pCurrInput->osSurface->dwOffset;
    pVeboxDiIecpCmdParams->CurrInputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.CurrentInputSurfMemObjCtl;

    // Update control bits for current surface
    if (m_mmc->IsMmcEnabled())
    {
        pSurface = m_veboxPacketSurface.pCurrInput;
        MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
        VeboxSurfCntlParams.bIsCompressed       = pSurface->osSurface->bIsCompressed;
        VeboxSurfCntlParams.CompressionMode     = pSurface->osSurface->CompressionMode;
        VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxSurfaceControlBits(
                                        &VeboxSurfCntlParams,
                                        (uint32_t *)&(pVeboxDiIecpCmdParams->CurrInputSurfCtrl.Value)));
    }

    // Reference surface
    if (m_veboxPacketSurface.pPrevInput)
    {
        VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pPrevInput->osSurface);
        pVeboxDiIecpCmdParams->pOsResPrevInput          = &m_veboxPacketSurface.pPrevInput->osSurface->OsResource;
        pVeboxDiIecpCmdParams->dwPrevInputSurfOffset    = m_veboxPacketSurface.pPrevInput->osSurface->dwOffset;
        pVeboxDiIecpCmdParams->PrevInputSurfCtrl.Value  = m_surfMemCacheCtl->DnDi.PreviousInputSurfMemObjCtl;

        // Update control bits for PastSurface surface
        if (m_mmc->IsMmcEnabled())
        {
            pSurface = m_veboxPacketSurface.pPrevInput;
            MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
            VeboxSurfCntlParams.bIsCompressed       = pSurface->osSurface->bIsCompressed;
            VeboxSurfCntlParams.CompressionMode     = pSurface->osSurface->CompressionMode;
            VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxSurfaceControlBits(
                &VeboxSurfCntlParams,
                (uint32_t *)&(pVeboxDiIecpCmdParams->PrevInputSurfCtrl.Value)));
        }
    }

    // VEBOX final output surface
    if (m_veboxPacketSurface.pCurrOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pCurrOutput->osSurface);
        pVeboxDiIecpCmdParams->pOsResCurrOutput         = &m_veboxPacketSurface.pCurrOutput->osSurface->OsResource;
        pVeboxDiIecpCmdParams->dwCurrOutputSurfOffset   = m_veboxPacketSurface.pCurrOutput->osSurface->dwOffset;
        pVeboxDiIecpCmdParams->CurrOutputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.CurrentOutputSurfMemObjCtl;

        // Update control bits for Current Output Surf
        if (m_mmc->IsMmcEnabled())
        {
            pSurface = m_veboxPacketSurface.pCurrOutput;
            MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
            VeboxSurfCntlParams.bIsCompressed   = pSurface->osSurface->bIsCompressed;
            VeboxSurfCntlParams.CompressionMode = pSurface->osSurface->CompressionMode;
            VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxSurfaceControlBits(
                &VeboxSurfCntlParams,
                (uint32_t *)&(pVeboxDiIecpCmdParams->CurrOutputSurfCtrl.Value)));
        }
    }

    if (m_veboxPacketSurface.pPrevOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pPrevOutput->osSurface);
        pVeboxDiIecpCmdParams->pOsResPrevOutput         = &m_veboxPacketSurface.pPrevOutput->osSurface->OsResource;
        pVeboxDiIecpCmdParams->PrevOutputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.CurrentOutputSurfMemObjCtl;

        // Update control bits for PrevOutput surface
        if (m_mmc->IsMmcEnabled())
        {
            pSurface = m_veboxPacketSurface.pPrevOutput;
            MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
            VeboxSurfCntlParams.bIsCompressed   = pSurface->osSurface->bIsCompressed;
            VeboxSurfCntlParams.CompressionMode = pSurface->osSurface->CompressionMode;
            VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxSurfaceControlBits(
                &VeboxSurfCntlParams,
                (uint32_t *)&(pVeboxDiIecpCmdParams->PrevOutputSurfCtrl.Value)));
        }
    }

    // DN intermediate output surface
    if (m_veboxPacketSurface.pDenoisedCurrOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pDenoisedCurrOutput->osSurface);
        pVeboxDiIecpCmdParams->pOsResDenoisedCurrOutput         = &m_veboxPacketSurface.pDenoisedCurrOutput->osSurface->OsResource;
        pVeboxDiIecpCmdParams->DenoisedCurrOutputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.DnOutSurfMemObjCtl;

        // Update control bits for DenoisedCurrOutputSurf surface
        if (m_mmc->IsMmcEnabled())
        {
            pSurface = m_veboxPacketSurface.pDenoisedCurrOutput;
            MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
            VeboxSurfCntlParams.bIsCompressed   = pSurface->osSurface->bIsCompressed;
            VeboxSurfCntlParams.CompressionMode = pSurface->osSurface->CompressionMode;
            VPHAL_RENDER_CHK_STATUS(pVeboxInterface->AddVeboxSurfaceControlBits(
                &VeboxSurfCntlParams,
                (uint32_t *)&(pVeboxDiIecpCmdParams->DenoisedCurrOutputSurfCtrl.Value)));
        }
    }

    // STMM surface
    if (m_veboxPacketSurface.pSTMMInput && m_veboxPacketSurface.pSTMMOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pSTMMInput->osSurface);
        VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pSTMMOutput->osSurface);

        // STMM in
        pVeboxDiIecpCmdParams->pOsResStmmInput         = &m_veboxPacketSurface.pSTMMInput->osSurface->OsResource;
        pVeboxDiIecpCmdParams->StmmInputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.STMMInputSurfMemObjCtl;

        // Update control bits for stmm input surface
        if (m_mmc->IsMmcEnabled())
        {
            pSurface = m_veboxPacketSurface.pSTMMInput;
            MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
            VeboxSurfCntlParams.bIsCompressed   = pSurface->osSurface->bIsCompressed;
            VeboxSurfCntlParams.CompressionMode = pSurface->osSurface->CompressionMode;
            VPHAL_RENDER_CHK_STATUS(pVeboxInterface->AddVeboxSurfaceControlBits(
                &VeboxSurfCntlParams,
                (uint32_t *)&(pVeboxDiIecpCmdParams->StmmInputSurfCtrl.Value)));
        }

        // STMM out
        pVeboxDiIecpCmdParams->pOsResStmmOutput         = &m_veboxPacketSurface.pSTMMOutput->osSurface->OsResource;
        pVeboxDiIecpCmdParams->StmmOutputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.STMMOutputSurfMemObjCtl;

        // Update control bits for stmm output surface
        if (m_mmc->IsMmcEnabled())
        {
            pSurface = m_veboxPacketSurface.pSTMMOutput;
            MOS_ZeroMemory(&VeboxSurfCntlParams, sizeof(VeboxSurfCntlParams));
            VeboxSurfCntlParams.bIsCompressed   = pSurface->osSurface->bIsCompressed;
            VeboxSurfCntlParams.CompressionMode = pSurface->osSurface->CompressionMode;
            VPHAL_RENDER_CHK_STATUS(pVeboxInterface->AddVeboxSurfaceControlBits(
                &VeboxSurfCntlParams,
                (uint32_t *)&(pVeboxDiIecpCmdParams->StmmOutputSurfCtrl.Value)));
        }
    }

    pVeboxDiIecpCmdParams->pOsResStatisticsOutput         = &m_veboxPacketSurface.pStatisticsOutput->osSurface->OsResource;
    pVeboxDiIecpCmdParams->StatisticsOutputSurfCtrl.Value = m_surfMemCacheCtl->DnDi.StatisticsOutputSurfMemObjCtl;

finish:
    return eStatus;
}

bool VpVeboxCmdPacket::UseKernelResource()
{
    return false;
}

MOS_STATUS VpVeboxCmdPacket::InitVeboxSurfaceParams(
    PVP_SURFACE                     pVpHalVeboxSurface,
    PMHW_VEBOX_SURFACE_PARAMS       pMhwVeboxSurface)
{
    MOS_STATUS                   eStatus = MOS_STATUS_SUCCESS;

    VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurface);
    VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurface->osSurface);
    VP_RENDER_CHK_NULL_RETURN(pMhwVeboxSurface);

    MOS_ZeroMemory(pMhwVeboxSurface, sizeof(*pMhwVeboxSurface));
    pMhwVeboxSurface->bActive                = true;
    pMhwVeboxSurface->Format                 = pVpHalVeboxSurface->osSurface->Format;
    pMhwVeboxSurface->dwWidth                = pVpHalVeboxSurface->osSurface->dwWidth;
    pMhwVeboxSurface->dwHeight               = pVpHalVeboxSurface->osSurface->dwHeight;
    pMhwVeboxSurface->dwPitch                = pVpHalVeboxSurface->osSurface->dwPitch;
    pMhwVeboxSurface->dwBitDepth             = pVpHalVeboxSurface->osSurface->dwDepth;
    pMhwVeboxSurface->TileType               = pVpHalVeboxSurface->osSurface->TileType;
    pMhwVeboxSurface->TileModeGMM            = pVpHalVeboxSurface->osSurface->TileModeGMM;
    pMhwVeboxSurface->bGMMTileEnabled        = pVpHalVeboxSurface->osSurface->bGMMTileEnabled;
    if (pVpHalVeboxSurface->rcMaxSrc.top == pVpHalVeboxSurface->rcMaxSrc.bottom ||
        pVpHalVeboxSurface->rcMaxSrc.left == pVpHalVeboxSurface->rcMaxSrc.right)
    {
        // If rcMaxSrc is invalid, just use rcSrc.
        pMhwVeboxSurface->rcMaxSrc           = pVpHalVeboxSurface->rcSrc;
    }
    else
    {
        pMhwVeboxSurface->rcMaxSrc           = pVpHalVeboxSurface->rcMaxSrc;
    }
    pMhwVeboxSurface->rcSrc                  = pVpHalVeboxSurface->rcSrc;
    pMhwVeboxSurface->bVEBOXCroppingUsed     = pVpHalVeboxSurface->bVEBOXCroppingUsed;
    pMhwVeboxSurface->pOsResource            = &pVpHalVeboxSurface->osSurface->OsResource;
    pMhwVeboxSurface->bIsCompressed          = pVpHalVeboxSurface->osSurface->bIsCompressed;

    if (pVpHalVeboxSurface->osSurface->dwPitch > 0)
    {
        pMhwVeboxSurface->dwUYoffset = ((pVpHalVeboxSurface->osSurface->UPlaneOffset.iSurfaceOffset - pVpHalVeboxSurface->osSurface->YPlaneOffset.iSurfaceOffset) / pVpHalVeboxSurface->osSurface->dwPitch)
                                       + pVpHalVeboxSurface->osSurface->UPlaneOffset.iYOffset;
    }
    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::SendVeboxCmd(MOS_COMMAND_BUFFER* commandBuffer)
{
    MOS_STATUS                              eStatus;
    int32_t                                 iRemaining;
    MHW_VEBOX_DI_IECP_CMD_PARAMS            VeboxDiIecpCmdParams;
    VPHAL_VEBOX_SURFACE_STATE_CMD_PARAMS    VeboxSurfaceStateCmdParams;
    MHW_VEBOX_SURFACE_STATE_CMD_PARAMS      MhwVeboxSurfaceStateCmdParams;
    MHW_VEBOX_STATE_CMD_PARAMS              VeboxStateCmdParams;
    MHW_MI_FLUSH_DW_PARAMS                  FlushDwParams;
    PMHW_VEBOX_INTERFACE                    pVeboxInterface;
    RENDERHAL_GENERIC_PROLOG_PARAMS         GenericPrologParams;

    eStatus                 = MOS_STATUS_SUCCESS;
    pVeboxInterface         = m_hwInterface->m_veboxInterface;
    iRemaining              = 0;

    VP_RENDER_CHK_NULL_RETURN(commandBuffer);

    eStatus = PrepareVeboxCmd(
                  commandBuffer,
                  GenericPrologParams,
                  iRemaining);

    if (eStatus != MOS_STATUS_SUCCESS)
    {
        CmdErrorHanlde(commandBuffer, iRemaining);
    }
    else
    {
        eStatus = RenderVeboxCmd(
                      commandBuffer,
                      VeboxDiIecpCmdParams,
                      VeboxSurfaceStateCmdParams,
                      MhwVeboxSurfaceStateCmdParams,
                      VeboxStateCmdParams,
                      FlushDwParams,
                      &GenericPrologParams);
        if (eStatus != MOS_STATUS_SUCCESS)
        {
          // Failed -> discard all changes in Command Buffer
            CmdErrorHanlde(commandBuffer, iRemaining);
        }
    }

    return eStatus;
}

void VpVeboxCmdPacket::CmdErrorHanlde(
    MOS_COMMAND_BUFFER  *CmdBuffer,
    int32_t             &iRemaining)
{
    int32_t     i= 0;

    VP_PUBLIC_CHK_NULL_NO_STATUS_RETURN(CmdBuffer);
    // Buffer overflow - display overflow size
    if (CmdBuffer->iRemaining < 0)
    {
        VP_RENDER_ASSERTMESSAGE("Command Buffer overflow by %d bytes", CmdBuffer->iRemaining);
    }

    // Move command buffer back to beginning
    i = iRemaining - CmdBuffer->iRemaining;
    CmdBuffer->iRemaining = iRemaining;
    CmdBuffer->iOffset -= i;
    CmdBuffer->pCmdPtr = CmdBuffer->pCmdBase + CmdBuffer->iOffset / sizeof(uint32_t);
}

MOS_STATUS VpVeboxCmdPacket::PrepareVeboxCmd(
    MOS_COMMAND_BUFFER*                      CmdBuffer,
    RENDERHAL_GENERIC_PROLOG_PARAMS&         GenericPrologParams,
    int32_t&                                 iRemaining)
{
    MOS_STATUS                              eStatus      = MOS_STATUS_SUCCESS;
    PMOS_INTERFACE                          pOsInterface = m_hwInterface->m_osInterface;
    VpVeboxRenderData                       *pRenderData  = GetLastExecRenderData();
    PMOS_RESOURCE                           gpuStatusBuffer = nullptr;

    VP_RENDER_CHK_NULL_RETURN(CmdBuffer);
    VP_RENDER_CHK_NULL_RETURN(pOsInterface);
    VP_RENDER_CHK_NULL_RETURN(m_currentSurface);
    VP_RENDER_CHK_NULL_RETURN(m_currentSurface->osSurface);

    // Set initial state
    iRemaining = CmdBuffer->iRemaining;

    //---------------------------
    // Set Performance Tags
    //---------------------------
    VP_RENDER_CHK_STATUS_RETURN(VeboxSetPerfTag());
    pOsInterface->pfnResetPerfBufferID(pOsInterface);
    pOsInterface->pfnSetPerfTag(pOsInterface, pRenderData->PerfTag);

    MOS_ZeroMemory(&GenericPrologParams, sizeof(GenericPrologParams));

    // Linux will do nothing here since currently no frame tracking support
   #ifndef EMUL
     if(pOsInterface->bEnableKmdMediaFrameTracking)
     {
         // Get GPU Status buffer
         VP_RENDER_CHK_STATUS_RETURN(pOsInterface->pfnGetGpuStatusBufferResource(pOsInterface, gpuStatusBuffer));
         VP_RENDER_CHK_NULL_RETURN(gpuStatusBuffer);
         // Register the buffer
         VP_RENDER_CHK_STATUS_RETURN(pOsInterface->pfnRegisterResource(pOsInterface, gpuStatusBuffer, true, true));

         GenericPrologParams.bEnableMediaFrameTracking = true;
         GenericPrologParams.presMediaFrameTrackingSurface = gpuStatusBuffer;
         GenericPrologParams.dwMediaFrameTrackingTag = pOsInterface->pfnGetGpuStatusTag(pOsInterface, pOsInterface->CurrentGpuContextOrdinal);
         GenericPrologParams.dwMediaFrameTrackingAddrOffset = pOsInterface->pfnGetGpuStatusTagOffset(pOsInterface, pOsInterface->CurrentGpuContextOrdinal);

         // Increment GPU Status Tag
         pOsInterface->pfnIncrementGpuStatusTag(pOsInterface, pOsInterface->CurrentGpuContextOrdinal);
     }
   #endif

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::RenderVeboxCmd(
    MOS_COMMAND_BUFFER                      *CmdBuffer,
    MHW_VEBOX_DI_IECP_CMD_PARAMS            &VeboxDiIecpCmdParams,
    VPHAL_VEBOX_SURFACE_STATE_CMD_PARAMS    &VeboxSurfaceStateCmdParams,
    MHW_VEBOX_SURFACE_STATE_CMD_PARAMS      &MhwVeboxSurfaceStateCmdParams,
    MHW_VEBOX_STATE_CMD_PARAMS              &VeboxStateCmdParams,
    MHW_MI_FLUSH_DW_PARAMS                  &FlushDwParams,
    PRENDERHAL_GENERIC_PROLOG_PARAMS        pGenericPrologParams)
{
    MOS_STATUS            eStatus = MOS_STATUS_SUCCESS;
    PRENDERHAL_INTERFACE  pRenderHal;
    PMOS_INTERFACE        pOsInterface;
    PMHW_MI_INTERFACE     pMhwMiInterface;
    PMHW_VEBOX_INTERFACE  pVeboxInterface;
    bool                  bDiVarianceEnable;
    const MHW_VEBOX_HEAP *pVeboxHeap     = nullptr;
    VpVeboxRenderData *   pRenderData    = GetLastExecRenderData();
    MediaPerfProfiler *   pPerfProfiler  = nullptr;
    MOS_CONTEXT *         pOsContext     = nullptr;
    PMHW_MI_MMIOREGISTERS pMmioRegisters = nullptr;
    MOS_COMMAND_BUFFER    CmdBufferInUse;
    PMOS_COMMAND_BUFFER   pCmdBufferInUse = nullptr;
    uint32_t              curPipe         = 0;
    uint8_t               inputPipe       = 0;
    uint32_t              numPipe         = 1;
    bool                  bMultipipe      = false;

    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_renderHal);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_mhwMiInterface);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_osInterface);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_veboxInterface);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_osInterface->pOsContext);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_mhwMiInterface->GetMmioRegisters());
    VP_RENDER_CHK_NULL_RETURN(pRenderData);
    VP_RENDER_CHK_NULL_RETURN(CmdBuffer);

    pRenderHal      = m_hwInterface->m_renderHal;
    pMhwMiInterface = m_hwInterface->m_mhwMiInterface;
    pOsInterface    = m_hwInterface->m_osInterface;
    pVeboxInterface = m_hwInterface->m_veboxInterface;
    pPerfProfiler   = pRenderHal->pPerfProfiler;
    pOsContext      = m_hwInterface->m_osInterface->pOsContext;
    pMmioRegisters  = pMhwMiInterface->GetMmioRegisters();
    pCmdBufferInUse = CmdBuffer;

    auto scalability = GetMediaScalability();

    VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->GetVeboxHeapInfo(&pVeboxHeap));
    VP_RENDER_CHK_NULL_RETURN(pVeboxHeap);

#ifdef _MMC_SUPPORTED

    VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->setVeboxPrologCmd(pMhwMiInterface, CmdBuffer));

#endif

    // Initialize the scalability
    curPipe    = scalability->GetCurrentPipe();
    inputPipe  = (uint8_t)curPipe;
    numPipe    = scalability->GetPipeNumber();
    bMultipipe = (numPipe > 1) ? true : false;

    bDiVarianceEnable = m_PacketCaps.bDI;

    SetupSurfaceStates(
        &VeboxSurfaceStateCmdParams);

    SetupVeboxState(
        &VeboxStateCmdParams);

    VP_RENDER_CHK_STATUS_RETURN(SetupDiIecpState(
        bDiVarianceEnable,
        &VeboxDiIecpCmdParams));

    VP_RENDER_CHK_STATUS_RETURN(IsCmdParamsValid(
        VeboxStateCmdParams,
        VeboxDiIecpCmdParams,
        VeboxSurfaceStateCmdParams));

    // Initialize command buffer and insert prolog
    VP_RENDER_CHK_STATUS_RETURN(InitCmdBufferWithVeParams(pRenderHal, *CmdBuffer, pGenericPrologParams));

    //---------------------------------
    // Initialize Vebox Surface State Params
    //---------------------------------
    VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceStateCmdParams(
        &VeboxSurfaceStateCmdParams, &MhwVeboxSurfaceStateCmdParams));

    for (curPipe = 0; curPipe < numPipe; curPipe++)
    {
        if (bMultipipe)
        {
            // initialize the command buffer struct
            MOS_ZeroMemory(&CmdBufferInUse, sizeof(MOS_COMMAND_BUFFER));

            scalability->SetCurrentPipeIndex((uint8_t)curPipe);
            scalability->GetCmdBuffer(&CmdBufferInUse);
            pCmdBufferInUse = &CmdBufferInUse;

            pVeboxInterface->SetVeboxIndex(curPipe, numPipe, m_IsSfcUsed);
        }
        else
        {
            pCmdBufferInUse = CmdBuffer;
        }

        HalOcaInterface::On1stLevelBBStart(*pCmdBufferInUse, *pOsContext, pOsInterface->CurrentGpuContextHandle, *pMhwMiInterface, *pMmioRegisters);

        char ocaMsg[] = "VP APG Vebox Packet";
        HalOcaInterface::TraceMessage(*pCmdBufferInUse, *pOsContext, ocaMsg, sizeof(ocaMsg));

        // Add vphal param to log.
        HalOcaInterface::DumpVphalParam(*pCmdBufferInUse, *pOsContext, pRenderHal->pVphalOcaDumper);

        VP_RENDER_CHK_STATUS_RETURN(pPerfProfiler->AddPerfCollectStartCmd((void *)pRenderHal, pOsInterface, pRenderHal->pMhwMiInterface, pCmdBufferInUse));

        VP_RENDER_CHK_STATUS_RETURN(NullHW::StartPredicate(pRenderHal->pMhwMiInterface, pCmdBufferInUse));

        // Add compressible info of input/output surface to log
        if (this->m_currentSurface && VeboxSurfaceStateCmdParams.pSurfOutput)
        {
            std::string info   = "in_comps = " + std::to_string(int(this->m_currentSurface->osSurface->bCompressible)) + ", out_comps = " + std::to_string(int(VeboxSurfaceStateCmdParams.pSurfOutput->osSurface->bCompressible));
            const char *ocaLog = info.c_str();
            HalOcaInterface::TraceMessage(*pCmdBufferInUse, *pOsContext, ocaLog, info.size());
        }

        if (bMultipipe)
        {
            // Insert prolog with VE params
#ifdef _MMC_SUPPORTED

            VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->setVeboxPrologCmd(pMhwMiInterface, pCmdBufferInUse));

#endif

            MHW_GENERIC_PROLOG_PARAMS genericPrologParams;
            MOS_ZeroMemory(&genericPrologParams, sizeof(genericPrologParams));
            genericPrologParams.pOsInterface  = pRenderHal->pOsInterface;
            genericPrologParams.pvMiInterface = pRenderHal->pMhwMiInterface;
            genericPrologParams.bMmcEnabled   = pGenericPrologParams ? pGenericPrologParams->bMmcEnabled : false;
            VP_RENDER_CHK_STATUS_RETURN(Mhw_SendGenericPrologCmd(pCmdBufferInUse, &genericPrologParams));

            VP_RENDER_CHK_STATUS_RETURN(scalability->SyncPipe(syncAllPipes, 0, pCmdBufferInUse));

            // Enable Watchdog Timer
            VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddWatchdogTimerStartCmd(pCmdBufferInUse));

#if (_DEBUG || _RELEASE_INTERNAL)
            // Add noop for simu no output issue
            if (curPipe > 0)
            {
                pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                if (m_IsSfcUsed)
                {
                    pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                    pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                    pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                    pMhwMiInterface->AddMiNoop(pCmdBufferInUse, nullptr);
                }
            }
#endif
        }

        //---------------------------------
        // Send CMD: Vebox_State
        //---------------------------------
        VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxState(
            pCmdBufferInUse,
            &VeboxStateCmdParams,
            0));

        //---------------------------------
        // Send CMD: Vebox_Surface_State
        //---------------------------------
        VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxSurfaces(
            pCmdBufferInUse,
            &MhwVeboxSurfaceStateCmdParams));

        //---------------------------------
        // Send CMD: SFC pipe commands
        //---------------------------------
        if (m_IsSfcUsed)
        {
            VP_RENDER_CHK_NULL_RETURN(m_sfcRender);

            if (bMultipipe)
            {
                VP_RENDER_CHK_STATUS_RETURN(m_sfcRender->SetSfcPipe(curPipe, numPipe));
            }

            VP_RENDER_CHK_STATUS_RETURN(m_sfcRender->SetupSfcState(m_renderTarget));

            VP_RENDER_CHK_STATUS_RETURN(m_sfcRender->SendSfcCmd(
                (pRenderData->DI.bDeinterlace || pRenderData->DN.bDnEnabled),
                pCmdBufferInUse));
        }

        HalOcaInterface::OnDispatch(*pCmdBufferInUse, *pOsContext, *pMhwMiInterface, *pMmioRegisters);

        //---------------------------------
        // Send CMD: Vebox_DI_IECP
        //---------------------------------
        VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AddVeboxDiIecp(
            pCmdBufferInUse,
            &VeboxDiIecpCmdParams));

        if (bMultipipe)
        {
            // MI FlushDw, for vebox output green block issue
            MOS_ZeroMemory(&FlushDwParams, sizeof(FlushDwParams));
            VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddMiFlushDwCmd(pCmdBufferInUse, &FlushDwParams));

            VP_RENDER_CHK_STATUS_RETURN(scalability->SyncPipe(syncAllPipes, 0, pCmdBufferInUse));
        }

        //---------------------------------
        // Write GPU Status Tag for Tag based synchronization
        //---------------------------------
        if (!pOsInterface->bEnableKmdMediaFrameTracking)
        {
            VP_RENDER_CHK_STATUS_RETURN(SendVecsStatusTag(
                pMhwMiInterface,
                pOsInterface,
                pCmdBufferInUse));
        }

        //---------------------------------
        // Write Sync tag for Vebox Heap Synchronization
        // If KMD frame tracking is on, the synchronization of Vebox Heap will use Status tag which
        // is updated using KMD frame tracking.
        //---------------------------------
        if (!pOsInterface->bEnableKmdMediaFrameTracking)
        {
            MOS_ZeroMemory(&FlushDwParams, sizeof(FlushDwParams));
            FlushDwParams.pOsResource      = (PMOS_RESOURCE)&pVeboxHeap->DriverResource;
            FlushDwParams.dwResourceOffset = pVeboxHeap->uiOffsetSync;
            FlushDwParams.dwDataDW1        = pVeboxHeap->dwNextTag;
            VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddMiFlushDwCmd(
                pCmdBufferInUse,
                &FlushDwParams));
        }

        if (bMultipipe)
        {
            // Disable Watchdog Timer
            VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddWatchdogTimerStopCmd(pCmdBufferInUse));
        }

        VP_RENDER_CHK_STATUS_RETURN(NullHW::StopPredicate(pRenderHal->pMhwMiInterface, pCmdBufferInUse));

        VP_RENDER_CHK_STATUS_RETURN(pPerfProfiler->AddPerfCollectEndCmd((void *)pRenderHal, pOsInterface, pRenderHal->pMhwMiInterface, pCmdBufferInUse));

        HalOcaInterface::On1stLevelBBEnd(*pCmdBufferInUse, *pOsInterface);

        if (pOsInterface->bNoParsingAssistanceInKmd)
        {
            VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddMiBatchBufferEnd(
                pCmdBufferInUse,
                nullptr));
        }
        else if (RndrCommonIsMiBBEndNeeded(pOsInterface))
        {
            // Add Batch Buffer end command (HW/OS dependent)
            VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddMiBatchBufferEnd(
                pCmdBufferInUse,
                nullptr));
        }

        if (bMultipipe)
        {
            scalability->ReturnCmdBuffer(pCmdBufferInUse);
        }
    }

    if (bMultipipe)
    {
        scalability->SetCurrentPipeIndex(inputPipe);
        WriteUserFeature(__MEDIA_USER_FEATURE_VALUE_ENABLE_VEBOX_SCALABILITY_MODE_ID, true, m_hwInterface->m_osInterface->pOsContext);
    }
    else
    {
        WriteUserFeature(__MEDIA_USER_FEATURE_VALUE_ENABLE_VEBOX_SCALABILITY_MODE_ID, false, m_hwInterface->m_osInterface->pOsContext);
    }

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::InitVeboxSurfaceStateCmdParams(
    PVPHAL_VEBOX_SURFACE_STATE_CMD_PARAMS    pVpHalVeboxSurfaceStateCmdParams,
    PMHW_VEBOX_SURFACE_STATE_CMD_PARAMS      pMhwVeboxSurfaceStateCmdParams)
{
    MOS_STATUS                       eStatus = MOS_STATUS_SUCCESS;

    VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurfaceStateCmdParams);
    VP_RENDER_CHK_NULL_RETURN(pMhwVeboxSurfaceStateCmdParams);

    MOS_ZeroMemory(pMhwVeboxSurfaceStateCmdParams, sizeof(*pMhwVeboxSurfaceStateCmdParams));

    pMhwVeboxSurfaceStateCmdParams->bDIEnable       = pVpHalVeboxSurfaceStateCmdParams->bDIEnable;
    pMhwVeboxSurfaceStateCmdParams->b3DlutEnable    = pVpHalVeboxSurfaceStateCmdParams->b3DlutEnable;

    if (pVpHalVeboxSurfaceStateCmdParams->pSurfInput)
    {
        VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurfaceStateCmdParams->pSurfInput->osSurface);
        VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceParams(
                                      pVpHalVeboxSurfaceStateCmdParams->pSurfInput,
                                      &pMhwVeboxSurfaceStateCmdParams->SurfInput));
        pMhwVeboxSurfaceStateCmdParams->SurfInput.dwYoffset = pVpHalVeboxSurfaceStateCmdParams->pSurfInput->osSurface->YPlaneOffset.iYOffset;
    }
    if (pVpHalVeboxSurfaceStateCmdParams->pSurfOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurfaceStateCmdParams->pSurfOutput->osSurface);
        pMhwVeboxSurfaceStateCmdParams->bOutputValid = true;
        VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceParams(
                                      pVpHalVeboxSurfaceStateCmdParams->pSurfOutput,
                                      &pMhwVeboxSurfaceStateCmdParams->SurfOutput));
        pMhwVeboxSurfaceStateCmdParams->SurfOutput.dwYoffset = pVpHalVeboxSurfaceStateCmdParams->pSurfOutput->osSurface->YPlaneOffset.iYOffset;
    }
    if (pVpHalVeboxSurfaceStateCmdParams->pSurfSTMM)
    {
        VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurfaceStateCmdParams->pSurfSTMM->osSurface);
        VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceParams(
                                      pVpHalVeboxSurfaceStateCmdParams->pSurfSTMM,
                                      &pMhwVeboxSurfaceStateCmdParams->SurfSTMM));
    }
    if (pVpHalVeboxSurfaceStateCmdParams->pSurfDNOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurfaceStateCmdParams->pSurfDNOutput->osSurface);
        VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceParams(
                                      pVpHalVeboxSurfaceStateCmdParams->pSurfDNOutput,
                                      &pMhwVeboxSurfaceStateCmdParams->SurfDNOutput));
        pMhwVeboxSurfaceStateCmdParams->SurfDNOutput.dwYoffset = pVpHalVeboxSurfaceStateCmdParams->pSurfDNOutput->osSurface->YPlaneOffset.iYOffset;
    }
    if (pVpHalVeboxSurfaceStateCmdParams->pSurfSkinScoreOutput)
    {
        VP_RENDER_CHK_NULL_RETURN(pVpHalVeboxSurfaceStateCmdParams->pSurfSkinScoreOutput->osSurface);
        VP_RENDER_CHK_STATUS_RETURN(InitVeboxSurfaceParams(
                                      pVpHalVeboxSurfaceStateCmdParams->pSurfSkinScoreOutput,
                                      &pMhwVeboxSurfaceStateCmdParams->SurfSkinScoreOutput));
    }

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::SendVecsStatusTag(
    PMHW_MI_INTERFACE                   pMhwMiInterface,
    PMOS_INTERFACE                      pOsInterface,
    PMOS_COMMAND_BUFFER                 pCmdBuffer)
{
    PMOS_RESOURCE                       gpuStatusBuffer = nullptr;
    MHW_MI_FLUSH_DW_PARAMS              FlushDwParams;
    MOS_STATUS                          eStatus = MOS_STATUS_SUCCESS;

    //------------------------------------
    VP_RENDER_CHK_NULL_RETURN(pMhwMiInterface);
    VP_RENDER_CHK_NULL_RETURN(pOsInterface);
    VP_RENDER_CHK_NULL_RETURN(pCmdBuffer);

    // Get GPU Status buffer
    pOsInterface->pfnGetGpuStatusBufferResource(pOsInterface, gpuStatusBuffer);
    VP_RENDER_CHK_NULL_RETURN(gpuStatusBuffer);

    // Register the buffer
    VP_RENDER_CHK_STATUS_RETURN(pOsInterface->pfnRegisterResource(
                                  pOsInterface,
                                  gpuStatusBuffer,
                                  true,
                                  true));

    MOS_ZeroMemory(&FlushDwParams, sizeof(FlushDwParams));
    FlushDwParams.pOsResource       = gpuStatusBuffer;
    FlushDwParams.dwResourceOffset  = pOsInterface->pfnGetGpuStatusTagOffset(pOsInterface, MOS_GPU_CONTEXT_VEBOX);
    FlushDwParams.dwDataDW1         = pOsInterface->pfnGetGpuStatusTag(pOsInterface, MOS_GPU_CONTEXT_VEBOX);
    VP_RENDER_CHK_STATUS_RETURN(pMhwMiInterface->AddMiFlushDwCmd(
                                  pCmdBuffer,
                                  &FlushDwParams));

    // Increase buffer tag for next usage
    pOsInterface->pfnIncrementGpuStatusTag(pOsInterface, MOS_GPU_CONTEXT_VEBOX);

    return eStatus;
}

bool VpVeboxCmdPacket::RndrCommonIsMiBBEndNeeded(
    PMOS_INTERFACE           pOsInterface)
{
    bool needed = false;

    if (nullptr == pOsInterface)
        return false;

    return needed;
}

MOS_STATUS VpVeboxCmdPacket::InitSfcRender()
{
    if (nullptr == m_sfcRender)
    {
        VP_RENDER_CHK_NULL_RETURN(m_hwInterface);
        VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_vpPlatformInterface);
        VP_RENDER_CHK_STATUS_RETURN(m_hwInterface->m_vpPlatformInterface->CreateSfcRender(
            m_sfcRender,
            *m_hwInterface,
            m_allocator));
        VP_RENDER_CHK_NULL_RETURN(m_sfcRender);
    }
    VP_PUBLIC_CHK_STATUS_RETURN(m_sfcRender->Init());
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::Init()
{
    MOS_STATUS    eStatus = MOS_STATUS_SUCCESS;
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_skuTable);

    VP_RENDER_CHK_STATUS_RETURN(InitSfcRender());

    if (nullptr == m_currentSurface)
    {
        m_currentSurface = m_allocator->AllocateVpSurface();
        VP_CHK_SPACE_NULL_RETURN(m_currentSurface);
    }
    else
    {
        m_currentSurface->Clean();
    }

    if (nullptr == m_previousSurface)
    {
        m_previousSurface = m_allocator->AllocateVpSurface();
        VP_CHK_SPACE_NULL_RETURN(m_previousSurface);
    }
    else
    {
        m_previousSurface->Clean();
    }

    if (nullptr == m_renderTarget)
    {
        m_renderTarget = m_allocator->AllocateVpSurface();
        VP_CHK_SPACE_NULL_RETURN(m_renderTarget);
    }
    else
    {
        m_renderTarget->Clean();
    }

    MOS_ZeroMemory(&m_veboxPacketSurface, sizeof(VEBOX_PACKET_SURFACE_PARAMS));
    m_surfSetting.Clean();

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::Prepare()
{
    VP_FUNC_CALL();

    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::PrepareState()
{
    VP_FUNC_CALL();

    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    if (m_packetResourcesdPrepared)
    {
        VP_RENDER_NORMALMESSAGE("Resource Prepared, skip this time");
        return MOS_STATUS_SUCCESS;
    }

    VP_RENDER_CHK_STATUS_RETURN(SetupIndirectStates());

    VP_RENDER_CHK_STATUS_RETURN(UpdateVeboxStates());

    m_packetResourcesdPrepared = true;

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::PacketInit(
    VP_SURFACE                          *inputSurface,
    VP_SURFACE                          *outputSurface,
    VP_SURFACE                          *previousSurface,
    VP_SURFACE_SETTING                  &surfSetting,
    VP_EXECUTE_CAPS                     packetCaps)
{
    VP_FUNC_CALL();

    VpVeboxRenderData       *pRenderData = GetLastExecRenderData();
    m_packetResourcesdPrepared = false;

    VP_RENDER_CHK_NULL_RETURN(pRenderData);
    VP_RENDER_CHK_NULL_RETURN(inputSurface);
    VP_RENDER_CHK_NULL_RETURN(outputSurface);
    VP_RENDER_CHK_STATUS_RETURN(pRenderData->Init());

    m_PacketCaps      = packetCaps;

    VP_RENDER_CHK_STATUS_RETURN(Init());
    VP_RENDER_CHK_NULL_RETURN(m_allocator);
    VP_RENDER_CHK_NULL_RETURN(m_currentSurface);
    VP_RENDER_CHK_NULL_RETURN(m_renderTarget);
    VP_RENDER_CHK_NULL_RETURN(m_previousSurface);

    VP_RENDER_CHK_STATUS_RETURN(InitSurfMemCacheControl(packetCaps));

    m_IsSfcUsed = packetCaps.bSFC;

    //update VEBOX resource GMM resource usage type
    m_allocator->UpdateResourceUsageType(&inputSurface->osSurface->OsResource, MOS_HW_RESOURCE_USAGE_VP_INPUT_PICTURE_FF);
    m_allocator->UpdateResourceUsageType(&outputSurface->osSurface->OsResource, MOS_HW_RESOURCE_USAGE_VP_OUTPUT_PICTURE_FF);

    // Set current src = current primary input
    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator->CopyVpSurface(*m_renderTarget ,*outputSurface));

    // Init packet surface params.
    m_surfSetting                                   = surfSetting;
    m_veboxPacketSurface.pCurrInput                 = GetSurface(SurfaceTypeVeboxInput);
    m_veboxPacketSurface.pStatisticsOutput          = GetSurface(SurfaceTypeStatistics);
    m_veboxPacketSurface.pCurrOutput                = GetSurface(SurfaceTypeVeboxCurrentOutput);
    m_veboxPacketSurface.pPrevInput                 = GetSurface(SurfaceTypeVeboxPreviousInput);
    m_veboxPacketSurface.pSTMMInput                 = GetSurface(SurfaceTypeSTMMIn);
    m_veboxPacketSurface.pSTMMOutput                = GetSurface(SurfaceTypeSTMMOut);
    m_veboxPacketSurface.pDenoisedCurrOutput        = GetSurface(SurfaceTypeDNOutput);
    m_veboxPacketSurface.pPrevOutput                = GetSurface(SurfaceTypeVeboxPreviousOutput);
    m_veboxPacketSurface.pAlphaOrVignette           = GetSurface(SurfaceTypeAlphaOrVignette);
    m_veboxPacketSurface.pLaceOrAceOrRgbHistogram   = GetSurface(SurfaceTypeLaceAceRGBHistogram);
    m_veboxPacketSurface.pSurfSkinScoreOutput       = GetSurface(SurfaceTypeSkinScore);

    VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pStatisticsOutput);
    VP_RENDER_CHK_NULL_RETURN(m_veboxPacketSurface.pLaceOrAceOrRgbHistogram);

    m_DNDIFirstFrame = (!m_PacketCaps.bRefValid && (m_PacketCaps.bDN || m_PacketCaps.bDI));
    m_DIOutputFrames = MEDIA_VEBOX_DI_OUTPUT_CURRENT;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::Submit(MOS_COMMAND_BUFFER* commandBuffer, uint8_t packetPhase)
{
    MOS_STATUS    eStatus = MOS_STATUS_SUCCESS;
    VpVeboxRenderData   *pRenderData = GetLastExecRenderData();
    VP_FUNC_CALL();

    if (m_currentSurface && m_currentSurface->osSurface)
    {
        // Ensure the input is ready to be read
        // Currently, mos RegisterResourcere cannot sync the 3d resource.
        // Temporaly, call sync resource to do the sync explicitly.
        // Sync need be done after switching context.
#if MOS_MEDIASOLO_SUPPORTED
        if (!m_hwInterface->m_osInterface->bSoloInUse)
#endif
        {
            m_allocator->SyncOnResource(
                &m_currentSurface->osSurface->OsResource,
                false);
        }
    }

    // Send vebox command
    VP_RENDER_CHK_STATUS_RETURN(SendVeboxCmd(commandBuffer));

    return eStatus;
}

void VpVeboxCmdPacket::CopySurfaceValue(
    VP_SURFACE                  *pTargetSurface,
    VP_SURFACE                  *pSourceSurface)
{
    if (pTargetSurface == nullptr)
    {
        VP_RENDER_ASSERTMESSAGE("Input pTargetSurface is null");
        return;
    }
    *pTargetSurface = *pSourceSurface;
}

VpVeboxCmdPacket::VpVeboxCmdPacket(
    MediaTask * task,
    PVP_MHWINTERFACE hwInterface,
    PVpAllocator &allocator,
    VPMediaMemComp *mmc):
    CmdPacket(task),
    VpCmdPacket(task, hwInterface, allocator, mmc, VP_PIPELINE_PACKET_VEBOX)
{

}

VpVeboxCmdPacket:: ~VpVeboxCmdPacket()
{
    MOS_Delete(m_sfcRender);
    MOS_Delete(m_lastExecRenderData);
    MOS_Delete(m_surfMemCacheCtl);

    m_allocator->DestroyVpSurface(m_currentSurface);
    m_allocator->DestroyVpSurface(m_previousSurface);
    m_allocator->DestroyVpSurface(m_renderTarget);
}

//!
//! \brief    Calculate offsets of statistics surface address based on the
//!           functions which were enabled in the previous call,
//!           and store the width and height of the per-block statistics into DNDI_STATE
//! \details
//! Layout of Statistics surface when Temporal DI enabled
//!     --------------------------------------------------------------\n
//!     | 16 bytes for x=0, Y=0       | 16 bytes for x=16, Y=0       | ...\n
//!     |-------------------------------------------------------------\n
//!     | 16 bytes for x=0, Y=4       | ...\n
//!     |------------------------------\n
//!     | ...\n
//!     |------------------------------\n
//!     | 16 bytes for x=0, Y=height-4| ...\n
//!     |-----------------------------------------------Pitch----------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 0 (Previous)| 17 DW Reserved         | 2 DW STD0 | 2 DW GCC0 | 11 DW Reserved |\n
//!     |--------------------------------------------------------------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 0 (Current) | 11 DW FMD0 | 6 DW GNE0 | 2 DW STD0 | 2 DW GCC0 | 11 DW Reserved |\n
//!     |--------------------------------------------------------------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 1 (Previous)| 17 DW Reserved         | 2 DW STD1 | 2 DW GCC1 | 11 DW Reserved |\n
//!     |--------------------------------------------------------------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 1 (Current) | 11 DW FMD1 | 6 DW GNE1 | 2 DW STD1 | 2 DW GCC1 | 11 DW Reserved |\n
//!     ---------------------------------------------------------------------------------------------------------------\n
//!
//! Layout of Statistics surface when DN or Spatial DI enabled (and Temporal DI disabled)
//!     --------------------------------------------------------------\n
//!     | 16 bytes for x=0, Y=0       | 16 bytes for x=16, Y=0       | ...\n
//!     |-------------------------------------------------------------\n
//!     | 16 bytes for x=0, Y=4       | ...\n
//!     |------------------------------\n
//!     | ...\n
//!     |------------------------------\n
//!     | 16 bytes for x=0, Y=height-4| ...\n
//!     |-----------------------------------------------Pitch----------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 0 (Input)   | 11 DW FMD0 | 6 DW GNE0 | 2 DW STD0 | 2 DW GCC0 | 11 DW Reserved |\n
//!     |--------------------------------------------------------------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 1 (Input)   | 11 DW FMD1 | 6 DW GNE1 | 2 DW STD1 | 2 DW GCC1 | 11 DW Reserved |\n
//!     ---------------------------------------------------------------------------------------------------------------\n
//!
//! Layout of Statistics surface when both DN and DI are disabled
//!     ------------------------------------------------Pitch----------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 0 (Input)   | 17 DW Reserved         | 2 DW STD0 | 2 DW GCC0 | 11 DW Reserved |\n
//!     |--------------------------------------------------------------------------------------------------------------\n
//!     | 256 DW of ACE histogram Slice 1 (Input)   | 17 DW Reserved         | 2 DW STD1 | 2 DW GCC1 | 11 DW Reserved |\n
//!     ---------------------------------------------------------------------------------------------------------------\n
//! \param    [out] pStatSlice0Offset
//!           Statistics surface Slice 0 base pointer
//! \param    [out] pStatSlice1Offset
//!           Statistics surface Slice 1 base pointer
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS VpVeboxCmdPacket::GetStatisticsSurfaceOffsets(
    int32_t*                    pStatSlice0Offset,
    int32_t*                    pStatSlice1Offset)
{
    uint32_t    uiPitch;
    int32_t     iOffset;
    MOS_STATUS  eStatus;

    eStatus     = MOS_STATUS_UNKNOWN;
    uiPitch     = 0;

    // Query platform dependent size of per frame information
    VP_RENDER_CHK_STATUS(QueryStatLayout(
        VEBOX_STAT_QUERY_PER_FRAME_SIZE, &uiPitch));

    // Get the base address of Frame based statistics for each slice
    if (m_PacketCaps.bDI || m_PacketCaps.bIECP) // VEBOX, VEBOX+IECP
    {
        // Frame based statistics begins after Encoder statistics
        iOffset = m_dwVeboxPerBlockStatisticsWidth *
                  m_dwVeboxPerBlockStatisticsHeight;

        *pStatSlice0Offset = iOffset + uiPitch;                                     // Slice 0 current frame
        *pStatSlice1Offset = iOffset + uiPitch * 3;                                 // Slice 1 current frame
    }
    else if (m_PacketCaps.bDN || m_PacketCaps.bIECP) // DN, DN_IECP, SpatialDI
    {
        // Frame based statistics begins after Encoder statistics
        iOffset = m_dwVeboxPerBlockStatisticsWidth *
                  m_dwVeboxPerBlockStatisticsHeight;

        *pStatSlice0Offset = iOffset;                                               // Slice 0 input frame
        *pStatSlice1Offset = iOffset + uiPitch;                                     // Slice 1 input frame
    }
    else // IECP only
    {
        *pStatSlice0Offset = 0;                                                     // Slice 0 input frame
        *pStatSlice1Offset = uiPitch;                                               // Slice 1 input frame
    }

finish:
    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::AddVeboxDndiState()
{                                                                          
    PMHW_VEBOX_INTERFACE             pVeboxInterface = m_hwInterface->m_veboxInterface;;
    VpVeboxRenderData               *pRenderData = GetLastExecRenderData();

    if (pRenderData->DN.bDnEnabled || pRenderData->DI.bDeinterlace || pRenderData->DI.bQueryVariance)
    {
        return pVeboxInterface->AddVeboxDndiState(&pRenderData->GetDNDIParams());
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::AddVeboxIECPState()
{
    PMHW_VEBOX_INTERFACE             pVeboxInterface = m_hwInterface->m_veboxInterface;;
    VpVeboxRenderData*              pRenderData      = GetLastExecRenderData();

    if (pRenderData->IECP.IsIecpEnabled())
    {
        return pVeboxInterface->AddVeboxIecpState(&pRenderData->GetIECPParams());
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::SetupIndirectStates()
{
    PMHW_VEBOX_INTERFACE            pVeboxInterface = nullptr;
    VpVeboxRenderData               *pRenderData    = GetLastExecRenderData();

    VP_RENDER_CHK_NULL_RETURN(pRenderData);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface);

    pVeboxInterface = m_hwInterface->m_veboxInterface;
    VP_RENDER_CHK_NULL_RETURN(pVeboxInterface);

    // Set FMD Params
    VP_RENDER_CHK_STATUS_RETURN(ConfigFMDParams(pRenderData->GetDNDIParams().bProgressiveDN, pRenderData->DN.bAutoDetect));

    //----------------------------------
    // Allocate and reset VEBOX state
    //----------------------------------
    VP_RENDER_CHK_STATUS_RETURN(pVeboxInterface->AssignVeboxState());

    // Set IECP State
    VP_RENDER_CHK_STATUS_RETURN(AddVeboxIECPState());

    // Set DNDI State
    VP_RENDER_CHK_STATUS_RETURN(AddVeboxDndiState());

    // Set GAMUT State
    VP_RENDER_CHK_STATUS_RETURN(AddVeboxGamutState());

    return MOS_STATUS_SUCCESS;
}

void VpVeboxCmdPacket::VeboxGetBeCSCMatrix(
    VPHAL_CSPACE    inputColorSpace,
    VPHAL_CSPACE    outputColorSpace,
    MOS_FORMAT      inputFormat)
{
    // Get the matrix to use for conversion
    VpHal_GetCscMatrix(
        inputColorSpace,
        outputColorSpace,
        m_fCscCoeff,
        m_fCscInOffset,
        m_fCscOutOffset);

    // Vebox CSC converts RGB input to YUV for SFC
    // Vebox only supports A8B8G8R8 input, swap the 1st and 3rd
    // columns of the transfer matrix for A8R8G8B8 and X8R8G8B8
    // This only happens when SFC output is used
    if (inputFormat == Format_A8R8G8B8 ||
        inputFormat == Format_X8R8G8B8)
    {
        float   fTemp[3] = {};
        fTemp[0] = m_fCscCoeff[0];
        fTemp[1] = m_fCscCoeff[3];
        fTemp[2] = m_fCscCoeff[6];

        m_fCscCoeff[0] = m_fCscCoeff[2];
        m_fCscCoeff[3] = m_fCscCoeff[5];
        m_fCscCoeff[6] = m_fCscCoeff[8];

        m_fCscCoeff[2] = fTemp[0];
        m_fCscCoeff[5] = fTemp[1];
        m_fCscCoeff[8] = fTemp[2];
    }
}

MOS_STATUS VpVeboxCmdPacket::IsCmdParamsValid(
    const MHW_VEBOX_STATE_CMD_PARAMS            &VeboxStateCmdParams,
    const MHW_VEBOX_DI_IECP_CMD_PARAMS          &VeboxDiIecpCmdParams,
    const VPHAL_VEBOX_SURFACE_STATE_CMD_PARAMS  &VeboxSurfaceStateCmdParams)
{
    const MHW_VEBOX_MODE    &veboxMode          = VeboxStateCmdParams.VeboxMode;

    if (veboxMode.DIEnable)
    {
        if (nullptr == VeboxDiIecpCmdParams.pOsResPrevOutput &&
            (MEDIA_VEBOX_DI_OUTPUT_PREVIOUS == veboxMode.DIOutputFrames || MEDIA_VEBOX_DI_OUTPUT_BOTH == veboxMode.DIOutputFrames))
        {
            return MOS_STATUS_INVALID_PARAMETER;
        }
        if (nullptr == VeboxDiIecpCmdParams.pOsResCurrOutput &&
            (MEDIA_VEBOX_DI_OUTPUT_CURRENT == veboxMode.DIOutputFrames || MEDIA_VEBOX_DI_OUTPUT_BOTH == veboxMode.DIOutputFrames))
        {
            return MOS_STATUS_INVALID_PARAMETER;
        }
    }

    if (m_PacketCaps.bDN && !m_PacketCaps.bDI && !m_PacketCaps.bQueryVariance && !m_PacketCaps.bIECP)
    {
        if ((VeboxSurfaceStateCmdParams.pSurfInput->osSurface->TileModeGMM == VeboxSurfaceStateCmdParams.pSurfDNOutput->osSurface->TileModeGMM) &&
            (VeboxSurfaceStateCmdParams.pSurfInput->osSurface->dwPitch != VeboxSurfaceStateCmdParams.pSurfDNOutput->osSurface->dwPitch))
        {
            return MOS_STATUS_INVALID_PARAMETER;
        }
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::VeboxSetPerfTag()
{
    MOS_STATUS                  eStatus = MOS_STATUS_SUCCESS;
    PVPHAL_PERFTAG              pPerfTag = nullptr;
    VpVeboxRenderData           *pRenderData = GetLastExecRenderData();

    VP_PUBLIC_CHK_NULL_RETURN(pRenderData);
    VP_PUBLIC_CHK_NULL_RETURN(m_currentSurface);
    VP_PUBLIC_CHK_NULL_RETURN(m_currentSurface->osSurface);

    MOS_FORMAT srcFmt = m_currentSurface->osSurface->Format;

    pPerfTag = &pRenderData->PerfTag;

    switch (srcFmt)
    {
        case Format_NV12:
            return VeboxSetPerfTagNv12();

        CASE_PA_FORMAT:
            return VeboxSetPerfTagPaFormat();

        case Format_P010:
            // P010 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_P010;
            break;

        case Format_P016:
            // P016 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_P016;
            break;

        case Format_P210:
            // P210 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_P210;
            break;

        case Format_P216:
            // P216 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_P216;
            break;

        case Format_Y210:
            // Y210 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_Y210;
            break;

        case Format_Y216:
            // Y216 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_Y216;
            break;

        case Format_Y410:
            // Y410 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_Y410;
            break;

        case Format_Y416:
            // Y416 Input Support for VEBOX, SFC
            *pPerfTag = VPHAL_VEBOX_Y416;
            break;

        CASE_RGB32_FORMAT:
        case Format_AYUV:
        case Format_A16B16G16R16:
        case Format_A16R16G16B16:
        case Format_A16B16G16R16F:
        case Format_A16R16G16B16F:
            *pPerfTag = VPHAL_NONE;
            break;

        default:
            VPHAL_RENDER_ASSERTMESSAGE("Format Not found.");
            *pPerfTag = VPHAL_NONE;
            eStatus = MOS_STATUS_INVALID_PARAMETER;
    } // switch (srcFmt)

    return eStatus;
}

MOS_STATUS VpVeboxCmdPacket::VeboxSetPerfTagNv12()
{
    MOS_STATUS                  eStatus = MOS_STATUS_SUCCESS;
    PVPHAL_PERFTAG              pPerfTag = nullptr;
    VpVeboxRenderData           *pRenderData = GetLastExecRenderData();

    VP_PUBLIC_CHK_NULL_RETURN(pRenderData);
    VP_PUBLIC_CHK_NULL_RETURN(m_renderTarget);
    VP_PUBLIC_CHK_NULL_RETURN(m_renderTarget->osSurface);

    MOS_FORMAT dstFormat = m_renderTarget->osSurface->Format;

    pPerfTag = &pRenderData->PerfTag;

    if (pRenderData->IsDiEnabled())
    {
        if (pRenderData->DN.bDnEnabled ||
            pRenderData->DN.bChromaDnEnabled)
        {
            if (IsIECPEnabled())
            {
                *pPerfTag = VPHAL_NV12_DNDI_422CP;
            }
            else
            {
                *pPerfTag = VPHAL_NV12_DNDI_PA;
            }
        }
        else
        {
            if (IsIECPEnabled())
            {
                *pPerfTag = VPHAL_PL_DI_422CP;
            }
            else
            {
                *pPerfTag = VPHAL_PL_DI_PA;
            }
        }
    }
    else
    {
        if (pRenderData->DN.bDnEnabled ||
            pRenderData->DN.bChromaDnEnabled)
        {
            if (IsOutputPipeVebox())
            {
                switch (dstFormat)
                {
                    case Format_NV12:
                        *pPerfTag = VPHAL_NV12_DN_420CP;
                        break;
                    CASE_PA_FORMAT:
                        *pPerfTag = VPHAL_NV12_DN_422CP;
                        break;
                    case Format_RGB32:
                        *pPerfTag = VPHAL_NV12_DN_RGB32CP;
                    case Format_A8R8G8B8:
                    case Format_A8B8G8R8:
                        *pPerfTag = VPHAL_NV12_DN_RGB32CP;
                        break;
                    case Format_P010:
                    case Format_P016:
                    case Format_Y410:
                    case Format_Y416:
                    case Format_Y210:
                    case Format_Y216:
                    case Format_AYUV:
                    case Format_Y8:
                    case Format_Y16S:
                    case Format_Y16U:
                        *pPerfTag = VPHAL_NONE;
                        break;
                    default:
                        VP_PUBLIC_ASSERTMESSAGE("Output Format Not found.");
                        return MOS_STATUS_INVALID_PARAMETER;
                }
            }
            else if (IsIECPEnabled())
            {
                *pPerfTag = VPHAL_NV12_DN_420CP;
            }
            else
            {
                *pPerfTag = VPHAL_NV12_DN_NV12;
            }
        }
        else
        {
            if (IsOutputPipeVebox())
            {
                switch (dstFormat)
                {
                    case Format_NV12:
                        *pPerfTag = VPHAL_NV12_420CP;
                        break;
                    CASE_PA_FORMAT:
                        *pPerfTag = VPHAL_NV12_422CP;
                        break;
                    case Format_RGB32:
                        *pPerfTag = VPHAL_NV12_RGB32CP;
                    case Format_A8R8G8B8:
                    case Format_A8B8G8R8:
                    case Format_R10G10B10A2:
                    case Format_B10G10R10A2:
                        *pPerfTag = VPHAL_NV12_RGB32CP;
                        break;
                    case Format_P010:
                    case Format_P016:
                    case Format_Y410:
                    case Format_Y416:
                    case Format_Y210:
                    case Format_Y216:
                    case Format_AYUV:
                    case Format_Y8:
                    case Format_Y16S:
                    case Format_Y16U:
                        *pPerfTag = VPHAL_NONE;
                        break;
                    default:
                        VPHAL_RENDER_ASSERTMESSAGE("Output Format Not found.");
                        return MOS_STATUS_INVALID_PARAMETER;
                }
            }
            else
            {
                *pPerfTag = VPHAL_NV12_420CP;
            }
        }
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::VeboxSetPerfTagPaFormat()
{
    MOS_STATUS                  eStatus = MOS_STATUS_SUCCESS;
    PVPHAL_PERFTAG              pPerfTag = nullptr;
    VpVeboxRenderData           *pRenderData = GetLastExecRenderData();

    VP_PUBLIC_CHK_NULL_RETURN(pRenderData);
    VP_PUBLIC_CHK_NULL_RETURN(m_renderTarget);
    VP_PUBLIC_CHK_NULL_RETURN(m_renderTarget->osSurface);

    MOS_FORMAT dstFormat = m_renderTarget->osSurface->Format;

    pPerfTag = &pRenderData->PerfTag;

    if (pRenderData->IsDiEnabled())
    {
        if (pRenderData->DN.bDnEnabled ||
            pRenderData->DN.bChromaDnEnabled)
        {
            if (IsIECPEnabled())
            {
                *pPerfTag = VPHAL_PA_DNDI_422CP;
            }
            else
            {
                *pPerfTag = VPHAL_PA_DNDI_PA;
            }
        }
        else
        {
            if (IsIECPEnabled())
            {
                *pPerfTag = VPHAL_PA_DI_422CP;
            }
            else
            {
                *pPerfTag = VPHAL_PA_DI_PA;
            }
        }
    }
    else
    {
        if (pRenderData->DN.bDnEnabled ||
            pRenderData->DN.bChromaDnEnabled)
        {
            if (IsOutputPipeVebox())
            {
                switch (dstFormat)
                {
                    case Format_NV12:
                        *pPerfTag = VPHAL_PA_DN_420CP;
                        break;
                    CASE_PA_FORMAT:
                        *pPerfTag = VPHAL_PA_DN_422CP;
                        break;
                    case Format_RGB32:
                        *pPerfTag = VPHAL_PA_DN_RGB32CP;
                        break;
                    case Format_A8R8G8B8:
                    case Format_A8B8G8R8:
                    case Format_R10G10B10A2:
                    case Format_B10G10R10A2:
                        *pPerfTag = VPHAL_PA_RGB32CP;
                        break;
                    case Format_P010:
                    case Format_P016:
                    case Format_Y410:
                    case Format_Y416:
                    case Format_Y210:
                    case Format_Y216:
                    case Format_AYUV:
                    case Format_Y8:
                    case Format_Y16S:
                    case Format_Y16U:
                        *pPerfTag = VPHAL_NONE;
                        break;
                    default:
                        VPHAL_RENDER_ASSERTMESSAGE("Output Format Not found.");
                        return MOS_STATUS_INVALID_PARAMETER;
                }
            }
            else if (IsIECPEnabled())
            {
                *pPerfTag = VPHAL_PA_DN_422CP;
            }
            else
            {
                *pPerfTag = VPHAL_PA_DN_PA;
            }
        }
        else
        {
            if (IsOutputPipeVebox())
            {
                switch (dstFormat)
                {
                    case Format_NV12:
                        *pPerfTag = VPHAL_PA_420CP;
                        break;
                    CASE_PA_FORMAT:
                        *pPerfTag = VPHAL_PA_422CP;
                        break;
                    case Format_RGB32:
                        *pPerfTag = VPHAL_PA_RGB32CP;
                        break;
                    case Format_A8R8G8B8:
                    case Format_A8B8G8R8:
                    case Format_R10G10B10A2:
                    case Format_B10G10R10A2:
                        *pPerfTag = VPHAL_PA_RGB32CP;
                        break;
                    case Format_P010:
                    case Format_P016:
                    case Format_Y410:
                    case Format_Y416:
                    case Format_Y210:
                    case Format_Y216:
                    case Format_AYUV:
                    case Format_Y8:
                    case Format_Y16S:
                    case Format_Y16U:
                        *pPerfTag = VPHAL_NONE;
                        break;
                    default:
                        VPHAL_RENDER_ASSERTMESSAGE("Output Format Not found.");
                        return MOS_STATUS_INVALID_PARAMETER;
                }
            }
            else
            {
                *pPerfTag = VPHAL_PA_422CP;
            }
        }
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::UpdateVeboxStates()
{
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpVeboxCmdPacket::InitSurfMemCacheControl(VP_EXECUTE_CAPS packetCaps)
{
    MOS_HW_RESOURCE_DEF                 Usage           = MOS_HW_RESOURCE_DEF_MAX;
    MEMORY_OBJECT_CONTROL_STATE         MemObjCtrl      = {};
    PMOS_INTERFACE                      pOsInterface    = nullptr;
    PVP_VEBOX_CACHE_CNTL                pSettings       = nullptr;

    if (nullptr == m_surfMemCacheCtl)
    {
        m_surfMemCacheCtl = MOS_New(VP_VEBOX_CACHE_CNTL);
    }

    VP_PUBLIC_CHK_NULL_RETURN(m_surfMemCacheCtl);
    VP_PUBLIC_CHK_NULL_RETURN(m_hwInterface);
    VP_PUBLIC_CHK_NULL_RETURN(m_hwInterface->m_osInterface);

    MOS_ZeroMemory(m_surfMemCacheCtl, sizeof(VP_VEBOX_CACHE_CNTL));

    pOsInterface    = m_hwInterface->m_osInterface;
    pSettings       = m_surfMemCacheCtl;

    pSettings->bDnDi = true;

    if (pSettings->bDnDi)
    {
        pSettings->DnDi.bL3CachingEnabled = true;

        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.CurrentInputSurfMemObjCtl,        MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.PreviousInputSurfMemObjCtl,       MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.STMMInputSurfMemObjCtl,           MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.STMMOutputSurfMemObjCtl,          MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.DnOutSurfMemObjCtl,               MOS_MP_RESOURCE_USAGE_SurfaceState);

        if (packetCaps.bVebox && !packetCaps.bSFC && !packetCaps.bRender)
        {
            // Disable cache for output surface in vebox only condition
            VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.CurrentOutputSurfMemObjCtl,    MOS_MP_RESOURCE_USAGE_DEFAULT);
        }
        else
        {
            VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.CurrentOutputSurfMemObjCtl,    MOS_MP_RESOURCE_USAGE_SurfaceState);
        }

        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.StatisticsOutputSurfMemObjCtl,    MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.AlphaOrVignetteSurfMemObjCtl,     MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.LaceOrAceOrRgbHistogramSurfCtrl,  MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.SkinScoreSurfMemObjCtl,           MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.LaceLookUpTablesSurfMemObjCtl,    MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->DnDi.Vebox3DLookUpTablesSurfMemObjCtl, MOS_MP_RESOURCE_USAGE_SurfaceState);
    }
    if (pSettings->bLace)
    {
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->Lace.FrameHistogramSurfaceMemObjCtl,                       MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->Lace.AggregatedHistogramSurfaceMemObjCtl,                  MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->Lace.StdStatisticsSurfaceMemObjCtl,                        MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->Lace.PwlfInSurfaceMemObjCtl,                               MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->Lace.PwlfOutSurfaceMemObjCtl,                              MOS_MP_RESOURCE_USAGE_SurfaceState);
        VPHAL_SET_SURF_MEMOBJCTL(pSettings->Lace.WeitCoefSurfaceMemObjCtl,                             MOS_MP_RESOURCE_USAGE_SurfaceState);
    }

    return MOS_STATUS_SUCCESS;
}

MHW_CSPACE VpVeboxCmdPacket::VpHalCspace2MhwCspace(VPHAL_CSPACE cspace)
{
    switch (cspace)
    {
        case CSpace_Source:
            return MHW_CSpace_Source;

        case CSpace_RGB:
            return MHW_CSpace_RGB;

        case CSpace_YUV:
            return MHW_CSpace_YUV;

        case CSpace_Gray:
            return MHW_CSpace_Gray;

        case CSpace_Any:
            return MHW_CSpace_Any;

        case CSpace_sRGB:
            return MHW_CSpace_sRGB;

        case CSpace_stRGB:
            return MHW_CSpace_stRGB;

        case CSpace_BT601:
            return MHW_CSpace_BT601;

        case CSpace_BT601_FullRange:
            return MHW_CSpace_BT601_FullRange;

        case CSpace_BT709:
            return MHW_CSpace_BT709;

        case CSpace_BT709_FullRange:
            return MHW_CSpace_BT709_FullRange;

        case CSpace_xvYCC601:
            return MHW_CSpace_xvYCC601;

        case CSpace_xvYCC709:
            return MHW_CSpace_xvYCC709;

        case CSpace_BT601Gray:
            return MHW_CSpace_BT601Gray;

        case CSpace_BT601Gray_FullRange:
            return MHW_CSpace_BT601Gray_FullRange;

        case CSpace_BT2020:
            return MHW_CSpace_BT2020;

        case CSpace_BT2020_RGB:
            return MHW_CSpace_BT2020_RGB;

        case CSpace_BT2020_FullRange:
            return MHW_CSpace_BT2020_FullRange;

        case CSpace_BT2020_stRGB:
            return MHW_CSpace_BT2020_stRGB;

        case CSpace_None:
        default:
            return MHW_CSpace_None;
    }
}

}

