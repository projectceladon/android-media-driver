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
//! \file     vp_obj_factories.cpp
//! \brief    Factories for vp object creation.
//!
#include "vp_obj_factories.h"
using namespace vp;

/****************************************************************************************************/
/*                                      HwFilterPipeFactory                                         */
/****************************************************************************************************/

HwFilterPipeFactory::HwFilterPipeFactory(VpInterface &vpInterface) : m_allocator(vpInterface)
{
}

HwFilterPipeFactory::~HwFilterPipeFactory()
{
}

MOS_STATUS HwFilterPipeFactory::Create(SwFilterPipe &swfilterPipe,
    Policy &policy, HwFilterPipe *&pHwFilterPipe)
{
    pHwFilterPipe = m_allocator.Create();

    VP_PUBLIC_CHK_NULL_RETURN(pHwFilterPipe);
    MOS_STATUS status = pHwFilterPipe->Initialize(swfilterPipe, policy);
    if (MOS_FAILED(status))
    {
        Destory(pHwFilterPipe);
    }
    return status;
}

MOS_STATUS HwFilterPipeFactory::Destory(HwFilterPipe *&pHwfilterPipe)
{
    return m_allocator.Destory(pHwfilterPipe);
}

/****************************************************************************************************/
/*                                      HwFilterFactory                                             */
/****************************************************************************************************/

HwFilterFactory::HwFilterFactory(VpInterface &vpInterface) : m_allocatorVebox(vpInterface), m_allocatorVeboxSfc(vpInterface), m_allocatorRender(vpInterface)
{
}

HwFilterFactory::~HwFilterFactory()
{
}

HwFilter *HwFilterFactory::Create(HW_FILTER_PARAMS &param)
{
    HwFilter *p = nullptr;
    switch (param.Type)
    {
    case EngineTypeVebox:
        p = m_allocatorVebox.Create();
        break;
    case EngineTypeVeboxSfc:
        p = m_allocatorVeboxSfc.Create();
        break;
    case EngineTypeRender:
        p = m_allocatorRender.Create();
        break;
    default:
        return nullptr;
        break;
    }
    if (p)
    {
        if (MOS_FAILED(p->Initialize(param)))
        {
            Destory(p);
            return nullptr;
        }
    }
    return p;
}

void HwFilterFactory::Destory(HwFilter *&pHwFilter)
{
    if (pHwFilter)
    {
        switch (pHwFilter->GetEngineType())
        {
        case EngineTypeVebox:
        {
            HwFilterVebox *p = dynamic_cast<HwFilterVebox*>(pHwFilter);
            if (p)
            {
                m_allocatorVebox.Destory(p);
                pHwFilter = nullptr;
            }
            else
            {
                VP_PUBLIC_ASSERTMESSAGE("Invalid engine type for hwFilter!");
                MOS_Delete(pHwFilter);
            }
            break;
        }
        case EngineTypeVeboxSfc:
        {
            HwFilterVeboxSfc *p = dynamic_cast<HwFilterVeboxSfc*>(pHwFilter);
            if (p)
            {
                m_allocatorVeboxSfc.Destory(p);
                pHwFilter = nullptr;
            }
            else
            {
                VP_PUBLIC_ASSERTMESSAGE("Invalid engine type for hwFilter!");
                MOS_Delete(pHwFilter);
            }
            break;
        }
        case EngineTypeRender:
        {
            HwFilterRender *p = dynamic_cast<HwFilterRender*>(pHwFilter);
            if (p)
            {
                m_allocatorRender.Destory(p);
                pHwFilter = nullptr;
            }
            else
            {
                VP_PUBLIC_ASSERTMESSAGE("Invalid engine type for hwFilter!");
                MOS_Delete(pHwFilter);
            }
            break;
        }
        default:
            MOS_Delete(pHwFilter);
            return;
        }
        pHwFilter = nullptr;
    }
}

/****************************************************************************************************/
/*                                      SwFilterPipeFactory                                         */
/****************************************************************************************************/

SwFilterPipeFactory::SwFilterPipeFactory(VpInterface &vpInterface) : m_allocator(vpInterface)
{
}

SwFilterPipeFactory::~SwFilterPipeFactory()
{
}

MOS_STATUS SwFilterPipeFactory::Create(PVP_PIPELINE_PARAMS params, SwFilterPipe *&swFilterPipe)
{
    VP_PUBLIC_CHK_NULL_RETURN(params);
    swFilterPipe = m_allocator.Create();
    VP_PUBLIC_CHK_NULL_RETURN(swFilterPipe);

    FeatureRule featureRule;
    MOS_STATUS status = swFilterPipe->Initialize(*params, featureRule);

    if (MOS_FAILED(status))
    {
        m_allocator.Destory(swFilterPipe);
    }
    return status;
}

MOS_STATUS SwFilterPipeFactory::Create(VEBOX_SFC_PARAMS *params, SwFilterPipe *&swFilterPipe)
{
    VP_PUBLIC_CHK_NULL_RETURN(params);
    swFilterPipe = m_allocator.Create();
    VP_PUBLIC_CHK_NULL_RETURN(swFilterPipe);

    MOS_STATUS status = swFilterPipe->Initialize(*params);

    if (MOS_FAILED(status))
    {
        m_allocator.Destory(swFilterPipe);
    }
    return status;
}

MOS_STATUS SwFilterPipeFactory::Create(SwFilterPipe *&swFilterPipe)
{
    swFilterPipe = m_allocator.Create();
    VP_PUBLIC_CHK_NULL_RETURN(swFilterPipe);

    return MOS_STATUS_SUCCESS;
}

void SwFilterPipeFactory::Destory(SwFilterPipe *&swFilterPipe)
{
    m_allocator.Destory(swFilterPipe);
}
