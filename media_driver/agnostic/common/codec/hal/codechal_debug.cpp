/*
* Copyright (c) 2011-2020, Intel Corporation
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
//! \file     codechal_debug.cpp
//! \brief    Defines the debug interface shared by codec only.
//! \details  The debug interface dumps output from Media based on in input config file.
//!
#include "codechal_debug.h"
#if USE_CODECHAL_DEBUG_TOOL
#include "codechal_debug_config_manager.h"

CodechalDebugInterface::CodechalDebugInterface()
{
    memset(&m_currPic, 0, sizeof(CODEC_PICTURE));
    memset(m_fileName, 0, sizeof(m_fileName));
    memset(m_path, 0, sizeof(m_path));
}

CodechalDebugInterface::~CodechalDebugInterface()
{
    if (nullptr != m_configMgr)
    {
        MOS_Delete(m_configMgr);
    }
}

MOS_STATUS CodechalDebugInterface::Initialize(
    CodechalHwInterface *hwInterface,
    CODECHAL_FUNCTION    codecFunction)
{
    CODECHAL_DEBUG_FUNCTION_ENTER;

    CODECHAL_DEBUG_CHK_NULL(hwInterface);
    m_hwInterface   = hwInterface;
    m_codecFunction = codecFunction;
    m_osInterface   = m_hwInterface->GetOsInterface();
    m_cpInterface   = m_hwInterface->GetCpInterface();
    m_miInterface   = m_hwInterface->GetMiInterface();

    //dump loctaion is codechaldump
    MediaDebugInterface::SetOutputFilePath();

    m_configMgr = MOS_New(CodecDebugConfigMgr, this, m_codecFunction, m_outputFilePath);
    CODECHAL_DEBUG_CHK_NULL(m_configMgr);
    CODECHAL_DEBUG_CHK_STATUS(m_configMgr->ParseConfig(m_osInterface->pOsContext));

    MediaDebugInterface::InitDumpLocation();

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS CodechalDebugInterface::DumpHucDmem(
    PMOS_RESOURCE             dmemResource,
    uint32_t                  dmemSize,
    uint32_t                  hucPassNum,
    CodechalHucRegionDumpType dumpType)
{
    CODECHAL_DEBUG_FUNCTION_ENTER;

    if (!m_configMgr->AttrIsEnabled(MediaDbgAttr::attrHuCDmem))
    {
        return MOS_STATUS_SUCCESS;
    }

    CODECHAL_DEBUG_CHK_NULL(dmemResource);
    if (Mos_ResourceIsNull(dmemResource))
    {
        return MOS_STATUS_NULL_POINTER;
    }

    std::string funcName = "";
    if (m_codecFunction == CODECHAL_FUNCTION_DECODE)
    {
        funcName = "DEC_";
    }
    else if (m_codecFunction == CODECHAL_FUNCTION_CENC_DECODE)
    {
        funcName = "DEC_Cenc_";
    }
    else
    {
        funcName = "ENC_";
    }

    std::string dmemName = MediaDbgBufferType::bufHucDmem;
    std::string passName = std::to_string(hucPassNum);
    switch (dumpType)
    {
    case hucRegionDumpInit:
        funcName = funcName + dmemName + "_InitPass" + passName;
        break;
    case hucRegionDumpUpdate:
        funcName = funcName + dmemName + "_UpdatePass" + passName;
        break;
    case hucRegionDumpRegionLocked:
        funcName = funcName + dmemName + "_RegionLocked" + passName;
        break;
    case hucRegionDumpCmdInitializer:
        funcName = funcName + dmemName + "_CmdInitializerPass" + passName;
        break;
    case hucRegionDumpPakIntegrate:
        funcName = funcName + dmemName + "_PakIntPass" + passName;
        break;
    case hucRegionDumpHpu:
        funcName = funcName + dmemName + "_HpuPass" + passName;
        break;
    case hucRegionDumpHpuSuperFrame:
        funcName = funcName + dmemName + "_HpuPass" + passName + "_SuperFramePass";
        break;
    case hucRegionDumpBackAnnotation:
        funcName = funcName + dmemName + "_BackAnnotationPass" + passName;
        break;
    default:
        funcName = funcName + dmemName + "_Pass" + passName;
        break;
    }

    return DumpBuffer(dmemResource, nullptr, funcName.c_str(), dmemSize);
}

MOS_USER_FEATURE_VALUE_ID CodechalDebugInterface::SetOutputPathKey()
{
    return __MEDIA_USER_FEATURE_VALUE_CODECHAL_DEBUG_OUTPUT_DIRECTORY_ID;
}

MOS_USER_FEATURE_VALUE_ID CodechalDebugInterface::InitDefaultOutput()
{
    m_outputFilePath.append(MEDIA_DEBUG_CODECHAL_DUMP_OUTPUT_FOLDER);
    return SetOutputPathKey();
}

MOS_STATUS CodechalDebugInterface::DumpHucRegion(
    PMOS_RESOURCE             region,
    uint32_t                  regionOffset,
    uint32_t                  regionSize,
    uint32_t                  regionNum,
    const char *              regionName,
    bool                      inputBuffer,
    uint32_t                  hucPassNum,
    CodechalHucRegionDumpType dumpType)
{
    CODECHAL_DEBUG_FUNCTION_ENTER;

    if (!m_configMgr->AttrIsEnabled(MediaDbgAttr::attrHucRegions))
    {
        return MOS_STATUS_SUCCESS;
    }
    CODECHAL_DEBUG_ASSERT(regionNum < 16);
    CODECHAL_DEBUG_CHK_NULL(region);

    if (Mos_ResourceIsNull(region))
    {
        return MOS_STATUS_NULL_POINTER;
    }

    std::string funcName = "";
    if (m_codecFunction == CODECHAL_FUNCTION_DECODE)
    {
        funcName = "DEC_";
    }
    else if (m_codecFunction == CODECHAL_FUNCTION_CENC_DECODE)
    {
        funcName = "DEC_CENC_";
    }
    else
    {
        funcName = "ENC_";
    }

    std::string bufName       = MediaDbgBufferType::bufHucRegion;
    std::string inputName     = (inputBuffer) ? "Input_" : "Output_";
    std::string regionNumName = std::to_string(regionNum);
    std::string passName      = std::to_string(hucPassNum);
    switch (dumpType)
    {
    case hucRegionDumpInit:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_InitPass" + passName;
        break;
    case hucRegionDumpUpdate:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_UpdatePass" + passName;
        break;
    case hucRegionDumpRegionLocked:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_RegionLockedPass" + passName;
        break;
    case hucRegionDumpCmdInitializer:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_CmdInitializerPass" + passName;
        break;
    case hucRegionDumpPakIntegrate:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_PakIntPass" + passName;
        break;
    case hucRegionDumpHpu:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_HpuPass" + passName;
        break;
    case hucRegionDumpBackAnnotation:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_BackAnnotationPass" + passName;
        break;
    default:
        funcName = funcName + inputName + bufName + regionNumName + regionName + "_Pass" + passName;
        break;
    }

    return DumpBuffer(region, nullptr, funcName.c_str(), regionSize, regionOffset);
}

#define FIELD_TO_OFS(type, name)                                       \
    ofs << #name << ": " << (int64_t) * ((type *)report) << std::endl; \
    report += sizeof(type) / sizeof(uint8_t);
#define POINTER_TO_OFS(type, name)                       \
    ofs << #name << ": " << (void *)report << std::endl; \
    report += sizeof(void *) / sizeof(uint8_t);
MOS_STATUS CodechalDebugInterface::DumpEncodeStatusReport(uint8_t *report)
{
    CODECHAL_DEBUG_FUNCTION_ENTER;

    CODECHAL_DEBUG_CHK_NULL(report);

    const char *bufferName = "EncodeStatusReport_Parsed";
    const char *attrName   = MediaDbgAttr::attrStatusReport;
    if (!m_configMgr->AttrIsEnabled(attrName))
    {
        return MOS_STATUS_SUCCESS;
    }

    const char *  filePath = CreateFileName(bufferName, attrName, MediaDbgExtType::txt);
    std::ofstream ofs(filePath);

    if (ofs.fail())
    {
        return MOS_STATUS_UNKNOWN;
    }

    FIELD_TO_OFS(CODECHAL_STATUS, CodecStatus);
    FIELD_TO_OFS(uint32_t, StatusReportNumber);
    FIELD_TO_OFS(uint32_t, CurrOriginalPic.FrameIdx);
    FIELD_TO_OFS(CODEC_PICTURE_FLAG, CurrOriginalPic.PicFlags);
    FIELD_TO_OFS(uint32_t, CurrOriginalPic.PicEntry);
    FIELD_TO_OFS(uint32_t, Func);
    POINTER_TO_OFS(PCODEC_REF_LIST, pCurrRefList);
    ofs << std::endl;

    FIELD_TO_OFS(bool, bSequential);
    FIELD_TO_OFS(uint32_t, bitstreamSize);
    FIELD_TO_OFS(int8_t, QpY);
    FIELD_TO_OFS(int8_t, SuggestedQpYDelta);
    FIELD_TO_OFS(uint8_t, NumberPasses);
    FIELD_TO_OFS(uint8_t, AverageQp);
    FIELD_TO_OFS(uint64_t, HWCounterValue.IV);
    FIELD_TO_OFS(uint64_t, HWCounterValue.Count);
    POINTER_TO_OFS(uint64_t *, hwctr);
    FIELD_TO_OFS(uint32_t, QueryStatusFlags);
    ofs << std::endl;

    FIELD_TO_OFS(uint32_t, MAD);
    FIELD_TO_OFS(uint32_t, loopFilterLevel);
    FIELD_TO_OFS(int8_t, LongTermIndication);
    FIELD_TO_OFS(uint16_t, NextFrameWidthMinus1);
    FIELD_TO_OFS(uint16_t, NextFrameHeightMinus1);
    FIELD_TO_OFS(uint8_t, NumberSlices);

    FIELD_TO_OFS(uint16_t, PSNRx100[0]);
    FIELD_TO_OFS(uint16_t, PSNRx100[1]);
    FIELD_TO_OFS(uint16_t, PSNRx100[2]);

    FIELD_TO_OFS(uint32_t, NumberTilesInFrame);
    FIELD_TO_OFS(uint8_t, UsedVdBoxNumber);
    FIELD_TO_OFS(uint32_t, SizeOfSliceSizesBuffer);
    POINTER_TO_OFS(uint16_t *, pSliceSizes);
    FIELD_TO_OFS(uint32_t, SizeOfTileInfoBuffer);
    POINTER_TO_OFS(void *, pHEVCTileinfo);
    FIELD_TO_OFS(uint32_t, NumTileReported);
    ofs << std::endl;

    FIELD_TO_OFS(uint32_t, StreamId);
    POINTER_TO_OFS(void *, pLookaheadStatus);
    ofs.close();

    return MOS_STATUS_SUCCESS;
}
#undef FIELD_TO_OFS
#undef POINTER_TO_OFS

#endif // USE_CODECHAL_DEBUG_TOOL
