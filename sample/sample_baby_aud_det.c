#define LOG_TAG "SampleFD"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "middleware_utils.h"
#include "sample_log.h"
#include "sample_utils.h"
#include "vi_vo_utils.h"

#include <core/utils/vpss_helper.h>
#include <cvi_comm.h>
#include <cvi_sys.h>
#include <cvi_vb.h>
#include <cvi_vi.h>
#include <cviai.h>
#include <rtsp.h>
#include <sample_comm.h>

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile bool bExit = false;

static cvai_object_t g_stFaceMeta = {0};

MUTEXAUTOLOCK_INIT(ResultMutex);

#define AUDIOFORMATSIZE 2
#define SECOND 3
#define CVI_AUDIO_BLOCK_MODE -1
#define PERIOD_SIZE 640
#define SAMPLE_RATE 16000
#define FRAME_SIZE SAMPLE_RATE *AUDIOFORMATSIZE *SECOND  // PCM_FORMAT_S16_LE (2bytes) 3 seconds

// This model has 6 classes, including Sneezing, Coughing, Clapping, Baby Cry, Glass breaking,
// Office. There will be a Normal option in the end, becuase the score is lower than threshold, we
// will set it to Normal.
static const char *enumStr[] = {"no_baby_cry", "baby_cry"};
static uint32_t g_index = 0;
char *outpath = "./test";
bool record = false;

typedef struct {
  SAMPLE_AI_MW_CONTEXT *pstMWContext;
  cviai_service_handle_t stServiceHandle;
} SAMPLE_AI_VENC_THREAD_ARG_S;

// Get frame and set it to global buffer
void *thread_uplink_audio(void *pHandle) {
  AI_LOGI("Enter thread_uplink_audio thread\n");
  CVI_S32 s32Ret;
  AUDIO_FRAME_S stFrame;
  AEC_FRAME_S stAecFrm;
  int loop = SAMPLE_RATE / PERIOD_SIZE * SECOND;  // 3 seconds
  int size = PERIOD_SIZE * AUDIOFORMATSIZE;       // PCM_FORMAT_S16_LE (2bytes)

  // Set video frame interface
  CVI_U8 buffer[FRAME_SIZE];  // 3 seconds
  memset(buffer, 0, FRAME_SIZE);
  VIDEO_FRAME_INFO_S Frame;
  Frame.stVFrame.pu8VirAddr[0] = buffer;  // Global buffer
  Frame.stVFrame.u32Height = 1;
  Frame.stVFrame.u32Width = FRAME_SIZE;
  cviai_handle_t pstAIHandle = (cviai_handle_t)pHandle;

  // classify the sound result
  int index = -1;
  int maxval_sound = 0;
  while (bExit == false) {
    for (int i = 0; i < loop; ++i) {
      s32Ret = CVI_AI_GetFrame(0, 0, &stFrame, &stAecFrm, CVI_AUDIO_BLOCK_MODE);  // Get audio frame
      if (s32Ret != CVIAI_SUCCESS) {
        printf("CVI_AI_GetFrame --> none!!\n");
        continue;
      } else {
        memcpy(buffer + i * size, (CVI_U8 *)stFrame.u64VirAddr[0],
               size);  // Set the period size date to global buffer
      }
    }
    int16_t *psound = (int16_t *)buffer;
    float meanval = 0;
    for (int i = 0; i < SAMPLE_RATE * SECOND; i++) {
      meanval += psound[i];
      if (psound[i] > maxval_sound) {
        maxval_sound = psound[i];
      }
    }
    printf("maxvalsound:%d,meanv:%f\n", maxval_sound, meanval / (SAMPLE_RATE * SECOND));
    if (!record) {
      int ret = CVI_AI_SoundClassification_V2(pstAIHandle, &Frame, &index);  // Detect the audio
      if (ret == CVIAI_SUCCESS) {
        printf("esc class: %s\n", enumStr[index]);
        g_index = index;
      } else {
        printf("detect failed\n");
        g_index = 0;
      }
    } else {
      char strinfo[128];
      static uint32_t count = 0;
      sprintf(strinfo, "%s_%d.raw", outpath, count);
      FILE *fp = fopen(strinfo, "wb");
      printf("to write:%s\n", strinfo);
      fwrite((char *)buffer, 1, FRAME_SIZE, fp);
      fclose(fp);
      count++;
      //   bExit = true;
    }
  }
  s32Ret = CVI_AI_ReleaseFrame(0, 0, &stFrame, &stAecFrm);
  if (s32Ret != CVIAI_SUCCESS) printf("CVI_AI_ReleaseFrame Failed!!\n");

  pthread_exit(NULL);
}

CVI_S32 SET_AUDIO_ATTR(CVI_VOID) {
  // STEP 1: cvitek_audin_set
  //_update_audin_config
  AIO_ATTR_S AudinAttr;
  AudinAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)SAMPLE_RATE;
  AudinAttr.u32ChnCnt = 1;
  AudinAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
  AudinAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
  AudinAttr.enWorkmode = AIO_MODE_I2S_MASTER;
  AudinAttr.u32EXFlag = 0;
  AudinAttr.u32FrmNum = 10;                /* only use in bind mode */
  AudinAttr.u32PtNumPerFrm = PERIOD_SIZE;  // sample rate / fps
  AudinAttr.u32ClkSel = 0;
  AudinAttr.enI2sType = AIO_I2STYPE_INNERCODEC;
  CVI_S32 s32Ret;
  // STEP 2:cvitek_audin_uplink_start
  //_set_audin_config
  s32Ret = CVI_AI_SetPubAttr(0, &AudinAttr);
  if (s32Ret != CVIAI_SUCCESS) printf("CVI_AI_SetPubAttr failed with %#x!\n", s32Ret);

  s32Ret = CVI_AI_Enable(0);
  if (s32Ret != CVIAI_SUCCESS) printf("CVI_AI_Enable failed with %#x!\n", s32Ret);

  s32Ret = CVI_AI_EnableChn(0, 0);
  if (s32Ret != CVIAI_SUCCESS) printf("CVI_AI_EnableChn failed with %#x!\n", s32Ret);

  s32Ret = CVI_AI_SetVolume(0, 4);
  if (s32Ret != CVIAI_SUCCESS) printf("CVI_AI_SetVolume failed with %#x!\n", s32Ret);

  printf("SET_AUDIO_ATTR success!!\n");
  return CVIAI_SUCCESS;
}

void *run_venc(void *args) {
  AI_LOGI("Enter encoder thread\n");
  SAMPLE_AI_VENC_THREAD_ARG_S *pstArgs = (SAMPLE_AI_VENC_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;
  CVI_S32 s32Ret;
  cvai_object_t stFaceMeta = {0};

  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, 0, &stFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    {
      MutexAutoLock(ResultMutex, lock);
      CVI_AI_CopyObjectMeta(&g_stFaceMeta, &stFaceMeta);
    }

    s32Ret = CVI_AI_Service_ObjectDrawRect(pstArgs->stServiceHandle, &stFaceMeta, &stFrame, false,
                                           CVI_AI_Service_GetDefaultBrush());
    for (uint32_t j = 0; j < stFaceMeta.size; j++) {
      CVI_AI_Service_ObjectWriteText("baby", stFaceMeta.info[j].bbox.x1, stFaceMeta.info[j].bbox.y1,
                                     &stFrame, 0, 200, 0);
    }
    CVI_AI_Service_ObjectWriteText(enumStr[g_index], 100, 100, &stFrame, 0, 200, 0);
    if (s32Ret != CVIAI_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      AI_LOGE("Draw fame fail!, ret=%x\n", s32Ret);
      goto error;
    }

    s32Ret = SAMPLE_AI_Send_Frame_RTSP(&stFrame, pstArgs->pstMWContext);
    if (s32Ret != CVI_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      AI_LOGE("Send Output Frame NG, ret=%x\n", s32Ret);
      goto error;
    }
  error:
    CVI_AI_Free(&stFaceMeta);
    CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
    }
  }
  AI_LOGI("Exit encoder thread\n");
  pthread_exit(NULL);
}

void *run_ai_thread(void *pHandle) {
  AI_LOGI("Enter AI thread\n");
  cviai_handle_t pstAIHandle = (cviai_handle_t)pHandle;

  VIDEO_FRAME_INFO_S stFrame;
  cvai_object_t stFaceMeta = {0};

  CVI_S32 s32Ret;
  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, VPSS_CHN1, &stFrame, 2000);

    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("CVI_VPSS_GetChnFrame failed with %#x\n", s32Ret);
      goto get_frame_failed;
    }

    s32Ret = CVI_AI_MobileDetV2_Pedestrian(pstAIHandle, &stFrame, &stFaceMeta);
    if (s32Ret != CVIAI_SUCCESS) {
      AI_LOGE("inference failed!, ret=%x\n", s32Ret);
      goto inf_error;
    }

    AI_LOGI("baby count: %d\n", stFaceMeta.size);
    {
      MutexAutoLock(ResultMutex, lock);
      CVI_AI_CopyObjectMeta(&stFaceMeta, &g_stFaceMeta);
    }

  inf_error:
    CVI_VPSS_ReleaseChnFrame(0, 1, &stFrame);
  get_frame_failed:
    CVI_AI_Free(&stFaceMeta);
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
    }
  }

  AI_LOGI("Exit AI thread\n");
  pthread_exit(NULL);
}

static void SampleHandleSig(CVI_S32 signo) {
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  AI_LOGI("handle signal, signo: %d\n", signo);
  if (SIGINT == signo || SIGTERM == signo) {
    bExit = true;
  }
}

int main(int argc, char *argv[]) {
  //   if (argc != 2) {
  //     printf(
  //         "\nUsage: %s RETINA_MODEL_PATH.\n\n"
  //         "\tRETINA_MODEL_PATH, path to retinaface model.\n",
  //         argv[0]);
  //     return CVIAI_FAILURE;
  //   }

  signal(SIGINT, SampleHandleSig);
  signal(SIGTERM, SampleHandleSig);

  SAMPLE_AI_MW_CONFIG_S stMWConfig = {0};

  CVI_S32 s32Ret = SAMPLE_AI_Get_VI_Config(&stMWConfig.stViConfig);
  if (s32Ret != CVI_SUCCESS || stMWConfig.stViConfig.s32WorkingViNum <= 0) {
    AI_LOGE("Failed to get senor infomation from ini file (/mnt/data/sensor_cfg.ini).\n");
    return -1;
  }

  // Get VI size
  PIC_SIZE_E enPicSize;
  s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stMWConfig.stViConfig.astViInfo[0].stSnsInfo.enSnsType,
                                          &enPicSize);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("Cannot get senor size\n");
    return -1;
  }

  SIZE_S stSensorSize;
  s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSensorSize);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("Cannot get senor size\n");
    return -1;
  }

  // Setup frame size of video encoder to 1080p
  SIZE_S stVencSize = {
      .u32Width = 1920,
      .u32Height = 1080,
  };

  stMWConfig.stVBPoolConfig.u32VBPoolCount = 3;

  // VBPool 0 for VPSS Grp0 Chn0
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].enFormat = VI_PIXEL_FORMAT;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32BlkCount = 3;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32Height = stSensorSize.u32Height;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32Width = stSensorSize.u32Width;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].bBind = true;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32VpssChnBinding = VPSS_CHN0;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32VpssGrpBinding = (VPSS_GRP)0;

  // VBPool 1 for VPSS Grp0 Chn1
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].enFormat = VI_PIXEL_FORMAT;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32BlkCount = 3;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32Height = stVencSize.u32Height;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32Width = stVencSize.u32Width;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].bBind = true;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32VpssChnBinding = VPSS_CHN1;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32VpssGrpBinding = (VPSS_GRP)0;

  // VBPool 2 for AI preprocessing
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].enFormat = PIXEL_FORMAT_BGR_888_PLANAR;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].u32BlkCount = 1;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].u32Height = 720;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].u32Width = 1280;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].bBind = false;

  // Setup VPSS Grp0
  stMWConfig.stVPSSPoolConfig.u32VpssGrpCount = 1;
  stMWConfig.stVPSSPoolConfig.stVpssMode.aenInput[0] = VPSS_INPUT_MEM;
  stMWConfig.stVPSSPoolConfig.stVpssMode.enMode = VPSS_MODE_DUAL;
  stMWConfig.stVPSSPoolConfig.stVpssMode.ViPipe[0] = 0;
  stMWConfig.stVPSSPoolConfig.stVpssMode.aenInput[1] = VPSS_INPUT_ISP;
  stMWConfig.stVPSSPoolConfig.stVpssMode.ViPipe[1] = 0;

  SAMPLE_AI_VPSS_CONFIG_S *pstVpssConfig = &stMWConfig.stVPSSPoolConfig.astVpssConfig[0];
  pstVpssConfig->bBindVI = true;

  // Assign device 1 to VPSS Grp0, because device1 has 3 outputs in dual mode.
  VPSS_GRP_DEFAULT_HELPER2(&pstVpssConfig->stVpssGrpAttr, stSensorSize.u32Width,
                           stSensorSize.u32Height, VI_PIXEL_FORMAT, 1);
  pstVpssConfig->u32ChnCount = 2;
  pstVpssConfig->u32ChnBindVI = 0;
  VPSS_CHN_DEFAULT_HELPER(&pstVpssConfig->astVpssChnAttr[0], stVencSize.u32Width,
                          stVencSize.u32Height, VI_PIXEL_FORMAT, true);
  VPSS_CHN_DEFAULT_HELPER(&pstVpssConfig->astVpssChnAttr[1], stVencSize.u32Width,
                          stVencSize.u32Height, VI_PIXEL_FORMAT, true);

  // Get default VENC configurations
  SAMPLE_AI_Get_Input_Config(&stMWConfig.stVencConfig.stChnInputCfg);
  stMWConfig.stVencConfig.u32FrameWidth = stVencSize.u32Width;
  stMWConfig.stVencConfig.u32FrameHeight = stVencSize.u32Height;

  // Get default RTSP configurations
  SAMPLE_AI_Get_RTSP_Config(&stMWConfig.stRTSPConfig.stRTSPConfig);

  SAMPLE_AI_MW_CONTEXT stMWContext = {0};
  s32Ret = SAMPLE_AI_Init_WM(&stMWConfig, &stMWContext);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("init middleware failed! ret=%x\n", s32Ret);
    return -1;
  }

  cviai_handle_t stAIHandle = NULL;

  // Create AI handle and assign VPSS Grp1 Device 0 to AI SDK
  GOTO_IF_FAILED(CVI_AI_CreateHandle2(&stAIHandle, 1, 0), s32Ret, create_ai_fail);

  GOTO_IF_FAILED(CVI_AI_SetVBPool(stAIHandle, 0, 2), s32Ret, create_service_fail);

  CVI_AI_SetVpssTimeout(stAIHandle, 1000);

  // audio
  if (CVI_AUDIO_INIT() == CVIAI_SUCCESS) {
    printf("CVI_AUDIO_INIT success!!\n");
  }
  SET_AUDIO_ATTR();

  cviai_service_handle_t stServiceHandle = NULL;
  GOTO_IF_FAILED(CVI_AI_Service_CreateHandle(&stServiceHandle, stAIHandle), s32Ret,
                 create_service_fail);

  GOTO_IF_FAILED(
      CVI_AI_OpenModel(stAIHandle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, argv[1]), s32Ret,
      setup_ai_fail);

  GOTO_IF_FAILED(
      CVI_AI_OpenModel(stAIHandle, CVI_AI_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, argv[2]), s32Ret,
      setup_ai_fail);

  CVI_AI_SetModelThreshold(stAIHandle, CVI_AI_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, 0.7);
  CVI_AI_SetModelThreshold(stAIHandle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, 0.5);
  if (argc == 4) {
    record = atoi(argv[3]) ? true : false;
  }

  pthread_t stVencThread, stAIThread, pcm_output_thread;
  SAMPLE_AI_VENC_THREAD_ARG_S args = {
      .pstMWContext = &stMWContext,
      .stServiceHandle = stServiceHandle,
  };

  pthread_create(&stVencThread, NULL, run_venc, &args);
  pthread_create(&stAIThread, NULL, run_ai_thread, stAIHandle);
  pthread_create(&pcm_output_thread, NULL, thread_uplink_audio, stAIHandle);

  pthread_join(stVencThread, NULL);
  pthread_join(stAIThread, NULL);
  pthread_join(pcm_output_thread, NULL);

setup_ai_fail:
  CVI_AI_Service_DestroyHandle(stServiceHandle);
create_service_fail:
  CVI_AI_DestroyHandle(stAIHandle);
create_ai_fail:
  SAMPLE_AI_Destroy_MW(&stMWContext);

  return 0;
}
