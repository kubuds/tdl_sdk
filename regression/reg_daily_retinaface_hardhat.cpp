#include <fstream>
#include <string>
#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "evaluation/cviai_evaluation.h"
#include "evaluation/cviai_media.h"
#include "json.hpp"

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf(
        "Usage: %s <model_dir>\n"
        "          <image_dir>\n"
        "          <regression_json>\n",
        argv[0]);
    return CVI_FAILURE;
  }
  // CVI_S32 ret = CVI_SUCCESS;
  std::string model_dir = std::string(argv[1]);
  std::string image_dir = std::string(argv[2]);

  nlohmann::json m_json_read;
  std::ofstream m_ofs_results;

  std::ifstream filestr(argv[3]);
  filestr >> m_json_read;
  filestr.close();

  std::string fd_model_name = std::string(m_json_read["model"]);
  std::string fd_model_path = model_dir + "/" + fd_model_name;
  printf("fd_model_path: %s\n", fd_model_path.c_str());

  int img_num = int(m_json_read["test_images"].size());
  printf("img_num: %d\n", img_num);

  float threshold_bbox = float(m_json_read["threshold_bbox"]);
  printf("threshold_bbox: %f\n", threshold_bbox);
  float threshold_score = float(m_json_read["threshold_score"]);
  printf("threshold_score: %f\n", threshold_score);

  static CVI_S32 vpssgrp_width = 1920;
  static CVI_S32 vpssgrp_height = 1080;
  cviai_handle_t handle = NULL;

  CVI_S32 ret = CVIAI_SUCCESS;
  ret = MMF_INIT_HELPER2(vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 3, vpssgrp_width,
                         vpssgrp_height, PIXEL_FORMAT_RGB_888, 3);
  if (ret != CVIAI_SUCCESS) {
    printf("Init sys failed with %#x!\n", ret);
    return ret;
  }
  ret = CVI_AI_CreateHandle(&handle);
  if (ret != CVIAI_SUCCESS) {
    printf("Create handle failed with %#x!\n", ret);
    return ret;
  }

  ret = CVI_AI_OpenModel(handle, CVI_AI_SUPPORTED_MODEL_RETINAFACE_HARDHAT, fd_model_path.c_str());
  if (ret != CVIAI_SUCCESS) {
    printf("Set model failed with %#x!\n", ret);
    return ret;
  }

  CVI_AI_SetSkipVpssPreprocess(handle, CVI_AI_SUPPORTED_MODEL_RETINAFACE_HARDHAT, false);

  for (int img_idx = 0; img_idx < img_num; img_idx++) {
    std::string image_path = image_dir + "/" + std::string(m_json_read["test_images"][img_idx]);
    printf("[%d] %s\n", img_idx, image_path.c_str());

    int expected_res_num = int(m_json_read["expected_results"][img_idx][0]);

    VB_BLK blk1;
    VIDEO_FRAME_INFO_S frame;
    CVI_S32 ret = CVI_AI_ReadImage(image_path.c_str(), &blk1, &frame, PIXEL_FORMAT_BGR_888);
    if (ret != CVIAI_SUCCESS) {
      printf("Read image failed with %#x!\n", ret);
      return ret;
    }

    cvai_face_t face_meta;
    memset(&face_meta, 0, sizeof(cvai_face_t));

    CVI_AI_RetinaFace_Hardhat(handle, &frame, &face_meta);
    // CVI_AI_Service_FaceDrawRect(NULL, &face_meta, &frame, false,
    // CVI_AI_Service_GetDefaultBrush());
    bool pass = (expected_res_num == int(face_meta.size));
#if 0
    printf("[%d] pass: %d; expected det nums: %d, result: %d\n", img_idx, pass, expected_res_num,
           face_meta.size);
#endif
    if (!pass) {
      return CVIAI_FAILURE;
    }

    for (uint32_t i = 0; i < face_meta.size; i++) {
      float expected_res_x1 = float(m_json_read["expected_results"][img_idx][1][i][0]);
      float expected_res_y1 = float(m_json_read["expected_results"][img_idx][1][i][1]);
      float expected_res_x2 = float(m_json_read["expected_results"][img_idx][1][i][2]);
      float expected_res_y2 = float(m_json_read["expected_results"][img_idx][1][i][3]);
      float expected_res_bbox_conf = float(m_json_read["expected_results"][img_idx][1][i][4]);
      float expected_res_hardhat_score = float(m_json_read["expected_results"][img_idx][1][i][5]);

      bool pass =
          (abs(face_meta.info[i].bbox.x1 - expected_res_x1) < threshold_bbox) &
          (abs(face_meta.info[i].bbox.y1 - expected_res_y1) < threshold_bbox) &
          (abs(face_meta.info[i].bbox.x2 - expected_res_x2) < threshold_bbox) &
          (abs(face_meta.info[i].bbox.y2 - expected_res_y2) < threshold_bbox) &
          (abs(face_meta.info[i].bbox.score - expected_res_bbox_conf) < threshold_score) &
          (abs(face_meta.info[i].hardhat_score - expected_res_hardhat_score) < threshold_score);

#if 0
      printf(
          "[%d][%d] pass: %d, x1, y1, x2, y2, bbox_conf, hardhat_score, \n expected : [%f, %f, %f, %f, %f, %f], result : [%f, %f, %f, %f, %f, %f]\n",
          img_idx, i, pass, expected_res_x1, expected_res_y1, expected_res_x2, expected_res_y2,
          expected_res_bbox_conf, expected_res_hardhat_score,
          face_meta.info[i].bbox.x1, face_meta.info[i].bbox.y1, face_meta.info[i].bbox.x2,
          face_meta.info[i].bbox.y2, face_meta.info[i].bbox.score, face_meta.info[i].hardhat_score);
#endif
      if (!pass) {
        return CVIAI_FAILURE;
      }
    }

    CVI_AI_Free(&face_meta);
    CVI_VB_ReleaseBlock(blk1);
  }
  CVI_AI_DestroyHandle(handle);
  CVI_SYS_Exit();
  printf("retinaface hardhat regression result: all pass\n");
  return CVIAI_SUCCESS;
}