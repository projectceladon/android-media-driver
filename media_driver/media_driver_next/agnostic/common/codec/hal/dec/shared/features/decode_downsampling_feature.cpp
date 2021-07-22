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
//! \file     decode_downsampling_feature.cpp
//! \brief    Defines the common interface for decode downsampling features
//! \details  The decode downsampling feature interface is further sub-divided by codec standard,
//!           this file is for the base interface which is shared by all codecs.
//!
#include "decode_downsampling_feature.h"
#include "decode_utils.h"
#include "codechal_debug.h"

#ifdef _DECODE_PROCESSING_SUPPORTED

namespace decode
{
DecodeDownSamplingFeature::DecodeDownSamplingFeature(
    MediaFeatureManager *featureManager, DecodeAllocator *allocator, CodechalHwInterface *hwInterface):
    m_hwInterface(hwInterface), m_allocator(allocator)
{
    m_featureManager = featureManager;
}

DecodeDownSamplingFeature::~DecodeDownSamplingFeature()
{
    if (m_histogramBuffer != nullptr &&
        !m_allocator->ResourceIsNull(&m_histogramBuffer->OsResource))
    {
        MOS_STATUS eStatus = m_allocator->Destroy(m_histogramBuffer);
        if (eStatus != MOS_STATUS_SUCCESS)
        {
            DECODE_ASSERTMESSAGE("Failed to free histogram internal buffer!");
        }
    }
}

MOS_STATUS DecodeDownSamplingFeature::Init(void *setting)
{
    DECODE_FUNC_CALL();
    DECODE_CHK_NULL(m_featureManager);
    DECODE_CHK_NULL(m_allocator);

    DECODE_CHK_STATUS(m_internalTargets.Init(*m_allocator));

    m_basicFeature = dynamic_cast<DecodeBasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
    DECODE_CHK_NULL(m_basicFeature);

    MOS_ZeroMemory(&m_outputSurface, sizeof(m_outputSurface));

    m_histogramBuffer = m_allocator->AllocateBuffer(HISTOGRAM_BINCOUNT * m_histogramBinWidth,
        "Histogram internal buffer",
        resourceInternalReadWriteCache,
        true,
        0,
        false);
    DECODE_CHK_NULL(m_histogramBuffer);
    if (m_allocator->ResourceIsNull(&m_histogramBuffer->OsResource))
    {
        DECODE_ASSERTMESSAGE("Failed to allocate hsitogram internal buffer!");
    }

#if (_DEBUG || _RELEASE_INTERNAL)
    MOS_USER_FEATURE_VALUE_DATA userFeatureData;
    PMOS_INTERFACE pOsInterface = m_hwInterface->GetOsInterface();
    MOS_ZeroMemory(&userFeatureData, sizeof(userFeatureData));
    MOS_UserFeature_ReadValue_ID(
        nullptr,
        __MEDIA_USER_FEATURE_VALUE_DECODE_HISTOGRAM_DEBUG_ID,
        &userFeatureData,
        pOsInterface ? pOsInterface->pOsContext : nullptr);
    m_histogramDebug = userFeatureData.u32Data ? true : false;
#endif

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodeDownSamplingFeature::Update(void *params)
{
    DECODE_FUNC_CALL();
    DECODE_CHK_NULL(params);

    CodechalDecodeParams *decodeParams = (CodechalDecodeParams *)params;

    if (decodeParams->m_procParams == nullptr)
    {
        m_inputSurface  = nullptr;
        m_enabled       = false;
        return MOS_STATUS_SUCCESS;
    }
    else
    {
        m_enabled = true;
    }

    DecodeProcessingParams *procParams = (DecodeProcessingParams *)decodeParams->m_procParams;

    m_chromaSitingType             = procParams->m_chromaSitingType;
    m_rotationState                = procParams->m_rotationState;
    m_blendState                   = procParams->m_blendState;
    m_mirrorState                  = procParams->m_mirrorState;
    m_scalingMode                  = procParams->m_scalingMode;
    m_isReferenceOnlyPattern       = procParams->m_isReferenceOnlyPattern;

    DECODE_CHK_NULL(procParams->m_outputSurface);
    m_outputSurface = *(procParams->m_outputSurface);
    DECODE_CHK_STATUS(m_allocator->GetSurfaceInfo(&m_outputSurface));

    m_outputSurfaceRegion.m_x      = procParams->m_outputSurfaceRegion.m_x;
    m_outputSurfaceRegion.m_y      = procParams->m_outputSurfaceRegion.m_y;
    m_outputSurfaceRegion.m_width  = (procParams->m_outputSurfaceRegion.m_width == 0) ?
        m_outputSurface.dwWidth : procParams->m_outputSurfaceRegion.m_width;
    m_outputSurfaceRegion.m_height = (procParams->m_outputSurfaceRegion.m_height == 0) ?
        m_outputSurface.dwHeight : procParams->m_outputSurfaceRegion.m_height;

    if (procParams->m_inputSurface != nullptr)
    {
        m_inputSurface = procParams->m_inputSurface;
        DECODE_CHK_STATUS(m_allocator->GetSurfaceInfo(m_inputSurface));

        m_inputSurfaceRegion.m_x      = procParams->m_inputSurfaceRegion.m_x;
        m_inputSurfaceRegion.m_y      = procParams->m_inputSurfaceRegion.m_y;
        m_inputSurfaceRegion.m_width  = (procParams->m_inputSurfaceRegion.m_width == 0) ?
            m_inputSurface->dwWidth : procParams->m_inputSurfaceRegion.m_width;
        m_inputSurfaceRegion.m_height = (procParams->m_inputSurfaceRegion.m_height == 0) ?
            m_inputSurface->dwHeight : procParams->m_inputSurfaceRegion.m_height;
    }
    else
    {
        if (m_basicFeature->m_curRenderPic.FrameIdx >= decodeParams->m_refFrameCnt)
        {
            DECODE_ASSERTMESSAGE("Invalid Downsampling Reference Frame Index !");
            return MOS_STATUS_INVALID_PARAMETER;
        }

        DECODE_CHK_STATUS(UpdateInternalTargets(*m_basicFeature));

        m_inputSurface = m_internalTargets.GetCurSurf();
        DECODE_CHK_NULL(m_inputSurface);

        m_inputSurfaceRegion.m_x      = 0;
        m_inputSurfaceRegion.m_y      = 0;
        m_inputSurfaceRegion.m_width  = m_basicFeature->m_width;
        m_inputSurfaceRegion.m_height = m_basicFeature->m_height;
    }

    // Histogram
    if (m_allocator->ResourceIsNull(&decodeParams->m_histogramSurface.OsResource) && !m_histogramDebug)
    {
        m_histogramDestSurf = nullptr;
    }
    else
    {
        m_histogramDestSurf = &decodeParams->m_histogramSurface;
    }

    // Update decode output in basic feature
    DECODE_CHK_STATUS(UpdateDecodeTarget(*m_inputSurface));

#if (_DEBUG || _RELEASE_INTERNAL)
    m_outputSurfaceList[m_basicFeature->m_curRenderPic.FrameIdx] = m_outputSurface;
#endif

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodeDownSamplingFeature::UpdateInternalTargets(DecodeBasicFeature &basicFeature)
{
    DECODE_FUNC_CALL();

    uint32_t curFrameIdx = basicFeature.m_curRenderPic.FrameIdx;

    std::vector<uint32_t> refFrameList;
    DECODE_CHK_STATUS(GetRefFrameList(refFrameList));
    DECODE_CHK_STATUS(m_internalTargets.UpdateRefList(curFrameIdx, refFrameList));

    MOS_SURFACE surface;
    MOS_ZeroMemory(&surface, sizeof(surface));
    DECODE_CHK_STATUS(GetDecodeTargetSize(surface.dwWidth, surface.dwHeight));
    DECODE_CHK_STATUS(GetDecodeTargetFormat(surface.Format));
    DECODE_CHK_STATUS(m_internalTargets.ActiveCurSurf(
        curFrameIdx, &surface, basicFeature.IsMmcEnabled(), resourceOutputPicture));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodeDownSamplingFeature::DumpSfcOutputs(CodechalDebugInterface* debugInterface)
{
    DECODE_FUNC_CALL();
    DECODE_CHK_NULL(debugInterface);
    DECODE_CHK_NULL(m_basicFeature);

    // Dump histogram
    if ((m_histogramDestSurf != nullptr || m_histogramDebug) &&
        m_histogramBuffer != nullptr &&
        !m_allocator->ResourceIsNull(&m_histogramBuffer->OsResource))
    {
        CODECHAL_DEBUG_TOOL(
            debugInterface->m_bufferDumpFrameNum = m_basicFeature->m_frameNum;
            DECODE_CHK_STATUS(debugInterface->DumpBuffer(
                &m_histogramBuffer->OsResource,
                CodechalDbgAttr::attrSfcHistogram,
                "_DEC",
                HISTOGRAM_BINCOUNT * m_histogramBinWidth));)
    }

    // Dump SFC
    if (!m_allocator->ResourceIsNull(&m_outputSurface.OsResource) &&
        m_inputSurface != nullptr)
    {
        CODECHAL_DEBUG_TOOL(
            debugInterface->m_bufferDumpFrameNum = m_basicFeature->m_frameNum;
            DECODE_CHK_STATUS(debugInterface->DumpYUVSurface(
                &m_outputSurface,
                CodechalDbgAttr::attrSfcOutputSurface,
                "_SFCSurf"));)
    }

    return MOS_STATUS_SUCCESS;
}
}

#endif
