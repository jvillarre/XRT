/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <functional>

#include "plugin/xdp/plugin_loader.h"
#include "plugin/xdp/profile_counters.h" 
#include "plugin/xdp/profile_trace.h"

#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/api/xclbin_int.h"

#include "xocl/core/command_queue.h"
#include "xocl/core/program.h"
#include "xocl/core/context.h"

namespace xdp {

namespace opencl_trace {
  // ******** OpenCL Host Trace Plugin *********

  // All of the function pointers that will be dynamically linked to
  //  callback functions on the XDP plugin side
  std::function<void (const char*,
		      unsigned long long int,
		      unsigned long long int)> function_start_cb ;
  std::function<void (const char*,
		      unsigned long long int,
		      unsigned long long int)> function_end_cb ;
  std::function<void (unsigned long long int,
		      unsigned long long int)> dependency_cb ;
  std::function<void (unsigned long long int, 
		      bool, 
		      unsigned long long int, 
		      const char*, 
		      size_t, 
		      bool)> read_cb ;
  std::function<void (unsigned long long int,
		      bool,
		      unsigned long long int,
		      const char*,
		      size_t,
		      bool)> write_cb ;
  std::function<void (unsigned long long int,
		      bool,
		      unsigned long long int,
		      const char*,
		      unsigned long long int,
		      const char*,
		      size_t,
		      bool)> copy_cb ;
  std::function<void (unsigned long long int,
		      bool,
		      const char*,
		      const char*,
		      const char*,
		      size_t,
		      size_t,
		      size_t,
		      size_t)> ndrange_cb ;

  void register_opencl_trace_functions(void* handle)
  {
    using func_type     = void (*)(const char*, unsigned long long int,
                                   unsigned long long int) ;
    using dep_type      = void (*)(unsigned long long int,
                                   unsigned long long int) ;
    using transfer_type = void (*)(unsigned long long int, bool,
                                   unsigned long long int, const char*, size_t,
                                   bool) ;
    using copy_type     = void (*)(unsigned long long int, bool,
                                   unsigned long long int, const char*,
                                   unsigned long long int, const char*,
                                   size_t, bool) ;
    using ndrange_type  = void (*)(unsigned long long int, bool,
                                   const char*, const char*, const char*,
                                   size_t, size_t, size_t, size_t) ;

    function_start_cb =
      reinterpret_cast<func_type>(xrt_core::dlsym(handle, "function_start"));
    function_end_cb =
      reinterpret_cast<func_type>(xrt_core::dlsym(handle, "function_end"));
    dependency_cb =
      reinterpret_cast<dep_type>(xrt_core::dlsym(handle, "add_dependency"));
    read_cb =
      reinterpret_cast<transfer_type>(xrt_core::dlsym(handle, "action_read"));
    write_cb =
      reinterpret_cast<transfer_type>(xrt_core::dlsym(handle, "action_write"));
    copy_cb =
      reinterpret_cast<copy_type>(xrt_core::dlsym(handle, "action_copy"));
    ndrange_cb =
      reinterpret_cast<ndrange_type>(xrt_core::dlsym(handle, "action_ndrange"));
  }

  void opencl_trace_warning_function()
  {
    // No warnings currently
  }

  void load()
  {
    static xrt_core::module_loader
      opencl_trace_loader("xdp_opencl_trace_plugin",
			  register_opencl_trace_functions,
			  opencl_trace_warning_function) ;
  }

} // end namespace opencl_trace

namespace device_offload {
  // ******** OpenCL Device Trace Plugin *********

  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;

  void register_device_offload_functions(void* handle) 
  {
    using cb_type = void (*)(void*) ;

    update_device_cb =
      reinterpret_cast<cb_type>(xrt_core::dlsym(handle, "updateDeviceOpenCL"));
    flush_device_cb =
      reinterpret_cast<cb_type>(xrt_core::dlsym(handle, "flushDeviceOpenCL"));
  }

  void device_offload_warning_function()
  {
    // No warnings at this level
  }

  void load()
  {
    static xrt_core::module_loader 
      xdp_device_offload_loader("xdp_device_offload_plugin",
				register_device_offload_functions,
				device_offload_warning_function) ;
  }

} // end namespace device_offload
} // end namespace xdp

namespace xocl::profile {

    // ******** OpenCL API Trace Callbacks *********
    OpenCLAPILogger::OpenCLAPILogger(const char* function) :
      OpenCLAPILogger(function, 0)
    {
    }

    OpenCLAPILogger::OpenCLAPILogger(const char* function, uint64_t address) :
      m_funcid(0), m_name(function), m_address(address)
    {
      // Use the OpenCL API logger as the hook to load all of the OpenCL
      //  level XDP plugins.  Once loaded, they are completely independent,
      //  but this provides us a common place where all OpenCL applications
      //  can safely load them.
      static bool s_load_plugins = xdp::plugins::load() ;

      // Log the trace for this function
      if (xdp::opencl_trace::function_start_cb && s_load_plugins) {
        m_funcid = xrt_core::utils::issue_id() ;
	xdp::opencl_trace::function_start_cb(m_name, m_address, m_funcid) ;
      }

      // Log the stats for this function
      if (counter_function_start_cb)
      {
	bool isOOO = false ;
	if (address != 0)
	{
	  auto command_queue = reinterpret_cast<cl_command_queue>(address) ;
	  auto xqueue = xocl::xocl(command_queue) ;
	  isOOO = xqueue->get_properties() & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE ;
	}
	counter_function_start_cb(m_name, address, isOOO) ;
      }
    }

    OpenCLAPILogger::~OpenCLAPILogger()
    {
      if (xdp::opencl_trace::function_end_cb)
	xdp::opencl_trace::function_end_cb(m_name, m_address, m_funcid) ;

      if (counter_function_end_cb)
	counter_function_end_cb(m_name) ;
    }

    // ******** OpenCL Host Trace Callbacks *********
    void log_dependency(uint64_t id, uint64_t dependency)
    {
      if (xdp::opencl_trace::dependency_cb)
	xdp::opencl_trace::dependency_cb(static_cast<unsigned long long int>(id),
		      static_cast<unsigned long long int>(dependency)) ;
    }

    //static std::map<uint64_t, uint64_t> XRTIdToXDPId ;

    std::function<void (xocl::event*, cl_int)>
    action_read(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status)
             {
	       if (!xdp::opencl_trace::read_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;
	       
	       // Before we cross over to XDP, collect all the 
	       //  information we need from the event
	       auto xmem = xocl::xocl(buffer) ;
	       auto ext_flags = xmem->get_ext_flags() ;
	       bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

	       // For start events, dig in and find all the information
	       //  necessary to show the tooltip and send it.
	       if (status == CL_RUNNING)
	       {
		 //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;

		 // Get memory bank information
		 uint64_t address = 0 ;
		 std::string bank = "Unknown" ;
		 try { 
		   xmem->try_get_address_bank(address, bank) ;
		 }
		 catch (const xocl::error& /*e*/)
		 {
		 }

		 // Perform the callback
		 xdp::opencl_trace::read_cb(static_cast<unsigned long long int>(e->get_uid()),
			 true,
			 static_cast<unsigned long long int>(address),
			 bank.c_str(),
			 xmem->get_size(),
                         isP2P) ;
	       }
	       // For end events, just send the minimal information
	       else if (status == CL_COMPLETE)
	       {
		 xdp::opencl_trace::read_cb(static_cast<unsigned long long int>(e->get_uid()),
			 false,
			 0,
			 nullptr,
			 0,
                         isP2P) ;
	       }
             } ; 
    }

    std::function<void (xocl::event*, cl_int)>
    action_write(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status)
	     {
	       if (!xdp::opencl_trace::write_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       // Before we cross over to XDP, collect all the 
	       //  information we need from the event
	       auto xmem = xocl::xocl(buffer) ;
	       auto ext_flags = xmem->get_ext_flags() ;
	       bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

	       // For start events, dig in and find all the information
	       //  necessary to show the tooltip and send it.
	       if (status == CL_RUNNING)
	       {
		 //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;

		 // Get memory bank information
		 uint64_t address = 0 ;
		 std::string bank = "Unknown" ;
		 try { 
		   xmem->try_get_address_bank(address, bank) ;
		 }
		 catch (const xocl::error& /*e*/)
		 {
		 }

		 // Perform the callback
		 xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			  true,
			  static_cast<unsigned long long int>(address),
			  bank.c_str(),
			  xmem->get_size(),
			  isP2P) ;
	       }
	       // For end events, just send the minimal information
	       else if (status == CL_COMPLETE)
	       {
		 xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			  false,
			  0,
			  nullptr,
			  0,
			  isP2P) ;
	       }

	     } ;
    }

    std::function<void (xocl::event*, cl_int)>
    action_map(cl_mem buffer, cl_map_flags flags)
    {
      return [buffer, flags](xocl::event* e, cl_int status)
	     {
	       if (!xdp::opencl_trace::read_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       // Ignore if mapping an invalidated region
	       if (flags & CL_MAP_WRITE_INVALIDATE_REGION) return ;

	       auto xmem = xocl::xocl(buffer) ;
	       auto queue = e->get_command_queue() ;
	       auto device = queue->get_device() ;
	       
	       // Ignore if the buffer is *not* resident on the device
	       if (!(xmem->is_resident(device))) return ;

	       if (status == CL_RUNNING)
	       {
		 //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;

		 uint64_t address = 0 ;
		 std::string bank = "Unknown" ;
		 try {
		   xmem->try_get_address_bank(address, bank) ;
		 }
		 catch (const xocl::error& /*e*/)
		 {
		 }

		 // Perform the callback
		 xdp::opencl_trace::read_cb(static_cast<unsigned long long int>(e->get_uid()),
			 true,
			 static_cast<unsigned long long int>(address),
			 bank.c_str(),
			 xmem->get_size(),
                         false) ; // isP2P
	       }
	       else if (status == CL_COMPLETE)
	       {
		 xdp::opencl_trace::read_cb(static_cast<unsigned long long int>(e->get_uid()),
			 false,
			 0,
			 nullptr,
			 0,
			 false) ; // isP2P
	       }
	     } ;
    }

    std::function<void (xocl::event*, cl_int)>
    action_migrate(cl_mem mem0, cl_mem_migration_flags flags)
    {
      // Don't do anything for this migration
      if (flags & CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED)
      {
	return [](xocl::event*, cl_int) { } ;
      }

      // Migrate actions could be either a read or a write.
      if (flags & CL_MIGRATE_MEM_OBJECT_HOST)
      {
	// Read
	return [mem0](xocl::event* e, cl_int status)
	       {
		 if (!xdp::opencl_trace::read_cb) return ;
		 if (status != CL_RUNNING && status != CL_COMPLETE) return ;

		 // Before we cross over to XDP, collect all the 
		 //  information we need from the event
		 auto xmem = xocl::xocl(mem0) ;

		 // For start events, dig in and find all the information
		 //  necessary to show the tooltip and send it.
		 if (status == CL_RUNNING)
		 {
		   //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;
		   // Get memory bank information
		   uint64_t address = 0 ;
		   std::string bank = "Unknown" ;
		   try { 
		     xmem->try_get_address_bank(address, bank) ;
		   }
		   catch (const xocl::error& /*e*/)
		   {
		   }

		   // Perform the callback
		   xdp::opencl_trace::read_cb(static_cast<unsigned long long int>(e->get_uid()),
			   true,
			   static_cast<unsigned long long int>(address),
			   bank.c_str(),
			   xmem->get_size(),
			   false) ; // isP2P
		 }
		 // For end events, just send the minimal information
		 else if (status == CL_COMPLETE)
		 {
		   xdp::opencl_trace::read_cb(static_cast<unsigned long long int>(e->get_uid()),
			   false,
			   0,
			   nullptr,
			   0,
			   false) ; // isP2P
		 }
	       } ;
      }
      else
      {
	// Write
	return [mem0](xocl::event* e, cl_int status)
	       {
		 if (!xdp::opencl_trace::write_cb) return ;
		 if (status != CL_RUNNING && status != CL_COMPLETE) return ;

		 // Before we cross over to XDP, collect all the 
		 //  information we need from the event
		 auto xmem = xocl::xocl(mem0) ;

		 // For start events, dig in and find all the information
		 //  necessary to show the tooltip and send it.
		 if (status == CL_RUNNING)
		 {
		   //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;
		   // Get memory bank information
		   uint64_t address = 0 ;
		   std::string bank = "Unknown" ;
		   try { 
		     xmem->try_get_address_bank(address, bank) ;
		   }
		   catch (const xocl::error& /*e*/)
		   {
		   }
		   
		   // Perform the callback
		   xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			    true,
			    static_cast<unsigned long long int>(address),
			    bank.c_str(),
			    xmem->get_size(),
			    false) ; // isP2P
		 }
		 // For end events, just send the minimal information
		 else if (status == CL_COMPLETE)
		 {
		   xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			    false,
			    0,
			    nullptr,
			    0,
			    false) ; // isP2P
		 }
	       } ;
      }
    }

    std::function<void (xocl::event*, cl_int)>
    action_ndrange_migrate(cl_event event, cl_kernel kernel)
    {
      auto xevent = xocl::xocl(event) ;
      auto xkernel = xocl::xocl(kernel) ;

      auto queue = xevent->get_command_queue() ;
      auto device = queue->get_device() ;

      cl_mem mem0 = nullptr ;

      // See how many of the arguments will be migrated and mark them
      for (auto& arg : xkernel->get_xargument_range())
      {
	if (auto mem = arg->get_memory_object())
	{
	  /*
	  if (arg->is_progvar() && 
	      arg->get_address_qualifier() == CL_KERNEL_ARG_ADDRESS_GLOBAL)
	    continue ;
	  else 
	  */
	  if (mem->is_resident(device))
	    continue ;
	  else if (!(mem->get_flags() & 
		     (CL_MEM_WRITE_ONLY|CL_MEM_HOST_NO_ACCESS)))
	  {
	    mem0 = mem ;
	  }

	}
      }
      
      if (mem0 == nullptr)
      {
	return [](xocl::event*, cl_int)
	       {
	       } ;
      }

      return [mem0](xocl::event* e, cl_int status)
	     {
	       if (!xdp::opencl_trace::write_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

		 // Before we cross over to XDP, collect all the 
		 //  information we need from the event
		 auto xmem = xocl::xocl(mem0) ;

		 // For start events, dig in and find all the information
		 //  necessary to show the tooltip and send it.
		 if (status == CL_RUNNING)
		 {
		   //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;
		   // Get memory bank information
		   uint64_t address = 0 ;
		   std::string bank = "Unknown" ;
		   try { 
		     xmem->try_get_address_bank(address, bank) ;
		   }
		   catch (const xocl::error& /*e*/)
		   {
		   }
		   
		   // Perform the callback
		   xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			    true,
			    static_cast<unsigned long long int>(address),
			    bank.c_str(),
			    xmem->get_size(),
			    false) ; // isP2P
		 }
		 // For end events, just send the minimal information
		 else if (status == CL_COMPLETE)
		 {
		   xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			    false,
			    0,
			    nullptr,
			    0,
			    false) ; // isP2P
		 }
	     } ;
    }

    std::function<void (xocl::event*, cl_int)>
    action_ndrange(cl_event event, cl_kernel kernel)
    {
      auto xevent  = xocl::xocl(event) ;
      auto xkernel = xocl::xocl(kernel) ;

      auto xcontext = xevent->get_execution_context() ;
      auto workGroupSize = xkernel->get_wg_size() ;
      auto device = xevent->get_command_queue()->get_device() ;
      auto xclbin = device->get_xrt_xclbin();
      
      std::string deviceName = device->get_name() ;
      std::string kernelName = xkernel->get_name() ;
      std::string binaryName = xrt_core::xclbin_int::get_project_name(xclbin);

      size_t localWorkDim[3] = {0, 0, 0} ;
      range_copy(xkernel->get_compile_wg_size_range(), localWorkDim) ;
      if (localWorkDim[0] == 0 &&
	  localWorkDim[1] == 0 &&
	  localWorkDim[2] == 0)
      {
	std::copy(xcontext->get_local_work_size(),
		  xcontext->get_local_work_size() + 3,
		  localWorkDim) ;
      }

      return [workGroupSize, deviceName, kernelName, binaryName, localWorkDim](xocl::event* e, cl_int status) 
	     {
	       if (!xdp::opencl_trace::ndrange_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       if (status == CL_RUNNING)
	       {
		 //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;

		 xdp::opencl_trace::ndrange_cb(static_cast<unsigned long long int>(e->get_uid()),
			    true,
			    deviceName.c_str(),
			    binaryName.c_str(),
			    kernelName.c_str(),
			    localWorkDim[0],
			    localWorkDim[1],
			    localWorkDim[2],
			    workGroupSize) ;
	       }
	       else if (status == CL_COMPLETE)
	       {
		 xdp::opencl_trace::ndrange_cb(static_cast<unsigned long long int>(e->get_uid()),
			    false,
			    deviceName.c_str(),
			    binaryName.c_str(),
			    kernelName.c_str(),
			    0,
			    0,
			    0,
			    0) ;
	       }

	     } ;
    }

    std::function<void (xocl::event*, cl_int)>
    action_unmap(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status)
	     {
	       if (!xdp::opencl_trace::write_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       auto xmem = xocl::xocl(buffer) ;

	       // If P2P buffer, don't mark anything
	       if (xmem->no_host_memory()) return ;

	       // If buffer is not resident on device, don't mark anything
	       auto queue = e->get_command_queue() ;
	       auto device = queue->get_device() ;
	       if (!(xmem->is_resident(device))) return ;

	       if (status == CL_RUNNING)
	       {
		 //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;
		 // Get memory bank information
		 uint64_t address = 0 ;
		 std::string bank = "Unknown" ;
		 try { 
		   xmem->try_get_address_bank(address, bank) ;
		 }
		 catch (const xocl::error& /*e*/)
		 {
		 }

		 // Perform the callback
		 xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			  true,
			  static_cast<unsigned long long int>(address),
			  bank.c_str(),
			  xmem->get_size(),
			  false) ; // isP2P
	       }
	       else if (status == CL_COMPLETE)
	       {
		 xdp::opencl_trace::write_cb(static_cast<unsigned long long int>(e->get_uid()),
			  false,
			  0,
			  nullptr,
			  0,
			  false) ; // isP2P
	       }
	     } ;
    }

    std::function<void (xocl::event*, cl_int)>
    action_copy(cl_mem src_buffer, cl_mem dst_buffer)
    {      
      return [src_buffer, dst_buffer](xocl::event* e, cl_int status)
	     {
	       if (!xdp::opencl_trace::copy_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       auto xSrcMem = xocl::xocl(src_buffer) ;
	       auto xDstMem = xocl::xocl(dst_buffer) ;

	       auto srcFlags = xSrcMem->get_ext_flags() ;
	       auto dstFlags = xDstMem->get_ext_flags() ;

	       bool isP2P = (srcFlags & XCL_MEM_EXT_P2P_BUFFER) ||
		 (dstFlags & XCL_MEM_EXT_P2P_BUFFER) ;

	       if (status == CL_RUNNING)
	       {
		 //XRTIdToXDPId[e->get_uid()] = xrt_core::utils::issue_id() ;
		 // Get memory bank information
		 uint64_t srcAddress = 0;
		 uint64_t dstAddress = 0 ;
		 std::string srcBank = "Unknown" ;
		 std::string dstBank = "Unknown" ;

		 try {
		   xSrcMem->try_get_address_bank(srcAddress, srcBank) ;
		 }
		 catch (const xocl::error& /*e*/)
		 {
		 }
		 
		 try {
		   xDstMem->try_get_address_bank(dstAddress, dstBank) ;
		 }
		 catch (const xocl::error& /* e */)
		 {
		 }

		 xdp::opencl_trace::copy_cb(static_cast<unsigned long long int>(e->get_uid()),
			 true,
			 static_cast<unsigned long long int>(srcAddress),
			 srcBank.c_str(),
			 static_cast<unsigned long long int>(dstAddress),
			 dstBank.c_str(),
			 xSrcMem->get_size(),
			 isP2P);
	       }
	       else if (status == CL_COMPLETE)
	       {
		 xdp::opencl_trace::copy_cb(static_cast<unsigned long long int>(e->get_uid()),
			 false,
			 0,
			 nullptr,
			 0,
			 nullptr,
			 0,
                         isP2P) ;
	       }
	     } ;
    }

    // ******** OpenCL Device Trace Callbacks *********
    void flush_device(xrt_xocl::device* handle)
    {
      if (xdp::device_offload::flush_device_cb)
	xdp::device_offload::flush_device_cb(handle) ;
    }

    void update_device(xrt_xocl::device* handle)
    {
      if (xdp::device_offload::update_device_cb)
	xdp::device_offload::update_device_cb(handle) ;
    }

} // end namespace xocl::profile
