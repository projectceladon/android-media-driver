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
//! \file     decode_pipeline.cpp
//! \brief    Defines the common interface for decode pipeline
//! \details  The decode pipeline interface is further sub-divided by codec standard,
//!           this file is for the base interface which is shared by all codecs.
//!
#include "decode_pipeline.h"
#include "decode_utils.h"
#include "decode_status_report.h"
#include "media_packet.h"
#include "decode_predication_packet.h"
#include "decode_marker_packet.h"
#include "decode_downsampling_packet.h"
#include "codechal_setting.h"
#include "decode_basic_feature.h"
#include "mos_solo_generic.h"
#include "decode_sfc_histogram_postsubpipeline.h"
#include "decode_common_feature_defs.h"

namespace decode {

DecodePipeline::DecodePipeline(
    CodechalHwInterface *hwInterface,
    CodechalDebugInterface *debugInterface):
    MediaPipeline(hwInterface ? hwInterface->GetOsInterface() : nullptr)
{
    DECODE_FUNC_CALL();

    DECODE_ASSERT(hwInterface != nullptr);
    m_hwInterface = hwInterface;

    m_singleTaskPhaseSupported =
        ReadUserFeature(__MEDIA_USER_FEATURE_VALUE_SINGLE_TASK_PHASE_ENABLE_ID, m_osInterface ? m_osInterface->pOsContext : nullptr).i32Data ? true : false;

    CODECHAL_DEBUG_TOOL(
        DECODE_ASSERT(debugInterface != nullptr);
        m_debugInterface = debugInterface;
    );
}

MOS_STATUS DecodePipeline::CreateStatusReport()
{
    m_statusReport = MOS_New(DecodeStatusReport, m_allocator, true);
    DECODE_CHK_NULL(m_statusReport);
    DECODE_CHK_STATUS(m_statusReport->Create());

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::CreatePreSubPipeLines(DecodeSubPipelineManager &subPipelineManager)
{
    m_bitstream = MOS_New(DecodeInputBitstream, this, m_task, m_numVdbox);
    DECODE_CHK_NULL(m_bitstream);
    DECODE_CHK_STATUS(subPipelineManager.Register(*m_bitstream));

    m_streamout = MOS_New(DecodeStreamOut, this, m_task, m_numVdbox);
    DECODE_CHK_NULL(m_streamout);
    DECODE_CHK_STATUS(subPipelineManager.Register(*m_streamout));
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::CreatePostSubPipeLines(DecodeSubPipelineManager &subPipelineManager)
{
    DECODE_FUNC_CALL();

#ifdef _DECODE_PROCESSING_SUPPORTED
    auto sfcHistogramPostSubPipeline = MOS_New(DecodeSfcHistogramSubPipeline, this, m_task, m_numVdbox);
    DECODE_CHK_NULL(sfcHistogramPostSubPipeline);
    DECODE_CHK_STATUS(m_postSubPipeline->Register(*sfcHistogramPostSubPipeline));
#endif

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::CreateSubPipeLineManager(CodechalSetting* codecSettings)
{
    m_preSubPipeline = MOS_New(DecodeSubPipelineManager, *this);
    DECODE_CHK_NULL(m_preSubPipeline);
    DECODE_CHK_STATUS(CreatePreSubPipeLines(*m_preSubPipeline));
    DECODE_CHK_STATUS(m_preSubPipeline->Init(*codecSettings));

    m_postSubPipeline = MOS_New(DecodeSubPipelineManager, *this);
    DECODE_CHK_NULL(m_postSubPipeline);
    DECODE_CHK_STATUS(CreatePostSubPipeLines(*m_postSubPipeline));
    DECODE_CHK_STATUS(m_postSubPipeline->Init(*codecSettings));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::CreateSubPackets(DecodeSubPacketManager& subPacketManager, CodechalSetting &codecSettings)
{
    DecodePredicationPkt *predicationPkt = MOS_New(DecodePredicationPkt, this, m_hwInterface);
    DECODE_CHK_NULL(predicationPkt);
    DECODE_CHK_STATUS(subPacketManager.Register(
                        DecodePacketId(this, predicationSubPacketId), *predicationPkt));

    DecodeMarkerPkt *markerPkt = MOS_New(DecodeMarkerPkt, this, m_hwInterface);
    DECODE_CHK_NULL(markerPkt);
    DECODE_CHK_STATUS(subPacketManager.Register(
                        DecodePacketId(this, markerSubPacketId), *markerPkt));
    return MOS_STATUS_SUCCESS;
}

DecodeSubPacket* DecodePipeline::GetSubPacket(uint32_t subPacketId)
{
    return m_subPacketManager->GetSubPacket(subPacketId);
}

MOS_STATUS DecodePipeline::CreateSubPacketManager(CodechalSetting* codecSettings)
{
    DECODE_CHK_NULL(codecSettings);
    m_subPacketManager = MOS_New(DecodeSubPacketManager);
    DECODE_CHK_NULL(m_subPacketManager);
    DECODE_CHK_STATUS(CreateSubPackets(*m_subPacketManager, *codecSettings));
    DECODE_CHK_STATUS(m_subPacketManager->Init());
    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::Initialize(void *settings)
{
    DECODE_FUNC_CALL();

    DECODE_CHK_NULL(settings);

    DECODE_CHK_STATUS(MediaPipeline::InitPlatform());
    DECODE_CHK_STATUS(MediaPipeline::CreateMediaCopy());

    DECODE_CHK_NULL(m_waTable);

    auto *codecSettings = (CodechalSetting*)settings;
    DECODE_CHK_NULL(m_hwInterface);
    DECODE_CHK_STATUS(m_hwInterface->Initialize(codecSettings));

    m_mediaContext = MOS_New(MediaContext, scalabilityDecoder, m_hwInterface, m_osInterface);
    DECODE_CHK_NULL(m_mediaContext);

    m_task = CreateTask(MediaTask::TaskType::cmdTask);
    DECODE_CHK_NULL(m_task);

    m_numVdbox = GetSystemVdboxNumber();

    m_allocator = MOS_New(DecodeAllocator, m_osInterface);
    DECODE_CHK_NULL(m_allocator);

    DECODE_CHK_STATUS(CreateStatusReport());

    m_decodecp = Create_DecodeCpInterface(codecSettings, m_hwInterface);
    if(m_decodecp)
    {
        m_decodecp->RegisterParams(codecSettings);
    }
    DECODE_CHK_STATUS(CreateFeatureManager());
    DECODE_CHK_STATUS(m_featureManager->Init(codecSettings));

    DECODE_CHK_STATUS(CreateSubPipeLineManager(codecSettings));
    DECODE_CHK_STATUS(CreateSubPacketManager(codecSettings));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::Uninitialize()
{
    DECODE_FUNC_CALL();

    Delete_DecodeCpInterface(m_decodecp);
    m_decodecp = nullptr;

    MOS_Delete(m_mediaContext);

    MOS_Delete(m_statusReport);

    MOS_Delete(m_featureManager);

    MOS_Delete(m_preSubPipeline);
    MOS_Delete(m_postSubPipeline);
    MOS_Delete(m_subPacketManager);

    MOS_Delete(m_allocator);

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::UserFeatureReport()
{
    DECODE_FUNC_CALL();
    return MediaPipeline::UserFeatureReport();
}

bool DecodePipeline::IsFirstProcessPipe(const DecodePipelineParams& pipelineParams)
{
    if (pipelineParams.m_pipeMode != decodePipeModeProcess)
    {
        return false;
    }

    CodechalDecodeParams *decodeParams = pipelineParams.m_params;
    if (decodeParams == nullptr)
    {
        return false;
    }

    return (decodeParams->m_executeCallIndex == 0);
}

uint8_t DecodePipeline::GetSystemVdboxNumber()
{
    uint8_t numVdbox = 1;

    MEDIA_SYSTEM_INFO *gtSystemInfo = m_osInterface->pfnGetGtSystemInfo(m_osInterface);
    if (gtSystemInfo != nullptr)
    {
        // Both VE mode and media solo mode should be able to get the VDBOX number via the same interface
        numVdbox = (uint8_t)(gtSystemInfo->VDBoxInfo.NumberOfVDBoxEnabled);
    }

    return numVdbox;
}

MOS_STATUS DecodePipeline::Prepare(void *params)
{
    DECODE_FUNC_CALL();

    DECODE_CHK_NULL(params);
    DecodePipelineParams *pipelineParams = (DecodePipelineParams *)params;
    CodechalDecodeParams *decodeParams = pipelineParams->m_params;
    DECODE_CHK_NULL(decodeParams);

    DECODE_CHK_STATUS(m_task->Clear());
    m_activePacketList.clear();

    DECODE_CHK_NULL(m_featureManager);
    DECODE_CHK_STATUS(m_featureManager->CheckFeatures(decodeParams));
    DECODE_CHK_STATUS(m_featureManager->Update(decodeParams));
    if(m_decodecp)
    {
        m_decodecp->UpdateParams(true);
    }
    DECODE_CHK_STATUS(m_subPacketManager->Prepare());

    DECODE_CHK_STATUS(Mos_Solo_SetGpuAppTaskEvent(m_osInterface, decodeParams->m_gpuAppTaskEvent));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::ExecuteActivePackets()
{
    DECODE_FUNC_CALL();
    MOS_TraceEventExt(EVENT_PIPE_EXE, EVENT_TYPE_START, nullptr, 0, nullptr, 0);

    // Last element in m_activePacketList must be immediately submitted
    m_activePacketList.back().immediateSubmit = true;

    for (PacketProperty prop : m_activePacketList)
    {
        prop.stateProperty.singleTaskPhaseSupported = m_singleTaskPhaseSupported;
        prop.stateProperty.statusReport = m_statusReport;
        MOS_TraceEventExt(EVENT_PIPE_EXE, EVENT_TYPE_INFO, &prop.packetId, sizeof(uint32_t), nullptr, 0);

        MediaTask *task = prop.packet->GetActiveTask();
        DECODE_CHK_STATUS(task->AddPacket(&prop));
        if (prop.immediateSubmit)
        {
            DECODE_CHK_STATUS(task->Submit(true, m_scalability, m_debugInterface));
        }
    }

    m_activePacketList.clear();
    MOS_TraceEventExt(EVENT_PIPE_EXE, EVENT_TYPE_END, nullptr, 0, nullptr, 0);
    return MOS_STATUS_SUCCESS;
}

bool DecodePipeline::IsCompleteBitstream()
{
    return (m_bitstream == nullptr) ? false : m_bitstream->IsComplete();
}

#ifdef _DECODE_PROCESSING_SUPPORTED
bool DecodePipeline::IsDownSamplingSupported()
{
    DECODE_ASSERT(m_subPacketManager != nullptr);

    DecodeDownSamplingPkt *downSamplingPkt = dynamic_cast<DecodeDownSamplingPkt *>(
        GetSubPacket(DecodePacketId(this, downSamplingSubPacketId)));
    if (downSamplingPkt == nullptr)
    {
        return false;
    }

    return downSamplingPkt->IsSupported();
}
#endif

MOS_SURFACE* DecodePipeline::GetDummyReference()
{
    auto* feature = dynamic_cast<DecodeBasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
    return (feature != nullptr) ? &(feature->m_dummyReference) : nullptr;
}

CODECHAL_DUMMY_REFERENCE_STATUS DecodePipeline::GetDummyReferenceStatus()
{
    auto* feature = dynamic_cast<DecodeBasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
    return (feature != nullptr) ? feature->m_dummyReferenceStatus : CODECHAL_DUMMY_REFERENCE_INVALID;
}

void DecodePipeline::SetDummyReferenceStatus(CODECHAL_DUMMY_REFERENCE_STATUS status)
{
    auto* feature = dynamic_cast<DecodeBasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
    if (feature != nullptr)
    {
        feature->m_dummyReferenceStatus = status;
    }
}

#if USE_CODECHAL_DEBUG_TOOL
#ifdef _DECODE_PROCESSING_SUPPORTED
MOS_STATUS DecodePipeline::DumpDownSamplingParams(DecodeDownSamplingFeature &downSamplingParams)
{
    CODECHAL_DEBUG_FUNCTION_ENTER;
    if (!m_debugInterface->DumpIsEnabled(CodechalDbgAttr::attrDecodeProcParams))
    {
        return MOS_STATUS_SUCCESS;
    }

    if(!downSamplingParams.IsEnabled())
    {
        return MOS_STATUS_SUCCESS;
    }

    DECODE_CHK_NULL(downSamplingParams.m_inputSurface);

    std::ostringstream oss;
    oss.setf(std::ios::showbase | std::ios::uppercase);

    oss << "Input Surface Resolution: "
        << +downSamplingParams.m_inputSurface->dwWidth << " x " << +downSamplingParams.m_inputSurface->dwHeight << std::endl;
    oss << "Input Region Resolution: "
        << +downSamplingParams.m_inputSurfaceRegion.m_width << " x " << +downSamplingParams.m_inputSurfaceRegion.m_height << std::endl;
    oss << "Input Region Offset: ("
        << +downSamplingParams.m_inputSurfaceRegion.m_x << "," << +downSamplingParams.m_inputSurfaceRegion.m_y << ")" << std::endl;
    oss << "Input Surface Format: "
        << (downSamplingParams.m_inputSurface->Format == Format_NV12 ? "NV12" : "P010" )<< std::endl;
    oss << "Output Surface Resolution: "
        << +downSamplingParams.m_outputSurface.dwWidth << " x " << +downSamplingParams.m_outputSurface.dwHeight << std::endl;
    oss << "Output Region Resolution: "
        << +downSamplingParams.m_outputSurfaceRegion.m_width << " x " << +downSamplingParams.m_outputSurfaceRegion.m_height << std::endl;
    oss << "Output Region Offset: ("
        << +downSamplingParams.m_outputSurfaceRegion.m_x << ", " << +downSamplingParams.m_outputSurfaceRegion.m_y << ")" << std::endl;
    oss << "Output Surface Format: "
        << (downSamplingParams.m_outputSurface.Format == Format_NV12 ? "NV12" : "YUY2" )<< std::endl;

    const char* filePath = m_debugInterface->CreateFileName(
        "_DEC",
        CodechalDbgBufferType::bufDecProcParams,
        CodechalDbgExtType::txt);

    std::ofstream ofs(filePath, std::ios::out);
    ofs << oss.str();
    ofs.close();

    return MOS_STATUS_SUCCESS;
}
#endif

MOS_STATUS DecodePipeline::DumpOutput(const DecodeStatusReportData& reportData)
{
    DECODE_FUNC_CALL();

    if (m_debugInterface->DumpIsEnabled(CodechalDbgAttr::attrDecodeOutputSurface))
    {
        MOS_SURFACE dstSurface;
        MOS_ZeroMemory(&dstSurface, sizeof(dstSurface));
        dstSurface.Format     = Format_NV12;
        dstSurface.OsResource = reportData.currDecodedPicRes;
        DECODE_CHK_STATUS(m_allocator->GetSurfaceInfo(&dstSurface));

        DECODE_CHK_STATUS(m_debugInterface->DumpYUVSurface(
            &dstSurface, CodechalDbgAttr::attrDecodeOutputSurface, "DstSurf"));
    }

#ifdef _DECODE_PROCESSING_SUPPORTED
    DecodeDownSamplingFeature* downSamplingFeature = dynamic_cast<DecodeDownSamplingFeature*>(
        m_featureManager->GetFeature(DecodeFeatureIDs::decodeDownSampling));
    if (downSamplingFeature != nullptr && downSamplingFeature->IsEnabled())
    {
        if (reportData.currSfcOutputPicRes != nullptr &&
            m_debugInterface->DumpIsEnabled(CodechalDbgAttr::attrSfcOutputSurface))
        {
            MOS_SURFACE sfcDstSurface;
            MOS_ZeroMemory(&sfcDstSurface, sizeof(sfcDstSurface));
            sfcDstSurface.Format     = Format_NV12;
            sfcDstSurface.OsResource = *reportData.currSfcOutputPicRes;

            if (!Mos_ResourceIsNull(&sfcDstSurface.OsResource))
            {
                DECODE_CHK_STATUS(m_allocator->GetSurfaceInfo(&sfcDstSurface));
                DECODE_CHK_STATUS(m_debugInterface->DumpYUVSurface(
                    &sfcDstSurface, CodechalDbgAttr::attrSfcOutputSurface, "SfcDstSurf"));
            }
        }
    }
#endif

    return MOS_STATUS_SUCCESS;
}
#endif

#if (_DEBUG || _RELEASE_INTERNAL)
MOS_STATUS DecodePipeline::ReportVdboxIds(const DecodeStatusMfx& status)
{
    DECODE_FUNC_CALL();

    // report the VDBOX IDs to user feature
    uint32_t vdboxIds = ReadUserFeature(__MEDIA_USER_FEATURE_VALUE_VDBOX_ID_USED, m_osInterface->pOsContext).u32Data;

    for (auto i = 0; i < csInstanceIdMax; i++)
    {
        CsEngineId csEngineId;
        csEngineId.value = status.m_mmioCsEngineIdReg[i];
        if (csEngineId.value != 0)
        {
            DECODE_ASSERT(csEngineId.fields.classId == classIdVideoEngine);
            DECODE_ASSERT(csEngineId.fields.instanceId < csInstanceIdMax);
            vdboxIds |= 1 << ((csEngineId.fields.instanceId) << 2);
        }
    }

    if (vdboxIds != 0)
    {
        WriteUserFeature(__MEDIA_USER_FEATURE_VALUE_VDBOX_ID_USED, vdboxIds, m_osInterface->pOsContext);
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS DecodePipeline::StatusCheck()
{
    DECODE_FUNC_CALL();

    uint32_t completedCount = m_statusReport->GetCompletedCount();
    if (completedCount <= m_statusCheckCount)
    {
        DECODE_CHK_COND(completedCount < m_statusCheckCount, "Invalid statuc check count");
        return MOS_STATUS_SUCCESS;
    }

    DecodeStatusReport* statusReport = dynamic_cast<DecodeStatusReport*>(m_statusReport);
    DECODE_CHK_NULL(statusReport);

    while (m_statusCheckCount < completedCount)
    {
        const DecodeStatusMfx& status = statusReport->GetMfxStatus(m_statusCheckCount);
        if (status.status != DecodeStatusReport::queryEnd)
        {
            DECODE_NORMALMESSAGE("Media reset may have occured at frame %d, status is %d, completedCount is %d.",
                m_statusCheckCount, status.status, completedCount);
        }
        DECODE_NORMALMESSAGE("hucStatus2 is 0x%x at frame %d.", status.m_hucErrorStatus2, m_statusCheckCount);
        DECODE_NORMALMESSAGE("hucStatus is 0x%x at frame %d.", status.m_hucErrorStatus, m_statusCheckCount);

        DECODE_CHK_STATUS(ReportVdboxIds(status));

#if USE_CODECHAL_DEBUG_TOOL
        const DecodeStatusReportData& reportData = statusReport->GetReportData(m_statusCheckCount);

        auto bufferDumpNumTemp = m_debugInterface->m_bufferDumpFrameNum;
        auto currPicTemp       = m_debugInterface->m_currPic;
        auto frameTypeTemp     = m_debugInterface->m_frameType;

        m_debugInterface->m_bufferDumpFrameNum = m_statusCheckCount;
        m_debugInterface->m_currPic            = reportData.currDecodedPic;
        m_debugInterface->m_frameType          = reportData.frameType;

        DECODE_CHK_STATUS(DumpOutput(reportData));

        m_debugInterface->m_bufferDumpFrameNum = bufferDumpNumTemp;
        m_debugInterface->m_currPic            = currPicTemp;
        m_debugInterface->m_frameType          = frameTypeTemp;
#endif

        m_statusCheckCount++;
    }

    return MOS_STATUS_SUCCESS;
}
#endif

}
