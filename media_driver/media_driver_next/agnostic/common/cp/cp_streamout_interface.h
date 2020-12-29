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
//! \file     cp_streamout_interface.h
//! \brief    Defines the interface of cp streamout
//!

#ifndef __CP_STREAMOUT_INTERFACE_H__
#define __CP_STREAMOUT_INTERFACE_H__

#include "media_pipeline.h"
#include "codechal_hw.h"

class CpStreamOutInterface
{
public:
    virtual ~CpStreamOutInterface() {}
};


//!
//! \brief    Create CpStreamOutInterface Object
//!
//! \return   Return CP Wrapper Object if CPLIB not loaded
//
CpStreamOutInterface *Create_CpStreamOutInterface(
    MediaPipeline *pipeline,
    MediaTask *task,
    CodechalHwInterface *hwInterface);

//!
//! \brief    Delete the CpStreamOutInterface Object
//!
//! \param    [in] *pCpStreamOutInterface
//!           CpStreamOutInterface
//!
void Delete_CpStreamOutInterface(CpStreamOutInterface *pCpStreamOutInterface);

#endif
