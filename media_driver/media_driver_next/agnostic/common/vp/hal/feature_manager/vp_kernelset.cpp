/* Copyright (c) 2020, Intel Corporation
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
//! \file     vp_render_kernelset.cpp
//! \brief    The header file of the base class of kernel set
//! \details  The kernel set will include kernel generation from binary.
//!           It's responsible for setting up HW states and generating the SFC
//!           commands.
//!
#include "vp_kernelset.h"

using namespace vp;

VpKernelSet::VpKernelSet(PVP_MHWINTERFACE hwInterface) :
    m_hwInterface(hwInterface)
{
    m_kernelPool = hwInterface->m_vpPlatformInterface->GetKernel();
}

MOS_STATUS VpKernelSet::GetKernelInfo(uint32_t kuid, uint32_t& size, void*& kernel)
{
    Kdll_State* kernelState = m_kernelPool->GetKdllState();

    if (kernelState)
    {
        size   = kernelState->ComponentKernelCache.pCacheEntries[kuid].iSize;
        kernel = kernelState->ComponentKernelCache.pCacheEntries[kuid].pBinary;
        return MOS_STATUS_SUCCESS;
    }
    else
    {
        VP_PUBLIC_ASSERTMESSAGE("Kernel State not inplenmented, return error");
        return MOS_STATUS_UNINITIALIZED;
    }
}
