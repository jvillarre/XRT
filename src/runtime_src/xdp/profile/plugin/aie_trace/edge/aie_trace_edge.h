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

#ifndef AIE_TRACE_EDGE_DOT_H
#define AIE_TRACE_EDGE_DOT_H

#include <cstdint>
#include <set>
#include <vector>

#include "core/edge/common/aie_parser.h"
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"

extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  // This implementation is for platforms like the VCK190, where the host
  // code runs on the PS and the AIE is directly accessible
  class AieTrace_edgeHostImpl : public AieTraceImpl
  {
  private:
    bool continuousTrace;
    uint64_t offloadIntervalms;

  public:
    explicit AieTrace_edgeHostImpl(VPDatabase* db);
    AieTrace_edgeHostImpl() = delete;
    virtual ~AieTrace_edgeHostImpl() = default;

    virtual void updateDevice(void* handle) override;
    virtual void flushDevice(void* handle) override;
    virtual void finishFlushDevice(void* handle) override;
  };

} // end namespace xdp

#endif
