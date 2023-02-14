#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/utils/vpss_helper.h"
#include "cviai.h"

int ReleaseImage(VIDEO_FRAME_INFO_S *frame) {
  CVI_S32 ret = CVI_SUCCESS;
  if (frame->stVFrame.u64PhyAddr[0] != 0) {
    ret = CVI_SYS_IonFree(frame->stVFrame.u64PhyAddr[0], frame->stVFrame.pu8VirAddr[0]);
    frame->stVFrame.u64PhyAddr[0] = (CVI_U64)0;
    frame->stVFrame.u64PhyAddr[1] = (CVI_U64)0;
    frame->stVFrame.u64PhyAddr[2] = (CVI_U64)0;
    frame->stVFrame.pu8VirAddr[0] = NULL;
    frame->stVFrame.pu8VirAddr[1] = NULL;
    frame->stVFrame.pu8VirAddr[2] = NULL;
  }
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

  ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, argv[1]);
  if (ret != CVI_SUCCESS) {
    printf("open model failed with %#x!\n", ret);
    return ret;
  }
  VIDEO_FRAME_INFO_S bg;
  // printf("toread image:%s\n",argv[1]);
  // VB_BLK blk_fr;

  printf("to read image\n");
  if (CVI_SUCCESS != CVI_AI_LoadBinImage(argv[2], &bg, PIXEL_FORMAT_RGB_888_PLANAR)) {
    printf("cviai read image failed.");
    // CVI_VB_ReleaseBlock(blk_fr);
    CVI_AI_DestroyHandle(ai_handle);
    return -1;
  }
  int loop_count = 1;
  if (argc == 4) {
    loop_count = atoi(argv[3]);
  }
  if (ret != CVI_SUCCESS) {
    printf("open img failed with %#x!\n", ret);
    return ret;
  } else {
    printf("image read,width:%d\n", bg.stVFrame.u32Width);
  }

  for (int i = 0; i < loop_count; i++) {
    cvai_object_t obj_meta = {0};
    CVI_AI_MobileDetV2_Pedestrian(ai_handle, &bg, &obj_meta);
    // std::stringstream ss;
    // ss << "boxes=[";
    // for (int i = 0; i < obj_meta.size; i++) {
    //   cvai_bbox_t b = obj_meta.info[i].bbox;
    //   printf("box=[%.1f,%.1f,%.1f,%.1f]\n", b.x1, b.y1, b.x2, b.y2);

    //   // ss << "[" << obj_meta.info[i].bbox.x1 << "," << obj_meta.info[i].bbox.y1 << ","
    //   //   << obj_meta.info[i].bbox.x2 << "," << obj_meta.info[i].bbox.y2 << "],";
    // }
    // // str_res = ss.str();
    // printf("objsize:%u\n", obj_meta.size);
    CVI_AI_Free(&obj_meta);
  }
  // std::cout<<str_res<<std::endl;
  // CVI_VB_ReleaseBlock(blk_fr);
  ReleaseImage(&bg);
  CVI_AI_DestroyHandle(ai_handle);

  return ret;
}
