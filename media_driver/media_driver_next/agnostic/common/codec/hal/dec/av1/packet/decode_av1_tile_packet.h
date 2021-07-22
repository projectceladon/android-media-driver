/*
* Copyright (c) 2019, Intel Corporation
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
//! \file     decode_av1_slice_packet.h
//! \brief    Defines the implementation of av1 decode slice packet
//!

#ifndef __DECODE_AV1_TILE_PACKET_H__
#define __DECODE_AV1_TILE_PACKET_H__

#include "media_cmd_packet.h"
#include "decode_av1_pipeline.h"
#include "decode_utils.h"
#include "decode_av1_basic_feature.h"
#include "decode_av1_tile_coding.h"

namespace decode
{

class Av1DecodeTilePkt : public DecodeSubPacket
{
public:
    Av1DecodeTilePkt(Av1Pipeline *pipeline, CodechalHwInterface *hwInterface)
        : DecodeSubPacket(pipeline, hwInterface), m_av1Pipeline(pipeline)
    {
        if (m_hwInterface != nullptr)
        {
            m_avpInterface  =  static_cast<CodechalHwInterfaceG12*>(hwInterface)->GetAvpInterface();
        }
    }
    virtual ~Av1DecodeTilePkt(){};

    //!
    //! \brief  Initialize the media packet, allocate required resources
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Init() override;

    //!
    //! \brief  Prepare interal parameters, should be invoked for each frame
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Prepare() override;

    //!
    //! \brief  Execute av1 tile packet
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Execute(MOS_COMMAND_BUFFER& cmdBuffer, int16_t tileIdx) = 0;

    //!
    //! \brief  Calculate Command Size
    //!
    //! \param  [in, out] commandBufferSize
    //!         requested size
    //! \param  [in, out] requestedPatchListSize
    //!         requested size
    //! \return MOS_STATUS
    //!         status
    //!
    MOS_STATUS CalculateCommandSize(
        uint32_t &commandBufferSize,
        uint32_t &requestedPatchListSize) override;

protected:

    virtual MOS_STATUS SetBsdObjParams(MhwVdboxAvpBsdParams &bsdObjParams,
                                       int16_t tileIdx);

    virtual MOS_STATUS AddBsdObj(MOS_COMMAND_BUFFER &cmdBuffer, int16_t tileIdx);

    virtual MOS_STATUS SetAvpTileCodingParams(MhwVdboxAvpTileCodingParams &tileCodingParams,
                                    int16_t tileIdx);

    virtual MOS_STATUS SetInloopFilterStateParams(MhwVdboxAvpPicStateParams &picStateParams);
    virtual MOS_STATUS AddAvpInloopFilterStateCmd(MOS_COMMAND_BUFFER &cmdBuffer);

    MOS_STATUS AddAvpTileState(MOS_COMMAND_BUFFER &cmdBuffer, int16_t tileIdx);

    //!
    //! \brief  Calculate tile level command Buffer Size
    //!
    //! \return uint32_t
    //!         Command buffer size calculated
    //!
    virtual MOS_STATUS CalculateTileStateCommandSize();

    Av1Pipeline *          m_av1Pipeline     = nullptr;
    MhwVdboxAvpInterface * m_avpInterface     = nullptr;
    Av1BasicFeature *      m_av1BasicFeature = nullptr;
    DecodeAllocator *      m_allocator        = nullptr;

    // Parameters passed from application
    CodecAv1PicParams               *m_av1PicParams                         = nullptr;      //!< Pointer to AV1 picture parameter
    CodecAv1SegmentsParams          *m_segmentParams                        = nullptr;      //!< Pointer to AV1 segments parameter
    CodecAv1TileParams              *m_av1TileParams                        = nullptr;      //!< Pointer to AV1 tiles parameter

    uint32_t m_tileStatesSize      = 0;  //!< Tile state command size
    uint32_t m_tilePatchListSize   = 0;  //!< Tile patch list size
};

}  // namespace decode
#endif
