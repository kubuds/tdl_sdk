#pragma once
#include "core.hpp"
#include "core/face/cvai_face_types.h"

#include "anchor_generator.h"
#include "opencv2/opencv.hpp"

namespace cviai {

class RetinaFace final : public Core {
 public:
  RetinaFace();
  virtual ~RetinaFace();
  int inference(VIDEO_FRAME_INFO_S *srcFrame, cvai_face_t *meta);

 private:
  virtual int setupInputPreprocess(std::vector<InputPreprecessSetup> *data) override;
  virtual int onModelOpened() override;
  void outputParser(int image_width, int image_height, int frame_width, int frame_height,
                    cvai_face_t *meta);
  virtual bool allowExportChannelAttribute() const override { return true; }
  std::vector<int> m_feat_stride_fpn = {32, 16, 8};
  std::map<std::string, std::vector<anchor_box>> m_anchors;
  std::map<std::string, int> m_num_anchors;
};
}  // namespace cviai