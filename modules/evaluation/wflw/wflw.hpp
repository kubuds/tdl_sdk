#pragma once
#include "core/face/cvai_face_types.h"
#include "core/utils/vpss_helper.h"
#include "face_utils.hpp"

#include <string>
#include <vector>

namespace cviai {
namespace evaluation {

class wflwEval {
 public:
  wflwEval(const char *fiilepath);
  int getEvalData(const char *fiilepath);
  uint32_t getTotalImage();
  void getImage(const int index, std::string *name);
  void insertPoints(const int index, const cvai_pts_t points, const int width, const int height);
  void distance();

 private:
  std::vector<std::string> m_img_name;
  std::vector<std::vector<float>> m_img_points;
  std::vector<std::vector<float>> m_result_points;
};
}  // namespace evaluation
}  // namespace cviai