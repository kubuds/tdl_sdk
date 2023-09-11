#pragma once
#include <bitset>
#include "core.hpp"
#include "core/object/cvai_object_types.h"

namespace cviai {

class Yolov6 final : public Core {
 public:
  Yolov6();
  virtual ~Yolov6();
  int inference(VIDEO_FRAME_INFO_S *srcFrame, cvai_object_t *obj_meta);
  void set_param(YoloPreParam *p_preprocess_cfg, YoloAlgParam *p_alg_param);

 private:
  virtual int onModelOpened() override;
  int vpssPreprocess(VIDEO_FRAME_INFO_S *srcFrame, VIDEO_FRAME_INFO_S *dstFrame,
                     VPSSConfig &vpss_config) override;
  virtual int setupInputPreprocess(std::vector<InputPreprecessSetup> *data) override;
  void decode_bbox_feature_map(int stride, int anchor_idx, std::vector<float> &decode_box);
  void clip_bbox(int frame_width, int frame_height, cvai_bbox_t *bbox);
  cvai_bbox_t boxRescale(int frame_width, int frame_height, int width, int height,
                         cvai_bbox_t bbox);
  void postProcess(Detections &dets, int frame_width, int frame_height, cvai_object_t *obj_meta);
  void outputParser(const int image_width, const int image_height, const int frame_width,
                    const int frame_hegiht, cvai_object_t *obj_meta);

  YoloPreParam *p_preprocess_cfg_;
  YoloAlgParam *p_alg_param_;

  std::vector<int> strides;
  std::map<int, std::string> class_out_names;
  std::map<int, std::string> bbox_out_names;
};
}  // namespace cviai