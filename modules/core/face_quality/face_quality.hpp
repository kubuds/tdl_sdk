#pragma once
#include <cvi_comm_vb.h>
#include "core.hpp"
#include "core/face/cvai_face_types.h"

#include "opencv2/opencv.hpp"

namespace cviai {

class FaceQuality final : public Core {
 public:
  FaceQuality();
  virtual ~FaceQuality();
  int inference(VIDEO_FRAME_INFO_S *frame, cvai_face_t *meta, bool *skip);

  /* TOOL CODE */
  int getAlignedFace(VIDEO_FRAME_INFO_S *srcFrame, VIDEO_FRAME_INFO_S *dstFrame,
                     cvai_face_info_t *face_info);

 private:
  virtual int setupInputPreprocess(std::vector<InputPreprecessSetup> *data) override;
  virtual int onModelOpened() override;

  VB_BLK m_gdc_blk = (VB_BLK)-1;
  VIDEO_FRAME_INFO_S m_wrap_frame;
};
}  // namespace cviai
