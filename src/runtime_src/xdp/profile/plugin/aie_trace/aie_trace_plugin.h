/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#ifndef AIE_TRACE_PLUGIN_H
#define AIE_TRACE_PLUGIN_H

#include <cstdint>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  // AIE trace configurations can be done in different ways depending
  // on the platform.  For example, platforms like the VCK5000 or
  // discovery platform, where the host code runs on the x86 and the AIE
  // is not directly accessible, will require configuration be done via
  // PS kernel. 
  class AieTraceImpl
  {
  protected:
    VPDatabase* db = nullptr;
  public:
    explicit AieTraceImpl(VPDatabase* database) : db(database) {}
    AieTraceImpl() = delete;
    virtual ~AieTraceImpl() = default;

    virtual void updateDevice(void* handle) = 0;
    virtual void flushDevice(void* handle) = 0;
    virtual void finishFlushDevice(void* handle) = 0;

    virtual uint64_t getNumStreams() { return 0; }
    virtual uint64_t getDeviceId() { return 0; }
    virtual bool isRuntimeMetrics() { return false; }
  };

  class AieTracePlugin : public XDPPlugin
  {
  private:
    AieTraceImpl* implementation;

  public:
    XDP_EXPORT AieTracePlugin();
    XDP_EXPORT ~AieTracePlugin();
    XDP_EXPORT void updateAIEDevice(void* handle);
    XDP_EXPORT void flushAIEDevice(void* handle);
    XDP_EXPORT void finishFlushAIEDevice(void* handle);
    XDP_EXPORT virtual void writeAll(bool openNewFiles);
  };
    
}   
    
#endif
