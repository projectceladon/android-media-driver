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
#include "vp_pipeline_adapter.h"
#include "vp_platform_interface.h"

VpPipelineAdapter::VpPipelineAdapter(
    vp::VpPlatformInterface     &vpPlatformInterface,
    MOS_STATUS                  &eStatus) :
    m_vpPlatformInterface(vpPlatformInterface)
{
    if (MOS_FAILED(eStatus))
    {
        MOS_OS_ASSERTMESSAGE("VpPipelineAdapter construct failed due to VphalStateG12Tgllp() returned failure: eStatus = %d.", eStatus);
        return;
    }
}

//!
//! \brief    VpPipelineAdapter Destuctor
//! \details  Destroys VpPipelineAdapter and all internal states and objects
//! \return   void
//!
VpPipelineAdapter::~VpPipelineAdapter()
{
    Destroy();
};

MOS_STATUS VpPipelineAdapter::Init(
  const VphalSettings     *pVpHalSettings, VphalState &vphalState)
{
    VP_FUNC_CALL();

    m_vpPipeline = std::make_shared<vp::VpPipeline>(vphalState.GetOsInterface());
    VP_PUBLIC_CHK_NULL_RETURN(m_vpPipeline);
    VP_PUBLIC_CHK_NULL_RETURN(vphalState.GetRenderHal());

    VP_MHWINTERFACE vpMhwinterface = {};   //!< vp Mhw Interface

    vpMhwinterface.m_platform = vphalState.GetPlatform();
    vpMhwinterface.m_waTable  = vphalState.GetWaTable();
    vpMhwinterface.m_skuTable = vphalState.GetSkuTable();

    vpMhwinterface.m_osInterface      = vphalState.GetOsInterface();
    vpMhwinterface.m_renderHal        = vphalState.GetRenderHal();
    vpMhwinterface.m_veboxInterface   = vphalState.GetVeboxInterface();
    vpMhwinterface.m_sfcInterface     = vphalState.GetSfcInterface();
    vpMhwinterface.m_renderer         = vphalState.GetRenderer();
    vpMhwinterface.m_cpInterface      = vphalState.GetCpInterface();
    vpMhwinterface.m_mhwMiInterface   = vphalState.GetRenderHal()->pMhwMiInterface;
    vpMhwinterface.m_statusTable      = &vphalState.GetStatusTable();
    vpMhwinterface.m_vpPlatformInterface = &m_vpPlatformInterface;

    if (vphalState.GetVeboxInterface() &&
        vphalState.GetVeboxInterface()->m_veboxSettings.uiNumInstances > 0 &&
        vphalState.GetVeboxInterface()->m_veboxHeap == nullptr)
    {
        // Allocate VEBOX Heap
        VP_PUBLIC_CHK_STATUS_RETURN(vphalState.GetVeboxInterface()->CreateHeap());
    }

    return m_vpPipeline->Init(&vpMhwinterface);
}

MOS_STATUS VpPipelineAdapter::Execute(PVP_PIPELINE_PARAMS params)
{
    MOS_STATUS eStatus = MOS_STATUS_UNKNOWN;
    vp::VP_PARAMS vpParams = {};

    VP_FUNC_CALL();

    vpParams.type = vp::PIPELINE_PARAM_TYPE_LEGACY;
    vpParams.renderParams = params;

    eStatus = m_vpPipeline->Prepare(&vpParams);
    if (eStatus != MOS_STATUS_SUCCESS)
    {
        if (eStatus == MOS_STATUS_UNIMPLEMENTED)
        {
            VP_PUBLIC_NORMALMESSAGE("Features are UNIMPLEMENTED on APG now \n");
            return eStatus;
        }
        else
        {
            VP_PUBLIC_CHK_STATUS_RETURN(eStatus);
        }
    }

    return m_vpPipeline->Execute();
}

void VpPipelineAdapter::Destroy()
{
    VP_FUNC_CALL();
    if (m_vpPipeline)
    {
        m_vpPipeline->Destroy();
        m_vpPipeline = nullptr;
    }

    vp::VpPlatformInterface *pIntf = &m_vpPlatformInterface;
    MOS_Delete(pIntf);
}

MOS_STATUS VpPipelineAdapter::Render(PCVPHAL_RENDER_PARAMS pcRenderParams)
{
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;
    VP_PIPELINE_PARAMS params = {};

    VP_PUBLIC_CHK_NULL_RETURN(pcRenderParams);
    VP_PUBLIC_CHK_NULL_RETURN(m_vpPipeline);

    params = *(PVP_PIPELINE_PARAMS)pcRenderParams;
    // default render of video
    params.bIsDefaultStream = true;

    eStatus = Execute(&params);

    if (eStatus == MOS_STATUS_SUCCESS)
    {
        m_bApgEnabled = true;
        VP_PUBLIC_NORMALMESSAGE("APG Execution successfully, return \n");
        return eStatus;
    }
    else
    {
        m_bApgEnabled = false;
        return eStatus;
    }
}
