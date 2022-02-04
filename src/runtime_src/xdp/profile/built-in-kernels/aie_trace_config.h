
#ifndef AIE_TRACE_CONFIG_DOT_H
#define AIE_TRACE_CONFIG_DOT_H

namespace xdp {
namespace built_in {

  enum MetricSet { FUNCTIONS = 0,
                   PARTIAL_STALLS = 1,
                   ALL_STALLS = 2,
                   ALL = 3
                 } ;

  enum ErrorCode {
  } ;

  // This struct is used for input for the PS kernel.  It contains all of
  // the information gathered from the user controls in the xrt.ini file
  // and the information we can infer from the debug ip layout file.
  // The struct should be constructed and then transferred via a buffer object.
  //
  // Since this is transferred from host to device, it should have
  // a C-Style interface.
  struct InputConfiguration
  {
    static constexpr auto MAX_LENGTH = 256;
    char delayStr[MAX_LENGTH];
    char counterScheme[MAX_LENGTH];
    uint8_t metric;
    uint8_t userControl;
    uint8_t flush;
    uint8_t GMIO;

    // If GMIO is specified, then the PS kernel has to know all the
    // buffer addresses and sizes.
    static constexpr auto MAX_NUM_STREAMS = 16 ;
    uint64_t buffer_addresses[MAX_NUM_STREAMS];
    size_t buffer_sizes[MAX_NUM_STREAMS];
  };

  // This struct is used as output from the PS kernel.  It should be zeroed out
  // and passed as a buffer object to and from the PS kernel.  The PS kernel
  // will fill in the different values.
  //
  // Since this is transferred from host to device, it should have
  // a C-Style interface.
  struct OutputValues
  {
  };

} // end namespace built_in
} // end namespace xdp

#endif
