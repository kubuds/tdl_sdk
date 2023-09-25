#include "dms_landmark.hpp"
#include <core/core/cvai_errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <error_msg.hpp>
#include <iostream>
#include <iterator>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include "coco_utils.hpp"
#include "core/core/cvai_errno.h"
#include "core/cviai_types_mem.h"
#include "core/cviai_types_mem_internal.h"
#include "core/utils/vpss_helper.h"
#include "core_utils.hpp"
#include "cvi_sys.h"

namespace cviai {

DMSLandmarkerDet::DMSLandmarkerDet() : Core(CVI_MEM_DEVICE) {}

int DMSLandmarkerDet::onModelOpened() {
  for (size_t j = 0; j < getNumOutputTensor(); j++) {
    TensorInfo oinfo = getOutputTensorInfo(j);
    if (oinfo.tensor_name.find('x') != std::string::npos) {
      out_names_["simcc_x"] = oinfo.tensor_name;
    } else {
      out_names_["simcc_y"] = oinfo.tensor_name;
    }
  }
  if (out_names_.count("simcc_x") == 0 || out_names_.count("simcc_y") == 0) {
    return CVIAI_FAILURE;
  }
  return CVIAI_SUCCESS;
}

DMSLandmarkerDet::~DMSLandmarkerDet() {}

int DMSLandmarkerDet::setupInputPreprocess(std::vector<InputPreprecessSetup> *data) {
  if (data->size() != 1) {
    LOGE("DMSLandmarkerDet only has 1 input.\n");
    return CVIAI_ERR_INVALID_ARGS;
  }

  (*data)[0].factor[0] = 1 / 59.395;
  (*data)[0].factor[1] = 1 / 57.12;
  (*data)[0].factor[2] = 1 / 57.375;
  (*data)[0].mean[0] = 2.1179;
  (*data)[0].mean[1] = 2.0357;
  (*data)[0].mean[2] = 1.8044;
  (*data)[0].format = PIXEL_FORMAT_RGB_888_PLANAR;
  (*data)[0].use_quantize_scale = true;
  (*data)[0].rescale_type = RESCALE_NOASPECT;
  (*data)[0].keep_aspect_ratio = false;
  return CVIAI_SUCCESS;
}

int DMSLandmarkerDet::vpssPreprocess(VIDEO_FRAME_INFO_S *srcFrame, VIDEO_FRAME_INFO_S *dstFrame,
                                     VPSSConfig &vpss_config) {
  auto &vpssChnAttr = vpss_config.chn_attr;
  auto &factor = vpssChnAttr.stNormalize.factor;
  auto &mean = vpssChnAttr.stNormalize.mean;
  vpssChnAttr.stNormalize.bEnable = false;
  vpssChnAttr.stAspectRatio.enMode = ASPECT_RATIO_NONE;

  VPSS_CHN_SQ_RB_HELPER(&vpssChnAttr, srcFrame->stVFrame.u32Width, srcFrame->stVFrame.u32Height,
                        vpssChnAttr.u32Width, vpssChnAttr.u32Height, PIXEL_FORMAT_RGB_888_PLANAR,
                        factor, mean, false);
  int ret = mp_vpss_inst->sendFrame(srcFrame, &vpssChnAttr, &vpss_config.chn_coeff, 1);
  if (ret != CVI_SUCCESS) {
    LOGE("vpssPreprocess Send frame failed: %s!\n", get_vpss_error_msg(ret));
    return CVIAI_ERR_VPSS_SEND_FRAME;
  }

  ret = mp_vpss_inst->getFrame(dstFrame, 0, m_vpss_timeout);
  if (ret != CVI_SUCCESS) {
    LOGE("get frame failed: %s!\n", get_vpss_error_msg(ret));
    return CVIAI_ERR_VPSS_GET_FRAME;
  }
  return CVIAI_SUCCESS;
}

int DMSLandmarkerDet::inference(VIDEO_FRAME_INFO_S *srcFrame, cvai_face_t *facemeta) {
  std::vector<VIDEO_FRAME_INFO_S *> frames = {srcFrame};
  int ret = run(frames);
  if (ret != CVIAI_SUCCESS) {
    LOGE("FaceLandmarkerDet2 run inference failed\n");
    return ret;
  }

  CVI_SHAPE shape = getInputShape(0);

  outputParser(shape.dim[3], shape.dim[2], srcFrame->stVFrame.u32Width,
               srcFrame->stVFrame.u32Height, facemeta);
  model_timer_.TicToc("post");
  return CVIAI_SUCCESS;
}

void DMSLandmarkerDet::outputParser(const int image_width, const int image_height,
                                    const int frame_width, const int frame_height,
                                    cvai_face_t *facemeta) {
  TensorInfo oinfo_x = getOutputTensorInfo(out_names_["simcc_x"]);
  float *output_point_x = getOutputRawPtr<float>(oinfo_x.tensor_name);

  TensorInfo oinfo_y = getOutputTensorInfo(out_names_["simcc_y"]);
  float *output_point_y = getOutputRawPtr<float>(oinfo_y.tensor_name);

  CVI_AI_MemAllocInit(1, 68, facemeta);
  facemeta->width = frame_width;
  facemeta->height = frame_height;
  //   int hidden_dim = sizeof(output_point_x) / sizeof(output_point_x[0]);

  for (int i = 0; i < 68; i++) {
    float tmp_x = 0, tmp_y = 0;
    int idx_x = 0, idx_y = 0;
    for (int j = 0; j < 512; j++) {
      // x loc
      if (tmp_x < output_point_x[i * 512 + j]) {
        tmp_x = output_point_x[i * 512 + j];
        idx_x = j;
      }
      // y loc
      if (tmp_y < output_point_y[i * 512 + j]) {
        tmp_y = output_point_y[i * 512 + j];
        idx_y = j;
      }
    }
    float x = idx_x / 2.0 / 256.0 * frame_width;
    float y = idx_y / 2.0 / 256.0 * frame_height;
    facemeta->info[0].pts.x[i] = x;
    facemeta->info[0].pts.y[i] = y;
  }
  //   for (int i = 0; i < 5; i++) {
  //     float x = output_point_x[i] * frame_width;
  //     float y = output_point_y[i] * frame_height;
  //     facemeta->info[0].pts.x[i] = x;
  //     facemeta->info[0].pts.y[i] = y;
  //   }
}
// namespace cviai
}  // namespace cviai
