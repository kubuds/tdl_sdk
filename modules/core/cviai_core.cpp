#include "core/cviai_core.h"
#include "cviai_core_internal.hpp"

#include "cviai_experimental.h"
#include "cviai_perfetto.h"
#include "face_attribute/face_attribute.hpp"
#include "face_quality/face_quality.hpp"
#include "liveness/liveness.hpp"
#include "mask_classification/mask_classification.hpp"
#include "mask_face_recognition/mask_face_recognition.hpp"
#include "object_detection/mobiledetv2/mobiledetv2.hpp"
#include "object_detection/yolov3/yolov3.hpp"
#include "osnet/osnet.hpp"
#include "retina_face/retina_face.hpp"
#include "thermal_face_detection/thermal_face.hpp"

#include "cviai_trace.hpp"

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace cviai;

void CVI_AI_PerfettoInit() {
#if __GNUC__ >= 7
  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kInProcessBackend;
  args.backends |= perfetto::kSystemBackend;

  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();
#endif
}

//*************************************************
// Experimental features
void CVI_AI_EnableGDC(cviai_handle_t handle, bool use_gdc) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  ctx->use_gdc_wrap = use_gdc;
}
//*************************************************

inline void __attribute__((always_inline)) removeCtx(cviai_context_t *ctx) {
  delete ctx->td_model;
  CVI_IVE_DestroyHandle(ctx->ive_handle);
  for (auto it : ctx->vec_vpss_engine) {
    delete it;
  }
  delete ctx;
}

int CVI_AI_CreateHandle(cviai_handle_t *handle) { return CVI_AI_CreateHandle2(handle, -1); }

int CVI_AI_CreateHandle2(cviai_handle_t *handle, const VPSS_GRP vpssGroupId) {
  cviai_context_t *ctx = new cviai_context_t;
  ctx->ive_handle = CVI_IVE_CreateHandle();
  ctx->vec_vpss_engine.push_back(new VpssEngine());
  if (ctx->vec_vpss_engine[0]->init(false, vpssGroupId) != CVI_SUCCESS) {
    syslog(LOG_ERR, "cviai_handle_t create failed.");
    removeCtx(ctx);
    return CVI_FAILURE;
  }
  syslog(LOG_INFO, "cviai_handle_t created.");
  *handle = ctx;
  return CVI_SUCCESS;
}

int CVI_AI_DestroyHandle(cviai_handle_t handle) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  CVI_AI_CloseAllModel(handle);
  removeCtx(ctx);
  syslog(LOG_INFO, "cviai_handle_t destroyed.");
  return CVI_SUCCESS;
}

int CVI_AI_SetModelPath(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                        const char *filepath) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  ctx->model_cont[config].model_path = filepath;
  return CVI_SUCCESS;
}

int CVI_AI_GetModelPath(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config, char **filepath) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  char *path = (char *)malloc(ctx->model_cont[config].model_path.size());
  snprintf(path, strlen(path), "%s", ctx->model_cont[config].model_path.c_str());
  *filepath = path;
  return CVI_SUCCESS;
}

int CVI_AI_SetSkipVpssPreprocess(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                                 bool skip) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  auto &m_t = ctx->model_cont[config];
  m_t.skip_vpss_preprocess = skip;
  if (m_t.instance != nullptr) {
    m_t.instance->skipVpssPreprocess(m_t.skip_vpss_preprocess);
  }
  return CVI_SUCCESS;
}

int CVI_AI_GetSkipVpssPreprocess(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                                 bool *skip) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  *skip = ctx->model_cont[config].skip_vpss_preprocess;
  return CVI_SUCCESS;
}

int CVI_AI_SetModelThreshold(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                             float threshold) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  auto &m_t = ctx->model_cont[config];
  m_t.model_threshold = threshold;
  if (m_t.instance != nullptr) {
    m_t.instance->setModelThreshold(m_t.model_threshold);
  }
  return CVI_SUCCESS;
}

int CVI_AI_GetModelThreshold(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                             float *threshold) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  *threshold = ctx->model_cont[config].model_threshold;
  return CVI_SUCCESS;
}

int CVI_AI_SetVpssThread(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                         const uint32_t thread) {
  return CVI_AI_SetVpssThread2(handle, config, thread, -1);
}

int CVI_AI_SetVpssThread2(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config,
                          const uint32_t thread, const VPSS_GRP vpssGroupId) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  uint32_t vpss_thread = thread;
  if (thread >= ctx->vec_vpss_engine.size()) {
    auto inst = new VpssEngine();
    if (inst->init(false, vpssGroupId) != CVI_SUCCESS) {
      syslog(LOG_ERR, "Vpss init failed\n");
      delete inst;
      return CVI_FAILURE;
    }

    ctx->vec_vpss_engine.push_back(inst);
    if (thread != ctx->vec_vpss_engine.size() - 1) {
      syslog(
          LOG_WARNING,
          "Thread %u is not in use, thus %u is changed to %u automatically. Used vpss group id is "
          "%u.\n",
          vpss_thread, thread, vpss_thread, inst->getGrpId());
      vpss_thread = ctx->vec_vpss_engine.size() - 1;
    }
  } else {
    syslog(LOG_WARNING, "Thread %u already exists, given group id %u will not be used.\n", thread,
           vpssGroupId);
  }
  auto &m_t = ctx->model_cont[config];
  m_t.vpss_thread = vpss_thread;
  if (m_t.instance != nullptr) {
    m_t.instance->setVpssEngine(ctx->vec_vpss_engine[m_t.vpss_thread]);
  }
  return CVI_SUCCESS;
}

int CVI_AI_GetVpssThread(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config, uint32_t *thread) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  *thread = ctx->model_cont[config].vpss_thread;
  return CVI_SUCCESS;
}

int CVI_AI_GetVpssGrpIds(cviai_handle_t handle, VPSS_GRP **groups, uint32_t *num) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  VPSS_GRP *ids = (VPSS_GRP *)malloc(ctx->vec_vpss_engine.size() * sizeof(VPSS_GRP));
  for (size_t i = 0; i < ctx->vec_vpss_engine.size(); i++) {
    ids[i] = ctx->vec_vpss_engine[i]->getGrpId();
  }
  *groups = ids;
  *num = ctx->vec_vpss_engine.size();
  return CVI_SUCCESS;
}

int CVI_AI_CloseAllModel(cviai_handle_t handle) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  for (auto &m_inst : ctx->model_cont) {
    if (m_inst.second.instance != nullptr) {
      m_inst.second.instance->modelClose();
      delete m_inst.second.instance;
      m_inst.second.instance = nullptr;
    }
  }
  ctx->model_cont.clear();
  return CVI_SUCCESS;
}

int CVI_AI_CloseModel(cviai_handle_t handle, CVI_AI_SUPPORTED_MODEL_E config) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  cviai_model_t &m_t = ctx->model_cont[config];
  if (m_t.instance == nullptr) {
    return CVI_FAILURE;
  }
  m_t.instance->modelClose();
  delete m_t.instance;
  m_t.instance = nullptr;
  return CVI_SUCCESS;
}

template <class C, typename... Arguments>
inline C *__attribute__((always_inline))
getInferenceInstance(const CVI_AI_SUPPORTED_MODEL_E index, cviai_context_t *ctx,
                     Arguments &&... arg) {
  cviai_model_t &m_t = ctx->model_cont[index];
  if (m_t.instance == nullptr) {
    if (m_t.model_path.empty()) {
      syslog(LOG_ERR, "Model path for FaceAttribute is empty.\n");
      return nullptr;
    }
    m_t.instance = new C(arg...);
    if (m_t.instance->modelOpen(m_t.model_path.c_str()) != CVI_SUCCESS) {
      syslog(LOG_ERR, "Open model failed (%s).\n", m_t.model_path.c_str());
      return nullptr;
    }
    m_t.instance->setVpssEngine(ctx->vec_vpss_engine[m_t.vpss_thread]);
    m_t.instance->skipVpssPreprocess(m_t.skip_vpss_preprocess);
    if (m_t.model_threshold == -1) {
      m_t.model_threshold = m_t.instance->getModelThreshold();
    } else {
      m_t.instance->setModelThreshold(m_t.model_threshold);
    }
  }
  C *class_inst = dynamic_cast<C *>(m_t.instance);
  return class_inst;
}

inline int __attribute__((always_inline))
CVI_AI_FaceAttributeBase(const CVI_AI_SUPPORTED_MODEL_E index, const cviai_handle_t handle,
                         VIDEO_FRAME_INFO_S *frame, cvai_face_t *faces, int face_idx,
                         bool set_attribute) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  FaceAttribute *face_attr = getInferenceInstance<FaceAttribute>(index, ctx, ctx->use_gdc_wrap);
  if (face_attr == nullptr) {
    syslog(LOG_ERR, "No instance found for FaceAttribute.\n");
    return CVI_FAILURE;
  }
  face_attr->setWithAttribute(set_attribute);
  return face_attr->inference(frame, faces, face_idx);
}

int CVI_AI_FaceRecognition(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                           cvai_face_t *faces) {
  TRACE_EVENT("cviai_core", "CVI_AI_FaceRecognition");
  return CVI_AI_FaceAttributeBase(CVI_AI_SUPPORTED_MODEL_FACERECOGNITION, handle, frame, faces, -1,
                                  false);
}

int CVI_AI_FaceRecognitionOne(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                              cvai_face_t *faces, int face_idx) {
  TRACE_EVENT("cviai_core", "CVI_AI_FaceRecognitionOne");
  return CVI_AI_FaceAttributeBase(CVI_AI_SUPPORTED_MODEL_FACERECOGNITION, handle, frame, faces,
                                  face_idx, false);
}

int CVI_AI_FaceAttribute(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                         cvai_face_t *faces) {
  TRACE_EVENT("cviai_core", "CVI_AI_FaceAttribute");
  return CVI_AI_FaceAttributeBase(CVI_AI_SUPPORTED_MODEL_FACEATTRIBUTE, handle, frame, faces, -1,
                                  true);
}

int CVI_AI_FaceAttributeOne(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                            cvai_face_t *faces, int face_idx) {
  TRACE_EVENT("cviai_core", "CVI_AI_FaceAttributeOne");
  return CVI_AI_FaceAttributeBase(CVI_AI_SUPPORTED_MODEL_FACEATTRIBUTE, handle, frame, faces,
                                  face_idx, true);
}

int CVI_AI_Yolov3(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                  cvai_obj_det_type_t det_type) {
  TRACE_EVENT("cviai_core", "CVI_AI_Yolov3");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  Yolov3 *yolov3 = getInferenceInstance<Yolov3>(CVI_AI_SUPPORTED_MODEL_YOLOV3, ctx);
  if (yolov3 == nullptr) {
    syslog(LOG_ERR, "No instance found for Yolov3.\n");
    return CVI_FAILURE;
  }
  return yolov3->inference(frame, obj, det_type);
}

inline int __attribute__((always_inline))
MobileDetV2Base(const CVI_AI_SUPPORTED_MODEL_E index, const MobileDetV2::Model model_type,
                cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                cvai_obj_det_type_t det_type) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  MobileDetV2 *detector = getInferenceInstance<MobileDetV2>(index, ctx, model_type);
  if (detector == nullptr) {
    syslog(LOG_ERR, "No instance found for detector.\n");
    return CVI_RC_FAILURE;
  }
  return detector->inference(frame, obj, det_type);
}

int CVI_AI_MobileDetV2_D0(cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                          cvai_obj_det_type_t det_type) {
  TRACE_EVENT("cviai_core", "CVI_AI_MobileDetV2_D0");
  return MobileDetV2Base(CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_D0, MobileDetV2::Model::d0, handle,
                         frame, obj, det_type);
}

int CVI_AI_MobileDetV2_D1(cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                          cvai_obj_det_type_t det_type) {
  TRACE_EVENT("cviai_core", "CVI_AI_MobileDetV2_D1");
  return MobileDetV2Base(CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_D1, MobileDetV2::Model::d1, handle,
                         frame, obj, det_type);
}

int CVI_AI_MobileDetV2_D2(cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                          cvai_obj_det_type_t det_type) {
  TRACE_EVENT("cviai_core", "CVI_AI_MobileDetV2_D2");
  return MobileDetV2Base(CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_D2, MobileDetV2::Model::d2, handle,
                         frame, obj, det_type);
}

inline int __attribute__((always_inline))
CVI_AI_OSNetBase(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                 int obj_idx) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  OSNet *osnet = getInferenceInstance<OSNet>(CVI_AI_SUPPORTED_MODEL_OSNET, ctx);
  if (osnet == nullptr) {
    syslog(LOG_ERR, "No instance found for OSNet.\n");
    return CVI_FAILURE;
  }
  return osnet->inference(frame, obj, -1);
}

int CVI_AI_OSNetOne(cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj,
                    int obj_idx) {
  TRACE_EVENT("cviai_core", "CVI_AI_OSNetOne");
  return CVI_AI_OSNetBase(handle, frame, obj, obj_idx);
}

int CVI_AI_OSNet(cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj) {
  TRACE_EVENT("cviai_core", "CVI_AI_OSNet");
  return CVI_AI_OSNetBase(handle, frame, obj, -1);
}

int CVI_AI_RetinaFace(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_face_t *faces,
                      int *face_count) {
  TRACE_EVENT("cviai_core", "CVI_AI_RetinaFace");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  RetinaFace *retina_face =
      getInferenceInstance<RetinaFace>(CVI_AI_SUPPORTED_MODEL_RETINAFACE, ctx);
  if (retina_face == nullptr) {
    syslog(LOG_ERR, "No instance found for RetinaFace.\n");
    return CVI_FAILURE;
  }
  return retina_face->inference(frame, faces, face_count);
}

int CVI_AI_Liveness(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *rgbFrame,
                    VIDEO_FRAME_INFO_S *irFrame, cvai_face_t *face,
                    cvai_liveness_ir_position_e ir_position) {
  TRACE_EVENT("cviai_core", "CVI_AI_Liveness");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  Liveness *liveness =
      getInferenceInstance<Liveness>(CVI_AI_SUPPORTED_MODEL_LIVENESS, ctx, ir_position);
  if (liveness == nullptr) {
    syslog(LOG_ERR, "No instance found for Liveness.\n");
    return CVI_FAILURE;
  }
  return liveness->inference(rgbFrame, irFrame, face);
}

int CVI_AI_FaceQuality(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_face_t *face) {
  TRACE_EVENT("cviai_core", "CVI_AI_FaceQuality");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  FaceQuality *face_quality =
      getInferenceInstance<FaceQuality>(CVI_AI_SUPPORTED_MODEL_FACEQUALITY, ctx);
  if (face_quality == nullptr) {
    syslog(LOG_ERR, "No instance found for FaceQuality.\n");
    return CVI_FAILURE;
  }
  return face_quality->inference(frame, face);
}

int CVI_AI_MaskClassification(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                              cvai_face_t *face) {
  TRACE_EVENT("cviai_core", "CVI_AI_MaskClassification");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  MaskClassification *mask_classification =
      getInferenceInstance<MaskClassification>(CVI_AI_SUPPORTED_MODEL_MASKCLASSIFICATION, ctx);
  if (mask_classification == nullptr) {
    syslog(LOG_ERR, "No instance found for MaskClassification.\n");
    return CVI_FAILURE;
  }
  return mask_classification->inference(frame, face);
}

int CVI_AI_ThermalFace(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_face_t *faces) {
  TRACE_EVENT("cviai_core", "CVI_AI_ThermalFace");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  ThermalFace *thermal_face =
      getInferenceInstance<ThermalFace>(CVI_AI_SUPPORTED_MODEL_THERMALFACE, ctx);
  if (thermal_face == nullptr) {
    syslog(LOG_ERR, "No instance found for ThermalFace.\n");
    return CVI_FAILURE;
  }
  return thermal_face->inference(frame, faces);
}

int CVI_AI_MaskFaceRecognition(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                               cvai_face_t *faces) {
  TRACE_EVENT("cviai_core", "CVI_AI_MaskFaceRecognition");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  MaskFaceRecognition *mask_face_rec =
      getInferenceInstance<MaskFaceRecognition>(CVI_AI_SUPPORTED_MODEL_MASKFACERECOGNITION, ctx);
  if (mask_face_rec == nullptr) {
    syslog(LOG_ERR, "No instance found for MaskFaceRecognition.\n");
    return CVI_FAILURE;
  }

  return mask_face_rec->inference(frame, faces);
}

int CVI_AI_TamperDetection(const cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                           float *moving_score) {
  TRACE_EVENT("cviai_core", "CVI_AI_TamperDetection");
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  TamperDetectorMD *td_model = ctx->td_model;
  if (td_model == nullptr) {
    syslog(LOG_INFO, "Init Tamper Detection Model.\n");
    ctx->td_model = new TamperDetectorMD(ctx->ive_handle, frame, (float)0.05, (int)10);
    ctx->td_model->print_info();

    *moving_score = -1.0;
    return 0;
  }
  return ctx->td_model->detect(frame, moving_score);
}
