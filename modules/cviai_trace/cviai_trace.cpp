#include "cviai_trace.hpp"
// clang-format off
#ifdef ENABLE_TRACE
  #if __GNUC__ >= 7
  PERFETTO_TRACK_EVENT_STATIC_STORAGE();
  #endif
#endif
// clang-format on