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

#include <cstring>
#include <string>

#include "core/common/config_reader.h"
#include "core/include/experimental/xrt_pskernel.h"
#include "core/pcie/linux/shim.h"
#include "xdp/profile/built-in-kernels/aie_trace_config.h"
#include "xdp/profile/plugin/aie_trace/x86/aie_trace_x86.h"

namespace xdp {

  AieTrace_x86HostImpl::AieTrace_x86HostImpl(VPDatabase* db)
    : AieTraceImpl(db)
    , numStreams(0)
    , pskernelInput()
    , pskernelOutput()
  {
    delayStr      = xrt_core::config::get_aie_trace_start_delay();
    counterScheme = xrt_core::config::get_aie_trace_counter_scheme();
    metricsStr    = xrt_core::config::get_aie_trace_metrics();
    userControl   = xrt_core::config::get_aie_trace_user_control();
    flush         = xrt_core::config::get_aie_trace_flush();
  }

  bool AieTrace_x86HostImpl::updateDatabase(void* handle)
  {
    constexpr int MAX_PATH = 512;
    char pathBuf[MAX_PATH] = {0};
    xclGetDebugIPlayoutPath(handle, pathBuf, MAX_PATH-1);

    std::string path(pathBuf);
    if (path == "")
      return false;

    uint64_t deviceId = db->addDevice(path);
    if (!(db->getStaticInfo().isDeviceReady(deviceId))) {
      db->getStaticInfo().updateDevice(deviceId, handle);

      struct xclDeviceInfo2 info;
      if (xclGetDeviceInfo2(handle, &info) == 0)
        db->getStaticInfo().setDeviceName(deviceId, std::string(info.mName));
    }
    return true;
  }

  bool AieTrace_x86HostImpl::configurePL(void* handle)
  {
    return true;
  }

  bool AieTrace_x86HostImpl::configureAIE(void* handle)
  {
    // The way the AIE is configured is via PS kernel.  This function
    // is responsible for setting up the input parameters, launching the
    // PS kernel, and then collecting the output.
    xdp::built_in::InputConfiguration input = {0};

    strncpy(input.delayStr, delayStr.c_str(), xdp::built_in::InputConfiguration::MAX_LENGTH - 1);
    strncpy(input.counterScheme, counterScheme.c_str(), xdp::built_in::InputConfiguration::MAX_LENGTH - 1);
    if (metricsStr == "functions")
      input.metric = static_cast<uint8_t>(xdp::built_in::FUNCTIONS);
    else if (metricsStr == "functions_partial_stalls")
      input.metric = static_cast<uint8_t>(xdp::built_in::PARTIAL_STALLS);
    else if (metricsStr == "functions_all_stalls")
      input.metric = static_cast<uint8_t>(xdp::built_in::ALL_STALLS);
    else if (metricsStr == "all")
      input.metric = static_cast<uint8_t>(xdp::built_in::ALL);
    else
      input.metric = static_cast<uint8_t>(xdp::built_in::FUNCTIONS);
    if (userControl)
      input.userControl = 1;
    if (flush)
      input.flush = 1;

    // Create a buffer object for the input parameters of the PS kernel
    pskernelInput = xrt::bo{handle, &input, sizeof(input), xrt::bo::flags::normal,0 };
    pskernelOutput = xrt::bo{};
    
    // The name of the PS kernel is "configure" and it should be available
    // in the current xclbin.  As a pre-built, it should be visible in
    // every xclbin.
    xrt::pskernel configure;

    // Call the ps kernel
    configure(pskernelInput, pskernelOutput); 

    return true;
  }

  void AieTrace_x86HostImpl::updateDevice(void* handle)
  {
    // We should have been passed an xrtDeviceHandle
    if (handle == nullptr)
      return;

    // The xclbin has been loaded, so grab all the data and keep it in our db
    if (updateDatabase(handle) == false)
      return;

    // Configure the PL portions of event trace
    if (configurePL(handle) == false)
      return;

    // Use the PS kernel to configure AIE
    if (configureAIE(handle) == false)
      return;
  }

  void AieTrace_x86HostImpl::flushDevice(void* handle)
  {
    // Sync the buffers from the device to the x86
  }

  void AieTrace_x86HostImpl::finishFlushDevice(void* handle)
  {
  }

} // end namespace xdp
