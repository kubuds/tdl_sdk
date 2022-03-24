#include "app/cviai_app.h"
#include "cviai_log.hpp"

#include "face_capture/face_capture.h"

CVI_S32 CVI_AI_APP_CreateHandle(cviai_app_handle_t *handle, cviai_handle_t ai_handle) {
  if (ai_handle == NULL) {
    LOGE("ai_handle is empty.");
    return CVIAI_FAILURE;
  }
  cviai_app_context_t *ctx = (cviai_app_context_t *)malloc(sizeof(cviai_app_context_t));
  ctx->ai_handle = ai_handle;
  ctx->face_cpt_info = NULL;
  *handle = ctx;
  return CVIAI_SUCCESS;
}

CVI_S32 CVI_AI_APP_DestroyHandle(cviai_app_handle_t handle) {
  cviai_app_context_t *ctx = handle;
  _FaceCapture_Free(ctx->face_cpt_info);
  ctx->face_cpt_info = NULL;
  return CVIAI_SUCCESS;
}

CVI_S32 CVI_AI_APP_FaceCapture_Init(const cviai_app_handle_t handle, uint32_t buffer_size) {
  cviai_app_context_t *ctx = handle;
  return _FaceCapture_Init(&(ctx->face_cpt_info), buffer_size);
}

CVI_S32 CVI_AI_APP_FaceCapture_QuickSetUp(const cviai_app_handle_t handle,
                                          const char *fd_model_path, const char *fr_model_path,
                                          const char *fq_model_path) {
  cviai_app_context_t *ctx = handle;
  return _FaceCapture_QuickSetUp(ctx->ai_handle, fd_model_path, fr_model_path, fq_model_path);
}

CVI_S32 CVI_AI_APP_FaceCapture_GetDefaultConfig(face_capture_config_t *cfg) {
  return _FaceCapture_GetDefaultConfig(cfg);
}

CVI_S32 CVI_AI_APP_FaceCapture_SetConfig(const cviai_app_handle_t handle,
                                         face_capture_config_t *cfg) {
  cviai_app_context_t *ctx = handle;
  return _FaceCapture_SetConfig(ctx->face_cpt_info, cfg, handle->ai_handle);
}

CVI_S32 CVI_AI_APP_FaceCapture_Run(const cviai_app_handle_t handle, VIDEO_FRAME_INFO_S *frame) {
  cviai_app_context_t *ctx = handle;
  return _FaceCapture_Run(ctx->face_cpt_info, ctx->ai_handle, frame);
}

CVI_S32 CVI_AI_APP_FaceCapture_SetMode(const cviai_app_handle_t handle, capture_mode_e mode) {
  cviai_app_context_t *ctx = handle;
  return _FaceCapture_SetMode(ctx->face_cpt_info, mode);
}

CVI_S32 CVI_AI_APP_FaceCapture_CleanAll(const cviai_app_handle_t handle) {
  cviai_app_context_t *ctx = handle;
  return _FaceCapture_CleanAll(ctx->face_cpt_info);
}
