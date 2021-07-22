/*
* Copyright (c) 2021, Intel Corporation
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
//! \file     vp_render_cmd_packet.cpp
//! \brief    render packet which used in by mediapipline.
//! \details  render packet provide the structures and generate the cmd buffer which mediapipline will used.
//!
#include "vp_render_cmd_packet.h"
#include "vp_platform_interface.h"
#include "vp_pipeline_common.h"
#include "vp_render_kernel_obj.h"
#include "hal_oca_interface.h"

namespace vp
{
static inline RENDERHAL_SURFACE_TYPE InitRenderHalSurfType(VPHAL_SURFACE_TYPE vpSurfType)
{
    switch (vpSurfType)
    {
    case SURF_IN_BACKGROUND:
        return RENDERHAL_SURF_IN_BACKGROUND;

    case SURF_IN_PRIMARY:
        return RENDERHAL_SURF_IN_PRIMARY;

    case SURF_IN_SUBSTREAM:
        return RENDERHAL_SURF_IN_SUBSTREAM;

    case SURF_IN_REFERENCE:
        return RENDERHAL_SURF_IN_REFERENCE;

    case SURF_OUT_RENDERTARGET:
        return RENDERHAL_SURF_OUT_RENDERTARGET;

    case SURF_NONE:
    default:
        return RENDERHAL_SURF_NONE;
    }
}

VpRenderCmdPacket::VpRenderCmdPacket(MediaTask *task, PVP_MHWINTERFACE hwInterface, PVpAllocator &allocator, VPMediaMemComp *mmc, VpKernelSet *kernelSet) : CmdPacket(task),
                                                                                                                                                            RenderCmdPacket(task, hwInterface->m_osInterface, hwInterface->m_renderHal),
                                                                                                                                                            VpCmdPacket(task, hwInterface, allocator, mmc, VP_PIPELINE_PACKET_RENDER),
                                                                                                                                                            m_filter(nullptr),
                                                                                                                                                            m_firstFrame(true),
                                                                                                                                                            m_kernelSet(kernelSet)
{
}

VpRenderCmdPacket::~VpRenderCmdPacket()
{
    for (auto it : m_surfSetting.surfGroup)
    {
        if (it.second)
        {
            m_allocator->DestroyVpSurface(it.second);
        }
    }

    for (auto &samplerstate : m_kernelSamplerStateGroup)
    {
        if (samplerstate.second.SamplerType == MHW_SAMPLER_TYPE_AVS)
        {
            MOS_FreeMemAndSetNull(samplerstate.second.Avs.pMhwSamplerAvsTableParam);
        }
    }

}

MOS_STATUS VpRenderCmdPacket::Prepare()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_renderHal);
    VP_RENDER_CHK_NULL_RETURN(m_kernelSet);

    if (m_packetResourcesdPrepared)
    {
        VP_RENDER_NORMALMESSAGE("Resource Prepared, skip this time");
        return MOS_STATUS_SUCCESS;
    }

    m_PacketId = VP_PIPELINE_PACKET_RENDER;

    VP_RENDER_CHK_STATUS_RETURN(m_kernelSet->CreateKernelObjects(
        m_renderKernelParams,
        m_surfSetting.surfGroup,
        m_kernelSamplerStateGroup,
        m_kernelConfigs,
        m_kernelObjs));

    if (m_submissionMode == MULTI_KERNELS_WITH_MULTI_MEDIA_STATES)
    {
        for (auto it = m_kernelObjs.begin(); it != m_kernelObjs.end(); it++)
        {
            m_kernel = it->second;
            // reset render Data for current kernel
            MOS_ZeroMemory(&m_renderData, sizeof(KERNEL_PACKET_RENDER_DATA));

            VP_RENDER_CHK_STATUS_RETURN(RenderEngineSetup());

            VP_RENDER_CHK_STATUS_RETURN(KernelStateSetup());

            VP_RENDER_CHK_STATUS_RETURN(SetupSurfaceState());  // once Surface setup done, surface index should be created here

            VP_RENDER_CHK_STATUS_RETURN(SetupCurbeState());  // Set Curbe with updated surface index

            VP_RENDER_CHK_STATUS_RETURN(LoadKernel());

            VP_RENDER_CHK_STATUS_RETURN(SetupSamplerStates());

            VP_RENDER_CHK_STATUS_RETURN(SetupWalkerParams());

            VP_RENDER_CHK_STATUS_RETURN(m_renderHal->pfnSetVfeStateParams(
                m_renderHal,
                MEDIASTATE_DEBUG_COUNTER_FREE_RUNNING,
                RENDERHAL_USE_MEDIA_THREADS_MAX,
                m_renderData.iCurbeLength,
                m_renderData.iInlineLength,
                nullptr));

            m_kernelRenderData.insert(std::make_pair(it->first, m_renderData));
        }
    }
    else if (m_submissionMode == MULTI_KERNELS_WITH_ONE_MEDIA_STATE)
    {
        MOS_ZeroMemory(&m_renderData, sizeof(KERNEL_PACKET_RENDER_DATA));
        VP_RENDER_CHK_STATUS_RETURN(RenderEngineSetup());

        // for multi-kernel prepare together
        for (auto it = m_kernelObjs.begin(); it != m_kernelObjs.end(); it++)
        {
            m_kernel = it->second;
            if (it != m_kernelObjs.begin())
            {
                // reset render Data for current kernel
                PRENDERHAL_MEDIA_STATE pMediaState = m_renderData.mediaState;
                MOS_ZeroMemory(&m_renderData, sizeof(KERNEL_PACKET_RENDER_DATA));
                m_renderData.mediaState = pMediaState;
                // Assign and Reset binding table
                RENDER_PACKET_CHK_STATUS_RETURN(m_renderHal->pfnAssignBindingTable(
                    m_renderHal,
                    &m_renderData.bindingTable));
            }

            VP_RENDER_CHK_STATUS_RETURN(KernelStateSetup());

            VP_RENDER_CHK_STATUS_RETURN(SetupSurfaceState());  // once Surface setup done, surface index should be created here

            VP_RENDER_CHK_STATUS_RETURN(SetupCurbeState());  // Set Curbe with updated surface index

            VP_RENDER_CHK_STATUS_RETURN(LoadKernel());

            VP_RENDER_CHK_STATUS_RETURN(SetupSamplerStates());

            VP_RENDER_CHK_STATUS_RETURN(SetupWalkerParams());

            m_kernelRenderData.insert(std::make_pair(it->first, m_renderData));
        }

        VP_RENDER_CHK_STATUS_RETURN(m_renderHal->pfnSetVfeStateParams(
            m_renderHal,
            MEDIASTATE_DEBUG_COUNTER_FREE_RUNNING,
            RENDERHAL_USE_MEDIA_THREADS_MAX,
            m_totalCurbeSize,
            m_totoalInlineSize,
            nullptr));
    }
    else
    {
        return MOS_STATUS_INVALID_PARAMETER;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetupSamplerStates()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_renderHal);
    VP_RENDER_CHK_NULL_RETURN(m_kernel);

    KERNEL_SAMPLER_STATES& samplerStates = m_kernel->GetSamplerStates();

    if (!samplerStates.empty())
    {
        if (samplerStates.size() > MHW_RENDER_ENGINE_SAMPLERS_MAX)
        {
            MOS_STATUS_INVALID_PARAMETER;
        }

        VP_RENDER_CHK_STATUS_RETURN(m_renderHal->pfnSetSamplerStates(
            m_renderHal,
            m_renderData.mediaID,
            &samplerStates[0],
            samplerStates.size()));
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::Submit(MOS_COMMAND_BUFFER *commandBuffer, uint8_t packetPhase)
{
    VP_FUNC_CALL();
    if (m_kernelObjs.empty())
    {
        VP_RENDER_ASSERTMESSAGE("No Kernel Object Creation");
        return MOS_STATUS_NULL_POINTER;
    }

    if (m_submissionMode == MULTI_KERNELS_WITH_MULTI_MEDIA_STATES)
    {
        VP_RENDER_CHK_STATUS_RETURN(SetupMediaWalker());
        VP_RENDER_CHK_STATUS_RETURN(RenderCmdPacket::Submit(commandBuffer, packetPhase));
    }
    else if (m_submissionMode == MULTI_KERNELS_WITH_ONE_MEDIA_STATE)
    {
        VP_RENDER_CHK_STATUS_RETURN(SubmitWithMultiKernel(commandBuffer, packetPhase));
    }
    else
    {
        return MOS_STATUS_INVALID_PARAMETER;
    }

    VP_RENDER_CHK_STATUS_RETURN(m_kernelSet->DestroyKernelObjects(m_kernelObjs));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetVeboxUpdateParams(PVEBOX_UPDATE_PARAMS params)
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(params);

    if (params->kernelGroup.empty())
    {
        VP_RENDER_ASSERTMESSAGE("No Kernel need to be processed");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    for (auto it : params->kernelGroup)
    {
        m_kernelConfigs.insert(std::make_pair((KernelId)it, (void *)params));
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetSecureCopyParams(bool copyNeeded)
{
    return MOS_STATUS();
}

MOS_STATUS VpRenderCmdPacket::PacketInit(
    VP_SURFACE *        inputSurface,
    VP_SURFACE *        outputSurface,
    VP_SURFACE *        previousSurface,
    VP_SURFACE_SETTING &surfSetting,
    VP_EXECUTE_CAPS     packetCaps)
{
    // will remodify when normal render path enabled
    VP_UNUSED(inputSurface);
    VP_UNUSED(outputSurface);
    VP_UNUSED(previousSurface);

    VP_SURFACE *input  = m_allocator->AllocateVpSurface();
    VP_RENDER_CHK_NULL_RETURN(input);
    VP_SURFACE *output = m_allocator->AllocateVpSurface();
    VP_RENDER_CHK_NULL_RETURN(output);

    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator->CopyVpSurface(*input, *inputSurface));
    VP_PUBLIC_CHK_STATUS_RETURN(m_allocator->CopyVpSurface(*output, *outputSurface));

    surfSetting.surfGroup.insert(std::make_pair(SurfaceTypeRenderInput, input));
    surfSetting.surfGroup.insert(std::make_pair(SurfaceTypeRenderOutput, output));

    m_PacketCaps = packetCaps;

    // Init packet surface params.
    m_surfSetting = surfSetting;

    m_packetResourcesdPrepared = false;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::KernelStateSetup()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_kernel);

    // // Initialize States
    MOS_ZeroMemory(m_filter, sizeof(m_filter));
    MOS_ZeroMemory(&m_renderData.KernelEntry, sizeof(Kdll_CacheEntry));

    // Store pointer to Kernel Parameter
    VP_RENDER_CHK_STATUS_RETURN(m_kernel->GetKernelSettings(m_renderData.KernelParam, m_kernel->GetKernelID()));

    // Set Parameters for Kernel Entry
    m_renderData.KernelEntry.iKUID       = m_kernel->GetKernelID();
    m_renderData.KernelEntry.iKCID       = -1;
    m_renderData.KernelEntry.iFilterSize = 2;
    m_renderData.KernelEntry.pFilter     = m_filter;
    m_renderData.KernelEntry.iSize       = m_kernel->GetKernelSize();
    m_renderData.KernelEntry.pBinary     = (uint8_t *)m_kernel->GetKernelBinary();

    // set the Inline Data length
    void *   InlineData    = nullptr;
    uint32_t iInlineLength = 0;
    m_kernel->GetInlineState(&InlineData, iInlineLength);
    m_renderData.iInlineLength = iInlineLength;
    m_totoalInlineSize += iInlineLength;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetupSurfaceState()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_kernel);

    if (!m_kernel->GetKernelSurfaceConfig().empty())
    {
        for (auto surface = m_kernel->GetKernelSurfaceConfig().begin(); surface != m_kernel->GetKernelSurfaceConfig().end(); surface++)
        {
            KERNEL_SURFACE_STATE_PARAM *kernelSurfaceParam = &surface->second;
            SurfaceType                 type               = SurfaceType(surface->first);

            RENDERHAL_SURFACE_NEXT renderHalSurface;
            MOS_ZeroMemory(&renderHalSurface, sizeof(RENDERHAL_SURFACE_NEXT));

            RENDERHAL_SURFACE_STATE_PARAMS renderSurfaceParams;
            MOS_ZeroMemory(&renderSurfaceParams, sizeof(RENDERHAL_SURFACE_STATE_PARAMS));
            if (kernelSurfaceParam->surfaceOverwriteParams.updatedRenderSurfaces)
            {
                renderSurfaceParams = kernelSurfaceParam->surfaceOverwriteParams.renderSurfaceParams;
            }
            else
            {
                renderSurfaceParams.bRenderTarget    = (kernelSurfaceParam->renderTarget == true) ? 1 : 0;
                renderSurfaceParams.Boundary         = RENDERHAL_SS_BOUNDARY_ORIGINAL;  // Add conditional in future for Surfaces out of range
                renderSurfaceParams.bWidth16Align    = false;
                renderSurfaceParams.bWidthInDword_Y  = true;
                renderSurfaceParams.bWidthInDword_UV = true;
            }

            VP_SURFACE *vpSurface = nullptr;

            if (m_surfSetting.surfGroup.find(type) != m_surfSetting.surfGroup.end())
            {
                vpSurface = m_surfSetting.surfGroup.find(type)->second;
            }

            if (vpSurface)
            {
                // Prepare surfaces tracked in Resource manager
                VP_RENDER_CHK_STATUS_RETURN(InitRenderHalSurface(*vpSurface, renderHalSurface));
            }
            else
            {
                // State Heaps are not tracked in resource manager till now
                VP_RENDER_CHK_STATUS_RETURN(InitStateHeapSurface(type, renderHalSurface));
            }

            VP_RENDER_CHK_STATUS_RETURN(UpdateRenderSurface(renderHalSurface, *kernelSurfaceParam));

            uint32_t index = 0;

            if (kernelSurfaceParam->surfaceOverwriteParams.bindedKernel)
            {
                index = SetSurfaceForHwAccess(
                    &renderHalSurface.OsSurface,
                    &renderHalSurface,
                    &renderSurfaceParams,
                    kernelSurfaceParam->surfaceOverwriteParams.bindIndex,
                    renderSurfaceParams.bRenderTarget);
            }
            else
            {
                if ((kernelSurfaceParam->surfaceOverwriteParams.updatedSurfaceParams &&
                        kernelSurfaceParam->surfaceOverwriteParams.bufferResource) ||
                    (!kernelSurfaceParam->surfaceOverwriteParams.updatedSurfaceParams &&
                        renderHalSurface.OsSurface.Type == MOS_GFXRES_BUFFER))
                {
                    index = SetBufferForHwAccess(
                        &renderHalSurface.OsSurface,
                        &renderHalSurface,
                        &renderSurfaceParams,
                        renderSurfaceParams.bRenderTarget);
                }
                else
                {
                    index = SetSurfaceForHwAccess(
                        &renderHalSurface.OsSurface,
                        &renderHalSurface,
                        &renderSurfaceParams,
                        renderSurfaceParams.bRenderTarget);
                }
            }

            VP_RENDER_CHK_STATUS_RETURN(m_kernel->UpdateCurbeBindingIndex(type, index));
        }
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetupCurbeState()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_kernel);

    // set the Curbe Data length
    void *   curbeData   = nullptr;
    uint32_t curbeLength = 0;
    VP_RENDER_CHK_STATUS_RETURN(m_kernel->GetCurbeState(curbeData, curbeLength));

    m_renderData.iCurbeOffset = m_renderHal->pfnLoadCurbeData(
        m_renderHal,
        m_renderData.mediaState,
        curbeData,
        curbeLength);

    if (m_renderData.iCurbeOffset < 0)
    {
        RENDER_PACKET_ASSERTMESSAGE("Curbe Set Fail, return error");
        return MOS_STATUS_UNKNOWN;
    }

    m_renderData.iCurbeLength = MOS_ALIGN_CEIL(curbeLength, m_renderHal->dwCurbeBlockAlign);
    m_totalCurbeSize += m_renderData.iCurbeLength;

    MOS_SafeFreeMemory(curbeData);

    return MOS_STATUS_SUCCESS;
}

VP_SURFACE *VpRenderCmdPacket::GetSurface(SurfaceType type)
{
    auto        it   = m_surfSetting.surfGroup.find(type);
    VP_SURFACE *surf = (m_surfSetting.surfGroup.end() != it) ? it->second : nullptr;

    return surf;
}

MOS_STATUS VpRenderCmdPacket::SetupMediaWalker()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_kernel);

    m_renderData.walkerParam = m_kernel->GetWalkerSetting();
    switch (m_walkerType)
    {
    case WALKER_TYPE_MEDIA:
        MOS_ZeroMemory(&m_mediaWalkerParams, sizeof(MHW_WALKER_PARAMS));
        // Prepare Media Walker Params
        VP_RENDER_CHK_STATUS_RETURN(PrepareMediaWalkerParams(m_renderData.walkerParam, m_mediaWalkerParams));
        break;
    case WALKER_TYPE_COMPUTE:
        // Parepare Compute Walker Param
        MOS_ZeroMemory(&m_gpgpuWalkerParams, sizeof(MHW_GPGPU_WALKER_PARAMS));
        VP_RENDER_CHK_STATUS_RETURN(PrepareComputeWalkerParams(m_renderData.walkerParam, m_gpgpuWalkerParams));
        break;
    case WALKER_TYPE_DISABLED:
    default:
        // using BB for walker setting
        return MOS_STATUS_UNIMPLEMENTED;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetupWalkerParams()
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(m_kernel);

    m_renderData.walkerParam = m_kernel->GetWalkerSetting();

    m_renderData.walkerParam.iBindingTable = m_renderData.bindingTable;

    m_renderData.walkerParam.iMediaID = m_renderData.mediaID;

    m_renderData.walkerParam.iCurbeOffset = m_renderData.iCurbeOffset;

    m_renderData.walkerParam.iCurbeLength = m_renderData.iCurbeLength;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::InitRenderHalSurface(VP_SURFACE &surface, RENDERHAL_SURFACE &renderSurface)
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(surface.osSurface);
    VP_RENDER_CHK_STATUS_RETURN(RenderCmdPacket::InitRenderHalSurface(*surface.osSurface, &renderSurface));

    renderSurface.rcSrc    = surface.rcSrc;
    renderSurface.rcDst    = surface.rcDst;
    renderSurface.rcMaxSrc = surface.rcMaxSrc;
    renderSurface.SurfType =
        InitRenderHalSurfType(surface.SurfType);

    return MOS_STATUS_SUCCESS;
}
MOS_STATUS VpRenderCmdPacket::InitStateHeapSurface(SurfaceType type, RENDERHAL_SURFACE &renderSurface)
{
    VP_FUNC_CALL();
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    MOS_SURFACE mosSurface;

    MOS_ZeroMemory(&mosSurface, sizeof(MOS_SURFACE));

    // Check for Vebox Heap readiness
    const MHW_VEBOX_HEAP *pVeboxHeap = nullptr;
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface);
    VP_RENDER_CHK_NULL_RETURN(m_hwInterface->m_veboxInterface);

    VP_RENDER_CHK_STATUS_RETURN(m_hwInterface->m_veboxInterface->GetVeboxHeapInfo(
        &pVeboxHeap));
    VP_RENDER_CHK_NULL_RETURN(pVeboxHeap);

    switch (type)
    {
    case SurfaceTypeVeboxStateHeap_Drv:
        mosSurface.OsResource = pVeboxHeap->DriverResource;
        break;
    case SurfaceTypeVeboxStateHeap_Knr:
        mosSurface.OsResource = pVeboxHeap->KernelResource;
        break;
    default:
        eStatus = MOS_STATUS_UNIMPLEMENTED;
        VP_RENDER_ASSERTMESSAGE("Not Inplenmented in driver now, return fail");
        break;
    }

    VP_RENDER_CHK_STATUS_RETURN(RenderCmdPacket::InitRenderHalSurface(mosSurface, &renderSurface));

    return eStatus;
}
MOS_STATUS VpRenderCmdPacket::UpdateRenderSurface(RENDERHAL_SURFACE_NEXT &renderSurface, KERNEL_SURFACE_STATE_PARAM &kernelParams)
{
    VP_FUNC_CALL();
    auto &overwriteParam = kernelParams.surfaceOverwriteParams;
    if (overwriteParam.updatedSurfaceParams)
    {
        if (overwriteParam.width && overwriteParam.height)
        {
            renderSurface.OsSurface.dwWidth  = overwriteParam.width;
            renderSurface.OsSurface.dwHeight = overwriteParam.height;
            renderSurface.OsSurface.dwQPitch = overwriteParam.height;
        }

        renderSurface.OsSurface.dwPitch = overwriteParam.pitch != 0 ? overwriteParam.pitch : renderSurface.OsSurface.dwPitch;

        if (renderSurface.OsSurface.dwPitch < renderSurface.OsSurface.dwWidth)
        {
            VP_RENDER_ASSERTMESSAGE("Invalid Surface where Pitch < Width, return invalid Overwrite Params");
            return MOS_STATUS_INVALID_PARAMETER;
        }

        renderSurface.OsSurface.Format = (overwriteParam.format != 0) ? overwriteParam.format : renderSurface.OsSurface.Format;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::ReadSRWeights(uint16_t *pBuf, const uint8_t *pWeight, const uint32_t uWeightSize, uint32_t outChannels, uint32_t inChannels, uint32_t nWeightsPerChannel, uint32_t layer)
{
    VP_FUNC_CALL();
    MOS_STATUS eStatus        = MOS_STATUS_SUCCESS;
    uint16_t * tempBuf        = nullptr;
    uint32_t   sizeWeightsBuf = inChannels * outChannels * nWeightsPerChannel * sizeof(uint16_t);
    int        writeIndex     = 0;

    tempBuf = (uint16_t *)MOS_AllocAndZeroMemory(sizeWeightsBuf);

    VPHAL_RENDER_CHK_NULL(tempBuf);

    VPHAL_RENDER_CHK_STATUS(MOS_SecureMemcpy((uint8_t *)tempBuf, sizeWeightsBuf, pWeight, uWeightSize));

    // re-order weights, and apply padding
    for (uint32_t o = 0; o < outChannels; o++)
    {
        for (uint32_t i = 0; i < inChannels; i++)
        {
            for (uint32_t w = 0; w < nWeightsPerChannel; w++)
            {
                // ugly hack, to solve different ordering of weights for layers
                if (layer >= 1 && layer <= 7)
                {
                    pBuf[writeIndex++] = tempBuf[w * (inChannels * outChannels) + o * inChannels + i];
                }
                else
                {
                    pBuf[writeIndex++] = tempBuf[w * (inChannels * outChannels) + i * outChannels + o];
                }
            }

            if (layer <= 7)
            {
                int padding = 64 - (writeIndex % 64);
                if (padding < 9)
                {
                    // add input channel padding
                    for (int p = 0; p < padding; p++)
                    {
                        pBuf[writeIndex++] = 0;
                    }
                }
            }
            else if (layer == 9)  // for layer 9 we add padding between every inchannel
            {
                int padding = 16 - (writeIndex % 16);

                // add input channel padding
                for (int p = 0; p < padding; p++)
                {
                    pBuf[writeIndex++] = 0;
                }
            }
        }

        if (layer <= 7)
        {
            int padding = 64 - (writeIndex % 64);

            // add input channel padding
            for (int p = 0; p < padding; p++)
            {
                pBuf[writeIndex++] = 0;
            }
        }
        else if (layer == 8)
        {
            // add output channel padding
            for (int p = 0; p < 32 - (int)inChannels; p++)
            {
                pBuf[writeIndex++] = 0;
            }
        }
    }

finish:
    MOS_FreeMemAndSetNull(tempBuf);
    return eStatus;
}

MOS_STATUS VpRenderCmdPacket::SetSRParams(PRENDER_SR_PARAMS params)
{
    VP_FUNC_CALL();
    VP_RENDER_CHK_NULL_RETURN(params);
    VP_RENDER_CHK_NULL_RETURN(m_allocator);

    if (params->bEnableSR)
    {
        for (auto &layer : params->layersParam)
        {
            VP_SURFACE * pSurface    = nullptr;
            MOS_ALLOC_GFXRES_PARAMS allocParams = {};
            KERNEL_PARAMS kernelParams = {};

            allocParams.dwWidth = layer.uWidth;
            allocParams.dwHeight = layer.uHeight;
            allocParams.Format = layer.format;
            allocParams.bIsCompressible = false;
            allocParams.CompressionMode = MOS_MMC_DISABLED;
            allocParams.pBufName = "SR intermediate Surface";
            allocParams.dwArraySize = 1;
            allocParams.ResUsageType = MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_RENDER;
            allocParams.m_tileModeByForce = MOS_TILE_UNSET_GMM;
            allocParams.TileType = MOS_TILE_Y;
            allocParams.Type = MOS_GFXRES_2D;

            pSurface = m_allocator->AllocateVpSurface(allocParams, true);
            VP_RENDER_CHK_NULL_RETURN(pSurface);

            m_surfSetting.surfGroup.insert(std::make_pair(layer.outputSurface, pSurface));

            if (layer.uWeightBufferSize)
            {
                MOS_ZeroMemory(&allocParams, sizeof(MOS_ALLOC_GFXRES_PARAMS));
                allocParams.dwWidth = layer.uWeightBufferSize;
                allocParams.dwHeight = 1;
                allocParams.Format = Format_Buffer;
                allocParams.bIsCompressible = false;
                allocParams.CompressionMode = MOS_MMC_DISABLED;
                allocParams.pBufName = "SR intermediate Buffer";
                allocParams.dwArraySize = 1;
                allocParams.ResUsageType = MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_RENDER;
                allocParams.m_tileModeByForce = MOS_TILE_UNSET_GMM;
                allocParams.TileType = MOS_TILE_LINEAR;
                allocParams.Type = MOS_GFXRES_BUFFER;

                pSurface = m_allocator->AllocateVpSurface(allocParams, true);
                VP_RENDER_CHK_NULL_RETURN(pSurface);

                uint16_t *pTempBuffer = (uint16_t *)MOS_AllocAndZeroMemory(layer.uWeightBufferSize);
                VP_PUBLIC_CHK_NULL_RETURN(pTempBuffer);

                ReadSRWeights(pTempBuffer,
                    (*params->sr2xConvWeightTable)[layer.uLayerID - 1],
                    (*params->sr2xConvWeightTableSize)[layer.uLayerID - 1],
                    layer.uOutChannels,
                    layer.uInChannels,
                    layer.uWeightsPerChannel,
                    layer.uLayerID - 1);

                m_allocator->Write1DSurface(pSurface, (uint8_t *)pTempBuffer, layer.uWeightBufferSize);

                m_surfSetting.surfGroup.insert(std::make_pair(layer.weightBuffer, pSurface));

                MOS_FreeMemAndSetNull(pTempBuffer);
            }

            if (layer.uBiasBufferSize)
            {
                MOS_ZeroMemory(&allocParams, sizeof(MOS_ALLOC_GFXRES_PARAMS));
                allocParams.dwWidth = layer.uBiasBufferSize;
                allocParams.dwHeight = 1;
                allocParams.Format = Format_Buffer;
                allocParams.bIsCompressible = false;
                allocParams.CompressionMode = MOS_MMC_DISABLED;
                allocParams.pBufName = "SR intermediate Buffer";
                allocParams.dwArraySize = 1;
                allocParams.ResUsageType = MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_RENDER;
                allocParams.m_tileModeByForce = MOS_TILE_UNSET_GMM;
                allocParams.TileType = MOS_TILE_LINEAR;
                allocParams.Type = MOS_GFXRES_BUFFER;

                pSurface = m_allocator->AllocateVpSurface(allocParams, true);
                VP_RENDER_CHK_NULL_RETURN(pSurface);

                m_allocator->Write1DSurface(pSurface, (*params->sr2xConvBiasTable)[layer.uLayerID - 1], (*params->sr2xConvBiasTableSize)[layer.uLayerID - 1]);

                m_surfSetting.surfGroup.insert(std::make_pair(layer.biasBuffer, pSurface));
            }

            if (layer.uReluBufferSize)
            {
                MOS_ZeroMemory(&allocParams, sizeof(MOS_ALLOC_GFXRES_PARAMS));
                allocParams.dwWidth = layer.uReluBufferSize;
                allocParams.dwHeight = 1;
                allocParams.Format = Format_Buffer;
                allocParams.bIsCompressible = false;
                allocParams.CompressionMode = MOS_MMC_DISABLED;
                allocParams.pBufName = "SR intermediate Buffer";
                allocParams.dwArraySize = 1;
                allocParams.ResUsageType = MOS_HW_RESOURCE_USAGE_VP_INTERNAL_READ_WRITE_RENDER;
                allocParams.m_tileModeByForce = MOS_TILE_UNSET_GMM;
                allocParams.TileType = MOS_TILE_LINEAR;
                allocParams.Type = MOS_GFXRES_BUFFER;

                pSurface = m_allocator->AllocateVpSurface(allocParams, true);
                VP_RENDER_CHK_NULL_RETURN(pSurface);

                if ((*params->sr2xConvPreluTableSize)[layer.uLayerID - 1])
                {
                    m_allocator->Write1DSurface(pSurface, (*params->sr2xConvPreluTable)[layer.uLayerID - 1], (*params->sr2xConvPreluTableSize)[layer.uLayerID - 1]);
                }
                else
                {
                    uint8_t *temp = (uint8_t *)MOS_AllocAndZeroMemory(layer.uReluBufferSize);
                    VP_RENDER_CHK_NULL_RETURN(temp);
                    m_allocator->Write1DSurface(pSurface, temp, layer.uReluBufferSize);
                    MOS_FreeMemAndSetNull(temp);
                }

                m_surfSetting.surfGroup.insert(std::make_pair(layer.reluBuffer, pSurface));
            }

            kernelParams.kernelId = layer.uKernelID;
            kernelParams.kernelArgs = layer.kernelArgs;
            kernelParams.kernelThreadSpace.uHeight = layer.uThreadHeight;
            kernelParams.kernelThreadSpace.uWidth = layer.uThreadWidth;
            kernelParams.syncFlag = true;

            m_renderKernelParams.push_back(kernelParams);
        }

        RENDER_PACKET_CHK_STATUS_RETURN(SetSRChromaParams(params));

        m_submissionMode = MULTI_KERNELS_WITH_ONE_MEDIA_STATE;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetSRChromaParams(PRENDER_SR_PARAMS params)
{
    VP_FUNC_CALL();
    RENDER_PACKET_CHK_NULL_RETURN(params);

    KERNEL_PARAMS kernelParams = {};
    kernelParams.kernelId = params->chromaLayerParam.uKernelID;
    kernelParams.kernelArgs = params->chromaLayerParam.kernelArgs;
    kernelParams.kernelThreadSpace.uWidth = params->chromaLayerParam.uThreadWidth;
    kernelParams.kernelThreadSpace.uHeight = params->chromaLayerParam.uThreadHeight;

    MHW_SAMPLER_STATE_PARAM samplerStateParam = {};
    MOS_ZeroMemory(&samplerStateParam, sizeof(samplerStateParam));

    samplerStateParam.Avs.pMhwSamplerAvsTableParam = (PMHW_SAMPLER_AVS_TABLE_PARAM)MOS_AllocAndZeroMemory(sizeof(MHW_SAMPLER_AVS_TABLE_PARAM));

    samplerStateParam.bInUse      = true;
    samplerStateParam.SamplerType = MHW_SAMPLER_TYPE_AVS;
    samplerStateParam.ElementType = MHW_Sampler128Elements;

    RENDER_PACKET_CHK_STATUS_RETURN(SetSamplerAvsParams(samplerStateParam, params));

    SamplerIndex samplerindex = m_kernelSamplerStateGroup.size();
    kernelParams.kernelSamplerIndex.push_back(samplerindex);

    KRN_ARG& krnArg = kernelParams.kernelArgs.at(0);
    if (krnArg.eArgKind == ARG_KIND_SAMPLER)
    {
        *(uint32_t *)krnArg.pData = samplerindex;
    }
    else
    {
        return MOS_STATUS_INVALID_PARAMETER;
    }

    m_kernelSamplerStateGroup.insert(std::make_pair(samplerindex, samplerStateParam));

    kernelParams.syncFlag = true;

    m_renderKernelParams.push_back(kernelParams);

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetSamplerAvsParams(MHW_SAMPLER_STATE_PARAM &samplerStateParam, PRENDER_SR_PARAMS params)
{
    VP_FUNC_CALL();
    // Set Surface Smapler Status
    samplerStateParam.Avs.bEnableAVS          = true;
    samplerStateParam.Avs.BypassIEF           = 0;
    samplerStateParam.Avs.AvsType             = 0;  // Adaptive
    samplerStateParam.Avs.EightTapAFEnable    = 1;
    samplerStateParam.Avs.GainFactor          = 44;  // should be default
    samplerStateParam.Avs.GlobalNoiseEstm     = 255;
    samplerStateParam.Avs.StrongEdgeThr       = 8;
    samplerStateParam.Avs.WeakEdgeThr         = 1;
    samplerStateParam.Avs.StrongEdgeWght      = 7;
    samplerStateParam.Avs.RegularWght         = 2;
    samplerStateParam.Avs.NonEdgeWght         = 1;
    samplerStateParam.Avs.EightTapAFEnable    = true;
    samplerStateParam.Avs.bEnableSTDE         = 0;
    samplerStateParam.Avs.b8TapAdaptiveEnable = 0;
    samplerStateParam.Avs.bSkinDetailFactor   = 0;
    samplerStateParam.Avs.bHdcDwEnable        = true;
    samplerStateParam.Avs.bWritebackStandard  = true;
    samplerStateParam.Avs.bEnableIEF          = 0;
    samplerStateParam.Avs.wIEFFactor          = 0;

    // IEF params - default value
    samplerStateParam.Avs.wR3xCoefficient  = 6;
    samplerStateParam.Avs.wR3cCoefficient  = 15;
    samplerStateParam.Avs.wR5xCoefficient  = 9;
    samplerStateParam.Avs.wR5cxCoefficient = 8;
    samplerStateParam.Avs.wR5cCoefficient  = 3;

    // AVS_STATE
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->bBypassXAdaptiveFiltering  = 0;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->bBypassYAdaptiveFiltering  = 0;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->byteDefaultSharpnessLevel  = 255;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->byteMaxDerivative4Pixels   = 7;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->byteMaxDerivative8Pixels   = 20;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->byteTransitionArea8Pixels  = 5;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->byteTransitionArea4Pixels  = 4;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->bEnableRGBAdaptive         = false;
    samplerStateParam.Avs.pMhwSamplerAvsTableParam->bAdaptiveFilterAllChannels = true;

    MHW_AVS_PARAMS avs_params = {};
    // Allocate AVS coefficients, One set each for X and Y
    int32_t size = ((NUM_POLYPHASE_Y_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float)) + (NUM_POLYPHASE_UV_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float))) * 2;

    char *ptr = (char *)MOS_AllocAndZeroMemory(size);
    VPHAL_RENDER_CHK_NULL_RETURN(ptr);

    avs_params.piYCoefsX = (int32_t *)ptr;

    ptr += (NUM_POLYPHASE_Y_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float));
    avs_params.piUVCoefsX = (int32_t *)ptr;

    ptr += (NUM_POLYPHASE_UV_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float));
    avs_params.piYCoefsY = (int32_t *)ptr;

    ptr += (NUM_POLYPHASE_Y_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float));
    avs_params.piUVCoefsY = (int32_t *)ptr;

    SamplerAvsCalcScalingTable(avs_params, (params->chromaLayerParam.kernelFormat) ? Format_YV12 : Format_NV12, false, params->chromaLayerParam.fScaleX, params->chromaLayerParam.fChromaScaleX, CHROMA_SITING_HORZ_LEFT | CHROMA_SITING_VERT_TOP, false);

    SamplerAvsCalcScalingTable(avs_params, (params->chromaLayerParam.kernelFormat) ? Format_YV12 : Format_NV12, true, params->chromaLayerParam.fScaleY, params->chromaLayerParam.fChromaScaleY, CHROMA_SITING_HORZ_LEFT | CHROMA_SITING_VERT_TOP, false);

    avs_params.Format = (params->chromaLayerParam.kernelFormat) ? Format_YV12 : Format_NV12;

    // Assign the coefficient table;
    for (uint32_t i = 0; i < MHW_NUM_HW_POLYPHASE_TABLES; i++)
    {
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[0] = (uint8_t)avs_params.piYCoefsX[i * 8 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[1] = (uint8_t)avs_params.piYCoefsX[i * 8 + 1];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[2] = (uint8_t)avs_params.piYCoefsX[i * 8 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[3] = (uint8_t)avs_params.piYCoefsX[i * 8 + 3];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[4] = (uint8_t)avs_params.piYCoefsX[i * 8 + 4];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[5] = (uint8_t)avs_params.piYCoefsX[i * 8 + 5];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[6] = (uint8_t)avs_params.piYCoefsX[i * 8 + 6];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroXFilterCoefficient[7] = (uint8_t)avs_params.piYCoefsX[i * 8 + 7];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[0] = (uint8_t)avs_params.piYCoefsY[i * 8 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[1] = (uint8_t)avs_params.piYCoefsY[i * 8 + 1];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[2] = (uint8_t)avs_params.piYCoefsY[i * 8 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[3] = (uint8_t)avs_params.piYCoefsY[i * 8 + 3];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[4] = (uint8_t)avs_params.piYCoefsY[i * 8 + 4];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[5] = (uint8_t)avs_params.piYCoefsY[i * 8 + 5];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[6] = (uint8_t)avs_params.piYCoefsY[i * 8 + 6];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .ZeroYFilterCoefficient[7] = (uint8_t)avs_params.piYCoefsY[i * 8 + 7];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneXFilterCoefficient[0] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneXFilterCoefficient[1] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 1];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneXFilterCoefficient[2] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneXFilterCoefficient[3] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 3];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneYFilterCoefficient[0] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneYFilterCoefficient[1] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 1];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneYFilterCoefficient[2] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParam[i]
            .OneYFilterCoefficient[3] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 3];
    }

    // Assign the coefficient table;
    for (uint32_t i = 0; i < MHW_NUM_HW_POLYPHASE_EXTRA_TABLES_G9; i++)
    {
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[0] = (uint8_t)avs_params.piYCoefsX[i * 8 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[1] = (uint8_t)avs_params.piYCoefsX[i * 8 + 1];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[2] = (uint8_t)avs_params.piYCoefsX[i * 8 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[3] = (uint8_t)avs_params.piYCoefsX[i * 8 + 3];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[4] = (uint8_t)avs_params.piYCoefsX[i * 8 + 4];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[5] = (uint8_t)avs_params.piYCoefsX[i * 8 + 5];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[6] = (uint8_t)avs_params.piYCoefsX[i * 8 + 6];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroXFilterCoefficient[7] = (uint8_t)avs_params.piYCoefsX[i * 8 + 7];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[0] = (uint8_t)avs_params.piYCoefsY[i * 8 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[1] = (uint8_t)avs_params.piYCoefsY[i * 8 + 1];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[2] = (uint8_t)avs_params.piYCoefsY[i * 8 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[3] = (uint8_t)avs_params.piYCoefsY[i * 8 + 3];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[4] = (uint8_t)avs_params.piYCoefsY[i * 8 + 4];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[5] = (uint8_t)avs_params.piYCoefsY[i * 8 + 5];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[6] = (uint8_t)avs_params.piYCoefsY[i * 8 + 6];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .ZeroYFilterCoefficient[7] = (uint8_t)avs_params.piYCoefsY[i * 8 + 7];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneXFilterCoefficient[0] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneXFilterCoefficient[1] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 1];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneXFilterCoefficient[2] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneXFilterCoefficient[3] = (uint8_t)avs_params.piUVCoefsX[i * 4 + 3];

        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneYFilterCoefficient[0] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 0];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneYFilterCoefficient[1] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 1];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneYFilterCoefficient[2] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 2];
        samplerStateParam.Avs.pMhwSamplerAvsTableParam->paMhwAvsCoeffParamExtra[i]
            .OneYFilterCoefficient[3] = (uint8_t)avs_params.piUVCoefsY[i * 4 + 3];
    }

    MOS_SafeFreeMemory(avs_params.piYCoefsX);

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SamplerAvsCalcScalingTable(
    MHW_AVS_PARAMS &avsParameters,
    MOS_FORMAT      SrcFormat,
    bool            bVertical,
    float           fLumaScale,
    float           fChromaScale,
    uint32_t        dwChromaSiting,
    bool            b8TapAdaptiveEnable)
{
    VP_FUNC_CALL();
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;
    MHW_PLANE  Plane;
    int32_t    iUvPhaseOffset;
    uint32_t   dwHwPhrase;
    uint32_t   YCoefTableSize;
    uint32_t   UVCoefTableSize;
    float      fLumaScaleParam;
    float      fChromaScaleParam;
    int32_t *  piYCoefsParam;
    int32_t *  piUVCoefsParam;
    float      fHPStrength;

    VP_PUBLIC_CHK_NULL_RETURN(avsParameters.piYCoefsY);
    VP_PUBLIC_CHK_NULL_RETURN(avsParameters.piYCoefsX);
    VP_PUBLIC_CHK_NULL_RETURN(avsParameters.piUVCoefsY);
    VP_PUBLIC_CHK_NULL_RETURN(avsParameters.piUVCoefsX);

    YCoefTableSize  = (NUM_POLYPHASE_Y_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float));
    UVCoefTableSize = (NUM_POLYPHASE_UV_ENTRIES * NUM_HW_POLYPHASE_TABLES_G9 * sizeof(float));
    dwHwPhrase      = NUM_HW_POLYPHASE_TABLES_G9;

    fHPStrength    = 0.0F;
    piYCoefsParam  = bVertical ? avsParameters.piYCoefsY : avsParameters.piYCoefsX;
    piUVCoefsParam = bVertical ? avsParameters.piUVCoefsY : avsParameters.piUVCoefsX;

    // Recalculate Horizontal or Vertical scaling table
    if (SrcFormat != avsParameters.Format)  //|| fLumaScale != fLumaScaleParam || fChromaScale != fChromaScaleParam
    {
        MOS_ZeroMemory(piYCoefsParam, YCoefTableSize);
        MOS_ZeroMemory(piUVCoefsParam, UVCoefTableSize);

        // 4-tap filtering for RGformat G-channel if 8tap adaptive filter is not enabled.
        Plane = (IS_RGB32_FORMAT(SrcFormat) && !b8TapAdaptiveEnable) ? MHW_U_PLANE : MHW_Y_PLANE;

        // For 1x scaling in horizontal direction, use special coefficients for filtering
        // we don't do this when bForcePolyPhaseCoefs flag is set
        if (fLumaScale == 1.0F && !avsParameters.bForcePolyPhaseCoefs)
        {
            VPHAL_RENDER_CHK_STATUS_RETURN(SetNearestModeTable(
                piYCoefsParam,
                Plane,
                true));
            // If the 8-tap adaptive is enabled for all channel, then UV/RB use the same coefficient as Y/G
            // So, coefficient for UV/RB channels caculation can be passed
            if (!b8TapAdaptiveEnable)
            {
                if (fChromaScale == 1.0F)
                {
                    VPHAL_RENDER_CHK_STATUS_RETURN(SetNearestModeTable(
                        piUVCoefsParam,
                        MHW_U_PLANE,
                        true));
                }
                else
                {
                    if (dwChromaSiting & (bVertical ? MHW_CHROMA_SITING_VERT_TOP : MHW_CHROMA_SITING_HORZ_LEFT))
                    {
                        // No Chroma Siting
                        VPHAL_RENDER_CHK_STATUS_RETURN(CalcPolyphaseTablesUV(
                            piUVCoefsParam,
                            2.0F,
                            fChromaScale));
                    }
                    else
                    {
                        // Chroma siting offset needs to be added
                        if (dwChromaSiting & (bVertical ? MHW_CHROMA_SITING_VERT_CENTER : MHW_CHROMA_SITING_HORZ_CENTER))
                        {
                            iUvPhaseOffset = MOS_UF_ROUND(0.5F * 16.0F);  // U0.4
                        }
                        else  //if (ChromaSiting & (bVertical ? MHW_CHROMA_SITING_VERT_BOTTOM : MHW_CHROMA_SITING_HORZ_RIGHT))
                        {
                            iUvPhaseOffset = MOS_UF_ROUND(1.0F * 16.0F);  // U0.4
                        }

                        VPHAL_RENDER_CHK_STATUS_RETURN(CalcPolyphaseTablesUVOffset(
                            piUVCoefsParam,
                            3.0F,
                            fChromaScale,
                            iUvPhaseOffset));
                    }
                }
            }
        }
        else
        {
            // Clamp the Scaling Factor if > 1.0x
            fLumaScale = MOS_MIN(1.0F, fLumaScale);

            VPHAL_RENDER_CHK_STATUS_RETURN(CalcPolyphaseTablesY(
                piYCoefsParam,
                fLumaScale,
                Plane,
                SrcFormat,
                fHPStrength,
                true,
                dwHwPhrase));

            // If the 8-tap adaptive is enabled for all channel, then UV/RB use the same coefficient as Y/G
            // So, coefficient for UV/RB channels caculation can be passed
            if (!b8TapAdaptiveEnable)
            {
                {
                    if (fChromaScale == 1.0F)
                    {
                        VPHAL_RENDER_CHK_STATUS_RETURN(SetNearestModeTable(
                            piUVCoefsParam,
                            MHW_U_PLANE,
                            true));
                    }
                    else
                    {
                        // If Chroma Siting info is present
                        if (dwChromaSiting & (bVertical ? MHW_CHROMA_SITING_VERT_TOP : MHW_CHROMA_SITING_HORZ_LEFT))
                        {
                            // No Chroma Siting
                            VPHAL_RENDER_CHK_STATUS_RETURN(CalcPolyphaseTablesUV(
                                piUVCoefsParam,
                                2.0F,
                                fChromaScale));
                        }
                        else
                        {
                            // Chroma siting offset needs to be added
                            if (dwChromaSiting & (bVertical ? MHW_CHROMA_SITING_VERT_CENTER : MHW_CHROMA_SITING_HORZ_CENTER))
                            {
                                iUvPhaseOffset = MOS_UF_ROUND(0.5F * 16.0F);  // U0.4
                            }
                            else  //if (ChromaSiting & (bVertical ? MHW_CHROMA_SITING_VERT_BOTTOM : MHW_CHROMA_SITING_HORZ_RIGHT))
                            {
                                iUvPhaseOffset = MOS_UF_ROUND(1.0F * 16.0F);  // U0.4
                            }

                            VPHAL_RENDER_CHK_STATUS_RETURN(CalcPolyphaseTablesUVOffset(
                                piUVCoefsParam,
                                3.0F,
                                fChromaScale,
                                iUvPhaseOffset));
                        }
                    }
                }
            }
        }
    }
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SetNearestModeTable(
    int32_t *iCoefs,
    uint32_t dwPlane,
    bool     bBalancedFilter)
{
    VP_FUNC_CALL();
    uint32_t   dwNumEntries;
    uint32_t   dwOffset;
    uint32_t   i;
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    MHW_FUNCTION_ENTER;

    MHW_CHK_NULL(iCoefs);

    if (dwPlane == MHW_GENERIC_PLANE || dwPlane == MHW_Y_PLANE)
    {
        dwNumEntries = NUM_POLYPHASE_Y_ENTRIES;
        dwOffset     = 3;
    }
    else  // if (dwPlane == MHW_U_PLANE || dwPlane == MHW_V_PLANE)
    {
        dwNumEntries = NUM_POLYPHASE_UV_ENTRIES;
        dwOffset     = 1;
    }

    for (i = 0; i <= NUM_HW_POLYPHASE_TABLES / 2; i++)
    {
        iCoefs[i * dwNumEntries + dwOffset] = 0x40;
    }

    if (bBalancedFilter)
    {
        // Fix offset so that filter is balanced
        for (i = (NUM_HW_POLYPHASE_TABLES / 2 + 1); i < NUM_HW_POLYPHASE_TABLES; i++)
        {
            iCoefs[i * dwNumEntries + dwOffset + 1] = 0x40;
        }
    }

finish:
    return eStatus;
}

MOS_STATUS VpRenderCmdPacket::CalcPolyphaseTablesUV(
    int32_t *piCoefs,
    float    fLanczosT,
    float    fInverseScaleFactor)
{
    VP_FUNC_CALL();
    int32_t    phaseCount, tableCoefUnit, centerPixel, sumQuantCoefs;
    double     phaseCoefs[MHW_SCALER_UV_WIN_SIZE];
    double     startOffset, sf, base, sumCoefs, pos;
    int32_t    minCoef[MHW_SCALER_UV_WIN_SIZE];
    int32_t    maxCoef[MHW_SCALER_UV_WIN_SIZE];
    int32_t    i, j;
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    MHW_FUNCTION_ENTER;

    MHW_CHK_NULL(piCoefs);

    phaseCount    = MHW_TABLE_PHASE_COUNT;
    centerPixel   = (MHW_SCALER_UV_WIN_SIZE / 2) - 1;
    startOffset   = (double)(-centerPixel);
    tableCoefUnit = 1 << MHW_TBL_COEF_PREC;
    sf            = MOS_MIN(1.0, fInverseScaleFactor);  // Sf isn't used for upscaling

    MOS_ZeroMemory(piCoefs, sizeof(int32_t) * MHW_SCALER_UV_WIN_SIZE * phaseCount);
    MOS_ZeroMemory(minCoef, sizeof(minCoef));
    MOS_ZeroMemory(maxCoef, sizeof(maxCoef));

    if (sf < 1.0F)
    {
        fLanczosT = 2.0F;
    }

    for (i = 0; i < phaseCount; ++i, piCoefs += MHW_SCALER_UV_WIN_SIZE)
    {
        // Write all
        // Note - to shift by a half you need to a half to each phase.
        base     = startOffset - (double)(i) / (double)(phaseCount);
        sumCoefs = 0.0;

        for (j = 0; j < MHW_SCALER_UV_WIN_SIZE; ++j)
        {
            pos           = base + (double)j;
            phaseCoefs[j] = MOS_Lanczos((float)(pos * sf), MHW_SCALER_UV_WIN_SIZE, fLanczosT);
            sumCoefs += phaseCoefs[j];
        }
        // Normalize coefs and save
        for (j = 0; j < MHW_SCALER_UV_WIN_SIZE; ++j)
        {
            piCoefs[j] = (int32_t)floor((0.5 + (double)(tableCoefUnit) * (phaseCoefs[j] / sumCoefs)));

            //For debug purposes:
            minCoef[j] = MOS_MIN(minCoef[j], piCoefs[j]);
            maxCoef[j] = MOS_MAX(maxCoef[j], piCoefs[j]);
        }

        // Recalc center coef
        sumQuantCoefs = 0;
        for (j = 0; j < MHW_SCALER_UV_WIN_SIZE; ++j)
        {
            sumQuantCoefs += piCoefs[j];
        }

        // Fix center coef so that filter is balanced
        if (i <= phaseCount / 2)
        {
            piCoefs[centerPixel] -= sumQuantCoefs - tableCoefUnit;
        }
        else
        {
            piCoefs[centerPixel + 1] -= sumQuantCoefs - tableCoefUnit;
        }
    }

finish:
    return eStatus;
}

MOS_STATUS VpRenderCmdPacket::CalcPolyphaseTablesY(
    int32_t *  iCoefs,
    float      fScaleFactor,
    uint32_t   dwPlane,
    MOS_FORMAT srcFmt,
    float      fHPStrength,
    bool       bUse8x8Filter,
    uint32_t   dwHwPhase)
{
    VP_FUNC_CALL();
    uint32_t   dwNumEntries;
    uint32_t   dwTableCoefUnit;
    uint32_t   i, j;
    int32_t    k;
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;
    float      fPhaseCoefs[NUM_POLYPHASE_Y_ENTRIES];
    float      fPhaseCoefsCopy[NUM_POLYPHASE_Y_ENTRIES];
    float      fStartOffset;
    float      fHPFilter[3], fHPSum, fHPHalfPhase;  // Only used for Y_PLANE
    float      fBase, fPos, fSumCoefs;
    float      fLanczosT;
    int32_t    iCenterPixel;
    int32_t    iSumQuantCoefs;

    MHW_FUNCTION_ENTER;

    MHW_CHK_NULL(iCoefs);
    MHW_ASSERT((dwHwPhase == MHW_NUM_HW_POLYPHASE_TABLES) || (dwHwPhase == NUM_HW_POLYPHASE_TABLES));

    if (dwPlane == MHW_GENERIC_PLANE || dwPlane == MHW_Y_PLANE)
    {
        dwNumEntries = NUM_POLYPHASE_Y_ENTRIES;
    }
    else  // if (dwPlane == MHW_U_PLANE || dwPlane == MHW_V_PLANE)
    {
        dwNumEntries = NUM_POLYPHASE_UV_ENTRIES;
    }

    MOS_ZeroMemory(fPhaseCoefs, sizeof(fPhaseCoefs));
    MOS_ZeroMemory(fPhaseCoefsCopy, sizeof(fPhaseCoefsCopy));

    dwTableCoefUnit = 1 << MHW_AVS_TBL_COEF_PREC;
    iCenterPixel    = dwNumEntries / 2 - 1;
    fStartOffset    = (float)(-iCenterPixel);

    if ((IS_YUV_FORMAT(srcFmt) &&
            dwPlane != MHW_U_PLANE &&
            dwPlane != MHW_V_PLANE) ||
        ((IS_RGB32_FORMAT(srcFmt) ||
             srcFmt == Format_Y410 ||
             srcFmt == Format_AYUV) &&
            dwPlane == MHW_Y_PLANE))
    {
        if (fScaleFactor < 1.0F)
        {
            fLanczosT = 4.0F;
        }
        else
        {
            fLanczosT = 8.0F;
        }
    }
    else  // if (dwPlane == MHW_U_PLANE || dwPlane == MHW_V_PLANE || (IS_RGB_FORMAT(srcFmt) && dwPlane != MHW_V_PLANE))
    {
        fLanczosT = 2.0F;
    }

    for (i = 0; i < dwHwPhase; i++)
    {
        fBase     = fStartOffset - (float)i / (float)NUM_POLYPHASE_TABLES;
        fSumCoefs = 0.0F;

        for (j = 0; j < dwNumEntries; j++)
        {
            fPos = fBase + (float)j;

            if (bUse8x8Filter)
            {
                fPhaseCoefs[j] = fPhaseCoefsCopy[j] = MOS_Lanczos(fPos * fScaleFactor, dwNumEntries, fLanczosT);
            }
            else
            {
                fPhaseCoefs[j] = fPhaseCoefsCopy[j] = MOS_Lanczos_g(fPos * fScaleFactor, NUM_POLYPHASE_5x5_Y_ENTRIES, fLanczosT);
            }

            fSumCoefs += fPhaseCoefs[j];
        }

        // Convolve with HP
        if (dwPlane == MHW_GENERIC_PLANE || dwPlane == MHW_Y_PLANE)
        {
            if (i <= NUM_POLYPHASE_TABLES / 2)
            {
                fHPHalfPhase = (float)i / (float)NUM_POLYPHASE_TABLES;
            }
            else
            {
                fHPHalfPhase = (float)(NUM_POLYPHASE_TABLES - i) / (float)NUM_POLYPHASE_TABLES;
            }
            fHPFilter[0] = fHPFilter[2] = -fHPStrength * MOS_Sinc(fHPHalfPhase * MOS_PI);
            fHPFilter[1]                = 1.0F + 2.0F * fHPStrength;

            for (j = 0; j < dwNumEntries; j++)
            {
                fHPSum = 0.0F;
                for (k = -1; k <= 1; k++)
                {
                    if ((((long)j + k) >= 0) && (j + k < dwNumEntries))
                    {
                        fHPSum += fPhaseCoefsCopy[(int32_t)j + k] * fHPFilter[k + 1];
                    }
                    fPhaseCoefs[j] = fHPSum;
                }
            }
        }

        // Normalize coefs and save
        iSumQuantCoefs = 0;
        for (j = 0; j < dwNumEntries; j++)
        {
            iCoefs[i * dwNumEntries + j] = (int32_t)floor(0.5F + (float)dwTableCoefUnit * fPhaseCoefs[j] / fSumCoefs);
            iSumQuantCoefs += iCoefs[i * dwNumEntries + j];
        }

        // Fix center coef so that filter is balanced
        if (i <= NUM_POLYPHASE_TABLES / 2)
        {
            iCoefs[i * dwNumEntries + iCenterPixel] -= iSumQuantCoefs - dwTableCoefUnit;
        }
        else
        {
            iCoefs[i * dwNumEntries + iCenterPixel + 1] -= iSumQuantCoefs - dwTableCoefUnit;
        }
    }

finish:
    return eStatus;
}

MOS_STATUS VpRenderCmdPacket::CalcPolyphaseTablesUVOffset(
    int32_t *piCoefs,
    float    fLanczosT,
    float    fInverseScaleFactor,
    int32_t  iUvPhaseOffset)
{
    VP_FUNC_CALL();
    int32_t    phaseCount, tableCoefUnit, centerPixel, sumQuantCoefs;
    double     phaseCoefs[MHW_SCALER_UV_WIN_SIZE];
    double     startOffset, sf, pos, sumCoefs, base;
    int32_t    minCoef[MHW_SCALER_UV_WIN_SIZE];
    int32_t    maxCoef[MHW_SCALER_UV_WIN_SIZE];
    int32_t    i, j;
    int32_t    adjusted_phase;
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    MHW_FUNCTION_ENTER;

    MHW_CHK_NULL(piCoefs);

    phaseCount    = MHW_TABLE_PHASE_COUNT;
    centerPixel   = (MHW_SCALER_UV_WIN_SIZE / 2) - 1;
    startOffset   = (double)(-centerPixel +
                           (double)iUvPhaseOffset / (double)(phaseCount));
    tableCoefUnit = 1 << MHW_TBL_COEF_PREC;

    MOS_ZeroMemory(minCoef, sizeof(minCoef));
    MOS_ZeroMemory(maxCoef, sizeof(maxCoef));
    MOS_ZeroMemory(piCoefs, sizeof(int32_t) * MHW_SCALER_UV_WIN_SIZE * phaseCount);

    sf = MOS_MIN(1.0, fInverseScaleFactor);  // Sf isn't used for upscaling
    if (sf < 1.0)
    {
        fLanczosT = 3.0;
    }

    for (i = 0; i < phaseCount; ++i, piCoefs += MHW_SCALER_UV_WIN_SIZE)
    {
        // Write all
        // Note - to shift by a half you need to a half to each phase.
        base     = startOffset - (double)(i) / (double)(phaseCount);
        sumCoefs = 0.0;

        for (j = 0; j < MHW_SCALER_UV_WIN_SIZE; ++j)
        {
            pos           = base + (double)j;
            phaseCoefs[j] = MOS_Lanczos((float)(pos * sf), 6 /*MHW_SCALER_UV_WIN_SIZE*/, fLanczosT);
            sumCoefs += phaseCoefs[j];
        }
        // Normalize coefs and save
        for (j = 0; j < MHW_SCALER_UV_WIN_SIZE; ++j)
        {
            piCoefs[j] = (int32_t)floor((0.5 + (double)(tableCoefUnit) * (phaseCoefs[j] / sumCoefs)));

            // For debug purposes:
            minCoef[j] = MOS_MIN(minCoef[j], piCoefs[j]);
            maxCoef[j] = MOS_MAX(maxCoef[j], piCoefs[j]);
        }

        // Recalc center coef
        sumQuantCoefs = 0;
        for (j = 0; j < MHW_SCALER_UV_WIN_SIZE; ++j)
        {
            sumQuantCoefs += piCoefs[j];
        }

        // Fix center coef so that filter is balanced
        adjusted_phase = i - iUvPhaseOffset;
        if (adjusted_phase <= phaseCount / 2)
        {
            piCoefs[centerPixel] -= sumQuantCoefs - tableCoefUnit;
        }
        else  // if(adjusted_phase < phaseCount)
        {
            piCoefs[centerPixel + 1] -= sumQuantCoefs - tableCoefUnit;
        }
    }

finish:
    return eStatus;
}

MOS_STATUS VpRenderCmdPacket::SubmitWithMultiKernel(MOS_COMMAND_BUFFER *commandBuffer, uint8_t packetPhase)
{
    VP_FUNC_CALL();
    PMOS_INTERFACE                  pOsInterface = nullptr;
    MOS_STATUS                      eStatus      = MOS_STATUS_SUCCESS;
    uint32_t                        dwSyncTag    = 0;
    int32_t                         i = 0, iRemaining = 0;
    PMHW_MI_INTERFACE               pMhwMiInterface     = nullptr;
    MhwRenderInterface *            pMhwRender          = nullptr;
    MHW_MEDIA_STATE_FLUSH_PARAM     FlushParam          = {};
    bool                            bEnableSLM          = false;
    RENDERHAL_GENERIC_PROLOG_PARAMS GenericPrologParams = {};
    MOS_RESOURCE                    GpuStatusBuffer     = {};
    MediaPerfProfiler *             pPerfProfiler       = nullptr;
    MOS_CONTEXT *                   pOsContext          = nullptr;
    PMHW_MI_MMIOREGISTERS           pMmioRegisters      = nullptr;

    RENDER_PACKET_CHK_NULL_RETURN(m_renderHal);
    RENDER_PACKET_CHK_NULL_RETURN(m_renderHal->pMhwRenderInterface);
    RENDER_PACKET_CHK_NULL_RETURN(m_renderHal->pMhwMiInterface);
    RENDER_PACKET_CHK_NULL_RETURN(m_renderHal->pMhwRenderInterface->GetMmioRegisters());
    RENDER_PACKET_CHK_NULL_RETURN(m_renderHal->pOsInterface);
    RENDER_PACKET_CHK_NULL_RETURN(m_renderHal->pOsInterface->pOsContext);

    eStatus         = MOS_STATUS_UNKNOWN;
    pOsInterface    = m_renderHal->pOsInterface;
    pMhwMiInterface = m_renderHal->pMhwMiInterface;
    pMhwRender      = m_renderHal->pMhwRenderInterface;
    iRemaining      = 0;
    FlushParam      = g_cRenderHal_InitMediaStateFlushParams;
    pPerfProfiler   = m_renderHal->pPerfProfiler;
    pOsContext      = pOsInterface->pOsContext;
    pMmioRegisters  = pMhwRender->GetMmioRegisters();

    RENDER_PACKET_CHK_STATUS_RETURN(SetPowerMode(CombinedFc));

    // Initialize command buffer and insert prolog
    RENDER_PACKET_CHK_STATUS_RETURN(m_renderHal->pfnInitCommandBuffer(m_renderHal, commandBuffer, &GenericPrologParams));

    RENDER_PACKET_CHK_STATUS_RETURN(pPerfProfiler->AddPerfCollectStartCmd((void *)m_renderHal, pOsInterface, pMhwMiInterface, commandBuffer));

    // Write timing data for 3P budget
    RENDER_PACKET_CHK_STATUS_RETURN(m_renderHal->pfnSendTimingData(m_renderHal, commandBuffer, true));

    bEnableSLM = false;  // Media walker first
    RENDER_PACKET_CHK_STATUS_RETURN(m_renderHal->pfnSetCacheOverrideParams(
        m_renderHal,
        &m_renderHal->L3CacheSettings,
        bEnableSLM));

    // Flush media states
    VP_RENDER_CHK_STATUS_RETURN(SendMediaStates(m_renderHal, commandBuffer));

    // Write back GPU Status tag
    if (!pOsInterface->bEnableKmdMediaFrameTracking)
    {
        RENDER_PACKET_CHK_STATUS_RETURN(m_renderHal->pfnSendRcsStatusTag(m_renderHal, commandBuffer));
    }

    RENDER_PACKET_CHK_STATUS_RETURN(pPerfProfiler->AddPerfCollectEndCmd((void *)m_renderHal, pOsInterface, pMhwMiInterface, commandBuffer));

    // Write timing data for 3P budget
    RENDER_PACKET_CHK_STATUS_RETURN(m_renderHal->pfnSendTimingData(m_renderHal, commandBuffer, false));

    MHW_PIPE_CONTROL_PARAMS PipeControlParams;

    MOS_ZeroMemory(&PipeControlParams, sizeof(PipeControlParams));
    PipeControlParams.dwFlushMode                   = MHW_FLUSH_WRITE_CACHE;
    PipeControlParams.bGenericMediaStateClear       = true;
    PipeControlParams.bIndirectStatePointersDisable = true;
    PipeControlParams.bDisableCSStall               = false;
    RENDER_PACKET_CHK_STATUS_RETURN(pMhwMiInterface->AddPipeControl(commandBuffer, nullptr, &PipeControlParams));

    if (MEDIA_IS_WA(m_renderHal->pWaTable, WaSendDummyVFEafterPipelineSelect))
    {
        MHW_VFE_PARAMS VfeStateParams       = {};
        VfeStateParams.dwNumberofURBEntries = 1;
        RENDER_PACKET_CHK_STATUS_RETURN(pMhwRender->AddMediaVfeCmd(commandBuffer, &VfeStateParams));
    }

    // Add media flush command in case HW not cleaning the media state
    if (MEDIA_IS_WA(m_renderHal->pWaTable, WaMSFWithNoWatermarkTSGHang))
    {
        FlushParam.bFlushToGo = true;
        if (m_walkerType == WALKER_TYPE_MEDIA)
        {
            FlushParam.ui8InterfaceDescriptorOffset = m_mediaWalkerParams.InterfaceDescriptorOffset;
        }
        else
        {
            RENDER_PACKET_ASSERTMESSAGE("ERROR, pWalkerParams is nullptr and cannot get InterfaceDescriptorOffset.");
        }
        RENDER_PACKET_CHK_STATUS_RETURN(pMhwMiInterface->AddMediaStateFlush(commandBuffer, nullptr, &FlushParam));
    }
    else if (MEDIA_IS_WA(m_renderHal->pWaTable, WaAddMediaStateFlushCmd))
    {
        RENDER_PACKET_CHK_STATUS_RETURN(pMhwMiInterface->AddMediaStateFlush(commandBuffer, nullptr, &FlushParam));
    }

    if (pBatchBuffer)
    {
        // Send Batch Buffer end command (HW/OS dependent)
        RENDER_PACKET_CHK_STATUS_RETURN(pMhwMiInterface->AddMiBatchBufferEnd(commandBuffer, nullptr));
    }
    else if (IsMiBBEndNeeded(pOsInterface))
    {
        // Send Batch Buffer end command for 1st level Batch Buffer
        RENDER_PACKET_CHK_STATUS_RETURN(pMhwMiInterface->AddMiBatchBufferEnd(commandBuffer, nullptr));
    }
    else if (m_renderHal->pOsInterface->bNoParsingAssistanceInKmd)
    {
        RENDER_PACKET_CHK_STATUS_RETURN(pMhwMiInterface->AddMiBatchBufferEnd(commandBuffer, nullptr));
    }

    // Return unused command buffer space to OS
    pOsInterface->pfnReturnCommandBuffer(pOsInterface, commandBuffer, 0);

    MOS_NULL_RENDERING_FLAGS NullRenderingFlags;

    NullRenderingFlags =
        pOsInterface->pfnGetNullHWRenderFlags(pOsInterface);

    if ((NullRenderingFlags.VPLgca ||
            NullRenderingFlags.VPGobal) == false)
    {
        dwSyncTag = m_renderHal->pStateHeap->dwNextTag++;

        // Set media state and batch buffer as busy
        m_renderHal->pStateHeap->pCurMediaState->bBusy = true;
        if (pBatchBuffer)
        {
            pBatchBuffer->bBusy     = true;
            pBatchBuffer->dwSyncTag = dwSyncTag;
        }
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpRenderCmdPacket::SendMediaStates(
    PRENDERHAL_INTERFACE pRenderHal,
    PMOS_COMMAND_BUFFER  pCmdBuffer)
{
    VP_FUNC_CALL();
    PMOS_INTERFACE                  pOsInterface          = nullptr;
    MhwRenderInterface *            pMhwRender            = nullptr;
    PMHW_MI_INTERFACE               pMhwMiInterface       = nullptr;
    PRENDERHAL_STATE_HEAP           pStateHeap            = nullptr;
    MOS_STATUS                      eStatus               = MOS_STATUS_SUCCESS;
    MHW_VFE_PARAMS *                pVfeStateParams       = nullptr;
    MOS_CONTEXT *                   pOsContext            = nullptr;
    MHW_MI_LOAD_REGISTER_IMM_PARAMS loadRegisterImmParams = {};
    PMHW_MI_MMIOREGISTERS           pMmioRegisters        = nullptr;
    MOS_OCA_BUFFER_HANDLE           hOcaBuf               = 0;

    //---------------------------------------
    MHW_RENDERHAL_CHK_NULL(pRenderHal);
    MHW_RENDERHAL_CHK_NULL(pRenderHal->pMhwRenderInterface);
    MHW_RENDERHAL_CHK_NULL(pRenderHal->pMhwMiInterface);
    MHW_RENDERHAL_CHK_NULL(pRenderHal->pStateHeap);
    MHW_RENDERHAL_CHK_NULL(pRenderHal->pRenderHalPltInterface);
    MHW_RENDERHAL_ASSERT(pRenderHal->pStateHeap->bGshLocked);
    MHW_RENDERHAL_CHK_NULL(pRenderHal->pMhwRenderInterface->GetMmioRegisters());

    //---------------------------------------
    pOsInterface    = pRenderHal->pOsInterface;
    pMhwRender      = pRenderHal->pMhwRenderInterface;
    pMhwMiInterface = pRenderHal->pMhwMiInterface;
    pStateHeap      = pRenderHal->pStateHeap;
    pOsContext      = pOsInterface->pOsContext;
    pMmioRegisters  = pMhwRender->GetMmioRegisters();

    // This need not be secure, since PPGTT will be used here. But moving this after
    // L3 cache configuration will delay UMD from fetching another media state.
    // Send Sync Tag
    MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendSyncTag(pRenderHal, pCmdBuffer));

    // Setup L3$ Config, LRI commands used here & hence must be launched from a secure bb
    pRenderHal->L3CacheSettings.bEnableSLM = (m_walkerType == WALKER_TYPE_COMPUTE && m_slmSize > 0) ? true : false;
    MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnEnableL3Caching(pRenderHal, &pRenderHal->L3CacheSettings));

    // Send L3 Cache Configuration
    MHW_RENDERHAL_CHK_STATUS(pMhwRender->SetL3Cache(pCmdBuffer));

    MHW_RENDERHAL_CHK_STATUS(pMhwRender->EnablePreemption(pCmdBuffer));

    // Send Pipeline Select command
    MHW_RENDERHAL_CHK_STATUS(pMhwRender->AddPipelineSelectCmd(pCmdBuffer, (m_walkerType == WALKER_TYPE_COMPUTE) ? true : false));

    // The binding table for surface states is at end of command buffer. No need to add it to indirect state heap.
    HalOcaInterface::OnIndirectState(*pCmdBuffer, *pOsContext, pRenderHal->StateBaseAddressParams.presInstructionBuffer, pStateHeap->CurIDEntryParams.dwKernelOffset, false, pStateHeap->iKernelUsedForDump);

    // Send State Base Address command
    MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendStateBaseAddress(pRenderHal, pCmdBuffer));

    if (pRenderHal->bComputeContextInUse)
    {
        pRenderHal->pRenderHalPltInterface->SendTo3DStateBindingTablePoolAlloc(pRenderHal, pCmdBuffer);
    }

    // Send Surface States
    MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendSurfaces(pRenderHal, pCmdBuffer));

    // Send SIP State if ASM debug enabled
    if (pRenderHal->bIsaAsmDebugEnable)
    {
        MHW_RENDERHAL_CHK_STATUS(pMhwRender->AddSipStateCmd(pCmdBuffer,
            &pRenderHal->SipStateParams));
    }

    pVfeStateParams = pRenderHal->pRenderHalPltInterface->GetVfeStateParameters();
    if (!pRenderHal->bComputeContextInUse)
    {
        // set VFE State
        MHW_RENDERHAL_CHK_STATUS(pMhwRender->AddMediaVfeCmd(pCmdBuffer, pVfeStateParams));
    }
    else
    {
        // set CFE State
        MHW_RENDERHAL_CHK_STATUS(pMhwRender->AddCfeStateCmd(pCmdBuffer, pVfeStateParams));
    }

    // Send CURBE Load
    if (!pRenderHal->bComputeContextInUse)
    {
        MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendCurbeLoad(pRenderHal, pCmdBuffer));
    }

    // Send Interface Descriptor Load
    if (!pRenderHal->bComputeContextInUse)
    {
        MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendMediaIdLoad(pRenderHal, pCmdBuffer));
    }

    // Send Chroma Keys
    MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendChromaKey(pRenderHal, pCmdBuffer));

    // Send Palettes in use
    MHW_RENDERHAL_CHK_STATUS(pRenderHal->pfnSendPalette(pRenderHal, pCmdBuffer));

    HalOcaInterface::OnDispatch(*pCmdBuffer, *pOsContext, *pRenderHal->pMhwMiInterface, *pMmioRegisters);

    for (uint32_t kernelIndex = 0; kernelIndex < m_kernelRenderData.size(); kernelIndex++)
    {
        auto it = m_kernelRenderData.find(kernelIndex);
        if (it == m_kernelRenderData.end())
        {
            eStatus = MOS_STATUS_INVALID_PARAMETER;
            goto finish;
        }

        if (kernelIndex > 0 && it->second.walkerParam.bSyncFlag)
        {
            MHW_PIPE_CONTROL_PARAMS pipeCtlParams = g_cRenderHal_InitPipeControlParams;
            pipeCtlParams.dwPostSyncOp            = MHW_FLUSH_NOWRITE;
            pipeCtlParams.dwFlushMode             = MHW_FLUSH_CUSTOM;
            pipeCtlParams.bInvalidateTextureCache = true;
            pipeCtlParams.bFlushRenderTargetCache = true;
            MHW_RENDERHAL_CHK_STATUS(pMhwMiInterface->AddPipeControl(pCmdBuffer,
                nullptr,
                &pipeCtlParams));
        }

        if (m_walkerType == WALKER_TYPE_MEDIA)
        {
            MOS_ZeroMemory(&m_mediaWalkerParams, sizeof(m_mediaWalkerParams));

            MHW_RENDERHAL_CHK_STATUS(PrepareMediaWalkerParams(it->second.walkerParam, m_mediaWalkerParams));

            MHW_RENDERHAL_CHK_STATUS(pMhwRender->AddMediaObjectWalkerCmd(
                pCmdBuffer,
                &m_mediaWalkerParams));
        }
        else if (m_walkerType == WALKER_TYPE_COMPUTE)
        {
            MOS_ZeroMemory(&m_gpgpuWalkerParams, sizeof(m_gpgpuWalkerParams));

            MHW_RENDERHAL_CHK_STATUS(PrepareComputeWalkerParams(it->second.walkerParam, m_gpgpuWalkerParams));

            MHW_RENDERHAL_CHK_STATUS(pRenderHal->pRenderHalPltInterface->SendComputeWalker(
                pRenderHal,
                pCmdBuffer,
                &m_gpgpuWalkerParams));
        }
        else
        {
            eStatus = MOS_STATUS_UNIMPLEMENTED;
            goto finish;
        }
    }

finish:
    return eStatus;
}

}  // namespace vp
