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

#ifndef AIE_TRACE_X86_DOT_H
#define AIE_TRACE_X86_DOT_H

#include <cstdint>
#include <string>

#include "core/include/xrt/xrt_bo.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"

namespace xdp {

  // This implementation is for platforms like the VCK5000 or discovery
  // platform, where the host code runs on x86 and the AIE is not
  // directly accessible.
  class AieTrace_x86HostImpl : public AieTraceImpl
  {
  private:
    uint64_t numStreams;

    // Values from the xrt.ini file
    std::string delayStr;
    std::string counterScheme;
    std::string metricsStr;
    bool userControl;
    bool flush;

    // The buffer objects for PS kernel input and output
    xrt::bo pskernelInput;
    xrt::bo pskernelOutput;

    // Helper functions
    bool updateDatabase(void* handle);
    bool configurePL(void* handle);
    bool configureAIE(void* handle);

  public:
    explicit AieTrace_x86HostImpl(VPDatabase* db);
    AieTrace_x86HostImpl() = delete;
    virtual ~AieTrace_x86HostImpl() = default;

    virtual void updateDevice(void* handle) override;
    virtual void flushDevice(void* handle) override;
    virtual void finishFlushDevice(void* handle) override;
  };

} // end namespace xdp

#endif
