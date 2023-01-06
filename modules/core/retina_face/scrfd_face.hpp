#ifndef FILE_SCRFD_HPP
#define FILE_SCRFD_HPP
#include "core.hpp"
#include "core/face/cvai_face_types.h"

#include "anchor_generator.h"

namespace cviai {

class ScrFDFace : public Core {
 public:
  ScrFDFace();
  virtual ~ScrFDFace();
  int inference(VIDEO_FRAME_INFO_S *srcFrame, cvai_face_t *meta);
  virtual bool allowExportChannelAttribute() const override { return true; }
  virtual void setModelThreshold(float threshold) override;

 private:
  virtual int setupInputPreprocess(std::vector<InputPreprecessSetup> *data) override;
  virtual int onModelOpened() override;
  void outputParser(int image_width, int image_height, int frame_width, int frame_height,
                    cvai_face_t *meta);
  std::vector<int> m_feat_stride_fpn;
  // std::map<std::string, std::vector<anchor_box>> m_anchors;
  // std::map<std::string, int> m_num_anchors;
  std::map<int, std::vector<std::vector<float>>> fpn_anchors_;
  std::map<int, std::map<std::string, std::string>>
      fpn_out_nodes_;  //{stride:{"box":"xxxx","score":"xxx","landmark":"xxxx"}}
  std::map<int, int> fpn_grid_anchor_num_;

  PROCESS process;
};
}  // namespace cviai
#endif