#include "tracker.hpp"
#include "core/cviai_types_mem.h"
#include "core/cviai_types_mem_internal.h"

#include <string.h>

#define DEFAULT_DT_ZOOM_TRANS_RATIO 0.1f
#include <iostream>
namespace cviai {
namespace service {
int Tracker::registerId(const VIDEO_FRAME_INFO_S *frame, const int64_t &id, const float x,
                        const float y) {
  m_timestamp = frame->stVFrame.u64PTS;
  tracker_pts_t pts;
  pts.x = x;
  pts.y = y;
  m_tracker[id].push_back({m_timestamp, pts});

  auto it = m_tracker.begin();
  while (it != m_tracker.end()) {
    auto &vec = it->second;
    if ((m_timestamp - vec[vec.size() - 1].first) > m_deleteduration) {
      it = m_tracker.erase(it);
    } else {
      ++it;
    }
  }
  return CVI_SUCCESS;
}
int Tracker::getLatestPos(const int64_t &id, float *x, float *y) {
  auto it = m_tracker.find(id);
  if (it != m_tracker.end()) {
    auto &vec = it->second;
    *x = vec[vec.size() - 1].second.x;
    *y = vec[vec.size() - 1].second.y;
    return CVI_SUCCESS;
  }
  return CVI_FAILURE;
}
}  // namespace service
}  // namespace cviai