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

#include <string>

#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"

#ifdef EDGE_BUILD
#include "xdp/profile/plugin/aie_trace/edge/aie_trace_edge.h"
#else
#include "xdp/profile/plugin/aie_trace/x86/aie_trace_x86.h"
#endif

#include "xdp/profile/plugin/vp_base/info.h"
//#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"

namespace xdp {

  AieTracePlugin::AieTracePlugin() : XDPPlugin()
  {
    db->registerPlugin(this);
    db->registerInfo(info::aie_trace);

#ifdef EDGE_BUILD
    implementation = new AieTrace_edgeHostImpl(db);
#else
    implementation = new AieTrace_x86HostImpl(db);
#endif
  }

  AieTracePlugin::~AieTracePlugin()
  {
    if (implementation)
      delete implementation;
  }

  void AieTracePlugin::updateAIEDevice(void* handle)
  {
    if (handle == nullptr)
      return;

    if (implementation) {
      implementation->updateDevice(handle);

      // Add all the writers

      if (implementation->isRuntimeMetrics()) {
        // This code needs to get the actual metric set so we can
        //  configure it.  I'm not sure yet what is needed for this
      }

      for (uint64_t n = 0; n < implementation->getNumStreams(); ++n) {
        auto deviceId = implementation->getDeviceId();
	std::string fileName = "aie_trace_" + std::to_string(deviceId) + "_" + std::to_string(n) + ".txt";
        VPWriter* writer = new AIETraceWriter(fileName.c_str(), deviceId, n,
                                              "", // version
                                              "", // creation time
                                              "", // xrt version
                                              ""); // tool version
        writers.push_back(writer);
        db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(),
                                          "AIE_EVENT_TRACE");

      }
    }
  }

  void AieTracePlugin::flushAIEDevice(void* handle)
  {
    if (implementation)
      implementation->flushDevice(handle);
  }

  void AieTracePlugin::finishFlushAIEDevice(void* handle)
  {
    if (implementation)
      implementation->finishFlushDevice(handle);
  }

  void AieTracePlugin::writeAll(bool openNewFiles)
  {
    XDPPlugin::endWrite(openNewFiles);
  }

} // end namespace xdp
