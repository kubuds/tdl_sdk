#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "evaluation/cviai_media.h"

// set preprocess and algorithm param for yolov7 detection
// if use official model, no need to change param
CVI_S32 init_param(const cviai_handle_t ai_handle) {
  // setup preprocess
  YoloPreParam preprocess_cfg = CVI_AI_Get_YOLO_Preparam(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7);

  for (int i = 0; i < 3; i++) {
    printf("asign val %d \n", i);
    preprocess_cfg.factor[i] = 0.003922;
    preprocess_cfg.mean[i] = 0.0;
  }
  preprocess_cfg.format = PIXEL_FORMAT_RGB_888_PLANAR;

  printf("setup yolov7 param \n");
  CVI_S32 ret = CVI_AI_Set_YOLO_Preparam(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, preprocess_cfg);
  if (ret != CVI_SUCCESS) {
    printf("Can not set Yolov5 preprocess parameters %#x\n", ret);
    return ret;
  }

  // setup yolo algorithm preprocess
  YoloAlgParam yolov7_param = CVI_AI_Get_YOLO_Algparam(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7);
  uint32_t *anchors = new uint32_t[18];
  uint32_t p_anchors[18] = {12, 16, 19,  36,  40,  28,  36,  75,  76,
                            55, 72, 146, 142, 110, 192, 243, 459, 401};
  memcpy(anchors, p_anchors, sizeof(p_anchors));
  yolov7_param.anchors = anchors;

  uint32_t *strides = new uint32_t[3];
  uint32_t p_strides[3] = {8, 16, 32};
  memcpy(strides, p_strides, sizeof(p_strides));
  yolov7_param.strides = strides;
  yolov7_param.cls = 80;

  printf("setup yolov7 algorithm param \n");
  ret = CVI_AI_Set_YOLO_Algparam(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, yolov7_param);
  if (ret != CVI_SUCCESS) {
    printf("Can not set Yolov5 algorithm parameters %#x\n", ret);
    return ret;
  }

  // set thershold
  CVI_AI_SetModelThreshold(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, 0.5);
  CVI_AI_SetModelNmsThreshold(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, 0.5);

  printf("yolov7 algorithm parameters setup success!\n");
  return ret;
}

int main(int argc, char *argv[]) {
  int vpssgrp_width = 1920;
  int vpssgrp_height = 1080;
  CVI_S32 ret = MMF_INIT_HELPER2(vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 1,
                                 vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 1);
  if (ret != CVIAI_SUCCESS) {
    printf("Init sys failed with %#x!\n", ret);
    return ret;
  }

  cviai_handle_t ai_handle = NULL;
  ret = CVI_AI_CreateHandle(&ai_handle);
  if (ret != CVI_SUCCESS) {
    printf("Create ai handle failed with %#x!\n", ret);
    return ret;
  }

  // change param of ppyoloe
  ret = init_param(ai_handle);

  std::string model_path = argv[1];
  std::string str_src_dir = argv[2];

  ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, model_path.c_str());
  if (ret != CVI_SUCCESS) {
    printf("open model failed %#x!\n", ret);
    return ret;
  }

  // set thershold
  CVI_AI_SetModelThreshold(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, 0.5);
  CVI_AI_SetModelNmsThreshold(ai_handle, CVI_AI_SUPPORTED_MODEL_YOLOV7, 0.5);

  std::cout << "model opened:" << model_path << std::endl;

  VIDEO_FRAME_INFO_S fdFrame;
  ret = CVI_AI_ReadImage(str_src_dir.c_str(), &fdFrame, PIXEL_FORMAT_RGB_888);
  std::cout << "CVI_AI_ReadImage done!\n";

  if (ret != CVI_SUCCESS) {
    std::cout << "Convert out video frame failed with :" << ret << ".file:" << str_src_dir
              << std::endl;
    // continue;
  }

  cvai_object_t obj_meta = {0};

  CVI_AI_Yolov7(ai_handle, &fdFrame, &obj_meta);

  for (uint32_t i = 0; i < obj_meta.size; i++) {
    printf("detect res: %f %f %f %f %f %d\n", obj_meta.info[i].bbox.x1, obj_meta.info[i].bbox.y1,
           obj_meta.info[i].bbox.x2, obj_meta.info[i].bbox.y2, obj_meta.info[i].bbox.score,
           obj_meta.info[i].classes);
  }

  CVI_VPSS_ReleaseChnFrame(0, 0, &fdFrame);
  CVI_AI_Free(&obj_meta);
  CVI_AI_DestroyHandle(ai_handle);

  return ret;
}