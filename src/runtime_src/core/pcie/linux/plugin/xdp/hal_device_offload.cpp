
#include <functional>

#include "hal_device_offload.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

namespace xdp::hal {
namespace device_offload {

  void load()
  {
    static xrt_core::module_loader
      xdp_hal_device_offload_loader("xdp_hal_device_offload_plugin",
                                    register_functions,
                                    warning_function,
                                    error_function);
  }

  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;
 
  void register_functions(void* handle)
  {
    using ftype = void (*)(void*);

    update_device_cb =
      reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateDeviceHAL"));
    flush_device_cb =
      reinterpret_cast<ftype>(xrt_core::dlsym(handle, "flushDeviceHAL"));
  }

  void warning_function()
  {
    // No warnings at this level
  }

  int error_function()
  {
    return 0 ;
  }

} // end namespace device_offload

  void flush_device(void* handle)
  {
    if (device_offload::flush_device_cb != nullptr)
    {
      device_offload::flush_device_cb(handle) ;
    }
  }

  void update_device(void* handle)
  {
    if (device_offload::update_device_cb != nullptr)
    {
      device_offload::update_device_cb(handle) ;
    }
  }
} // end namespace xdp::hal

