/**
 * Copyright (C) 2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#define XDP_SOURCE

#include "core/edge/user/shim.h"
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/plugin/aie_trace/edge/aie_trace_edge.h"

extern "C" {
#include <xaiengine.h>
}

// Anonymous namespace for helper functions
namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr;
    auto aieArray = drv->getAieArray();
    if (!aieArray)
      return nullptr;
    return aieArray->getDevInst();
  }

  static void* allocateAieDevice(void* devHandle)
  {
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle));
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false);
  }

  static void* deallocateAieDevice(void* aieDevice)
  {
    xaiefal::XAieDev* object = static_cast<xaiefal::XAieDev*>(aieDevice);
    if (object != nullptr)
      delete object;
  }

} // end anonymous namespace

namespace xdp {

  using severity_level = xrt_core::message::serverity_level;
  using module_type = xrt_core::edge::aie::module_type;

  AieTrace_edgeHostImpl::AieTrace_edgeHostImpl(VPDatabase* db)
    : AieTraceImpl(db)
    , continuousTrace(false)
    , offloadIntervalms(0)
  {
  }

  void AieTrace_edgeHostImpl::updateDevice(void* handle)
  {
  }

  void AieTrace_edgeHostImpl::flushDevice(void* handle)
  {
  }

  void AieTrace_edgeHostImpl::finishFlushDevice(void* handle)
  {
  }

} // end namespace xdp
