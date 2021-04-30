#define _GNU_SOURCE
#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "cviai_perfetto.h"

#define WRITE_RESULT_TO_FILE 0

typedef int (*InferenceFunc)(cviai_handle_t, VIDEO_FRAME_INFO_S *, cvai_object_t *,
                             cvai_obj_det_type_e);
typedef struct _ModelConfig {
  CVI_AI_SUPPORTED_MODEL_E model_id;
  int input_size;
  InferenceFunc inference;
} ModelConfig;

CVI_S32 createModelConfig(const char *model_name, ModelConfig *config) {
  CVI_S32 ret = CVI_SUCCESS;

  if (strcmp(model_name, "mobiledetv2-d0") == 0) {
    config->model_id = CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_D0;
    config->inference = CVI_AI_MobileDetV2_D0;
  } else if (strcmp(model_name, "mobiledetv2-d1") == 0) {
    config->model_id = CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_D1;
    config->inference = CVI_AI_MobileDetV2_D1;
  } else if (strcmp(model_name, "mobiledetv2-d2") == 0) {
    config->model_id = CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_D2;
    config->inference = CVI_AI_MobileDetV2_D2;
  } else if (strcmp(model_name, "yolov3") == 0) {
    config->model_id = CVI_AI_SUPPORTED_MODEL_YOLOV3;
    config->inference = CVI_AI_Yolov3;
  } else {
    ret = CVI_FAILURE;
  }
  return ret;
}

int main(int argc, char *argv[]) {
  CVI_AI_PerfettoInit();
  if (argc != 6) {
    printf(
        "Usage: %s <detection_model_name>\n"
        "          <detection_model_path>\n"
        "          <reid_model_path>\n"
        "          <sample_imagelist_path>\n"
        "          <inference_count>\n",
        argv[0]);
    return CVI_FAILURE;
  }
  CVI_S32 ret = CVI_SUCCESS;

  // Init VB pool size.
  const CVI_S32 vpssgrp_width = 1920;
  const CVI_S32 vpssgrp_height = 1080;
  ret = MMF_INIT_HELPER2(vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 5, vpssgrp_width,
                         vpssgrp_height, PIXEL_FORMAT_RGB_888_PLANAR, 5);
  if (ret != CVI_SUCCESS) {
    printf("Init sys failed with %#x!\n", ret);
    return ret;
  }
  cviai_handle_t ai_handle = NULL;

  ModelConfig model_config;
  if (createModelConfig(argv[1], &model_config) == CVI_FAILURE) {
    printf("unsupported model: %s\n", argv[1]);
    return CVI_FAILURE;
  }

  ret = CVI_AI_CreateHandle2(&ai_handle, 1);

  ret = CVI_AI_SetModelPath(ai_handle, model_config.model_id, argv[2]);
  ret |= CVI_AI_SetModelPath(ai_handle, CVI_AI_SUPPORTED_MODEL_OSNET, argv[3]);
  if (ret != CVI_SUCCESS) {
    printf("model open failed with %#x!\n", ret);
    return ret;
  }

  CVI_AI_SetSkipVpssPreprocess(ai_handle, model_config.model_id, false);
  CVI_AI_SetSkipVpssPreprocess(ai_handle, CVI_AI_SUPPORTED_MODEL_OSNET, false);

  // Init DeepSORT
  CVI_AI_DeepSORT_Init(ai_handle);
#if 1
  cvai_deepsort_config_t ds_conf;
  CVI_AI_DeepSORT_GetDefaultConfig(&ds_conf);
  ds_conf.ktracker_conf.P_std_alpha[0] = 2 * 1 / 20.;
  ds_conf.ktracker_conf.P_std_alpha[1] = 2 * 1 / 20.;
  ds_conf.ktracker_conf.P_std_alpha[3] = 2 * 1 / 20.;
  ds_conf.ktracker_conf.P_std_alpha[4] = 10 * 1 / 160.;
  ds_conf.ktracker_conf.P_std_alpha[5] = 10 * 1 / 160.;
  ds_conf.ktracker_conf.P_std_alpha[7] = 10 * 1 / 160.;
  ds_conf.ktracker_conf.P_std_beta[2] = 0.01;
  ds_conf.ktracker_conf.P_std_beta[6] = 1e-5;

  ds_conf.kfilter_conf.Q_std_beta[2] = 0.01;
  ds_conf.kfilter_conf.Q_std_beta[6] = 1e-5;

  ds_conf.kfilter_conf.R_std_beta[2] = 0.1;
  CVI_AI_DeepSORT_SetConfig(ai_handle, &ds_conf);
#endif

#if WRITE_RESULT_TO_FILE
  FILE *outFile;
  outFile = fopen("sample_MOT_result_1.txt", "w");
  if (outFile == NULL) {
    printf("There is a problem opening the output file.\n");
    exit(EXIT_FAILURE);
  }
#endif

  char *imagelist_path = argv[4];
  FILE *inFile;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  inFile = fopen(imagelist_path, "r");
  if (inFile == NULL) {
    printf("There is a problem opening the rcfile: %s\n", imagelist_path);
    exit(EXIT_FAILURE);
  }
  if ((read = getline(&line, &len, inFile)) == -1) {
    printf("get line error\n");
    exit(EXIT_FAILURE);
  }
  *strchrnul(line, '\n') = '\0';
  int imageNum = atoi(line);

#if WRITE_RESULT_TO_FILE
  fprintf(outFile, "%u\n", imageNum);
#endif

  int inference_count = atoi(argv[5]);

  for (int counter = 0; counter < imageNum; counter++) {
    if (counter == inference_count) {
      break;
    }
    if ((read = getline(&line, &len, inFile)) == -1) {
      printf("get line error\n");
      exit(EXIT_FAILURE);
    }
    *strchrnul(line, '\n') = '\0';
    char *image_path = line;
    printf("[%i] image path = %s\n", counter, image_path);

    VB_BLK blk_fr;
    VIDEO_FRAME_INFO_S frame;
    CVI_S32 ret = CVI_AI_ReadImage(image_path, &blk_fr, &frame, PIXEL_FORMAT_RGB_888);
    if (ret != CVI_SUCCESS) {
      printf("Read image failed with %#x!\n", ret);
      return ret;
    }

    cvai_object_t obj_meta;
    cvai_tracker_t tracker_meta;
    memset(&obj_meta, 0, sizeof(cvai_object_t));
    memset(&tracker_meta, 0, sizeof(cvai_tracker_t));

    //*******************************************
    // Tracking function calls.
    // Step 1. Object detect inference.
    model_config.inference(ai_handle, &frame, &obj_meta, CVI_DET_TYPE_PEOPLE);
    // Step 2. Object feature generator.
    CVI_AI_OSNet(ai_handle, &frame, &obj_meta);
    // Step 3. Tracker.
    CVI_AI_DeepSORT_Obj(ai_handle, &obj_meta, &tracker_meta, true);
    // Tracking function calls ends here.
    //*******************************************

#if WRITE_RESULT_TO_FILE
    fprintf(outFile, "%u\n", tracker_meta.size);
    for (uint32_t i = 0; i < tracker_meta.size; i++) {
      fprintf(outFile, "%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", obj_meta.info[i].unique_id,
              (int)obj_meta.info[i].bbox.x1, (int)obj_meta.info[i].bbox.y1,
              (int)obj_meta.info[i].bbox.x2, (int)obj_meta.info[i].bbox.y2,
              tracker_meta.info[i].state, (int)tracker_meta.info[i].bbox.x1,
              (int)tracker_meta.info[i].bbox.y1, (int)tracker_meta.info[i].bbox.x2,
              (int)tracker_meta.info[i].bbox.y2);
    }

    // fprintf(outFile, "%u\n", 0);
    char debug_info[8192];
    CVI_AI_DeepSORT_DebugInfo_1(ai_handle, debug_info);
    fprintf(outFile, debug_info);
#endif

    CVI_AI_Free(&obj_meta);
    CVI_AI_Free(&tracker_meta);
    CVI_VB_ReleaseBlock(blk_fr);
  }

#if WRITE_RESULT_TO_FILE
  fclose(outFile);
#endif
  CVI_AI_DestroyHandle(ai_handle);
  CVI_SYS_Exit();
}