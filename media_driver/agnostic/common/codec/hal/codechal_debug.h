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
//! \file     codechal_debug.h
//! \brief    Defines the debug interface shared by codec only.
//! \details  The debug interface dumps output from Media based on in input config file.
//!
#ifndef __CODEC_DEBUG_H__
#define __CODEC_DEBUG_H__

#include "media_debug_interface.h"
#if USE_MEDIA_DEBUG_TOOL

#define USE_CODECHAL_DEBUG_TOOL 1
#define CODECHAL_DEBUG_TOOL(expr) expr;

typedef struct _CODECHAL_DBG_CFG CODECHAL_DBG_CFG, *PCODECHAL_DBG_CFG;

namespace CodechalDbgFieldType   = MediaDbgFieldType;
namespace CodechalDbgExtType     = MediaDbgExtType;
namespace CodechalDbgSurfaceType = MediaDbgSurfaceType;
namespace CodechalDbgBufferType  = MediaDbgBufferType;
namespace CodechalDbgAttr        = MediaDbgAttr;
namespace CodechalDbgKernel      = MediaDbgKernel;

//------------------------------------------------------------------------------
// Macros specific to MOS_CODEC_SUBCOMP_DEBUG sub-comp
//------------------------------------------------------------------------------
#define CODECHAL_DEBUG_ASSERT(_expr) \
    MOS_ASSERT(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _expr)

#define CODECHAL_DEBUG_ASSERTMESSAGE(_message, ...) \
    MOS_ASSERTMESSAGE(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _message, ##__VA_ARGS__)

#define CODECHAL_DEBUG_NORMALMESSAGE(_message, ...) \
    MOS_NORMALMESSAGE(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _message, ##__VA_ARGS__)

#define CODECHAL_DEBUG_VERBOSEMESSAGE(_message, ...) \
    MOS_VERBOSEMESSAGE(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _message, ##__VA_ARGS__)

#define CODECHAL_DEBUG_FUNCTION_ENTER \
    MOS_FUNCTION_ENTER(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG)

#define CODECHAL_DEBUG_CHK_STATUS(_stmt) \
    MOS_CHK_STATUS_RETURN(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _stmt)

#define CODECHAL_DEBUG_CHK_STATUS_MESSAGE(_stmt, _message, ...) \
    MOS_CHK_STATUS_MESSAGE_RETURN(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _stmt, _message, ##__VA_ARGS__)

#define CODECHAL_DEBUG_CHK_NULL(_ptr) \
    MOS_CHK_NULL_RETURN(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _ptr)

#define CODECHAL_DEBUG_CHK_NULL_NO_STATUS(_ptr) \
    MOS_CHK_NULL_NO_STATUS_RETURN(MOS_COMPONENT_CODEC, MOS_CODEC_SUBCOMP_DEBUG, _ptr)

enum CodechalHucRegionDumpType
{
    hucRegionDumpDefault        = 0,
    hucRegionDumpInit           = 1,
    hucRegionDumpUpdate         = 2,
    hucRegionDumpRegionLocked   = 3,
    hucRegionDumpCmdInitializer = 4,
    hucRegionDumpPakIntegrate   = 5,
    hucRegionDumpHpu            = 6,
    hucRegionDumpBackAnnotation = 7,
    hucRegionDumpHpuSuperFrame  = 8
};

typedef struct _CODECHAL_ME_OUTPUT_PARAMS
{
    PMOS_SURFACE  psMeMvBuffer;
    PMOS_SURFACE  psMeBrcDistortionBuffer;
    PMOS_SURFACE  psMeDistortionBuffer;
    PMOS_RESOURCE pResVdenStreamInBuffer;
    bool          b16xMeInUse;
    bool          b32xMeInUse;
    bool          bVdencStreamInInUse;
} CODECHAL_ME_OUTPUT_PARAMS, *PCODECHAL_ME_OUTPUT_PARAMS;

class MediaDebugInterface;
class CodechalDebugInterface : public MediaDebugInterface
{
public:
    CodechalDebugInterface();
    virtual ~CodechalDebugInterface();

    virtual MOS_STATUS Initialize(
        CodechalHwInterface *hwInterface,
        CODECHAL_FUNCTION    codecFunction);

    MOS_STATUS DumpHucDmem(
        PMOS_RESOURCE             dmemResource,
        uint32_t                  dmemSize,
        uint32_t                  hucPassNum,
        CodechalHucRegionDumpType dumpType);

    MOS_STATUS DumpHucRegion(
        PMOS_RESOURCE             region,
        uint32_t                  regionOffset,
        uint32_t                  regionSize,
        uint32_t                  regionNum,
        const char *              regionName,
        bool                      inputBuffer,
        uint32_t                  hucPassNum,
        CodechalHucRegionDumpType dumpType);

    MOS_STATUS DumpEncodeStatusReport(
        uint8_t *report);

    CodechalHwInterface *m_hwInterface   = nullptr;
    CODECHAL_FUNCTION    m_codecFunction = CODECHAL_FUNCTION_INVALID;
    PCODECHAL_DBG_CFG    m_dbgCfgHead    = nullptr;

protected:
    MOS_USER_FEATURE_VALUE_ID SetOutputPathKey() override;
    MOS_USER_FEATURE_VALUE_ID InitDefaultOutput() override;
};
#else
#define USE_CODECHAL_DEBUG_TOOL 0
#define CODECHAL_DEBUG_TOOL(expr) ;

#endif  // USE_MEDIA_DEBUG_TOOL
#endif  /* __MEDIA_DEBUG_H__ */
