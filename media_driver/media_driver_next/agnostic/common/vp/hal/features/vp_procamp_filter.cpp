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
//! \file     vp_procamp_filter.cpp
//! \brief    Defines the common interface for Procamp
//!           this file is for the base interface which is shared by all Procamp in driver.
//!
#include "vp_procamp_filter.h"
#include "vp_vebox_cmd_packet.h"
#include "hw_filter.h"
#include "sw_filter_pipe.h"

namespace vp {
VpProcampFilter::VpProcampFilter(PVP_MHWINTERFACE vpMhwInterface) :
    VpFilter(vpMhwInterface)
{

}

MOS_STATUS VpProcampFilter::Init()
{
    VP_FUNC_CALL();

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpProcampFilter::Prepare()
{
    VP_FUNC_CALL();

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpProcampFilter::Destroy()
{
    VP_FUNC_CALL();

    if (m_pVeboxProcampParams)
    {
        MOS_FreeMemAndSetNull(m_pVeboxProcampParams);
    }

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpProcampFilter::SetExecuteEngineCaps(
    FeatureParamProcamp &procampParams,
    VP_EXECUTE_CAPS vpExecuteCaps)
{
    VP_FUNC_CALL();

    m_procampParams = procampParams;
    m_executeCaps   = vpExecuteCaps;

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS VpProcampFilter::CalculateEngineParams()
{
    VP_FUNC_CALL();

    if (m_executeCaps.bVebox)
    {
        // create a filter Param buffer
        if (!m_pVeboxProcampParams)
        {
            m_pVeboxProcampParams = (PVEBOX_PROCAMP_PARAMS)MOS_AllocAndZeroMemory(sizeof(VEBOX_PROCAMP_PARAMS));

            if (m_pVeboxProcampParams == nullptr)
            {
                VP_PUBLIC_ASSERTMESSAGE("Vebox Procamp Pamas buffer allocate failed, return nullpointer");
                return MOS_STATUS_NO_SPACE;
            }
        }
        else
        {
            MOS_ZeroMemory(m_pVeboxProcampParams, sizeof(VEBOX_PROCAMP_PARAMS));
        }

        m_pVeboxProcampParams->bEnableProcamp = m_procampParams.bEnableProcamp;
        m_pVeboxProcampParams->fBrightness = m_procampParams.fBrightness;
        m_pVeboxProcampParams->fContrast = m_procampParams.fContrast;
        m_pVeboxProcampParams->fHue = m_procampParams.fHue;
        m_pVeboxProcampParams->fSaturation = m_procampParams.fSaturation;
    }
    else
    {
        VP_PUBLIC_ASSERTMESSAGE("Wrong engine caps! Vebox should be used for Procamp");
        return MOS_STATUS_INVALID_PARAMETER;
    }

    return MOS_STATUS_SUCCESS;
}


/****************************************************************************************************/
/*                                   HwFilter Procamp Parameter                                         */
/****************************************************************************************************/
HwFilterParameter *HwFilterProcampParameter::Create(HW_FILTER_PROCAMP_PARAM &param, FeatureType featureType)
{
    HwFilterProcampParameter *p = MOS_New(HwFilterProcampParameter, featureType);
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

HwFilterProcampParameter::HwFilterProcampParameter(FeatureType featureType) : HwFilterParameter(featureType)
{
}

HwFilterProcampParameter::~HwFilterProcampParameter()
{
}

MOS_STATUS HwFilterProcampParameter::ConfigParams(HwFilter &hwFilter)
{
    return hwFilter.ConfigParam(m_Params);
}

MOS_STATUS HwFilterProcampParameter::Initialize(HW_FILTER_PROCAMP_PARAM &param)
{
    m_Params = param;
    return MOS_STATUS_SUCCESS;
}

/****************************************************************************************************/
/*                                   Packet Vebox Procamp Parameter                                       */
/****************************************************************************************************/
VpPacketParameter *VpVeboxProcampParameter::Create(HW_FILTER_PROCAMP_PARAM &param)
{
    if (nullptr == param.pPacketParamFactory)
    {
        return nullptr;
    }
    VpVeboxProcampParameter *p = dynamic_cast<VpVeboxProcampParameter *>(param.pPacketParamFactory->GetPacketParameter(param.pHwInterface));
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

VpVeboxProcampParameter::VpVeboxProcampParameter(PVP_MHWINTERFACE pHwInterface, PacketParamFactoryBase *packetParamFactory) :
    VpPacketParameter(packetParamFactory), m_procampFilter(pHwInterface)
{
}
VpVeboxProcampParameter::~VpVeboxProcampParameter() {}

bool VpVeboxProcampParameter::SetPacketParam(VpCmdPacket *pPacket)
{
    VpVeboxCmdPacket *pVeboxPacket = dynamic_cast<VpVeboxCmdPacket *>(pPacket);
    if (nullptr == pVeboxPacket)
    {
        return false;
    }

    VEBOX_PROCAMP_PARAMS *pParams = m_procampFilter.GetVeboxParams();
    if (nullptr == pParams)
    {
        return false;
    }
    return MOS_SUCCEEDED(pVeboxPacket->SetProcampParams(pParams));
}

MOS_STATUS VpVeboxProcampParameter::Initialize(HW_FILTER_PROCAMP_PARAM &params)
{
    VP_PUBLIC_CHK_STATUS_RETURN(m_procampFilter.Init());
    VP_PUBLIC_CHK_STATUS_RETURN(m_procampFilter.SetExecuteEngineCaps(params.procampParams, params.vpExecuteCaps));
    VP_PUBLIC_CHK_STATUS_RETURN(m_procampFilter.CalculateEngineParams());
    return MOS_STATUS_SUCCESS;
}

/****************************************************************************************************/
/*                                   Policy Vebox Procamp Handler                                         */
/****************************************************************************************************/
PolicyVeboxProcampHandler::PolicyVeboxProcampHandler()
{
    m_Type = FeatureTypeProcampOnVebox;
}
PolicyVeboxProcampHandler::~PolicyVeboxProcampHandler()
{
}

bool PolicyVeboxProcampHandler::IsFeatureEnabled(VP_EXECUTE_CAPS vpExecuteCaps)
{
    return vpExecuteCaps.bProcamp;
}

HwFilterParameter* PolicyVeboxProcampHandler::CreateHwFilterParam(VP_EXECUTE_CAPS vpExecuteCaps, SwFilterPipe& swFilterPipe, PVP_MHWINTERFACE pHwInterface)
{
    if (IsFeatureEnabled(vpExecuteCaps))
    {
        if (SwFilterPipeType1To1 != swFilterPipe.GetSwFilterPipeType())
        {
            VP_PUBLIC_ASSERTMESSAGE("Invalid parameter! Sfc only support 1To1 swFilterPipe!");
            return nullptr;
        }

        SwFilterProcamp *swFilter = dynamic_cast<SwFilterProcamp *>(swFilterPipe.GetSwFilter(true, 0, FeatureTypeProcampOnVebox));

        if (nullptr == swFilter)
        {
            VP_PUBLIC_ASSERTMESSAGE("Invalid parameter! Feature enabled in vpExecuteCaps but no swFilter exists!");
            return nullptr;
        }

        FeatureParamProcamp &param = swFilter->GetSwFilterParams();

        HW_FILTER_PROCAMP_PARAM paramProcamp = {};
        paramProcamp.type = m_Type;
        paramProcamp.pHwInterface = pHwInterface;
        paramProcamp.vpExecuteCaps = vpExecuteCaps;
        paramProcamp.pPacketParamFactory = &m_PacketParamFactory;
        paramProcamp.procampParams = param;
        paramProcamp.pfnCreatePacketParam = PolicyVeboxProcampHandler::CreatePacketParam;

        HwFilterParameter *pHwFilterParam = GetHwFeatureParameterFromPool();

        if (pHwFilterParam)
        {
            if (MOS_FAILED(((HwFilterProcampParameter*)pHwFilterParam)->Initialize(paramProcamp)))
            {
                ReleaseHwFeatureParameter(pHwFilterParam);
            }
        }
        else
        {
            pHwFilterParam = HwFilterProcampParameter::Create(paramProcamp, m_Type);
        }

        return pHwFilterParam;
    }
    else
    {
        return nullptr;
    }
}
}
