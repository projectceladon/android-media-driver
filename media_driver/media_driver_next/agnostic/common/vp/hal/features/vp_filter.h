/*
* Copyright (c) 2018-2021, Intel Corporation
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
//! \file     vp_filter.h
//! \brief    Defines the common interface for vp filters
//!           this file is for the base interface which is shared by all features.
//!
#ifndef __VP_FILTER_H__
#define __VP_FILTER_H__

#include <map>
#include "mos_defs.h"
#include "vp_pipeline_common.h"
#include "vp_sfc_common.h"
#include "vp_utils.h"
#include "sw_filter.h"
#include "vp_feature_caps.h"

namespace vp {
class VpCmdPacket;

class VpFilter
{
public:
    VpFilter(
        PVP_MHWINTERFACE vpMhwInterface);

    virtual ~VpFilter() {};

    //!
    //! \brief  Initialize the media filter, allocate required resources
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Init() = 0;

    //!
    //! \brief  Prepare the parameters for filter generation
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Prepare() = 0;

    //!
    //! \brief  Destroy the media Filter and release the resources
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Destroy() = 0;

    //!
    //! \brief  Get execute caps for this filter
    //! \return VP_EXECUTE_CAPS
    //!         return the caps of filters
    //!
    VP_EXECUTE_CAPS GetExecuteCaps()
    {
        return m_executeCaps;
    }

    //!
    //! \brief  Get current associated media Packet
    //! \return MediaTask*
    //!         return the media task pointer
    //!
    VpCmdPacket * GetActivePacket()
    {
        return m_packet;
    }

    //!
    //! \brief  Set current associated media Packet
    //! \return MediaTask*
    //!         return the media task pointer
    //!
    void SetPacket(VpCmdPacket* packet)
    {
        m_packet = packet;
    }

protected:

    PVP_MHWINTERFACE      m_pvpMhwInterface = nullptr;   // vp HW interfaces
    VP_EXECUTE_CAPS       m_executeCaps = {};        // Filter executed caps
    PVPHAL_SURFACE        m_tempSurface = nullptr;   // Inter-Media surface for Filter temp output

    VpCmdPacket         * m_packet = nullptr;

};

struct _SFC_SCALING_PARAMS
{
    // Scaling parameters
    uint32_t                        dwOutputFrameHeight;                        // Output Frame Height
    uint32_t                        dwOutputFrameWidth;                         // Output Frame Width
    uint32_t                        dwInputFrameHeight;                         // Input Frame Height
    uint32_t                        dwInputFrameWidth;                          // Input Frame Width
    MOS_FORMAT                      inputFrameFormat;                           // Input Frame Format

    bool                            bBilinearScaling;                           // true if bilinear scaling, otherwise avs scaling.
    uint32_t                        dwSourceRegionHeight;                       // Source/Crop region height
    uint32_t                        dwSourceRegionWidth;                        // Source/Crop region width
    uint32_t                        dwSourceRegionVerticalOffset;               // Source/Crop region vertical offset
    uint32_t                        dwSourceRegionHorizontalOffset;             // Source/Crop region horizontal offset
    uint32_t                        dwScaledRegionHeight;                       // Scaled region height
    uint32_t                        dwScaledRegionWidth;                        // Scaled region width
    uint32_t                        dwScaledRegionVerticalOffset;               // Scaled region vertical offset
    uint32_t                        dwScaledRegionHorizontalOffset;             // Scaled region horizontal offset
    float                           fAVSXScalingRatio;                          // X Scaling Ratio
    float                           fAVSYScalingRatio;                          // Y Scaling Ratio

    SFC_COLORFILL_PARAMS            sfcColorfillParams;                         // Colorfill Params

    VPHAL_SCALING_MODE              sfcScalingMode;                             // Bilinear, Nearest, AVS and future extension (configured by AVS coefficients)
    //Interlaced scaling parameters
    uint32_t                        interlacedScalingType;
    VPHAL_SAMPLE_TYPE               srcSampleType;
    VPHAL_SAMPLE_TYPE               dstSampleType;
};

struct _SFC_CSC_PARAMS
{
    bool                            bCSCEnabled;                                 // CSC Enabled
    bool                            bInputColorSpace;                            // 0: YUV color space, 1:RGB color space
    bool                            bIEFEnable;                                  // IEF Enabled
    bool                            bChromaUpSamplingEnable;                     // ChromaUpSampling
    bool                            b8tapChromafiltering;                        // Enables 8 tap filtering for Chroma Channels
    VPHAL_CSPACE                    inputColorSpcase;                            // Input Color Space
    MOS_FORMAT                      inputFormat;                                 // SFC Input Format
    MOS_FORMAT                      outputFormat;                                // SFC Output Format
    PVPHAL_IEF_PARAMS               iefParams;                                   // Vphal Params
    uint32_t                        sfcSrcChromaSiting;                          // SFC Source Chroma Siting location
    uint32_t                        chromaDownSamplingVerticalCoef;              // Chroma DownSampling Vertical Coeff
    uint32_t                        chromaDownSamplingHorizontalCoef;            // Chroma DownSampling Horizontal Coeff
};

struct _SFC_ROT_MIR_PARAMS
{
    VPHAL_ROTATION                  rotationMode;                               // Rotation mode -- 0, 90, 180 or 270
    uint32_t                        mirrorType;                               // Mirror Type -- vert/horiz
    bool                            bMirrorEnable;                              // Mirror mode -- enable/disable
};

struct _VEBOX_DN_PARAMS
{
    bool                            bDnEnabled;
    bool                            bChromaDenoise;                             // bEnableChroma && bEnableLuma
    bool                            bAutoDetect;
    float                           fDenoiseFactor;
    VPHAL_NOISELEVEL                NoiseLevel;
    bool                            bEnableHVSDenoise;
    VPHAL_HVSDENOISE_PARAMS         HVSDenoise;
    bool                            bProgressive;
};

struct _VEBOX_STE_PARAMS
{
    bool                            bEnableSTE;                                 // STE Enabled
    uint32_t                        dwSTEFactor;
};

struct _VEBOX_DI_PARAMS
{
    bool                            bDiEnabled;                                 // DI Enabled
    VPHAL_SAMPLE_TYPE               sampleTypeInput;
    bool                            b60fpsDi;
    VPHAL_DI_MODE                   diMode;                                     //!< DeInterlacing mode
    bool                            enableFMD;                                  //!< FMD
    bool                            bSCDEnabled;                                //!< Scene change detection
    bool                            bHDContent;
};

struct _VEBOX_ACE_PARAMS
{
    bool                            bEnableACE;                                 // ACE Enabled
    bool                            bAceLevelChanged;
    uint32_t                        dwAceLevel;
    uint32_t                        dwAceStrength;
    bool                            bAceHistogramEnabled;
};

struct _VEBOX_TCC_PARAMS
{
    bool                            bEnableTCC;                                 // TCC Enabled
    uint8_t                         Red;
    uint8_t                         Green;
    uint8_t                         Blue;
    uint8_t                         Cyan;
    uint8_t                         Magenta;
    uint8_t                         Yellow;
};

struct _VEBOX_CGC_PARAMS
{
    bool                                bEnableCGC;                                 // CGC Enabled
    VPHAL_CSPACE                        colorSpace;
    bool                                bExtendedSrcGamut;
    bool                                bExtendedDstGamut;
    VPHAL_GAMUT_MODE                    GCompMode;
    uint32_t                            dwAttenuation;
    float                               displayRGBW_x[4];
    float                               displayRGBW_y[4];
};

struct _VEBOX_PROCAMP_PARAMS
{
    bool                            bEnableProcamp;                            // Procamp Enabled
    float                           fBrightness;
    float                           fContrast;
    float                           fHue;
    float                           fSaturation;
};

struct _VEBOX_CSC_PARAMS
{
    bool                            bCSCEnabled;                                 // CSC Enabled
    VPHAL_CSPACE                    inputColorSpcase;                            // Input Color Space
    VPHAL_CSPACE                    outputColorSpcase;                            // Input Color Space
    MOS_FORMAT                      inputFormat;                                 // Input Format
    MOS_FORMAT                      outputFormat;                                // Output Format
    PVPHAL_ALPHA_PARAMS             alphaParams;                                 // Output Alpha Params
    bool                            bypassCUS;                                   // Bypass Chroma up sampling
    bool                            bypassCDS;                                   // Bypass Chroma down sampling
    uint32_t                        chromaUpSamplingVerticalCoef;                // Chroma UpSampling Vertical Coeff
    uint32_t                        chromaUpSamplingHorizontalCoef;              // Chroma UpSampling Horizontal Coeff
    uint32_t                        chromaDownSamplingVerticalCoef;              // Chroma DownSampling Vertical Coeff
    uint32_t                        chromaDownSamplingHorizontalCoef;            // Chroma DownSampling Horizontal Coeff
};

struct _VEBOX_HDR_PARAMS
{
    uint32_t                        uiMaxDisplayLum;       //!< Maximum Display Luminance
    uint32_t                        uiMaxContentLevelLum;  //!< Maximum Content Level Luminance
    VPHAL_HDR_MODE                  hdrMode;
    VPHAL_CSPACE                    srcColorSpace;
    VPHAL_CSPACE                    dstColorSpace;
    MOS_FORMAT                      dstFormat;
};

using SFC_SCALING_PARAMS    = _SFC_SCALING_PARAMS;
using PSFC_SCALING_PARAMS   = SFC_SCALING_PARAMS * ;
using SFC_CSC_PARAMS        = _SFC_CSC_PARAMS;
using PSFC_CSC_PARAMS       = SFC_CSC_PARAMS * ;
using SFC_ROT_MIR_PARAMS    = _SFC_ROT_MIR_PARAMS;
using PSFC_ROT_MIR_PARAMS   = SFC_ROT_MIR_PARAMS * ;
using VEBOX_DN_PARAMS       = _VEBOX_DN_PARAMS;
using PVEBOX_DN_PARAMS      = VEBOX_DN_PARAMS *;
using VEBOX_STE_PARAMS      = _VEBOX_STE_PARAMS;
using PVEBOX_STE_PARAMS     = VEBOX_STE_PARAMS *;
using VEBOX_DI_PARAMS       = _VEBOX_DI_PARAMS;
using PVEBOX_DI_PARAMS      = VEBOX_DI_PARAMS *;
using VEBOX_ACE_PARAMS      = _VEBOX_ACE_PARAMS;
using PVEBOX_ACE_PARAMS     = VEBOX_ACE_PARAMS *;
using VEBOX_TCC_PARAMS      = _VEBOX_TCC_PARAMS;
using PVEBOX_TCC_PARAMS     = VEBOX_TCC_PARAMS *;
using VEBOX_CGC_PARAMS      = _VEBOX_CGC_PARAMS;
using PVEBOX_CGC_PARAMS     = VEBOX_CGC_PARAMS *;
using VEBOX_PROCAMP_PARAMS  = _VEBOX_PROCAMP_PARAMS;
using PVEBOX_PROCAMP_PARAMS = VEBOX_PROCAMP_PARAMS *;
using VEBOX_CSC_PARAMS      = _VEBOX_CSC_PARAMS;
using PVEBOX_CSC_PARAMS     = VEBOX_CSC_PARAMS *;

struct _VEBOX_UPDATE_PARAMS
{
    VEBOX_DN_PARAMS                 denoiseParams;
    VP_EXECUTE_CAPS                 veboxExecuteCaps;
    std::vector<uint32_t>           kernelGroup;
};

using VEBOX_UPDATE_PARAMS      = _VEBOX_UPDATE_PARAMS;
using PVEBOX_UPDATE_PARAMS     = VEBOX_UPDATE_PARAMS *;
using VEBOX_HDR_PARAMS      = _VEBOX_HDR_PARAMS;
using PVEBOX_HDR_PARAMS     = VEBOX_HDR_PARAMS *;

class SwFilterPipe;
class HwFilter;
class PacketParamFactoryBase;

/////////////////////////////HwFilter Parameters///////////////////////////////////
class HwFilterParameter
{
public:
    HwFilterParameter(FeatureType featureType);
    virtual ~HwFilterParameter();
    virtual MOS_STATUS ConfigParams(HwFilter &hwFilter) = 0;

    FeatureType GetFeatureType()
    {
        return m_FeatureType;
    }

private:
    FeatureType m_FeatureType = FeatureTypeInvalid;
};

/////////////////////////////Packet Parameters///////////////////////////////////

class VpPacketParameter
{
public:
    VpPacketParameter(PacketParamFactoryBase *packetParamFactory);
    virtual ~VpPacketParameter();

    static void Destory(VpPacketParameter *&p);

    virtual bool SetPacketParam(VpCmdPacket *pPacket) = 0;

private:
    PacketParamFactoryBase *m_packetParamFactory = nullptr;
};

/////////////////////////////Policy Feature Handler//////////////////////////////

class PolicyFeatureHandler
{
public:
    PolicyFeatureHandler(VP_HW_CAPS &hwCaps);
    virtual ~PolicyFeatureHandler();
    virtual bool IsFeatureEnabled(SwFilterPipe &swFilterPipe);
    virtual HwFilterParameter *CreateHwFilterParam(VP_EXECUTE_CAPS vpExecuteCaps, SwFilterPipe &swFilterPipe, PVP_MHWINTERFACE pHwInterface);
    virtual bool IsFeatureEnabled(VP_EXECUTE_CAPS vpExecuteCaps);
    virtual MOS_STATUS UpdateFeaturePipe(VP_EXECUTE_CAPS caps, SwFilter &feature, SwFilterPipe &featurePipe, SwFilterPipe &executePipe, bool isInputPipe, int index);
    FeatureType GetType();
    HwFilterParameter *GetHwFeatureParameterFromPool();
    MOS_STATUS ReleaseHwFeatureParameter(HwFilterParameter *&pParam);
protected:
    FeatureType m_Type = FeatureTypeInvalid;
    std::vector<HwFilterParameter *> m_Pool;
    VP_HW_CAPS  &m_hwCaps;
};

class PacketParamFactoryBase
{
public:
    PacketParamFactoryBase();
    virtual ~PacketParamFactoryBase();
    virtual VpPacketParameter *GetPacketParameter(PVP_MHWINTERFACE pHwInterface) = 0;
    void ReturnPacketParameter(VpPacketParameter *&p);
protected:
    std::vector<VpPacketParameter *> m_Pool;
};

template<class T>
class PacketParamFactory : public PacketParamFactoryBase
{
public:
    PacketParamFactory()
    {
    }
    virtual ~PacketParamFactory()
    {
    }
    virtual VpPacketParameter *GetPacketParameter(PVP_MHWINTERFACE pHwInterface)
    {
        if (nullptr == pHwInterface)
        {
            return nullptr;
        }
        if (m_Pool.empty())
        {
            T *p = MOS_New(T, pHwInterface, this);
            if (nullptr == p)
            {
                return nullptr;
            }

            VpPacketParameter *pBase = dynamic_cast<VpPacketParameter *>(p);

            if (nullptr == pBase)
            {
                MOS_Delete(p);
            }
            return pBase;
        }
        else
        {
            VpPacketParameter *p = m_Pool.back();
            m_Pool.pop_back();
            return p;
        }
    }
};

struct HW_FILTER_PARAM
{
    FeatureType             type;
    PVP_MHWINTERFACE        pHwInterface;
    VP_EXECUTE_CAPS         vpExecuteCaps;
    PacketParamFactoryBase *pPacketParamFactory                     = nullptr;
    VpPacketParameter*    (*pfnCreatePacketParam)(HW_FILTER_PARAM&) = nullptr;
};
}



#endif // !__VP_FILTER_H__