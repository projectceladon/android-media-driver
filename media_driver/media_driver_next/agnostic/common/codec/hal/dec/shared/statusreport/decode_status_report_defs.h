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
//! \file     decode_status_report_defs.h
//! \brief    Defines the common struture for decode status report
//! \details
//!
#ifndef __DECODE_STATUS_REPORT_DEFS_H__
#define __DECODE_STATUS_REPORT_DEFS_H__

#include "mos_defs.h"
#include "codec_def_common.h"

namespace decode
{

typedef enum _CODECHAL_CS_ENGINE_ID_DEF
{
    // Instance ID
    CODECHAL_CS_INSTANCE_ID_VDBOX0 = 0,
    CODECHAL_CS_INSTANCE_ID_VDBOX1 = 1,
    CODECHAL_CS_INSTANCE_ID_VDBOX2 = 2,
    CODECHAL_CS_INSTANCE_ID_VDBOX3 = 3,
    CODECHAL_CS_INSTANCE_ID_VDBOX4 = 4,
    CODECHAL_CS_INSTANCE_ID_VDBOX5 = 5,
    CODECHAL_CS_INSTANCE_ID_VDBOX6 = 6,
    CODECHAL_CS_INSTANCE_ID_VDBOX7 = 7,
    CODECHAL_CS_INSTANCE_ID_MAX,
    // Class ID
    CODECHAL_CLASS_ID_VIDEO_ENGINE = 1,
} CODECHAL_CS_ENGINE_ID_DEF;

enum DecodeStatusReportType
{
    statusReportMfx,

    //! \brief decode error status
    DecErrorStatusOffset,

    //! \brief decode MB count
    DecMBCountOffset,

    //! \brief decode frame CRC
    DecFrameCrcOffset,

    //! \brief CS engine ID
    CsEngineIdOffset_0,
    CsEngineIdOffset_1,
    CsEngineIdOffset_2,
    CsEngineIdOffset_3,
    CsEngineIdOffset_4,
    CsEngineIdOffset_5,
    CsEngineIdOffset_6,
    CsEngineIdOffset_7,

    //! \brief MMIO HuCErrorStatus2
    HucErrorStatus2Reg,

    //! \brief mask of MMIO HuCErrorStatus2
    HucErrorStatus2Mask,

    //! \brief MMIO HuCErrorStatus
    HucErrorStatusReg,
    //! \brief mask of MMIO HuCErrorStatus
    HucErrorStatusMask,

    statusReportRcs,
    statusReportGlobalCount = 0x50,
    statusReportMaxNum
};

struct DecodeStatusParameters
{
    uint32_t           statusReportFeedbackNumber;
    uint32_t           numberTilesInFrame;
    uint16_t           pictureCodingType;
    CODEC_PICTURE      currOriginalPic;
    CODECHAL_FUNCTION  codecFunction;
    uint8_t            numUsedVdbox;
    PCODEC_REF_LIST    currRefList;
    uint16_t           picWidthInMb;
    uint16_t           frameFieldHeightInMb;
    uint32_t           numSlices;
    MOS_RESOURCE       currDecodedPicRes;
};

struct DecodeStatusMfx
{
    //!< HW requires a QW aligned offset for data storage
    uint32_t                status = 0;
    //! \brief Value of MMIO decoding effor eStatus register
    uint32_t                m_mmioErrorStatusReg = 0;
    //! \brief Value of MMIO decoding MB error register
    uint32_t                m_mmioMBCountReg = 0;
    //! \brief Frame CRC related to current frames
    uint32_t                m_mmioFrameCrcReg = 0;
    //! \brief Value of MMIO CS Engine ID register for each BB
    uint32_t                m_mmioCsEngineIdReg[CODECHAL_CS_INSTANCE_ID_MAX] = { 0 };
    //! \brief Huc error for HEVC Fix Function, DWORD0: mask value, DWORD1: reg value
    uint64_t                m_hucErrorStatus2 = 0;
    //! \brief Huc error for HEVC Fix Function, DWORD0: mask value, DWORD1: reg value
    uint64_t                m_hucErrorStatus = 0;
};

struct DecodeStatusRcs
{
    uint32_t                    status;
    uint32_t                    pad;        //!< Pad
};

}

#endif