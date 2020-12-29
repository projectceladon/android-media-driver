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
//! \file     decode_av1_feature_manager_g12.h
//! \brief    Defines the common interface for av1 decode feature manager for Gen12
//! \details  The av1 decode feature manager is further sub-divided by codec type
//!           this file is for the base interface which is shared by all components.
//!

#ifndef __DECODE_AV1_FEATURE_MANAGER_G12_H__
#define __DECODE_AV1_FEATURE_MANAGER_G12_H__

#include <vector>
#include "decode_av1_feature_manager.h"

namespace decode
{
    class DecodeAv1FeatureManagerG12 : public DecodeAv1FeatureManager
    {
    public:
        //!
        //! \brief  DecodeAv1FeatureManagerG12 constructor
        //! \param  [in] allocator
        //!         Pointer to DecodeAllocator
        //! \param  [in] hwInterface
        //!         Pointer to CodechalHwInterface
        //! \param  [in] trackedBuf
        //!         Pointer to TrackedBuffer
        //! \param  [in] recycleBuf
        //!         Pointer to RecycleResource
        //!
        DecodeAv1FeatureManagerG12(DecodeAllocator *allocator, CodechalHwInterface *hwInterface)
            : DecodeAv1FeatureManager(allocator, hwInterface)
        {}

        //!
        //! \brief  DecodeAv1FeatureManager deconstructor
        //!
        virtual ~DecodeAv1FeatureManagerG12() {}

    protected:
        //!
        //! \brief  Create features
        //! \param  [in] constsettings
        //!         feature const settings
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS CreateFeatures(void *constSettings);
    };

}
#endif // !__DECODE_AV1_FEATURE_MANAGER_G12_H__
