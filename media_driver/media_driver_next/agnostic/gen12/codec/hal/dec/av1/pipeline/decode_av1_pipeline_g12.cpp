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
//! \file     decode_av1_pipeline_g12.cpp
//! \brief    Defines the interface for av1 decode pipeline
//!
#include "decode_av1_pipeline_g12.h"
#include "decode_av1_packet_g12.h"
#include "decode_av1_picture_packet_g12.h"
#include "decode_av1_tile_packet_g12.h"
#include "decode_utils.h"
#include "codechal_debug.h"
#include "decode_av1_tile_coding.h"
#include "decode_av1_feature_manager_g12.h"
#include "decode_mem_compression_g12.h"

namespace decode
{
    Av1PipelineG12::Av1PipelineG12(
        CodechalHwInterface *   hwInterface,
        CodechalDebugInterface *debugInterface)
        : Av1Pipeline(hwInterface, debugInterface)
    {

    }

    MOS_STATUS Av1PipelineG12::Init(void *settings)
    {
        DECODE_FUNC_CALL();

        DECODE_CHK_NULL(settings);
        DECODE_CHK_STATUS(Initialize(settings));

        if (MEDIA_IS_SKU(m_skuTable, FtrWithSlimVdbox))
        {
            m_numVdbox = 1;
        }

        m_av1DecodePkt = MOS_New(Av1DecodePktG12, this, m_task, m_hwInterface);
        DECODE_CHK_STATUS(RegisterPacket(DecodePacketId(this, av1DecodePacketId), m_av1DecodePkt));
        DECODE_CHK_STATUS(m_av1DecodePkt->Init());

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::InitContext()
    {
        DECODE_FUNC_CALL();

        auto basicFeature = dynamic_cast<Av1BasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
        DECODE_CHK_NULL(basicFeature);

        DecodeScalabilityPars scalPars;
        MOS_ZeroMemory(&scalPars, sizeof(ScalabilityPars));
        scalPars.disableScalability = true;
        scalPars.enableVE = MOS_VE_SUPPORTED(m_osInterface);
        if (MEDIA_IS_SKU(m_skuTable, FtrWithSlimVdbox))
        {
            scalPars.usingSlimVdbox = true;
        }
        else
        {
            scalPars.usingSlimVdbox = false;
        }
        scalPars.numVdbox = m_numVdbox;

        m_mediaContext->SwitchContext(VdboxDecodeFunc, &scalPars, &m_scalability);
        DECODE_CHK_NULL(m_scalability);

        m_decodeContext = m_osInterface->pfnGetGpuContext(m_osInterface);

        m_passNum = basicFeature->m_tileCoding.CalcNumPass(*basicFeature->m_av1PicParams, basicFeature->m_av1TileParams);
        m_scalability->SetPassNumber(m_passNum);

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::Prepare(void *params)
    {
        DECODE_FUNC_CALL();

        DECODE_CHK_NULL(params);
        DecodePipelineParams *pipelineParams = (DecodePipelineParams *)params;
        m_pipeMode = pipelineParams->m_pipeMode;

        auto basicFeature = dynamic_cast<Av1BasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
        DECODE_CHK_NULL(basicFeature);

        if (IsFirstProcessPipe(*pipelineParams))
        {
            DECODE_CHK_STATUS(Av1Pipeline::Prepare(params));
        }

        DECODE_CHK_STATUS(m_preSubPipeline->Prepare(*pipelineParams));
        DECODE_CHK_STATUS(m_postSubPipeline->Prepare(*pipelineParams));

        if (m_pipeMode == decodePipeModeProcess)
        {
            if (IsCompleteBitstream())
            {
                CODECHAL_DEBUG_TOOL(DECODE_CHK_STATUS(DumpParams(*basicFeature)));

                DecodeStatusParameters inputParameters = {};
                MOS_ZeroMemory(&inputParameters, sizeof(DecodeStatusParameters));
                inputParameters.statusReportFeedbackNumber = basicFeature->m_av1PicParams->m_statusReportFeedbackNumber;
                inputParameters.codecFunction              = basicFeature->m_codecFunction;
                inputParameters.picWidthInMb               = basicFeature->m_picWidthInMb;
                inputParameters.pictureCodingType          = basicFeature->m_pictureCodingType;
                inputParameters.currOriginalPic            = basicFeature->m_curRenderPic;
                inputParameters.currDecodedPicRes          = basicFeature->m_destSurface.OsResource;
                inputParameters.numUsedVdbox               = m_numVdbox;
                m_allocator->UpdateResoreceUsageType(&inputParameters.currDecodedPicRes, resourceOutputPicture);
                m_statusReport->Init(&inputParameters);
            }
        }
     
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::Execute()
    {
        DECODE_FUNC_CALL();

        if (m_pipeMode == decodePipeModeProcess)
        {
            DECODE_CHK_STATUS(m_preSubPipeline->Execute());

            if (IsCompleteBitstream())
            {
                DECODE_CHK_STATUS(InitContext());
                DECODE_CHK_STATUS(ActivateDecodePackets());
                DECODE_CHK_STATUS(ExecuteActivePackets());
            }

            DECODE_CHK_STATUS(m_postSubPipeline->Execute());
        }
        else if (m_pipeMode == decodePipeModeEnd)
        {
            auto feature = dynamic_cast<DecodeBasicFeature*>(m_featureManager->GetFeature(FeatureIDs::basicFeature));
            DECODE_CHK_NULL(feature);
            CODECHAL_DEBUG_TOOL(
                m_debugInterface->m_bufferDumpFrameNum = feature->m_frameNum;
                DECODE_CHK_STATUS(m_debugInterface->DumpYUVSurface(
                    &feature->m_destSurface,
                    CodechalDbgAttr::attrDecodeOutputSurface,
                    "DstSurf"));)

            // Only update user features for the first frame.
            if (feature->m_frameNum == 0)
            {
                DECODE_CHK_STATUS(UserFeatureReport());
            }
            feature->m_frameNum++;

            DECODE_CHK_STATUS(m_statusReport->Reset());
        }

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::GetStatusReport(void *status, uint16_t numStatus)
    {
        DECODE_FUNC_CALL();

        m_statusReport->GetReport(numStatus, status);

        return MOS_STATUS_SUCCESS;
    }

    uint32_t Av1PipelineG12::GetCompletedReport()
    {
        DECODE_FUNC_CALL();

        uint32_t completedCount = m_statusReport->GetCompletedCount();
        uint32_t reportedCount = m_statusReport->GetReportedCount();

        if (reportedCount > completedCount)
        {
            DECODE_ASSERTMESSAGE("No report available at all");
            return 0;
        }
        else
        {
            uint32_t availableCount = completedCount - reportedCount;
            return availableCount;
        }
    }

    MOS_STATUS Av1PipelineG12::Destroy()
    {
        DECODE_FUNC_CALL();

        Uninitialize();

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::Initialize(void *settings)
    {
        DECODE_FUNC_CALL();

        DECODE_CHK_STATUS(Av1Pipeline::Initialize(settings));
        DECODE_CHK_STATUS(InitMmcState());

        //pre subpipeline for generate noise
        auto *codecSettings     = (CodechalSetting *)settings;
        m_fgGenNoiseSubPipeline = MOS_New(FilmGrainPreSubPipeline, this, m_task, m_numVdbox);
        DECODE_CHK_NULL(m_fgGenNoiseSubPipeline);
        DECODE_CHK_STATUS(m_preSubPipeline->Register(*m_fgGenNoiseSubPipeline));
        DECODE_CHK_STATUS(m_fgGenNoiseSubPipeline->Init(*codecSettings));  
        
        //post subpipeline for apply noise
        m_fgAppNoiseSubPipeline = MOS_New(FilmGrainPostSubPipeline, this, m_task, m_numVdbox);
        DECODE_CHK_NULL(m_fgAppNoiseSubPipeline);
        DECODE_CHK_STATUS(m_postSubPipeline->Register(*m_fgAppNoiseSubPipeline));
        DECODE_CHK_STATUS(m_fgAppNoiseSubPipeline->Init(*codecSettings));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::Uninitialize()
    {
        DECODE_FUNC_CALL();

        for (auto pair : m_packetList)
        {
            pair.second->Destroy();
        }

        if (m_mmcState != nullptr)
        {
            MOS_Delete(m_mmcState);
        }

        return Av1Pipeline::Uninitialize();
    }

    MOS_STATUS Av1PipelineG12::UserFeatureReport()
    {
        DECODE_FUNC_CALL();

        return Av1Pipeline::UserFeatureReport();
    }

    MOS_STATUS Av1PipelineG12::CreateSubPackets(DecodeSubPacketManager &subPacketManager)
    {
        DECODE_CHK_STATUS(DecodePipeline::CreateSubPackets(subPacketManager));

        Av1DecodePicPktG12 *pictureDecodePkt = MOS_New(Av1DecodePicPktG12, this, m_hwInterface);
        DECODE_CHK_NULL(pictureDecodePkt);
        DECODE_CHK_STATUS(subPacketManager.Register(
                            DecodePacketId(this, av1PictureSubPacketId), *pictureDecodePkt));

        Av1DecodeTilePktG12 *tileDecodePkt = MOS_New(Av1DecodeTilePktG12, this, m_hwInterface);
        DECODE_CHK_NULL(tileDecodePkt);
        DECODE_CHK_STATUS(subPacketManager.Register(
                            DecodePacketId(this, av1TileSubPacketId), *tileDecodePkt));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::InitMmcState()
    {
    #ifdef _MMC_SUPPORTED
        DECODE_CHK_NULL(m_hwInterface);
        m_mmcState = MOS_New(DecodeMemCompG12, m_hwInterface);
        DECODE_CHK_NULL(m_mmcState);
    #endif
        return MOS_STATUS_SUCCESS;
    }

 #if USE_CODECHAL_DEBUG_TOOL
    MOS_STATUS Av1PipelineG12::DumpParams(Av1BasicFeature &basicFeature)
    {
        m_debugInterface->m_frameType = basicFeature.m_av1PicParams->m_picInfoFlags.m_fields.m_frameType ? P_TYPE : I_TYPE;
        m_debugInterface->m_bufferDumpFrameNum = basicFeature.m_frameNum;

        DECODE_CHK_STATUS(DumpPicParams(
            basicFeature.m_av1PicParams));

        DECODE_CHK_STATUS(DumpBitstreamControlParams(
            basicFeature.m_av1TileParams));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::DumpBitstreamControlParams(CodecAv1TileParams *tileParams)
    {
        CODECHAL_DEBUG_FUNCTION_ENTER;

        if (!m_debugInterface->DumpIsEnabled(CodechalDbgAttr::attrSlcParams))
        {
            return MOS_STATUS_SUCCESS;
        }

        CODECHAL_DEBUG_CHK_NULL(tileParams);

        std::ostringstream oss;
        oss.setf(std::ios::showbase | std::ios::uppercase);

        oss << "BSTileDataLocation: " << +tileParams->m_bsTileDataLocation << std::endl;
        oss << "BSTileBytesInBuffer: " << +tileParams->m_bsTileBytesInBuffer << std::endl;
        oss << "wBadBSBufferChopping: " << +tileParams->m_badBSBufferChopping << std::endl;
        oss << "tile_row: " << +tileParams->m_tileRow << std::endl;
        oss << "tile_column: " << +tileParams->m_tileColumn << std::endl;
        oss << "tile_index: " << +tileParams->m_tileIndex << std::endl;
        oss << "StartTileIdx: " << +tileParams->m_startTileIdx << std::endl;
        oss << "EndTileIdx: " << +tileParams->m_endTileIdx << std::endl;
        oss << "anchor_frame_idx: " << +tileParams->m_anchorFrameIdx.FrameIdx << std::endl;
        oss << "BSTilePayloadSizeInBytes: " << +tileParams->m_bsTilePayloadSizeInBytes << std::endl;

        const char *fileName = m_debugInterface->CreateFileName(
            "DEC",
            "BitstreamControlParams",
            CodechalDbgExtType::txt);

        std::ofstream ofs;
        ofs.open(fileName, std::ios::out);
        ofs << oss.str();
        ofs.close();

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS Av1PipelineG12::DumpPicParams(CodecAv1PicParams *picParams)
    {
        CODECHAL_DEBUG_FUNCTION_ENTER;

        if (!m_debugInterface->DumpIsEnabled(CodechalDbgAttr::attrPicParams))
        {
            return MOS_STATUS_SUCCESS;
        }
        CODECHAL_DEBUG_CHK_NULL(picParams);

        std::ostringstream oss;
        oss.setf(std::ios::showbase | std::ios::uppercase);

        oss << "CurrPic FrameIdx: " << std::hex << +picParams->m_currPic.FrameIdx << std::endl;
        oss << "CurrDisplayPic FrameIdx: " << std::hex << +picParams->m_currDisplayPic.FrameIdx << std::endl;
        oss << "Profile: " << std::hex << +picParams->m_profile << std::endl;
        oss << "AnchorFrameInsertion: " << std::hex << +picParams->m_anchorFrameInsertion << std::endl;
        oss << "order_hint_bits_minus_1: " << std::hex << +picParams->m_orderHintBitsMinus1 << std::endl;
        oss << "BitDepthIdx: " << std::hex << +picParams->m_bitDepthIdx << std::endl;

        //Sequence Info Flags
        oss << "dwSeqInfoFlags: " << std::hex << +picParams->m_seqInfoFlags.m_value << std::endl;
        oss << "still_picture: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_stillPicture << std::endl;
        oss << "use_128x128_superblock: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_use128x128Superblock << std::endl;
        oss << "enable_filter_intra: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableFilterIntra << std::endl;
        oss << "enable_intra_edge_filter: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableIntraEdgeFilter << std::endl;
        oss << "enable_interintra_compound: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableInterintraCompound << std::endl;
        oss << "enable_masked_compound: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableMaskedCompound << std::endl;
        oss << "enable_dual_filter: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableDualFilter << std::endl;
        oss << "enable_order_hint: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableOrderHint << std::endl;
        oss << "enable_jnt_comp: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableJntComp << std::endl;
        oss << "enable_cdef: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_enableCdef << std::endl;
        oss << "mono_chrome: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_monoChrome << std::endl;
        oss << "color_range: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_colorRange << std::endl;
        oss << "subsampling_x: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_subsamplingX << std::endl;
        oss << "subsampling_y: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_subsamplingY << std::endl;
        oss << "chroma_sample_position: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_chromaSamplePosition << std::endl;
        oss << "film_grain_params_present: " << std::hex << +picParams->m_seqInfoFlags.m_fields.m_filmGrainParamsPresent << std::endl;

        //frame info
        oss << "dwPicInfoFlags: " << std::hex << +picParams->m_picInfoFlags.m_value << std::endl;
        oss << "frame_type: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_frameType << std::endl;
        oss << "show_frame: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_showFrame << std::endl;
        oss << "showable_frame: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_showableFrame << std::endl;
        oss << "error_resilient_mode: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_errorResilientMode << std::endl;
        oss << "disable_cdf_update: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_disableCdfUpdate << std::endl;
        oss << "allow_screen_content_tools: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_allowScreenContentTools << std::endl;
        oss << "force_integer_mv: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_forceIntegerMv << std::endl;
        oss << "allow_intrabc: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_allowIntrabc << std::endl;
        oss << "use_superres: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_useSuperres << std::endl;
        oss << "allow_high_precision_mv: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_allowHighPrecisionMv << std::endl;
        oss << "is_motion_mode_switchable: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_isMotionModeSwitchable << std::endl;
        oss << "use_ref_frame_mvs: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_useRefFrameMvs << std::endl;
        oss << "disable_frame_end_update_cdf: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_disableFrameEndUpdateCdf << std::endl;
        oss << "uniform_tile_spacing_flag: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_uniformTileSpacingFlag << std::endl;
        oss << "allow_warped_motion: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_allowWarpedMotion << std::endl;
        oss << "large_scale_tile: " << std::hex << +picParams->m_picInfoFlags.m_fields.m_largeScaleTile << std::endl;

        oss << "frame_width_minus1: " << std::hex << +picParams->m_frameWidthMinus1 << std::endl;
        oss << "frame_height_minus1: " << std::hex << +picParams->m_frameHeightMinus1 << std::endl;

        for (auto i = 0; i < 8; ++i)
        {
            oss << "ref_frame_map[" << +i << "] FrameIdx:" << std::hex << +picParams->m_refFrameMap[i].FrameIdx << std::endl;
            oss << "ref_frame_map[" << +i << "] PicFlags:" << std::hex << +picParams->m_refFrameMap[i].PicFlags << std::endl;
        }

        for (auto i = 0; i < 7; i++)
        {
            oss << "ref_frame_idx[" << +i << "]: " << std::hex << +picParams->m_refFrameIdx[i] << std::endl;
        }

        oss << "primary_ref_frame: " << std::hex << +picParams->m_primaryRefFrame << std::endl;
        oss << "output_frame_width_in_tiles_minus_1: " << std::hex << +picParams->m_outputFrameWidthInTilesMinus1 << std::endl;
        oss << "output_frame_height_in_tiles_minus_1: " << std::hex << +picParams->m_outputFrameHeightInTilesMinus1 << std::endl;

        for (auto i = 0; i < 2; i++)
        {
            oss << "filter_level[" << +i << "]: " << std::hex << +picParams->m_filterLevel[i] << std::endl;
        }
        oss << "filter_level_u: " << std::hex << +picParams->m_filterLevelU << std::endl;
        oss << "filter_level_v: " << std::hex << +picParams->m_filterLevelV << std::endl;

        //Loop Filter Info Flags
        oss << "cLoopFilterInfoFlags value: " << std::hex << +picParams->m_loopFilterInfoFlags.m_value << std::endl;
        oss << "sharpness_level: " << std::hex << +picParams->m_loopFilterInfoFlags.m_fields.m_sharpnessLevel << std::endl;
        oss << "mode_ref_delta_enabled: " << std::hex << +picParams->m_loopFilterInfoFlags.m_fields.m_modeRefDeltaEnabled << std::endl;
        oss << "mode_ref_delta_update: " << std::hex << +picParams->m_loopFilterInfoFlags.m_fields.m_modeRefDeltaUpdate << std::endl;

        oss << "order_hint: " << std::hex << +picParams->m_orderHint << std::endl;
        oss << "superres_scale_denominator: " << std::hex << +picParams->m_superresScaleDenominator << std::endl;
        oss << "interp_filter: " << std::hex << +picParams->m_interpFilter << std::endl;

        for (auto i = 0; i < 8; i++)
        {
            oss << "ref_deltas[" << +i << "]: " << std::hex << +picParams->m_refDeltas[i] << std::endl;
        }

        for (auto i = 0; i < 2; i++)
        {
            oss << "mode_deltas[" << +i << "]: " << std::hex << +picParams->m_modeDeltas[i] << std::endl;
        }

        oss << "base_qindex: " << std::hex << +picParams->m_baseQindex << std::endl;
        oss << "y_dc_delta_q: " << std::hex << +picParams->m_yDcDeltaQ << std::endl;
        oss << "u_dc_delta_q: " << std::hex << +picParams->m_uDcDeltaQ << std::endl;
        oss << "u_ac_delta_q: " << std::hex << +picParams->m_uAcDeltaQ << std::endl;
        oss << "v_dc_delta_q: " << std::hex << +picParams->m_vDcDeltaQ << std::endl;
        oss << "v_ac_delta_q: " << std::hex << +picParams->m_vAcDeltaQ << std::endl;

        // quantization_matrix
        oss << "wQMatrixFlags value: " << std::hex << +picParams->m_qMatrixFlags.m_value << std::endl;
        oss << "using_qmatrix: " << std::hex << +picParams->m_qMatrixFlags.m_fields.m_usingQmatrix<< std::endl;
        oss << "qm_y: " << std::hex << +picParams->m_qMatrixFlags.m_fields.m_qmY << std::endl;
        oss << "qm_u: " << std::hex << +picParams->m_qMatrixFlags.m_fields.m_qmU << std::endl;
        oss << "qm_v: " << std::hex << +picParams->m_qMatrixFlags.m_fields.m_qmV << std::endl;

        // Mode control flags
        oss << "dwModeControlFlags value: " << std::hex << +picParams->m_modeControlFlags.m_value << std::endl;
        oss << "delta_q_present_flag: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_deltaQPresentFlag << std::endl;
        oss << "log2_delta_q_res: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_log2DeltaQRes << std::endl;
        oss << "delta_lf_present_flag: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_deltaLfPresentFlag << std::endl;
        oss << "log2_delta_lf_res: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_log2DeltaLfRes << std::endl;
        oss << "delta_lf_multi: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_deltaLfMulti << std::endl;
        oss << "tx_mode: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_txMode << std::endl;
        oss << "reference_mode: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_referenceMode << std::endl;
        oss << "reduced_tx_set_used: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_reducedTxSetUsed << std::endl;
        oss << "skip_mode_present: " << std::hex << +picParams->m_modeControlFlags.m_fields.m_skipModePresent << std::endl;

        // Segmentation
        oss << "enabled: " << std::hex << +picParams->m_av1SegData.m_enabled << std::endl;
        oss << "update_map: " << std::hex << +picParams->m_av1SegData.m_updateMap << std::endl;
        oss << "temporal_update: " << std::hex << +picParams->m_av1SegData.m_temporalUpdate << std::endl;
        oss << "update_data: " << std::hex << +picParams->m_av1SegData.m_updateData << std::endl;

        for (auto i = 0; i < 8; i++)
        {
            for (auto j = 0; j < 8; j++)
            {
                oss << "feature_data[" << +i << "][" << +j << "]: " << std::hex << +picParams->m_av1SegData.m_featureData[i][j] << std::endl;
            }
        }
        for (auto i = 0; i < 8; i++)
        {
            oss << "feature_mask[" << +i << "]: " << std::hex << +picParams->m_av1SegData.m_featureMask [i] << std::endl;
        }

        oss << "tile_cols: " << std::hex << +picParams->m_tileCols << std::endl;
        for (auto i = 0; i < 63; i++)
        {
            oss << "width_in_sbs_minus_1[" << +i << "]: " << std::hex << +picParams->m_widthInSbsMinus1[i] << std::endl;
        }
        oss << "tile_rows: " << std::hex << +picParams->m_tileRows << std::endl;
        for (auto i = 0; i < 63; i++)
        {
            oss << "height_in_sbs_minus_1[" << +i << "]: " << std::hex << +picParams->m_heightInSbsMinus1[i] << std::endl;
        }


        oss << "tile_count_minus_1: " << std::hex << +picParams->m_tileCountMinus1 << std::endl;
        oss << "context_update_tile_id: " << std::hex << +picParams->m_contextUpdateTileId << std::endl;

        oss << "cdef_damping_minus_3: " << std::hex << +picParams->m_cdefDampingMinus3 << std::endl;
        oss << "cdef_bits: " << std::hex << +picParams->m_cdefBits << std::endl;
        for (auto i = 0; i < 8; i++)
        {
            oss << "cdef_y_strengths[" << +i << "]: " << std::hex << +picParams->m_cdefYStrengths[i] << std::endl;
        }
        for (auto i = 0; i < 8; i++)
        {
            oss << "cdef_uv_strengths[" << +i << "]: " << std::hex << +picParams->m_cdefUvStrengths[i] << std::endl;
        }

        // Loop Restoration Flags
        oss << "LoopRestorationFlags value: " << std::hex << +picParams->m_loopRestorationFlags.m_value << std::endl;
        oss << "yframe_restoration_type: " << std::hex << +picParams->m_loopRestorationFlags.m_fields.m_yframeRestorationType << std::endl;
        oss << "cbframe_restoration_type: " << std::hex << +picParams->m_loopRestorationFlags.m_fields.m_cbframeRestorationType << std::endl;
        oss << "crframe_restoration_type: " << std::hex << +picParams->m_loopRestorationFlags.m_fields.m_crframeRestorationType << std::endl;
        oss << "lr_unit_shift: " << std::hex << +picParams->m_loopRestorationFlags.m_fields.m_lrUnitShift << std::endl;
        oss << "lr_uv_shift: " << std::hex << +picParams->m_loopRestorationFlags.m_fields.m_lrUvShift << std::endl;

        for (auto i = 0; i < 7; i++)
        {
            oss << "wm[" << +i << "].wmtype: " << std::hex << +picParams->m_wm[i].m_wmtype << std::endl;
            for (auto j = 0; j < 8; j++)
            {
                oss << "wm[" << +i << "].wmmat[" << +j << "]: " << std::hex << +picParams->m_wm[i].m_wmmat[j] << std::endl;
            }
        }

        //Film Grain params
        oss << "apply_grain: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_applyGrain << std::endl;
        oss << "chroma_scaling_from_luma: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_chromaScalingFromLuma << std::endl;
        oss << "grain_scaling_minus_8: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_grainScalingMinus8 << std::endl;
        oss << "ar_coeff_lag: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_arCoeffLag << std::endl;
        oss << "ar_coeff_shift_minus_6: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_arCoeffShiftMinus6 << std::endl;
        oss << "grain_scale_shift: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_grainScaleShift << std::endl;
        oss << "overlap_flag: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_overlapFlag << std::endl;
        oss << "clip_to_restricted_range: " << std::hex << +picParams->m_filmGrainParams.m_filmGrainInfoFlags.m_fields.m_clipToRestrictedRange << std::endl;

        oss << "random_seed: " << std::hex << +picParams->m_filmGrainParams.m_randomSeed << std::endl;
        oss << "num_y_points: " << std::hex << +picParams->m_filmGrainParams.m_numYPoints << std::endl;
        for (auto i = 0; i < 14; i++)
        {
            oss << "point_y_value[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_pointYValue[i] << std::endl;
        }
        for (auto i = 0; i < 14; i++)
        {
            oss << "point_y_scaling[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_pointYScaling[i] << std::endl;
        }

        oss << "num_cb_points: " << std::hex << +picParams->m_filmGrainParams.m_numCbPoints << std::endl;
        for (auto i = 0; i < 10; i++)
        {
            oss << "point_cb_value[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_pointCbValue[i] << std::endl;
        }
        for (auto i = 0; i < 10; i++)
        {
            oss << "point_cb_scaling[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_pointCbScaling[i] << std::endl;
        }

        oss << "num_cr_points: " << std::hex << +picParams->m_filmGrainParams.m_numCrPoints << std::endl;
        for (auto i = 0; i < 10; i++)
        {
            oss << "point_cr_value[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_pointCrValue[i] << std::endl;
        }
        for (auto i = 0; i < 10; i++)
        {
            oss << "point_cr_scaling[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_pointCrScaling[i] << std::endl;
        }

        for (auto i = 0; i < 24; i++)
        {
            oss << "ar_coeffs_y[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_arCoeffsY[i] << std::endl;
        }
        for (auto i = 0; i < 25; i++)
        {
            oss << "ar_coeffs_cb[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_arCoeffsCb[i] << std::endl;
        }
        for (auto i = 0; i < 25; i++)
        {
            oss << "ar_coeffs_cr[" << +i << "]: " << std::hex << +picParams->m_filmGrainParams.m_arCoeffsCr[i] << std::endl;
        }

        oss << "cb_mult: " << std::hex << +picParams->m_filmGrainParams.m_cbMult << std::endl;
        oss << "cb_luma_mult: " << std::hex << +picParams->m_filmGrainParams.m_cbLumaMult << std::endl;
        oss << "cb_offset: " << std::hex << +picParams->m_filmGrainParams.m_cbOffset << std::endl;
        oss << "cr_mult: " << std::hex << +picParams->m_filmGrainParams.m_crMult << std::endl;
        oss << "cr_luma_mult: " << std::hex << +picParams->m_filmGrainParams.m_crLumaMult << std::endl;
        oss << "cr_offset: " << std::hex << +picParams->m_filmGrainParams.m_crOffset << std::endl;
        oss << "StatusReportFeedbackNumber: " << std::hex << +picParams->m_statusReportFeedbackNumber << std::endl;

        //Driver internal
        oss << "losslessMode: " << std::hex << +picParams->m_losslessMode << std::endl;
        oss << "superResUpscaledWidthMinus1: " << std::hex << +picParams->m_superResUpscaledWidthMinus1 << std::endl;
        oss << "superResUpscaledHeightMinus1: " << std::hex << +picParams->m_superResUpscaledHeightMinus1 << std::endl;
        for (auto i = 0; i < 8; i++)
        {
            oss << "activeRefBitMaskMfmv[" << +i << "]: " << std::hex << +picParams->m_activeRefBitMaskMfmv[i] << std::endl;
        }

        const char* fileName = m_debugInterface->CreateFileName(
            "DEC",
            CodechalDbgBufferType::bufPicParams,
            CodechalDbgExtType::txt);

        std::ofstream ofs(fileName, std::ios::out);
        ofs << oss.str();
        ofs.close();

        return MOS_STATUS_SUCCESS;
    }
#endif

    MOS_STATUS Av1PipelineG12::CreateFeatureManager()
    {
        DECODE_FUNC_CALL();
        m_featureManager = MOS_New(DecodeAv1FeatureManagerG12, m_allocator, m_hwInterface);
        DECODE_CHK_NULL(m_featureManager);
        return MOS_STATUS_SUCCESS;
    }

}
