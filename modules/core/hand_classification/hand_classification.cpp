#include "hand_classification.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include "core/core/cvai_errno.h"
#include "core/cviai_types_mem.h"
#include "cvi_sys.h"
#include "rescale_utils.hpp"

#define R_SCALE (0.003922 / 0.229)
#define G_SCALE (0.003922 / 0.224)
#define B_SCALE (0.003922 / 0.225)
#define R_MEAN (0.485 / 0.229)
#define G_MEAN (0.456 / 0.224)
#define B_MEAN (0.406 / 0.225)
#define CROP_PCT 0.875
#define HAND_OUTNAME "output0_Gemm_dequant"

namespace cviai {

HandClassification::HandClassification() : Core(CVI_MEM_DEVICE) {}

HandClassification::~HandClassification() {}

int HandClassification::setupInputPreprocess(std::vector<InputPreprecessSetup> *data) {
  if (data->size() != 1) {
    LOGE("Hand classification only has 1 input.\n");
    return CVIAI_ERR_INVALID_ARGS;
  }
  (*data)[0].factor[0] = R_SCALE;
  (*data)[0].factor[1] = G_SCALE;
  (*data)[0].factor[2] = B_SCALE;
  (*data)[0].mean[0] = R_MEAN;
  (*data)[0].mean[1] = G_MEAN;
  (*data)[0].mean[2] = B_MEAN;
  (*data)[0].use_quantize_scale = true;
  (*data)[0].use_crop = true;
  (*data)[0].keep_aspect_ratio = false;  // do not keep aspect ratio,resize directly

  return CVIAI_SUCCESS;
}

int HandClassification::inference(VIDEO_FRAME_INFO_S *stOutFrame, cvai_object_t *meta) {
  uint32_t img_width = stOutFrame->stVFrame.u32Width;
  uint32_t img_height = stOutFrame->stVFrame.u32Height;
  for (uint32_t i = 0; i < meta->size; i++) {
    // rescale hand detect bbox to original frame coordinate
    if (meta->info[i].classes != 0) {  // not hand object
      continue;
    }
    cvai_object_info_t hand_info = info_rescale_c(img_width, img_height, *meta, i);

    int box_x1 = hand_info.bbox.x1;
    int box_y1 = hand_info.bbox.y1;
    uint32_t box_w = hand_info.bbox.x2 - hand_info.bbox.x1;
    uint32_t box_h = hand_info.bbox.y2 - hand_info.bbox.y1;

    // expand box with 1.2 scale
    // box_x1 = box_x1 - box_w * 0.1;
    // box_y1 = box_y1 - box_h * 0.1;
    // box_w = box_w * 1.2;
    // box_h = box_h * 1.2;

    if (box_x1 < 0) box_x1 = 0;
    if (box_y1 < 0) box_y1 = 0;
    if (box_x1 + box_w > img_width) {
      box_x1 = img_width - box_w;
    }
    if (box_y1 + box_h > img_height) {
      box_y1 = img_height - box_h;
    }
    if (box_x1 < 0) box_x1 = 0;
    if (box_y1 < 0) box_y1 = 0;

    CVI_AI_FreeCpp(&hand_info);

    m_vpss_config[0].crop_attr.enCropCoordinate = VPSS_CROP_RATIO_COOR;
    m_vpss_config[0].crop_attr.stCropRect = {box_x1, box_y1, box_w, box_h};

    std::vector<VIDEO_FRAME_INFO_S *> frames = {stOutFrame};

    int ret = run(frames);

    if (ret != CVIAI_SUCCESS) {
      LOGW("hand classification inference failed\n");
      return ret;
    }

    // std::string classesnames[6] = {"fist", "five", "gun", "ok", "other", "thumbUp"};
    std::string classesnames[4] = {"fist", "five", "none", "two"};

    TensorInfo oinfo = getOutputTensorInfo(0);
    int num_cls = oinfo.shape.dim[1];
    float *out_data = getOutputRawPtr<float>(oinfo.tensor_name);
    float score = *(std::max_element(out_data, out_data + 6));
    int score_index = -1;
    float maxscore = -1;
    float cls_score[num_cls] = {0};
    float sum_score = 0;
    for (int k = 0; k < num_cls; k++) {
      cls_score[k] = 1.0 / (1 + exp(-out_data[k]));
      sum_score += cls_score[k];
      if (cls_score[k] > maxscore) {
        maxscore = cls_score[k];
        score_index = k;
      }
    }
    for (int k = 0; k < num_cls; k++) {
      cls_score[k] = cls_score[k] / sum_score;
    }
    maxscore = cls_score[score_index];
    std::cout << "index:" << score_index << ",score:" << maxscore
              << ",detscore:" << hand_info.bbox.score << ",clsnum:" << num_cls << std::endl;
    if (maxscore < 0.2) {
      score_index = 2;
    }
    meta->info[i].bbox.score = score;
    // meta->info[i].classes = score_index;

    const std::string &classname = classesnames[score_index];
    strncpy(meta->info[i].name, classname.c_str(), classname.size());
  }

  return CVIAI_SUCCESS;
}

}  // namespace cviai
