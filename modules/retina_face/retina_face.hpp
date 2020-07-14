#pragma once
#include "core.hpp"
#include "face/cvi_face_types.h"

#include "anchor_generator.h"

#include "opencv2/opencv.hpp"

namespace cviai {

class RetinaFace final : public Core {
 public:
  RetinaFace();
  virtual ~RetinaFace();
  int inference(VIDEO_FRAME_INFO_S *srcFrame, cvi_face_t *meta, int *face_count);

 private:
  void outputParser(float ratio, int image_width, int image_height,
                    std::vector<cvi_face_info_t> *bboxes_nms);
  void initFaceMeta(cvi_face_t *meta, int size);

  std::vector<int> m_feat_stride_fpn = {32, 16, 8};
  std::map<std::string, std::vector<anchor_box>> m_anchors;
  std::map<std::string, int> m_num_anchors;
};
}  // namespace cviai