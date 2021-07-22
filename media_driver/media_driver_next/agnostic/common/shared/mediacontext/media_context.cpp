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
//! \file     media_context.cpp
//! \brief    Defines the common interface for media context
//! \details  The media context interface is further sub-divided by component,
//!           this file is for the base interface which is shared by all components.
//!

#include "media_context.h"
#include "mos_gpucontextmgr.h"
#include "mos_interface.h"
#include "mos_os_virtualengine_next.h"
#include "codechal_hw.h"
#include "decode_scalability_defs.h"

MediaContext::MediaContext(uint8_t componentType, void *hwInterface, PMOS_INTERFACE osInterface)
{
    if (!hwInterface)
    {
        MOS_OS_ASSERTMESSAGE("null HW interface, failed to create Media Context");
        return;
    }

    if (!osInterface)
    {
        MOS_OS_ASSERTMESSAGE("null OS interface, failed to create Media Context");
        return;
    }

    if (componentType >= scalabilityTotal)
    {
        MOS_OS_ASSERTMESSAGE("Invalid component type, failed to create Media Context");
        return;
    }

    m_hwInterface = hwInterface;
    m_osInterface = osInterface;
    m_componentType = componentType;

    m_streamId = m_osInterface->streamIndex;
    m_gpuContextAttributeTable.clear();
}

MediaContext::~MediaContext()
{
    if (m_osInterface && m_osInterface->pfnWaitAllCmdCompletion)
    {
        m_osInterface->pfnWaitAllCmdCompletion(m_osInterface);
    }

    for (auto& curAttribute : m_gpuContextAttributeTable)
    {
        if (curAttribute.scalabilityState)
        {
            curAttribute.scalabilityState->Destroy();
            MOS_Delete(curAttribute.scalabilityState);
            // set legacy MOS ve interface to null to be compatible to legacy MOS
            m_osInterface->pVEInterf = nullptr;
        }
        else
        {
            MOS_OS_ASSERTMESSAGE("scalabilityState is nullptr, something must be wrong");
            return;
        }

        if (!m_osInterface || !m_osInterface->pOsContext)
        {
            MOS_OS_ASSERTMESSAGE("m_osInterface and OsContext cannot be nullptr");
            return;
        }

        if (curAttribute.gpuContext != MOS_GPU_CONTEXT_INVALID_HANDLE)
        {
            if (m_osInterface->apoMosEnabled)
            {
                auto status = MosInterface::DestroyGpuContext(m_osInterface->osStreamState, curAttribute.gpuContext);

                if (status != MOS_STATUS_SUCCESS)
                {
                    MOS_OS_NORMALMESSAGE("Gpu Context destory failed, something must be wrong");
                    return;
                }
            }
            else
            {
                auto gpuContextMgr = m_osInterface->pfnGetGpuContextMgr(m_osInterface);
                
                if (gpuContextMgr == nullptr)
                {
                    //No need to destory GPU context when adv_gpucontext not enabled in Os context
                    MOS_OS_NORMALMESSAGE("There is no Gpu context manager, adv gpu context not enabled, no need to destory GPU contexts.");
                    return;
                }
                else
                {
                    auto gpuContext = gpuContextMgr->GetGpuContext(curAttribute.gpuContext);
                    if (gpuContext)
                    {
                        gpuContextMgr->DestroyGpuContext(gpuContext);
                    }
                    else
                    {
                        MOS_OS_ASSERTMESSAGE("Not found gpu Context to destory, something must be wrong");
                        return;
                    }
                }
            }
        }
        else
        {
            MOS_OS_ASSERTMESSAGE("Invalid gpu Context handle in entry, something must be wrong");
            return;
        }

        // Be compatible to legacy MOS
        m_osInterface->pfnSetGpuContextHandle(m_osInterface, MOS_GPU_CONTEXT_INVALID_HANDLE, curAttribute.ctxForLegacyMos);
    }

    m_gpuContextAttributeTable.clear();
}

MOS_STATUS MediaContext::SwitchContext(MediaFunction func, ContextRequirement *requirement, MediaScalability **scalabilityState)
{
    MOS_OS_FUNCTION_ENTER;
    MOS_STATUS status = MOS_STATUS_SUCCESS;

    MOS_OS_CHK_NULL_RETURN(m_osInterface);
    MOS_OS_CHK_NULL_RETURN(m_osInterface->pOsContext);
    MOS_OS_CHK_NULL_RETURN(scalabilityState);
    MOS_OS_CHK_NULL_RETURN(requirement);

    if (func >= INVALID_MEDIA_FUNCTION)
    {
        MOS_OS_ASSERTMESSAGE("Func required is invalid");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    MediaScalability * veStateProvided = nullptr;

    if (MEDIA_IS_SKU(m_osInterface->pfnGetSkuTable(m_osInterface), FtrRAMode) && IS_RENDER_ENGINE_FUNCTION(func))
    {
        auto scalPars = (ScalabilityPars *)requirement;
        MOS_OS_CHK_NULL_RETURN(scalPars);
        MOS_OS_CHK_NULL_RETURN(m_osInterface->osCpInterface);
        scalPars->raMode = m_osInterface->osCpInterface->IsHMEnabled() ? 1 : 0;
        if (scalPars->raMode)
        {
            MOS_OS_NORMALMESSAGE("request RA mode context for protected render workload");
            WriteUserFeature(__MEDIA_USER_FEATURE_VALUE_RA_MODE_ENABLE_ID, 1, m_osInterface->pOsContext);
        }
    }

    uint32_t index = m_invalidContextAttribute;
    MOS_OS_CHK_STATUS_RETURN(SearchContext<ScalabilityPars*>(func, (ScalabilityPars*)requirement, index));
    if (index == m_invalidContextAttribute)
    {
        MOS_OS_CHK_STATUS_RETURN(CreateContext<ScalabilityPars*>(func, (ScalabilityPars*)requirement, index));
    }
    if (index == m_invalidContextAttribute || index >= m_gpuContextAttributeTable.size())
    {
        MOS_OS_ASSERTMESSAGE("Incorrect index get from Context attribute table");
        return MOS_STATUS_UNKNOWN;
    }

    // Be compatible to legacy MOS
    MOS_OS_CHK_STATUS_RETURN(m_osInterface->pfnSetGpuContext(m_osInterface, m_gpuContextAttributeTable[index].ctxForLegacyMos));
    veStateProvided = m_gpuContextAttributeTable[index].scalabilityState;

    if (requirement->IsEnc)
    {
        m_osInterface->pfnSetEncodeEncContext(m_osInterface, m_gpuContextAttributeTable[index].ctxForLegacyMos);
    }
    if (requirement->IsPak)
    {
        m_osInterface->pfnSetEncodePakContext(m_osInterface, m_gpuContextAttributeTable[index].ctxForLegacyMos);
    }

    m_osInterface->pfnResetOsStates(m_osInterface);

    *scalabilityState = veStateProvided;

    return status;
}

template<typename T>
MOS_STATUS MediaContext::SearchContext(MediaFunction func, T params, uint32_t& indexFound)
{
    MOS_OS_FUNCTION_ENTER;

    MOS_OS_CHK_NULL_RETURN(m_osInterface);

    indexFound = m_invalidContextAttribute;
    uint32_t index = 0;

    for (auto& curAttribute : m_gpuContextAttributeTable)
    {
        if (curAttribute.func == func)
        {
            MOS_OS_CHK_NULL_RETURN(curAttribute.scalabilityState);

            if (curAttribute.scalabilityState->IsScalabilityModeMatched(params))
            {
                // Found the matching GPU context and scalability state
                indexFound = index;

                // Be compatible to legacy MOS
                MOS_OS_CHK_STATUS_RETURN(m_osInterface->pfnSetGpuContextHandle(m_osInterface, curAttribute.gpuContext, curAttribute.ctxForLegacyMos));
                // set legacy MOS ve interface to current reused scalability state's ve interface
                m_osInterface->pVEInterf = curAttribute.scalabilityState->m_veInterface;
                if (m_osInterface->apoMosEnabled)
                {
                    MOS_OS_CHK_NULL_RETURN(curAttribute.scalabilityState->m_veState);
                    MOS_OS_CHK_STATUS_RETURN(MosInterface::SetVirtualEngineState(
                        m_osInterface->osStreamState, curAttribute.scalabilityState->m_veState));
                }
                break;
            }
        }

        index++;
    }

    return MOS_STATUS_SUCCESS;
}

template<typename T>
MOS_STATUS MediaContext::CreateContext(MediaFunction func, T params, uint32_t& indexReturn)
{
    MOS_OS_FUNCTION_ENTER;

    MOS_OS_CHK_NULL_RETURN(m_osInterface);

    // Reach the max number of Gpu Context attr entries in current Media Context
    if (m_gpuContextAttributeTable.size() == m_maxContextAttribute)
    {
        MOS_OS_ASSERTMESSAGE("Reached max num of entries of gpuContextAttributeTable: 4096. Cannot create more Gpu Contexts");
        return MOS_STATUS_NOT_ENOUGH_BUFFER;
    }

    GpuContextAttribute newAttr;

    // Setup func
    newAttr.func = func;
    if (newAttr.func >= INVALID_MEDIA_FUNCTION)
    {
        MOS_OS_ASSERTMESSAGE("Func required is invalid");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    // Create scalabilityState
    MOS_GPUCTX_CREATOPTIONS_ENHANCED option;
    MediaScalabilityFactory<T> scalabilityFactory;
    newAttr.scalabilityState = scalabilityFactory.CreateScalability(
        m_componentType, params, m_hwInterface, this, &option);
    if (newAttr.scalabilityState == nullptr)
    {
        MOS_OS_ASSERTMESSAGE("Failed to create scalability state");
        return MOS_STATUS_NO_SPACE;
    }

    // request to create or reuse gpuContext
    MOS_GPU_NODE node = MOS_GPU_NODE_MAX;
    MOS_OS_CHK_STATUS_RETURN(FunctionToNode(func, option, node));

    // WA for legacy MOS
    MOS_OS_CHK_STATUS_RETURN(FunctionToGpuContext(func, option, node, newAttr.ctxForLegacyMos));

    if(m_osInterface->bSetHandleInvalid)
    {
        MOS_OS_CHK_STATUS_RETURN(m_osInterface->pfnSetGpuContextHandle(m_osInterface, MOS_GPU_CONTEXT_INVALID_HANDLE, newAttr.ctxForLegacyMos));
    }

    MOS_OS_CHK_STATUS_RETURN(m_osInterface->pfnCreateGpuContext(m_osInterface, newAttr.ctxForLegacyMos, node, &option));
    m_osInterface->pfnSetGpuContext(m_osInterface, newAttr.ctxForLegacyMos);
    newAttr.gpuContext = m_osInterface->CurrentGpuContextHandle;

    // Add entry to the table
    indexReturn = m_gpuContextAttributeTable.size();
    m_gpuContextAttributeTable.push_back(newAttr);

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaContext::FunctionToNode(MediaFunction func, const MOS_GPUCTX_CREATOPTIONS_ENHANCED &option, MOS_GPU_NODE& node)
{
    MOS_OS_FUNCTION_ENTER;
    MOS_STATUS status = MOS_STATUS_SUCCESS;

    if (func >= INVALID_MEDIA_FUNCTION)
    {
        MOS_OS_ASSERTMESSAGE("Func required is invalid");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    switch (func)
    {
    case RenderGenericFunc:
        node = MOS_GPU_NODE_3D;
        break;
    case VdboxDecodeFunc:
        if (option.LRCACount >= 2)
        {
            node = MOS_GPU_NODE_VIDEO; // multiple pipe decode always using VIDEO node
        }
        else
        {
            MOS_OS_CHK_STATUS_RETURN(FunctionToNodeDecode(node));
        }
        break;
    case VdboxDecodeWaFunc:
    case VdboxDecrpytFunc:
    case VdboxEncodeFunc:
    case VdboxCpFunc:
        node = MOS_GPU_NODE_VIDEO;
        break;
    case VeboxVppFunc:
        node = MOS_GPU_NODE_VE;
        break;
    case ComputeMdfFunc:
    case ComputeVppFunc:
        node = MOS_GPU_NODE_COMPUTE;
        break;
    default:
        MOS_OS_ASSERTMESSAGE("Cannot find the GPU node by the func");
        status = MOS_STATUS_INVALID_PARAMETER;
        node = MOS_GPU_NODE_MAX;
        break;
    }

    return status;
}

MOS_STATUS MediaContext::FunctionToNodeDecode(MOS_GPU_NODE& node)
{
    CodechalHwInterface *hwInterface = static_cast<CodechalHwInterface *>(m_hwInterface);
    MhwVdboxMfxInterface *mfxInterface = hwInterface->GetMfxInterface();
    MOS_OS_CHK_NULL_RETURN(mfxInterface);

    MHW_VDBOX_GPUNODE_LIMIT gpuNodeLimit = { 0 };
    MOS_OS_CHK_STATUS_RETURN(mfxInterface->FindGpuNodeToUse(&gpuNodeLimit));
    node = (MOS_GPU_NODE)(gpuNodeLimit.dwGpuNodeToUse);

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaContext::FunctionToGpuContext(MediaFunction func, const MOS_GPUCTX_CREATOPTIONS_ENHANCED &option, const MOS_GPU_NODE &node, MOS_GPU_CONTEXT &ctx)
{
    MOS_OS_FUNCTION_ENTER;

    if (func >= INVALID_MEDIA_FUNCTION)
    {
        MOS_OS_ASSERTMESSAGE("Func is invalid");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    switch (func)
    {
    case VdboxEncodeFunc:
        MOS_OS_CHK_STATUS_RETURN(FunctionToGpuContextEncode(option, ctx));
        break;
    case VdboxDecodeFunc:
        MOS_OS_CHK_STATUS_RETURN(FunctionToGpuContextDecode(option, node, ctx));
        break;
    case VdboxDecodeWaFunc:
        ctx = MOS_GPU_CONTEXT_VIDEO2;
        break;
    case VdboxDecrpytFunc:
        ctx = MOS_GPU_CONTEXT_VDBOX2_VIDEO2;
        break;
    case VeboxVppFunc:
        ctx = MOS_GPU_CONTEXT_VEBOX;
        break;
    case RenderGenericFunc:
        ctx = MOS_GPU_CONTEXT_RENDER;
        break;
    case ComputeVppFunc:
        ctx = MOS_GPU_CONTEXT_COMPUTE;
        break;
    case ComputeMdfFunc:
        ctx = MOS_GPU_CONTEXT_CM_COMPUTE;
        break;
    case VdboxCpFunc:
        ctx = MOS_GPU_CONTEXT_VIDEO;
        break;
    default:
        ctx = MOS_GPU_CONTEXT_MAX;
        MOS_OS_ASSERTMESSAGE("Func is invalid");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaContext::FunctionToGpuContextDecode(const MOS_GPUCTX_CREATOPTIONS_ENHANCED &option, const MOS_GPU_NODE &node, MOS_GPU_CONTEXT &ctx)
{
    if (option.UsingSFC)
    {
        ctx = MOS_GPU_CONTEXT_VIDEO4;
    }
    else
    {
        switch (option.LRCACount)
        {
        case 0:
        case 1:
            ctx = (node == MOS_GPU_NODE_VIDEO) ? MOS_GPU_CONTEXT_VIDEO : MOS_GPU_CONTEXT_VDBOX2_VIDEO;
            break;
        case 2:
            ctx = MOS_GPU_CONTEXT_VIDEO5;
            break;
        case 3:
            ctx = MOS_GPU_CONTEXT_VIDEO7;
            break;
        default:
            ctx = MOS_GPU_CONTEXT_VIDEO;
            break;
        }
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS MediaContext::FunctionToGpuContextEncode(const MOS_GPUCTX_CREATOPTIONS_ENHANCED &option, MOS_GPU_CONTEXT &ctx)
{
    switch (option.LRCACount)
    {
    case 0:
    case 1:
        ctx = MOS_GPU_CONTEXT_VIDEO3;
        break;
    case 2:
        ctx = MOS_GPU_CONTEXT_VIDEO6;
        break;
    case 4:
        ctx = MOS_GPU_CONTEXT_VIDEO6;  //Change to proper slot when MOS_GPU_CONTEXT num extended
        break;
    default:
        ctx = MOS_GPU_CONTEXT_VIDEO3;
        break;
    }

    return MOS_STATUS_SUCCESS;
}
