#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "sample_comm.h"
#include "sample_utils.h"
#include "vi_vo_utils.h"

#include <cvi_sys.h>
#include <cvi_vb.h>
#include <cvi_vi.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static volatile bool bExit = false;

static void SampleHandleSig(CVI_S32 signo) {
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  if (SIGINT == signo || SIGTERM == signo) {
    bExit = true;
  }
}

char *floatToString(float number) {
  char *numArray = calloc(64, sizeof(char));
  // sprintf(numArray, "%g", number);
  sprintf(numArray, "%3f", number);
  return numArray;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf(
        "Usage: %s <face_mask_model_path> <video output>.\n"
        "\t face_mask_model_path, path to face mask detection model\n"
        "\t video output, 0: disable, 1: output to panel, 2: output through rtsp\n",
        argv[0]);
    return CVIAI_FAILURE;
  }
  CVI_S32 voType = atoi(argv[2]);

  // Set signal catch
  signal(SIGINT, SampleHandleSig);
  signal(SIGTERM, SampleHandleSig);

  CVI_S32 s32Ret = CVIAI_SUCCESS;
  VideoSystemContext vs_ctx = {0};
  SIZE_S aiInputSize = {.u32Width = 1920, .u32Height = 1080};

  if (InitVideoSystem(&vs_ctx, &aiInputSize, VI_PIXEL_FORMAT, voType) != CVI_SUCCESS) {
    printf("failed to init video system\n");
    return CVIAI_FAILURE;
  }

  cviai_handle_t ai_handle = NULL;
  GOTO_IF_FAILED(CVI_AI_CreateHandle2(&ai_handle, 1, 0), s32Ret, create_ai_fail);

  cviai_service_handle_t service_handle = NULL;
  GOTO_IF_FAILED(CVI_AI_Service_CreateHandle(&service_handle, ai_handle), s32Ret,
                 create_service_fail);

  GOTO_IF_FAILED(CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_FACEMASKDETECTION, argv[1]),
                 s32Ret, setup_ai_fail);

  GOTO_IF_FAILED(CVI_AI_SetModelThreshold(ai_handle, CVI_AI_SUPPORTED_MODEL_FACEMASKDETECTION, 0.7),
                 s32Ret, setup_ai_fail);

  VIDEO_FRAME_INFO_S stfdFrame, stVOFrame;
  cvai_face_t face;
  memset(&face, 0, sizeof(cvai_face_t));
  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(vs_ctx.vpssConfigs.vpssGrp, vs_ctx.vpssConfigs.vpssChnAI,
                                  &stfdFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      printf("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    CVI_AI_FaceMaskDetection(ai_handle, &stfdFrame, &face);
    printf("face_count %d\n", face.size);

    for (uint32_t i = 0; i < face.size; i++) {
      printf("mask score : %f\n", face.info[i].mask_score);
      char *maskscore = floatToString(face.info[i].mask_score);
      strncpy(face.info[i].name, maskscore, sizeof(face.info[i].name));
    }

    int s32Ret = CVI_SUCCESS;
    s32Ret = CVI_VPSS_ReleaseChnFrame(vs_ctx.vpssConfigs.vpssGrp, vs_ctx.vpssConfigs.vpssChnAI,
                                      &stfdFrame);
    if (s32Ret != CVI_SUCCESS) {
      printf("CVI_VPSS_ReleaseChnFrame chn0 NG\n");
      break;
    }

    // Send frame to VO if opened.
    if (voType) {
      s32Ret = CVI_VPSS_GetChnFrame(vs_ctx.vpssConfigs.vpssGrp,
                                    vs_ctx.vpssConfigs.vpssChnVideoOutput, &stVOFrame, 1000);
      if (s32Ret != CVI_SUCCESS) {
        printf("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
        break;
      }

      CVI_AI_Service_FaceDrawRect(service_handle, &face, &stVOFrame, true,
                                  CVI_AI_Service_GetDefaultBrush());
      s32Ret = SendOutputFrame(&stVOFrame, &vs_ctx.outputContext);
      if (s32Ret != CVI_SUCCESS) {
        printf("Send Output Frame NG\n");
        break;
      }

      s32Ret = CVI_VPSS_ReleaseChnFrame(vs_ctx.vpssConfigs.vpssGrp,
                                        vs_ctx.vpssConfigs.vpssChnVideoOutput, &stVOFrame);
      if (s32Ret != CVI_SUCCESS) {
        printf("CVI_VPSS_ReleaseChnFrame chn0 NG\n");
        break;
      }
    }

    CVI_AI_Free(&face);
  }

setup_ai_fail:
  CVI_AI_Service_DestroyHandle(service_handle);
create_service_fail:
  CVI_AI_DestroyHandle(ai_handle);
create_ai_fail:
  DestroyVideoSystem(&vs_ctx);
  CVI_SYS_Exit();
  CVI_VB_Exit();
}