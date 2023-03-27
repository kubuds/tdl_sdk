#define LOG_TAG "MiddlewareUtils"
#define LOG_LEVEL LOG_LEVEL_INFO
#include "middleware_utils.h"
#include "sample_log.h"

static void SAMPLE_AI_RTSP_ON_CONNECT(const char *ip, void *arg) {
  AI_LOGI("RTSP client connected from: %s\n", ip);
}

static void SAMPLE_AI_RTSP_ON_DISCONNECT(const char *ip, void *arg) {
  AI_LOGI("RTSP client disconnected from: %s\n", ip);
}

CVI_S32 SAMPLE_AI_Get_VI_Config(SAMPLE_VI_CONFIG_S *pstViConfig) {
  // Default sensor config parameters

  SAMPLE_INI_CFG_S stIniCfg = {
      .enSource = VI_PIPE_FRAME_SOURCE_DEV,
      .devNum = 1,
#ifdef _MIDDLEWARE_V2_
      .enSnsType[0] = SONY_IMX327_MIPI_2M_30FPS_12BIT,
      .enWDRMode[0] = WDR_MODE_NONE,
      .s32BusId[0] = 3,
      .s32SnsI2cAddr[0] = -1,
      .MipiDev[0] = 0xFF,
      .u8UseMultiSns = 0,
#else
      .enSnsType = SONY_IMX327_MIPI_2M_30FPS_12BIT,
      .enWDRMode = WDR_MODE_NONE,
      .s32BusId = 3,
      .MipiDev = 0xFF,
      .u8UseDualSns = 0,
      .enSns2Type = SONY_IMX327_SLAVE_MIPI_2M_30FPS_12BIT,
      .s32Sns2BusId = 0,
      .Sns2MipiDev = 0xFF,
#endif
  };

  // Get config from ini if found.
  if (SAMPLE_COMM_VI_ParseIni(&stIniCfg)) {
    AI_LOGI("sensor info is loaded from ini file.\n");
  }

  // convert ini config to vi config.
  if (SAMPLE_COMM_VI_IniToViCfg(&stIniCfg, pstViConfig) != CVI_SUCCESS) {
    AI_LOGE("cannot conver ini to vi config.\n");
  }
  return CVI_SUCCESS;
}

void SAMPLE_AI_Get_Input_Config(SAMPLE_COMM_CHN_INPUT_CONFIG_S *pstInCfg) {
  strcpy(pstInCfg->codec, "h264");
  pstInCfg->initialDelay = CVI_INITIAL_DELAY_DEFAULT;
  pstInCfg->width = 3840;
  pstInCfg->height = 2160;
  pstInCfg->vpssGrp = 1;
  pstInCfg->vpssChn = 0;
  pstInCfg->num_frames = -1;
  pstInCfg->bsMode = 0;
  pstInCfg->rcMode = SAMPLE_RC_CBR;
  pstInCfg->iqp = DEF_IQP;
  pstInCfg->pqp = DEF_PQP;
  pstInCfg->gop = DEF_264_GOP;
  pstInCfg->maxIprop = CVI_H26X_MAX_I_PROP_DEFAULT;
  pstInCfg->minIprop = CVI_H26X_MIN_I_PROP_DEFAULT;
  pstInCfg->bitrate = 500;
  pstInCfg->firstFrmstartQp = 30;
  pstInCfg->minIqp = DEF_264_MINIQP;
  pstInCfg->maxIqp = DEF_264_MAXIQP;
  pstInCfg->minQp = DEF_264_MINQP;
  pstInCfg->maxQp = DEF_264_MAXQP;
  pstInCfg->srcFramerate = 25;
  pstInCfg->framerate = 25;
  pstInCfg->bVariFpsEn = 0;
  pstInCfg->maxbitrate = -1;
  pstInCfg->statTime = -1;
  pstInCfg->chgNum = -1;
  pstInCfg->quality = -1;
  pstInCfg->pixel_format = 0;
  pstInCfg->bitstreamBufSize = 0;
  pstInCfg->single_LumaBuf = 0;
  pstInCfg->single_core = 0;
  pstInCfg->forceIdr = -1;
  pstInCfg->tempLayer = 0;
  pstInCfg->testRoi = 0;
  pstInCfg->bgInterval = 0;
}

void SAMPLE_AI_Get_RTSP_Config(CVI_RTSP_CONFIG *pstRTSPConfig) {
  memset(pstRTSPConfig, 0, sizeof(CVI_RTSP_CONFIG));
  pstRTSPConfig->port = 554;
}

PIC_SIZE_E SAMPLE_AI_Get_PIC_Size(CVI_S32 width, CVI_S32 height) {
  if (width == 1280 && height == 720) {
    return PIC_720P;
  } else if (width == 1920 && height == 1080) {
    return PIC_1080P;
  } else if (width == 3840 && height == 2160) {
    return PIC_3840x2160;
  } else if (width == 2560 && height == 1440) {
    return PIC_1440P;
  } else {
    return PIC_BUTT;
  }
}

CVI_S32 SAMPLE_AI_Init_WM(SAMPLE_AI_MW_CONFIG_S *pstMWConfig, SAMPLE_AI_MW_CONTEXT *pstMWContext) {
  MMF_VERSION_S stVersion;
  CVI_SYS_GetVersion(&stVersion);
  AI_LOGI("MMF Version:%s\n", stVersion.version);

  if (pstMWConfig->stViConfig.s32WorkingViNum <= 0) {
    AI_LOGE("Invalidate working vi number: %u\n", pstMWConfig->stViConfig.s32WorkingViNum);
    return CVI_FAILURE;
  }

  // Set sensor number
  CVI_VI_SetDevNum(pstMWConfig->stViConfig.s32WorkingViNum);

  // Setup VB
  if (pstMWConfig->stVBPoolConfig.u32VBPoolCount <= 0 ||
      pstMWConfig->stVBPoolConfig.u32VBPoolCount >= VB_MAX_COMM_POOLS) {
    AI_LOGE("Invalid number of vb pool: %u, which is outside the valid range of 1 to (%u - 1)\n",
            pstMWConfig->stVBPoolConfig.u32VBPoolCount, VB_MAX_COMM_POOLS);
    return CVI_FAILURE;
  }

  VB_CONFIG_S stVbConf;
  memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

  stVbConf.u32MaxPoolCnt = pstMWConfig->stVBPoolConfig.u32VBPoolCount;
  CVI_U32 u32TotalBlkSize = 0;
  for (uint32_t u32VBIndex = 0; u32VBIndex < stVbConf.u32MaxPoolCnt; u32VBIndex++) {
    CVI_U32 u32BlkSize =
        COMMON_GetPicBufferSize(pstMWConfig->stVBPoolConfig.astVBPoolSetup[u32VBIndex].u32Width,
                                pstMWConfig->stVBPoolConfig.astVBPoolSetup[u32VBIndex].u32Height,
                                pstMWConfig->stVBPoolConfig.astVBPoolSetup[u32VBIndex].enFormat,
                                DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[u32VBIndex].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[u32VBIndex].u32BlkCnt =
        pstMWConfig->stVBPoolConfig.astVBPoolSetup[u32VBIndex].u32BlkCount;
    CVI_U32 u32PoolSize = (u32BlkSize * stVbConf.astCommPool[u32VBIndex].u32BlkCnt);
    u32TotalBlkSize += u32PoolSize;
    AI_LOGI("Create VBPool[%u], size: (%u * %u) = %u bytes\n", u32VBIndex, u32BlkSize,
            stVbConf.astCommPool[u32VBIndex].u32BlkCnt, u32PoolSize);
  }
  AI_LOGI("Total memory of VB pool: %u bytes\n", u32TotalBlkSize);

  // Init system & vb pool
  AI_LOGI("Initialize SYS and VB\n");
  CVI_S32 s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("system init failed with %#x\n", s32Ret);
    return s32Ret;
  }

  // Init VI

  AI_LOGI("Initialize VI\n");
  VI_VPSS_MODE_S stVIVPSSMode;
  stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_ONLINE;
  CVI_SYS_SetVIVPSSMode(&stVIVPSSMode);

  memcpy(&pstMWContext->stViConfig, &pstMWConfig->stViConfig, sizeof(SAMPLE_VI_CONFIG_S));

  s32Ret = SAMPLE_PLAT_VI_INIT(&pstMWConfig->stViConfig);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("vi init failed. s32Ret: 0x%x !\n", s32Ret);
    goto vi_start_error;
  }

  // Init VPSS
  AI_LOGI("Initialize VPSS\n");
  memcpy(&pstMWContext->stVPSSPoolConfig, &pstMWConfig->stVPSSPoolConfig,
         sizeof(SAMPLE_AI_VPSS_POOL_CONFIG_S));
  VPSS_MODE_S stVPSSMode = pstMWConfig->stVPSSPoolConfig.stVpssMode;
  CVI_SYS_SetVPSSModeEx(&stVPSSMode);

  for (uint32_t u32VpssGrpIndex = 0;
       u32VpssGrpIndex < pstMWConfig->stVPSSPoolConfig.u32VpssGrpCount; u32VpssGrpIndex++) {
    SAMPLE_AI_VPSS_CONFIG_S *pstVPSSConf =
        &pstMWConfig->stVPSSPoolConfig.astVpssConfig[u32VpssGrpIndex];
    VPSS_GRP_ATTR_S *pstGrpAttr = &pstVPSSConf->stVpssGrpAttr;
    VPSS_CHN_ATTR_S *pastChnAttr = pstVPSSConf->astVpssChnAttr;
    AI_LOGD("---------VPSS[%u]---------\n", u32VpssGrpIndex);
    AI_LOGD("Input size: (%ux%u)\n", pstGrpAttr->u32MaxW, pstGrpAttr->u32MaxH);
    AI_LOGD("Input format: (%d)\n", pstGrpAttr->enPixelFormat);
    AI_LOGD("VPSS physical device number: %u\n", pstGrpAttr->u8VpssDev);
    AI_LOGD("Src Frame Rate: %d\n", pstGrpAttr->stFrameRate.s32SrcFrameRate);
    AI_LOGD("Dst Frame Rate: %d\n", pstGrpAttr->stFrameRate.s32DstFrameRate);

    CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM + 1] = {0};
    for (uint32_t u32ChnIndex = 0; u32ChnIndex < pstVPSSConf->u32ChnCount; u32ChnIndex++) {
      abChnEnable[u32ChnIndex] = true;
      AI_LOGD("    --------CHN[%u]-------\n", u32ChnIndex);
      AI_LOGD("    Output size: (%ux%u)\n", pastChnAttr[u32ChnIndex].u32Width,
              pastChnAttr[u32ChnIndex].u32Height);
      AI_LOGD("    Depth: %u\n", pastChnAttr[u32ChnIndex].u32Depth);
      AI_LOGD("    Do normalization: %d\n", pastChnAttr[u32ChnIndex].stNormalize.bEnable);
      if (pastChnAttr[u32ChnIndex].stNormalize.bEnable) {
        AI_LOGD("        factor=[%f, %f, %f]\n", pastChnAttr[u32ChnIndex].stNormalize.factor[0],
                pastChnAttr[u32ChnIndex].stNormalize.factor[1],
                pastChnAttr[u32ChnIndex].stNormalize.factor[2]);
        AI_LOGD("        mean=[%f, %f, %f]\n", pastChnAttr[u32ChnIndex].stNormalize.mean[0],
                pastChnAttr[u32ChnIndex].stNormalize.mean[1],
                pastChnAttr[u32ChnIndex].stNormalize.mean[2]);
        AI_LOGD("        rounding=%d\n", pastChnAttr[u32ChnIndex].stNormalize.rounding);
      }
      AI_LOGD("        Src Frame Rate: %d\n", pastChnAttr[u32ChnIndex].stFrameRate.s32SrcFrameRate);
      AI_LOGD("        Dst Frame Rate: %d\n", pastChnAttr[u32ChnIndex].stFrameRate.s32DstFrameRate);
      AI_LOGD("    ----------------------\n");
    }
    AI_LOGD("------------------------\n");

    s32Ret = SAMPLE_COMM_VPSS_Init(u32VpssGrpIndex, abChnEnable, pstGrpAttr, pastChnAttr);
    if (s32Ret != CVI_SUCCESS) {
      AI_LOGD("init vpss group failed. s32Ret: 0x%x !\n", s32Ret);
      goto vpss_start_error;
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(u32VpssGrpIndex, abChnEnable, pstGrpAttr, pastChnAttr);
    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
      goto vpss_start_error;
    }

    if (pstVPSSConf->bBindVI) {
      AI_LOGI("Bind VI with VPSS Grp(%u), Chn(%u)\n", u32VpssGrpIndex, pstVPSSConf->u32ChnBindVI);
#ifdef _MIDDLEWARE_V2_
      s32Ret = SAMPLE_COMM_VI_Bind_VPSS(0, pstVPSSConf->u32ChnBindVI, u32VpssGrpIndex);
#else
      s32Ret = SAMPLE_COMM_VI_Bind_VPSS(pstVPSSConf->u32ChnBindVI, u32VpssGrpIndex);
#endif
      if (s32Ret != CVI_SUCCESS) {
        AI_LOGE("vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto vpss_start_error;
      }
    }
  }

  // Attach VB to VPSS
  for (CVI_U32 u32VBPoolIndex = 0; u32VBPoolIndex < pstMWConfig->stVBPoolConfig.u32VBPoolCount;
       u32VBPoolIndex++) {
    SAMPLE_AI_VB_CONFIG_S *pstVBConfig =
        &pstMWConfig->stVBPoolConfig.astVBPoolSetup[u32VBPoolIndex];
    if (pstVBConfig->bBind) {
      AI_LOGI("Attach VBPool(%u) to VPSS Grp(%u) Chn(%u)\n", u32VBPoolIndex,
              pstVBConfig->u32VpssGrpBinding, pstVBConfig->u32VpssChnBinding);

      s32Ret = CVI_VPSS_AttachVbPool(pstVBConfig->u32VpssGrpBinding, pstVBConfig->u32VpssChnBinding,
                                     (VB_POOL)u32VBPoolIndex);
      if (s32Ret != CVI_SUCCESS) {
        AI_LOGE("Cannot attach VBPool(%u) to VPSS Grp(%u) Chn(%u): ret=%x\n", u32VBPoolIndex,
                pstVBConfig->u32VpssGrpBinding, pstVBConfig->u32VpssChnBinding, s32Ret);
        goto vpss_start_error;
      }
    }
  }

  // Init VENC
  VENC_GOP_ATTR_S stGopAttr;

  // Always use venc chn0 in sample.
  pstMWContext->u32VencChn = 0;

  SAMPLE_COMM_CHN_INPUT_CONFIG_S *pstInputConfig = &pstMWConfig->stVencConfig.stChnInputCfg;

  AI_LOGI("Initialize VENC\n");
  s32Ret = SAMPLE_COMM_VENC_GetGopAttr(VENC_GOPMODE_NORMALP, &stGopAttr);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("Venc Get GopAttr for %#x!\n", s32Ret);
    goto venc_start_error;
  }

  AI_LOGI("venc codec: %s\n", pstInputConfig->codec);
  AI_LOGI("venc frame size: %ux%u\n", pstMWConfig->stVencConfig.u32FrameWidth,
          pstMWConfig->stVencConfig.u32FrameHeight);

  PAYLOAD_TYPE_E enPayLoad;
  if (!strcmp(pstInputConfig->codec, "h264")) {
    enPayLoad = PT_H264;
  } else if (!strcmp(pstInputConfig->codec, "h265")) {
    enPayLoad = PT_H265;
  } else {
    AI_LOGE("Unsupported encode format in sample: %s\n", pstInputConfig->codec);
    s32Ret = CVI_FAILURE;
    goto venc_start_error;
  }

  PIC_SIZE_E enPicSize = SAMPLE_AI_Get_PIC_Size(pstMWConfig->stVencConfig.u32FrameWidth,
                                                pstMWConfig->stVencConfig.u32FrameHeight);
  if (enPicSize == PIC_BUTT) {
    s32Ret = CVI_FAILURE;
    AI_LOGE("Cannot get PIC SIZE from VENC frame size: %ux%u\n",
            pstMWConfig->stVencConfig.u32FrameWidth, pstMWConfig->stVencConfig.u32FrameHeight);
    goto venc_start_error;
  }

  s32Ret = SAMPLE_COMM_VENC_Start(pstInputConfig, pstMWContext->u32VencChn, enPayLoad, enPicSize,
                                  pstInputConfig->rcMode, 0, CVI_FALSE, &stGopAttr);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("Venc Start failed for %#x!\n", s32Ret);
    goto venc_start_error;
  }

  // RTSP
  AI_LOGI("Initialize RTSP\n");
  if (0 > CVI_RTSP_Create(&pstMWContext->pstRtspContext, &pstMWConfig->stRTSPConfig.stRTSPConfig)) {
    AI_LOGE("fail to create rtsp context\n");
    s32Ret = CVI_FAILURE;
    goto rtsp_create_error;
  }

  CVI_RTSP_SESSION_ATTR attr = {0};
  if (enPayLoad == PT_H264) {
    attr.video.codec = RTSP_VIDEO_H264;
    snprintf(attr.name, sizeof(attr.name), "h264");
  } else if (enPayLoad == PT_H265) {
    attr.video.codec = RTSP_VIDEO_H265;
    snprintf(attr.name, sizeof(attr.name), "h265");
  } else {
    AI_LOGE("Unsupported RTSP codec in sample: %d\n", attr.video.codec);
    s32Ret = CVI_FAILURE;
    goto rtsp_create_error;
  }

  CVI_RTSP_CreateSession(pstMWContext->pstRtspContext, &attr, &pstMWContext->pstSession);

  // Set listener to RTSP
  CVI_RTSP_STATE_LISTENER listener;
  listener.onConnect = pstMWConfig->stRTSPConfig.Lisener.onConnect != NULL
                           ? pstMWConfig->stRTSPConfig.Lisener.onConnect
                           : SAMPLE_AI_RTSP_ON_CONNECT;
  listener.argConn = pstMWContext->pstRtspContext;
  listener.onDisconnect = pstMWConfig->stRTSPConfig.Lisener.onDisconnect != NULL
                              ? pstMWConfig->stRTSPConfig.Lisener.onDisconnect
                              : SAMPLE_AI_RTSP_ON_DISCONNECT;
  CVI_RTSP_SetListener(pstMWContext->pstRtspContext, &listener);

  if (0 > CVI_RTSP_Start(pstMWContext->pstRtspContext)) {
    AI_LOGE("Cannot start RTSP\n");
    s32Ret = CVI_FAILURE;
    goto rts_start_error;
  }

  return CVI_SUCCESS;

rts_start_error:
  CVI_RTSP_DestroySession(pstMWContext->pstRtspContext, pstMWContext->pstSession);
  CVI_RTSP_Destroy(&pstMWContext->pstRtspContext);

rtsp_create_error:
  SAMPLE_COMM_VENC_Stop(0);

venc_start_error:
vpss_start_error:
  SAMPLE_COMM_VI_DestroyIsp(&pstMWConfig->stViConfig);
  SAMPLE_COMM_VI_DestroyVi(&pstMWConfig->stViConfig);
vi_start_error:
  SAMPLE_COMM_SYS_Exit();

  return s32Ret;
}

CVI_S32 SAMPLE_AI_Send_Frame_RTSP(VIDEO_FRAME_INFO_S *stVencFrame,
                                  SAMPLE_AI_MW_CONTEXT *pstMWContext) {
  CVI_S32 s32Ret = CVI_SUCCESS;

  CVI_S32 s32SetFrameMilliSec = 20000;
  VENC_STREAM_S stStream;
  VENC_CHN_ATTR_S stVencChnAttr;
  VENC_CHN_STATUS_S stStat;
  VENC_CHN VencChn = pstMWContext->u32VencChn;

  s32Ret = CVI_VENC_SendFrame(VencChn, stVencFrame, s32SetFrameMilliSec);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("CVI_VENC_SendFrame failed! %d\n", s32Ret);
    return s32Ret;
  }

  s32Ret = CVI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("CVI_VENC_GetChnAttr, VencChn[%d], s32Ret = %d\n", VencChn, s32Ret);
    return s32Ret;
  }

  s32Ret = CVI_VENC_QueryStatus(VencChn, &stStat);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("CVI_VENC_QueryStatus failed with %#x!\n", s32Ret);
    return s32Ret;
  }

  if (!stStat.u32CurPacks) {
    AI_LOGE("NOTE: Current frame is NULL!\n");
    return s32Ret;
  }

  stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
  if (stStream.pstPack == NULL) {
    AI_LOGE("malloc memory failed!\n");
    return s32Ret;
  }

  s32Ret = CVI_VENC_GetStream(VencChn, &stStream, -1);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("CVI_VENC_GetStream failed with %#x!\n", s32Ret);
    goto send_failed;
  }

  VENC_PACK_S *ppack;
  CVI_RTSP_DATA data = {0};
  memset(&data, 0, sizeof(CVI_RTSP_DATA));

  data.blockCnt = stStream.u32PackCount;
  for (unsigned int i = 0; i < stStream.u32PackCount; i++) {
    ppack = &stStream.pstPack[i];
    data.dataPtr[i] = ppack->pu8Addr + ppack->u32Offset;
    data.dataLen[i] = ppack->u32Len - ppack->u32Offset;
  }

  s32Ret =
      CVI_RTSP_WriteFrame(pstMWContext->pstRtspContext, pstMWContext->pstSession->video, &data);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("CVI_VENC_ReleaseStream, s32Ret = %d\n", s32Ret);
    goto send_failed;
  }

send_failed:
  CVI_VENC_ReleaseStream(VencChn, &stStream);
  free(stStream.pstPack);
  stStream.pstPack = NULL;
  return s32Ret;
}

void SAMPLE_AI_Stop_VPSS(SAMPLE_AI_VPSS_POOL_CONFIG_S *pstVPSSPoolConfig) {
  for (uint32_t u32VpssIndex = 0; u32VpssIndex < pstVPSSPoolConfig->u32VpssGrpCount;
       u32VpssIndex++) {
    AI_LOGI("stop VPSS (%u)\n", u32VpssIndex);
    CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM + 1] = {0};
    for (uint32_t u32VpssChnIndex = 0;
         u32VpssChnIndex < pstVPSSPoolConfig->astVpssConfig[u32VpssIndex].u32ChnCount;
         u32VpssChnIndex++) {
      abChnEnable[u32VpssChnIndex] = true;
    }
    SAMPLE_COMM_VPSS_Stop(u32VpssIndex, abChnEnable);
  }
}

void SAMPLE_AI_Destroy_MW(SAMPLE_AI_MW_CONTEXT *pstMWContext) {
  AI_LOGI("destroy middleware\n");
  CVI_RTSP_Stop(pstMWContext->pstRtspContext);
  CVI_RTSP_DestroySession(pstMWContext->pstRtspContext, pstMWContext->pstSession);
  CVI_RTSP_Destroy(&pstMWContext->pstRtspContext);
  SAMPLE_COMM_VENC_Stop(pstMWContext->u32VencChn);

  SAMPLE_COMM_VI_DestroyIsp(&pstMWContext->stViConfig);
  SAMPLE_COMM_VI_DestroyVi(&pstMWContext->stViConfig);
  SAMPLE_AI_Stop_VPSS(&pstMWContext->stVPSSPoolConfig);

  CVI_SYS_Exit();
  CVI_VB_Exit();
}
