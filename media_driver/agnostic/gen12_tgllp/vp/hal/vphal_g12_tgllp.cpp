/*
* Copyright (c) 2021, Intel Corporation
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
//! \file     vphal_g12_tgllp.cpp
//! \brief    Vphal Interface Definition
//! \details  Vphal Interface Definition Including:
//!           const and function
//!
#include "vphal_g12_tgllp.h"
#include "vphal_renderer_g12_tgllp.h"

MOS_STATUS VphalStateG12Tgllp::Allocate(
    const VphalSettings *pVpHalSettings)
{
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    VPHAL_PUBLIC_CHK_NULL(pVpHalSettings);
    VPHAL_PUBLIC_CHK_NULL(m_renderHal);

    // Update MOCS
    if (m_renderHal->pOsInterface && m_renderHal->pOsInterface->pfnCachePolicyGetMemoryObject && m_renderHal->pOsInterface->pfnGetGmmClientContext)
    {
        MHW_STATE_BASE_ADDR_PARAMS *pStateBaseParams = &m_renderHal->StateBaseAddressParams;
        MEMORY_OBJECT_CONTROL_STATE StateMocs        = m_renderHal->pOsInterface->pfnCachePolicyGetMemoryObject(MOS_MP_RESOURCE_USAGE_DEFAULT,
            m_renderHal->pOsInterface->pfnGetGmmClientContext(m_renderHal->pOsInterface));

        //update MOCS for Instruction Cache
        pStateBaseParams->mocs4InstructionCache = StateMocs.DwordValue;
        //update MOCS for General state
        pStateBaseParams->mocs4GeneralState = StateMocs.DwordValue;
        //update MOCS for Dynamic state
        pStateBaseParams->mocs4DynamicState = StateMocs.DwordValue;
        //update MOCS for Surface state
        pStateBaseParams->mocs4SurfaceState = StateMocs.DwordValue;
        //update MOCS for Indirect Object
        pStateBaseParams->mocs4IndirectObjectBuffer = StateMocs.DwordValue;
        //update MOCS for Stateless Dataport access
        pStateBaseParams->mocs4StatelessDataport = StateMocs.DwordValue;
    }

    eStatus = VphalState::Allocate(pVpHalSettings);

finish:
    return eStatus;
}

//!
//! \brief    Create instance of VphalRenderer
//! \details  Create instance of VphalRenderer
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS VphalStateG12Tgllp::CreateRenderer()
{
    MOS_STATUS eStatus = MOS_STATUS_UNKNOWN;

    // Setup rendering interface functions
    m_renderer = MOS_New(
        VphalRendererG12Tgllp,
        m_renderHal,
        &eStatus);

    if (m_renderer == nullptr)
    {
        return MOS_STATUS_NULL_POINTER;
    }
    else
    {
        if (eStatus != MOS_STATUS_SUCCESS)
        {
            MOS_Delete(m_renderer);
            m_renderer = nullptr;
            return eStatus;
        }
        else
        {
            m_renderer->SetStatusReportTable(&m_statusTable);
        }
    }

    eStatus = m_renderer->InitKdllParam();
    if (eStatus != MOS_STATUS_SUCCESS)
    {
        MOS_Delete(m_renderer);
        m_renderer = nullptr;
        return eStatus;
    }

    eStatus = m_renderer->AllocateRenderComponents(
                                m_veboxInterface,
                                m_sfcInterface);

    return eStatus;
}
