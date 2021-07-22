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
//! \file     media_interfaces_g12_adls.cpp
//! \brief    Helps with Gen12 ADLS factory creation.
//!

#include "media_interfaces_g12_adls.h"

extern template class MediaInterfacesFactory<MhwInterfaces>;
extern template class MediaInterfacesFactory<MmdDevice>;
extern template class MediaInterfacesFactory<MosUtilDevice>;
extern template class MediaInterfacesFactory<CodechalDevice>;
extern template class MediaInterfacesFactory<CMHalDevice>;
extern template class MediaInterfacesFactory<VphalDevice>;
extern template class MediaInterfacesFactory<RenderHalDevice>;
extern template class MediaInterfacesFactory<Nv12ToP010Device>;
extern template class MediaInterfacesFactory<DecodeHistogramDevice>;

static bool adlsRegisteredVphal =
    MediaInterfacesFactory<VphalDevice>::
    RegisterHal<VphalInterfacesG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

static bool adlsRegisteredMhw =
    MediaInterfacesFactory<MhwInterfaces>::
    RegisterHal<MhwInterfacesG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

#ifdef _MMC_SUPPORTED
static bool adlsRegisteredMmd =
    MediaInterfacesFactory<MmdDevice>::
        RegisterHal<MmdDeviceG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);
#endif

static bool adlsRegisteredNv12ToP010 =
    MediaInterfacesFactory<Nv12ToP010Device>::
    RegisterHal<Nv12ToP010DeviceG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

static bool adlsRegisteredCodecHal =
    MediaInterfacesFactory<CodechalDevice>::
    RegisterHal<CodechalInterfacesG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

static bool adlsRegisteredCMHal =
    MediaInterfacesFactory<CMHalDevice>::
    RegisterHal<CMHalInterfacesG12Adls>((uint32_t)IGFX_ALDERLAKE_S);

static bool adlsRegisteredMosUtil =
    MediaInterfacesFactory<MosUtilDevice>::
    RegisterHal<MosUtilDeviceG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

static bool adlsRegisteredRenderHal =
    MediaInterfacesFactory<RenderHalDevice>::
    RegisterHal<RenderHalInterfacesG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

static bool adlsRegisteredDecodeHistogram =
    MediaInterfacesFactory<DecodeHistogramDevice>::
    RegisterHal<DecodeHistogramDeviceG12Tgllp>((uint32_t)IGFX_ALDERLAKE_S);

#define PLATFORM_INTEL_ADLS 21
#define GENX_TGLLP 12

MOS_STATUS CMHalInterfacesG12Adls::Initialize(CM_HAL_STATE *pCmState)
{
    if (pCmState == nullptr)
    {
        MHW_ASSERTMESSAGE("pCmState is nullptr.")
        return MOS_STATUS_INVALID_PARAMETER;
    }

    m_cmhalDevice = MOS_New(CMHal, pCmState);
    if (m_cmhalDevice == nullptr)
    {
        MHW_ASSERTMESSAGE("Create CM Hal interfaces failed.")
        return MOS_STATUS_NO_SPACE;
    }

    m_cmhalDevice->SetGenPlatformInfo(PLATFORM_INTEL_ADLS, PLATFORM_INTEL_GT2, "TGLLP");
    uint32_t cisaIDs[] = {GENX_TGLLP};
    m_cmhalDevice->AddSupportedCisaIDs(cisaIDs, sizeof(cisaIDs) / sizeof(uint32_t));
    m_cmhalDevice->m_l3Plane       = TGL_L3_PLANE;
    m_cmhalDevice->m_l3ConfigCount = TGL_L3_CONFIG_NUM;
    return MOS_STATUS_SUCCESS;
}
