/*
* Copyright (c) 2020-2021, Intel Corporation
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
//! \file     vp_di_filter.cpp
//! \brief    Defines the common interface for Adaptive Contrast Enhancement
//!           this file is for the base interface which is shared by all Ace in driver.
//!
#include "vp_di_filter.h"
#include "vp_vebox_cmd_packet.h"
#include "hw_filter.h"
#include "sw_filter_pipe.h"

using namespace vp;

VpDiFilter::VpDiFilter(PVP_MHWINTERFACE vpMhwInterface) :
    VpFilter(vpMhwInterface)
{

}

MOS_STATUS VpDiFilter::Init()
{
    VP_FUNC_CALL();

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpDiFilter::Prepare()
{
    VP_FUNC_CALL();

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpDiFilter::Destroy()
{
    VP_FUNC_CALL();

    if (m_pVeboxDiParams)
    {
        MOS_FreeMemAndSetNull(m_pVeboxDiParams);
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpDiFilter::SetExecuteEngineCaps(
    FeatureParamDeinterlace &diParams,
    VP_EXECUTE_CAPS         vpExecuteCaps)
{
    VP_FUNC_CALL();

    m_diParams      = diParams;
    m_executeCaps   = vpExecuteCaps;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpDiFilter::CalculateEngineParams()
{
    VP_FUNC_CALL();
    if (m_executeCaps.bVebox)
    {
        // create a filter Param buffer
        if (!m_pVeboxDiParams)
        {
            m_pVeboxDiParams = (PVEBOX_DI_PARAMS)MOS_AllocAndZeroMemory(sizeof(VEBOX_DI_PARAMS));

            if (m_pVeboxDiParams == nullptr)
            {
                VP_PUBLIC_ASSERTMESSAGE("Vebox DI Pamas buffer allocate failed, return nullpointer");
                return MOS_STATUS_NO_SPACE;
            }
        }
        else
        {
            MOS_ZeroMemory(m_pVeboxDiParams, sizeof(VEBOX_DI_PARAMS));
        }

        m_pVeboxDiParams->bDiEnabled        = true;
        m_pVeboxDiParams->sampleTypeInput   = m_diParams.sampleTypeInput;
        m_pVeboxDiParams->b60fpsDi          = m_diParams.b60fpsDi;
        m_pVeboxDiParams->diMode            = m_diParams.DIMode;
        m_pVeboxDiParams->enableFMD         = m_diParams.bEnableFMD;
        m_pVeboxDiParams->bSCDEnabled       = m_diParams.bSCDEnable;
        m_pVeboxDiParams->bHDContent        = m_diParams.bHDContent;
    }
    else
    {
        VP_PUBLIC_ASSERTMESSAGE("Wrong engine caps! Vebox should be used for DI");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    return MOS_STATUS_SUCCESS;
}


/****************************************************************************************************/
/*                                   HwFilter DI Parameter                                         */
/****************************************************************************************************/
HwFilterParameter *HwFilterDiParameter::Create(HW_FILTER_DI_PARAM &param, FeatureType featureType)
{
    HwFilterDiParameter *p = MOS_New(HwFilterDiParameter, featureType);
    if (p)
    {
        if (MOS_FAILED(p->Initialize(param)))
        {
            MOS_Delete(p);
            return nullptr;
        }
    }
    return p;
}

HwFilterDiParameter::HwFilterDiParameter(FeatureType featureType) : HwFilterParameter(featureType)
{
}

HwFilterDiParameter::~HwFilterDiParameter()
{
}

MOS_STATUS HwFilterDiParameter::ConfigParams(HwFilter &hwFilter)
{
    return hwFilter.ConfigParam(m_Params);
}

MOS_STATUS HwFilterDiParameter::Initialize(HW_FILTER_DI_PARAM &param)
{
    m_Params = param;
    return MOS_STATUS_SUCCESS;
}

/****************************************************************************************************/
/*                                   Packet Vebox DI Parameter                                       */
/****************************************************************************************************/
VpPacketParameter *VpVeboxDiParameter::Create(HW_FILTER_DI_PARAM &param)
{
    if (nullptr == param.pPacketParamFactory)
    {
        return nullptr;
    }
    VpVeboxDiParameter *p = dynamic_cast<VpVeboxDiParameter *>(param.pPacketParamFactory->GetPacketParameter(param.pHwInterface));
    if (p)
    {
        if (MOS_FAILED(p->Initialize(param)))
        {
            VpPacketParameter *pParam = p;
            param.pPacketParamFactory->ReturnPacketParameter(pParam);
            return nullptr;
        }
    }
    return p;
}

VpVeboxDiParameter::VpVeboxDiParameter(PVP_MHWINTERFACE pHwInterface, PacketParamFactoryBase *packetParamFactory) :
    VpPacketParameter(packetParamFactory), m_diFilter(pHwInterface)
{
}
VpVeboxDiParameter::~VpVeboxDiParameter() {}

bool VpVeboxDiParameter::SetPacketParam(VpCmdPacket *pPacket)
{
    VpVeboxCmdPacket *pVeboxPacket = dynamic_cast<VpVeboxCmdPacket *>(pPacket);
    if (nullptr == pVeboxPacket)
    {
        return false;
    }

    VEBOX_DI_PARAMS *pParams = m_diFilter.GetVeboxParams();
    if (nullptr == pParams)
    {
        return false;
    }
    return MOS_SUCCEEDED(pVeboxPacket->SetDiParams(pParams));
}

MOS_STATUS VpVeboxDiParameter::Initialize(HW_FILTER_DI_PARAM &params)
{
    VP_PUBLIC_CHK_STATUS_RETURN(m_diFilter.Init());
    VP_PUBLIC_CHK_STATUS_RETURN(m_diFilter.SetExecuteEngineCaps(params.diParams, params.vpExecuteCaps));
    VP_PUBLIC_CHK_STATUS_RETURN(m_diFilter.CalculateEngineParams());
    return MOS_STATUS_SUCCESS;
}

/****************************************************************************************************/
/*                                   Policy Vebox DI Handler                                         */
/****************************************************************************************************/
PolicyVeboxDiHandler::PolicyVeboxDiHandler(VP_HW_CAPS &hwCaps) : PolicyFeatureHandler(hwCaps)
{
    m_Type = FeatureTypeDiOnVebox;
}
PolicyVeboxDiHandler::~PolicyVeboxDiHandler()
{
}

bool PolicyVeboxDiHandler::IsFeatureEnabled(VP_EXECUTE_CAPS vpExecuteCaps)
{
    return vpExecuteCaps.bDI;
}

HwFilterParameter* PolicyVeboxDiHandler::CreateHwFilterParam(VP_EXECUTE_CAPS vpExecuteCaps, SwFilterPipe& swFilterPipe, PVP_MHWINTERFACE pHwInterface)
{
    if (IsFeatureEnabled(vpExecuteCaps))
    {
        if (SwFilterPipeType1To1 != swFilterPipe.GetSwFilterPipeType())
        {
            VP_PUBLIC_ASSERTMESSAGE("Invalid parameter! Sfc only support 1To1 swFilterPipe!");
            return nullptr;
        }

        SwFilterDeinterlace *swFilter = dynamic_cast<SwFilterDeinterlace *>(swFilterPipe.GetSwFilter(true, 0, FeatureTypeDiOnVebox));

        if (nullptr == swFilter)
        {
            VP_PUBLIC_ASSERTMESSAGE("Invalid parameter! Feature enabled in vpExecuteCaps but no swFilter exists!");
            return nullptr;
        }

        FeatureParamDeinterlace &param = swFilter->GetSwFilterParams();

        HW_FILTER_DI_PARAM diParam = {};
        diParam.type = m_Type;
        diParam.pHwInterface = pHwInterface;
        diParam.vpExecuteCaps = vpExecuteCaps;
        diParam.pPacketParamFactory = &m_PacketParamFactory;
        diParam.diParams = param;
        diParam.pfnCreatePacketParam = PolicyVeboxDiHandler::CreatePacketParam;

        HwFilterParameter *pHwFilterParam = GetHwFeatureParameterFromPool();

        if (pHwFilterParam)
        {
            if (MOS_FAILED(((HwFilterDiParameter*)pHwFilterParam)->Initialize(diParam)))
            {
                ReleaseHwFeatureParameter(pHwFilterParam);
            }
        }
        else
        {
            pHwFilterParam = HwFilterDiParameter::Create(diParam, m_Type);
        }

        return pHwFilterParam;
    }
    else
    {
        return nullptr;
    }
}
