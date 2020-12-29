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
//! \file     sw_filter.h
//! \brief    Defines the common interface for vp features manager
//! \details  The vp manager is further sub-divided by vp type
//!           this file is for the base interface which is shared by all components.
//!
#ifndef __SW_FILTER_H__
#define __SW_FILTER_H__

#include "media_feature_manager.h"
#include "vp_utils.h"
#include "vp_pipeline_common.h"
#include <vector>
#include "media_sfc_interface.h"

namespace vp
{
class VpInterface;
enum FeatureType
{
    FeatureTypeInvalid          = 0,
    FeatureTypeCsc              = 0x100,
    FeatureTypeCscOnSfc,
    FeatureTypeCscOnVebox,
    FeatureTypeCscOnRender,
    FeatureTypeRotMir           = 0x200,
    FeatureTypeRotMirOnSfc,
    FeatureTypeRotMirOnRender,
    FeatureTypeScaling          = 0x300,
    FeatureTypeScalingOnSfc,
    FeatureTypeScalingOnRender,
    FeatureTypeDn               = 0x400,
    FeatureTypeDnOnVebox,
    FeatureTypeDi               = 0x500,
    FeatureTypeSte              = 0x600,
    FeatureTypeSteOnVebox,
    FeatureTypeAce              = 0x700,
    FeatureTypeAceOnVebox,
    FeatureTypeSecureVeboxUpdate = 0x800,
    FeatureTypeTcc              = 0x900,
    FeatureTypeTccOnVebox,
    FeatureTypeProcamp          = 0xA00,
    FeatureTypeProcampOnVebox,
    FeatureTypeProcampOnRender,
    FeatureTypeCgc              = 0xB00,
    FeatureTypeCgcOnVebox,
    // ...
    NumOfFeatureType
};

enum SurfaceType
{
    SurfaceTypeInvalid = 0,
    SurfaceTypeDNRef,
    SurfaceTypeDNOutput,
    SurfaceTypeVeboxoutput,
    SurfaceTypeScalar,
    SurfaceTypeSTMMIn,
    SurfaceTypeSTMMOut,
    SurfaceTypeAutoDNNoiseLevel, // with kernel path needed
    SurfaceTypeAutoDNSpatialConfig,
    SurfaceTypeACEHistory,
    SurfaceTypeFMDHistory,
    SurfaceTypeLaceAceRGBHistogram,
    SurfaceTypeLaceLut,
    SurfaceTypeStatistics,
    SurfaceTypeSkinScore,
    SurfaceType3dLut,
    SurfaceType1dLut,
    SurfaceTypeAlphaOrVignette,
    SurfaceTypeVeboxStateHeap_Drv,
    SurfaceTypeVeboxStateHeap_Knr,
    NumberOfSurfaceType
};

#define FEATURE_TYPE_MASK   0xffffff00

inline bool operator==(FeatureType a, FeatureType b)
{
    return (int)a == (int)b || (int)(a & FEATURE_TYPE_MASK) == (int)b || (int)a == (int)(FEATURE_TYPE_MASK & b);
}

inline bool operator!=(FeatureType a, FeatureType b)
{
    return !(a == b);
}

inline bool operator<(FeatureType a, FeatureType b)
{
    return a != b && (int)a < (int)b;
}

#define RECT_ROTATE(rcOut, rcIn)        \
{                                       \
    (rcOut).left    = (rcIn).top;       \
    (rcOut).right   = (rcIn).bottom;    \
    (rcOut).top     = (rcIn).left;      \
    (rcOut).bottom  = (rcIn).right;     \
}

struct FeatureParam
{
    FeatureType type;
    MOS_FORMAT  formatInput;
    MOS_FORMAT  formatOutput;
};

class SwFilterSet;

class SwFilter
{
public:
    SwFilter(VpInterface &vpInterface, FeatureType type);
    virtual ~SwFilter();
    virtual MOS_STATUS Clean()
    {
        MOS_ZeroMemory(&m_EngineCaps, sizeof(m_EngineCaps));
        return MOS_STATUS_SUCCESS;
    }
    virtual FeatureType GetFeatureType()
    {
        return m_type;
    }
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS &params, bool bInputSurf, int surfIndex) = 0;
    virtual MOS_STATUS Configure(PVP_SURFACE surfInput, VP_EXECUTE_CAPS caps)
    {
        return MOS_STATUS_UNIMPLEMENTED;
    }

    virtual MOS_STATUS Configure(VEBOX_SFC_PARAMS &params)
    {
        return MOS_STATUS_UNIMPLEMENTED;
    }

    virtual MOS_STATUS Configure(SwFilter* swFilter, VP_EXECUTE_CAPS caps)
    {
        return MOS_STATUS_UNIMPLEMENTED;
    }
    virtual SwFilter *Clone() = 0;
    virtual bool operator == (class SwFilter&) = 0;
    virtual MOS_STATUS Update(VP_SURFACE *inputSurf, VP_SURFACE *outputSurf) = 0;
    MOS_STATUS SetFeatureType(FeatureType type);
    SwFilter* CreateSwFilter(FeatureType type);
    void DestroySwFilter(SwFilter* p);

    void SetLocation(SwFilterSet *swFilterSet)
    {
        m_location = swFilterSet;
    }

    SwFilterSet *GetLocation()
    {
        return m_location;
    }

    VpInterface& GetVpInterface()
    {
        return m_vpInterface;
    }

    VP_EngineEntry& GetFilterEngineCaps()
    {
        return m_EngineCaps;
    }

protected:
    VpInterface &m_vpInterface;
    FeatureType m_type = FeatureTypeInvalid;
    // SwFilterSet current swFilter belongs to.
    SwFilterSet *m_location = nullptr;
    VP_EngineEntry  m_EngineCaps = {};
};

struct FeatureParamCsc : public FeatureParam
{
    PVPHAL_IEF_PARAMS   pIEFParams;
    PVPHAL_ALPHA_PARAMS pAlphaParams;
    VPHAL_CSPACE        colorSpaceInput;
    VPHAL_CSPACE        colorSpaceOutput;
    uint32_t            chromaSitingInput;
    uint32_t            chromaSitingOutput;
    FeatureParamCsc     *next;                //!< pointe to new/next generated CSC params
};

class SwFilterCsc : public SwFilter
{
public:
    SwFilterCsc(VpInterface &vpInterface);
    virtual ~SwFilterCsc();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS &params, bool isInputSurf, int surfIndex);
    virtual MOS_STATUS Configure(PVP_SURFACE surfInput, VP_EXECUTE_CAPS caps);
    virtual MOS_STATUS Configure(VEBOX_SFC_PARAMS &params);
    virtual FeatureParamCsc &GetSwFilterParams();
    virtual SwFilter *Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE *inputSurf, VP_SURFACE *outputSurf);

private:
    FeatureParamCsc m_Params = {};
};

struct FeatureParamScaling : public FeatureParam
{
    VPHAL_SCALING_MODE          scalingMode;
    VPHAL_SCALING_PREFERENCE    scalingPreference;              //!< DDI indicate Scaling preference
    bool                        bDirectionalScalar = false;     //!< Vebox Directional Scalar
    RECT                        rcSrcInput;
    RECT                        rcDstInput;                     //!< Input dst rect without rotate being applied.
    bool                        bRotateNeeded;                  //!< Whether rotate SwFilter exists on SwFilterPipe.
    RECT                        rcMaxSrcInput;
    uint32_t                    dwWidthInput;
    uint32_t                    dwHeightInput;
    RECT                        rcSrcOutput;
    RECT                        rcDstOutput;
    RECT                        rcMaxSrcOutput;
    uint32_t                    dwWidthOutput;
    uint32_t                    dwHeightOutput;
    PVPHAL_COLORFILL_PARAMS     pColorFillParams;               //!< ColorFill - BG only
    PVPHAL_ALPHA_PARAMS         pCompAlpha;                     //!< Alpha for composited surfaces
    VPHAL_CSPACE                colorSpaceOutput;
    FeatureParamScaling        *next;                           //!< pointe to new/next generated scaling params
};

class SwFilterScaling : public SwFilter
{
public:
    SwFilterScaling(VpInterface &vpInterface);
    virtual ~SwFilterScaling();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS &params, bool isInputSurf, int surfIndex);
    virtual MOS_STATUS Configure(VEBOX_SFC_PARAMS &params);
    virtual FeatureParamScaling &GetSwFilterParams();
    virtual SwFilter *Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE *inputSurf, VP_SURFACE *outputSurf);

private:
    FeatureParamScaling m_Params = {};
};

struct FeatureParamRotMir : public FeatureParam
{
    VPHAL_ROTATION rotation;
    MOS_TILE_TYPE  tileOutput;
};

class SwFilterRotMir : public SwFilter
{
public:
    SwFilterRotMir(VpInterface &vpInterface);
    virtual ~SwFilterRotMir();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS &params, bool isInputSurf, int surfIndex);
    virtual MOS_STATUS Configure(VEBOX_SFC_PARAMS &params);
    virtual FeatureParamRotMir &GetSwFilterParams();
    virtual SwFilter *Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE *inputSurf, VP_SURFACE *outputSurf);

private:
    FeatureParamRotMir m_Params = {};
};

struct FeatureParamDenoise : public FeatureParam
{
    VPHAL_SAMPLE_TYPE    sampleTypeInput;
    VPHAL_DENOISE_PARAMS denoiseParams;
    uint32_t             widthAlignUnitInput;
    uint32_t             heightAlignUnitInput;
    uint32_t             heightInput;
};

class SwFilterDenoise : public SwFilter
{
public:
    SwFilterDenoise(VpInterface& vpInterface);
    virtual ~SwFilterDenoise();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS& params, bool isInputSurf, int surfIndex);
    virtual FeatureParamDenoise& GetSwFilterParams();
    virtual SwFilter* Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE* inputSurf, VP_SURFACE* outputSurf);

private:
    FeatureParamDenoise m_Params = {};
};

struct FeatureParamSte : public FeatureParam
{
    bool                bEnableSTE;
    uint32_t            dwSTEFactor;
};

class SwFilterSte : public SwFilter
{
public:
    SwFilterSte(VpInterface& vpInterface);
    virtual ~SwFilterSte();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS& params, bool isInputSurf, int surfIndex);
    virtual FeatureParamSte& GetSwFilterParams();
    virtual SwFilter* Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE* inputSurf, VP_SURFACE* outputSurf);

private:
    FeatureParamSte m_Params = {};
};

struct FeatureParamTcc : public FeatureParam
{
    bool                bEnableTCC;
    uint8_t             Red;
    uint8_t             Green;
    uint8_t             Blue;
    uint8_t             Cyan;
    uint8_t             Magenta;
    uint8_t             Yellow;
};

class SwFilterTcc : public SwFilter
{
public:
    SwFilterTcc(VpInterface& vpInterface);
    virtual ~SwFilterTcc();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS& params, bool isInputSurf, int surfIndex);
    virtual FeatureParamTcc& GetSwFilterParams();
    virtual SwFilter* Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE* inputSurf, VP_SURFACE* outputSurf);

private:
    FeatureParamTcc m_Params = {};
};

struct FeatureParamProcamp : public FeatureParam
{
    bool                bEnableProcamp;
    float               fBrightness;
    float               fContrast;
    float               fHue;
    float               fSaturation;
};

class SwFilterProcamp : public SwFilter
{
public:
    SwFilterProcamp(VpInterface& vpInterface);
    virtual ~SwFilterProcamp();
    virtual MOS_STATUS Clean();
    virtual MOS_STATUS Configure(VP_PIPELINE_PARAMS& params, bool isInputSurf, int surfIndex);
    virtual FeatureParamProcamp& GetSwFilterParams();
    virtual SwFilter* Clone();
    virtual bool operator == (SwFilter& swFilter);
    virtual MOS_STATUS Update(VP_SURFACE* inputSurf, VP_SURFACE* outputSurf);

private:
    FeatureParamProcamp m_Params = {};
};

class SwFilterSet
{
public:
    SwFilterSet();
    virtual ~SwFilterSet();

    MOS_STATUS AddSwFilter(SwFilter *swFilter);
    MOS_STATUS RemoveSwFilter(SwFilter *swFilter);
    MOS_STATUS Update(VP_SURFACE *inputSurf, VP_SURFACE *outputSurf);
    MOS_STATUS Clean();
    SwFilter *GetSwFilter(FeatureType type);
    bool IsEmpty()
    {
        return m_swFilters.empty();
    }

    std::vector<class SwFilterSet *> *GetLocation();
    void SetLocation(std::vector<class SwFilterSet *> *location);

private:
    std::map<FeatureType, SwFilter *> m_swFilters;
    // nullptr if it is unordered filters, otherwise, it's the pointer to m_OrderedFilters it belongs to.
    std::vector<class SwFilterSet *> *m_location = nullptr;
};

}
#endif // !__SW_FILTER_H__
