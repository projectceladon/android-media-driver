/*
* Copyright (c) 2019-2020, Intel Corporation
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
//! \file     decode_scalability_singlepipe.cpp
//! \brief    Defines the common interface for media scalability singlepipe mode.
//! \details  The media scalability singlepipe interface is further sub-divided by component,
//!           this file is for the base interface which is shared by all components.
//!

#include "codechal_hw.h"
#include "decode_scalability_defs.h"
#include "decode_scalability_singlepipe.h"

#include "media_context.h"
#include "media_status_report.h"
#include "mhw_utilities.h"
#include "decode_status_report_defs.h"

namespace decode
{

DecodeScalabilitySinglePipe::DecodeScalabilitySinglePipe(void *hwInterface, MediaContext *mediaContext, uint8_t componentType) :
    MediaScalabilitySinglePipe(hwInterface, mediaContext, componentType)
{
    if (hwInterface == nullptr)
    {
        return;
    }
    m_hwInterface = (CodechalHwInterface *)hwInterface;
    m_osInterface = m_hwInterface->GetOsInterface();
}

MOS_STATUS DecodeScalabilitySinglePipe::Initialize(const MediaScalabilityOption &option)
{
    SCALABILITY_CHK_NULL_RETURN(m_osInterface);

    DecodeScalabilityOption *decodeScalabilityOption = MOS_New(DecodeScalabilityOption, (const DecodeScalabilityOption &)option);
    SCALABILITY_CHK_NULL_RETURN(decodeScalabilityOption);
    m_scalabilityOption = decodeScalabilityOption;

    m_frameTrackingEnabled = m_osInterface->bEnableKmdMediaFrameTracking ? true : false;

    // !Don't check the return status here, because this function will return fail if there's no regist key in register.
    // But it's normal that regist key not in register.
    Mos_CheckVirtualEngineSupported(m_osInterface, false, true);
    m_miInterface = m_hwInterface->GetMiInterface();
    SCALABILITY_CHK_NULL_RETURN(m_miInterface);

    SCALABILITY_CHK_STATUS_RETURN(MediaScalabilitySinglePipe::Initialize(option));
    PMOS_GPUCTX_CREATOPTIONS_ENHANCED gpuCtxCreateOption = dynamic_cast<PMOS_GPUCTX_CREATOPTIONS_ENHANCED>(m_gpuCtxCreateOption);
    SCALABILITY_CHK_NULL_RETURN(gpuCtxCreateOption);
    gpuCtxCreateOption->UsingSFC = decodeScalabilityOption->IsUsingSFC();
    if (decodeScalabilityOption->IsUsingSlimVdbox())
    {
        gpuCtxCreateOption->Flags |=  (1 << 2);
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodeScalabilitySinglePipe::VerifyCmdBuffer(uint32_t requestedSize, uint32_t requestedPatchListSize, bool &singleTaskPhaseSupportedInPak)
{
    SCALABILITY_FUNCTION_ENTER;

    return MediaScalabilitySinglePipe::VerifyCmdBuffer(requestedSize, requestedPatchListSize, singleTaskPhaseSupportedInPak);
}

MOS_STATUS DecodeScalabilitySinglePipe::VerifySpaceAvailable(uint32_t requestedSize, uint32_t requestedPatchListSize, bool &singleTaskPhaseSupportedInPak)
{
    SCALABILITY_FUNCTION_ENTER;

    uint8_t looptimes = 3;
    for(auto i = 0 ; i < looptimes ; i++)
    {
        bool bothPatchListAndCmdBufChkSuccess = false;
        SCALABILITY_CHK_STATUS_RETURN(MediaScalability::VerifySpaceAvailable(
            requestedSize, requestedPatchListSize, bothPatchListAndCmdBufChkSuccess));

        if (bothPatchListAndCmdBufChkSuccess)
        {
            return MOS_STATUS_SUCCESS;
        }

        MOS_STATUS statusPatchList = MOS_STATUS_SUCCESS;
        if (requestedPatchListSize > 0)
        {
            statusPatchList = (MOS_STATUS)m_osInterface->pfnVerifyPatchListSize(
                m_osInterface,
                requestedPatchListSize);
        }

        MOS_STATUS statusCmdBuf = (MOS_STATUS)m_osInterface->pfnVerifyCommandBufferSize(
            m_osInterface,
            requestedSize,
            0);

        if (statusCmdBuf == MOS_STATUS_SUCCESS && statusPatchList == MOS_STATUS_SUCCESS)
        {
            return MOS_STATUS_SUCCESS;
        }
    }

    SCALABILITY_ASSERTMESSAGE("Resize Command buffer failed with no space!");
    return MOS_STATUS_NO_SPACE;
}

MOS_STATUS DecodeScalabilitySinglePipe::UpdateState(void *statePars)
{
    SCALABILITY_FUNCTION_ENTER;
    SCALABILITY_CHK_NULL_RETURN(statePars);

    SCALABILITY_CHK_STATUS_RETURN(MediaScalabilitySinglePipe::UpdateState(statePars));

    StateParams *stateParams     = (StateParams *)statePars;
    m_singleTaskPhaseSupported   = stateParams->singleTaskPhaseSupported;
    m_statusReport               = stateParams->statusReport;
    m_currentPass                = stateParams->currentPass;
    m_componentState             = stateParams->componentState;
    SCALABILITY_CHK_NULL_RETURN(m_statusReport);

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodeScalabilitySinglePipe::ResizeCommandBufferAndPatchList(
    uint32_t                    requestedCommandBufferSize,
    uint32_t                    requestedPatchListSize)
{
    SCALABILITY_FUNCTION_ENTER;
    SCALABILITY_CHK_NULL_RETURN(m_hwInterface);

    return m_hwInterface->ResizeCommandBufferAndPatchList(requestedCommandBufferSize, requestedPatchListSize);
}

MOS_STATUS DecodeScalabilitySinglePipe::SendAttrWithFrameTracking(
    MOS_COMMAND_BUFFER &cmdBuffer,
    bool                frameTrackingRequested)
{
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    SCALABILITY_FUNCTION_ENTER;

    bool renderEngineUsed = m_mediaContext->IsRenderEngineUsed();

    // initialize command buffer attributes
    cmdBuffer.Attributes.bTurboMode               = m_hwInterface->m_turboMode;
    cmdBuffer.Attributes.bMediaPreemptionEnabled  = renderEngineUsed ? m_hwInterface->GetRenderInterface()->IsPreemptionEnabled() : 0;

    if (frameTrackingRequested && m_frameTrackingEnabled)
    {
        PMOS_RESOURCE resource = nullptr;
        uint32_t      offset   = 0;
        m_statusReport->GetAddress(decode::statusReportGlobalCount, resource, offset);

        cmdBuffer.Attributes.bEnableMediaFrameTracking    = true;
        cmdBuffer.Attributes.resMediaFrameTrackingSurface = resource;
        cmdBuffer.Attributes.dwMediaFrameTrackingTag      = m_statusReport->GetSubmittedCount() + 1;
        // Set media frame tracking address offset(the offset from the encoder status buffer page)
        cmdBuffer.Attributes.dwMediaFrameTrackingAddrOffset = 0;
    }

    return eStatus;
}

}


