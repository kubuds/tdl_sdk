#pragma once
#include <limits>
#include <memory>
#include <vector>
#include "anchors.hpp"
#include "core.hpp"
#include "core/object/cvai_object_types.h"

namespace cviai {

class MobileDetV2 final : public Core {
 public:
  enum class Model {
    d0,    // MobileDetV2-D0
    d1,    // MobileDetV2-D1
    d2,    // MobileDetV2-D2
    lite,  // MobileDetV2-Lite
  };

  struct ModelConfig {
    MobileDetV2::Model model;
    size_t min_level;
    size_t max_level;
    size_t num_scales;
    std::vector<std::pair<float, float>> aspect_ratios;
    float anchor_scale;

    size_t image_size;
    int num_classes;
    std::vector<int> strides;
    std::map<int, std::string> class_out_names;
    std::map<int, std::string> bbox_out_names;
    std::map<int, std::string> obj_max_names;
    std::map<int, float> class_dequant_thresh;
    std::map<int, float> bbox_dequant_thresh;
    float default_score_threshold;
    static ModelConfig create_config(MobileDetV2::Model model);
  };

  // TODO: remove duplicate struct
  struct object_detect_rect_t {
    float x1;
    float y1;
    float x2;
    float y2;
    float score;
    int label;
  };

  explicit MobileDetV2(MobileDetV2::Model model, float iou_thresh = 0.45);
  virtual ~MobileDetV2();
  int initAfterModelOpened(std::vector<initSetup> *data) override;
  int inference(VIDEO_FRAME_INFO_S *frame, cvai_object_t *meta, cvai_obj_det_type_e det_type);
  virtual void setModelThreshold(float threshold) override;

  // TODO: define in common header
  typedef std::shared_ptr<object_detect_rect_t> PtrDectRect;
  typedef std::vector<PtrDectRect> Detections;

 private:
  int vpssPreprocess(const std::vector<VIDEO_FRAME_INFO_S *> &srcFrames,
                     std::vector<std::shared_ptr<VIDEO_FRAME_INFO_S>> *dstFrames) override;
  void get_tensor_ptr_size(const std::string &tname, int8_t **ptr, size_t *size);
  void get_raw_outputs(std::vector<std::pair<int8_t *, size_t>> *cls_tensor_ptr,
                       std::vector<std::pair<int8_t *, size_t>> *objectness_tensor_ptr,
                       std::vector<std::pair<int8_t *, size_t>> *bbox_tensor_ptr);
  void generate_dets_for_each_stride(Detections *det_vec);
  void generate_dets_for_tensor(Detections *det_vec, float class_dequant_thresh,
                                float bbox_dequant_thresh, int8_t quant_thresh,
                                const int8_t *logits, const int8_t *objectness, int8_t *bboxes,
                                size_t class_tensor_size, const std::vector<AnchorBox> &anchors);
  std::vector<std::vector<AnchorBox>> m_anchors;
  ModelConfig m_model_config;
  float m_iou_threshold;

  // score threshold for quantized inverse threshold
  std::vector<int8_t> m_quant_inverse_score_threshold;
};

}  // namespace cviai
