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
//! \file     decode_av1_pipeline_adapter_g12.cpp
//! \brief    Defines the interface to adapt to av1 decode pipeline
//!

#include "decode_av1_pipeline_adapter_g12.h"
#include "decode_utils.h"

DecodeAv1PipelineAdapterG12::DecodeAv1PipelineAdapterG12(
    CodechalHwInterface *   hwInterface,
    CodechalDebugInterface *debugInterface)
    : DecodePipelineAdapter(hwInterface, debugInterface)
{
    DECODE_ASSERT(m_osInterface != nullptr);
    Mos_CheckVirtualEngineSupported(m_osInterface, true, true);
    Mos_SetVirtualEngineSupported(m_osInterface, true);
}

MOS_STATUS DecodeAv1PipelineAdapterG12::BeginFrame()
{
    DECODE_FUNC_CALL();
    decode::DecodePipelineParams decodeParams;
    decodeParams.m_pipeMode = decode::decodePipeModeBegin;
    DECODE_CHK_STATUS(m_decoder->Prepare(&decodeParams));
    return m_decoder->Execute();
}

MOS_STATUS DecodeAv1PipelineAdapterG12::EndFrame()
{
    DECODE_FUNC_CALL();
    decode::DecodePipelineParams decodeParams;
    decodeParams.m_pipeMode = decode::decodePipeModeEnd;
    DECODE_CHK_STATUS(m_decoder->Prepare(&decodeParams));
    return m_decoder->Execute();
}

MOS_STATUS DecodeAv1PipelineAdapterG12::Allocate(CodechalSetting *codecHalSettings)
{
    DECODE_FUNC_CALL();
    m_decoder = std::make_shared<decode::Av1PipelineG12>(m_hwInterface, m_debugInterface);
    DECODE_CHK_NULL(m_decoder);
    return m_decoder->Init(codecHalSettings);
}

MOS_STATUS DecodeAv1PipelineAdapterG12::Execute(void *params)
{
    DECODE_FUNC_CALL();
    decode::DecodePipelineParams decodeParams;
    decodeParams.m_params = (CodechalDecodeParams*)params;
    decodeParams.m_pipeMode = decode::decodePipeModeProcess;
    DECODE_CHK_STATUS(m_decoder->Prepare(&decodeParams));
    return m_decoder->Execute();
}

MOS_STATUS DecodeAv1PipelineAdapterG12::GetStatusReport(
    void                *status,
    uint16_t            numStatus)
{
    DECODE_FUNC_CALL();
    return m_decoder->GetStatusReport(status, numStatus);
}

bool DecodeAv1PipelineAdapterG12::IsIncompletePicture()
{
     return (!m_decoder->IsCompleteBitstream());
}

MOS_SURFACE *DecodeAv1PipelineAdapterG12::GetDummyReference()
{
    DECODE_FUNC_CALL();
    return m_decoder->GetDummyReference();
}

CODECHAL_DUMMY_REFERENCE_STATUS DecodeAv1PipelineAdapterG12::GetDummyReferenceStatus()
{
    DECODE_FUNC_CALL();
    return m_decoder->GetDummyReferenceStatus();
}

void DecodeAv1PipelineAdapterG12::SetDummyReferenceStatus(CODECHAL_DUMMY_REFERENCE_STATUS status)
{
    DECODE_FUNC_CALL();
    m_decoder->SetDummyReferenceStatus(status);
}

uint32_t DecodeAv1PipelineAdapterG12::GetCompletedReport()
{
    return m_decoder->GetCompletedReport();
}

void DecodeAv1PipelineAdapterG12::Destroy()
{
    DECODE_FUNC_CALL();

    m_decoder->Destroy();
}


MOS_GPU_CONTEXT DecodeAv1PipelineAdapterG12::GetDecodeContext()
{
    DECODE_FUNC_CALL();

    return m_decoder->GetDecodeContext();
}

