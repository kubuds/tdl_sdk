#include "core.hpp"
#include "core/utils/vpss_helper.h"
#include "cviai_trace.hpp"

namespace cviai {

int Core::modelOpen(const char *filepath) {
  TRACE_EVENT("cviai_core", "Core::modelOpen");
  if (!mp_config) {
    LOGE("config not set\n");
    return CVI_FAILURE;
  }
  CVI_RC ret = CVI_NN_RegisterModel(filepath, &mp_model_handle);
  if (ret != CVI_RC_SUCCESS) {
    LOGE("CVI_NN_RegisterModel failed, err %d\n", ret);
    modelClose();
    return CVI_FAILURE;
  }
  LOGI("CVI_NN_RegisterModel successed\n");
  if (mp_config->batch_size != 0) {
    CVI_NN_SetConfig(mp_model_handle, OPTION_BATCH_SIZE, mp_config->batch_size);
  }
  CVI_NN_SetConfig(mp_model_handle, OPTION_OUTPUT_ALL_TENSORS,
                   static_cast<int>(mp_config->debug_mode));
  CVI_NN_SetConfig(mp_model_handle, OPTION_SKIP_PREPROCESS,
                   static_cast<int>(mp_config->skip_preprocess));
  CVI_NN_SetConfig(mp_model_handle, OPTION_SKIP_POSTPROCESS,
                   static_cast<int>(mp_config->skip_postprocess));
  CVI_NN_SetConfig(mp_model_handle, OPTION_INPUT_MEM_TYPE, mp_config->input_mem_type);
  CVI_NN_SetConfig(mp_model_handle, OPTION_OUTPUT_MEM_TYPE, mp_config->output_mem_type);

  ret = CVI_NN_GetInputOutputTensors(mp_model_handle, &mp_input_tensors, &m_input_num,
                                     &mp_output_tensors, &m_output_num);
  if (ret != CVI_RC_SUCCESS) {
    LOGE("CVI_NN_GetINputsOutputs failed\n");
    modelClose();
    return CVI_FAILURE;
    ;
  }
  TRACE_EVENT_BEGIN("cviai_core", "InitAtferModelOpened");
  CVI_TENSOR *input = CVI_NN_GetTensorByName(CVI_NN_DEFAULT_TENSOR, mp_input_tensors, m_input_num);
  float quant_scale = CVI_NN_TensorQuantScale(input);
  // Assigning default values.
  float factor[3] = {0, 0, 0}, mean[3] = {0, 0, 0};
  bool pad_reverse = false, keep_aspect_ratio = true;
  bool use_model_threshold = quant_scale == 0 ? false : true;
  ret = initAfterModelOpened(factor, mean, pad_reverse, keep_aspect_ratio, use_model_threshold);
  if (ret != CVI_SUCCESS) {
    LOGE("Failed to init after open model.\n");
    modelClose();
    return CVI_FAILURE;
  }
  m_vpss_chn_attr.clear();
  VPSS_CHN_ATTR_S vpssChnAttr;
  if (use_model_threshold) {
    for (uint32_t i = 0; i < 3; i++) {
      factor[i] *= quant_scale;
      mean[i] *= quant_scale;
    }
    // FIXME: Behavior will changed in 1822.
    float factor_limit = 8191.f / 8192;
    for (uint32_t i = 0; i < 3; i++) {
      if (factor[i] > factor_limit) {
        factor[i] = factor_limit;
      }
    }
  }
  VPSS_CHN_SQ_HELPER(&vpssChnAttr, input->shape.dim[3], input->shape.dim[2],
                     PIXEL_FORMAT_RGB_888_PLANAR, factor, mean, false);
  if (!keep_aspect_ratio) {
    vpssChnAttr.stAspectRatio.enMode = ASPECT_RATIO_NONE;
  }
  m_vpss_chn_attr.push_back(vpssChnAttr);
  TRACE_EVENT_END("cviai_core");
  return CVI_SUCCESS;
}

int Core::modelClose() {
  TRACE_EVENT("cviai_core", "Core::modelClose");
  if (mp_model_handle != nullptr) {
    if (int ret = CVI_NN_CleanupModel(mp_model_handle) != CVI_RC_SUCCESS) {  // NOLINT
      LOGE("CVI_NN_CleanupModel failed, err %d\n", ret);
      return CVI_FAILURE;
    }
    mp_model_handle = nullptr;
  }
  return CVI_SUCCESS;
}

CVI_TENSOR *Core::getInputTensor(int idx) {
  if (idx >= m_input_num) {
    return NULL;
  }
  return mp_input_tensors + idx;
}

CVI_TENSOR *Core::getOutputTensor(int idx) {
  if (idx >= m_output_num) {
    return NULL;
  }
  return mp_output_tensors + idx;
}

int Core::setIveInstance(IVE_HANDLE handle) {
  ive_handle = handle;
  return CVI_SUCCESS;
}

int Core::setVpssEngine(VpssEngine *engine) {
  mp_vpss_inst = engine;
  return CVI_SUCCESS;
}

void Core::skipVpssPreprocess(bool skip) { m_skip_vpss_preprocess = skip; }
void Core::setModelThreshold(float threshold) { m_model_threshold = threshold; }
float Core::getModelThreshold() { return m_model_threshold; };
bool Core::isInitialized() { return mp_model_handle == nullptr ? false : true; }

int Core::initAfterModelOpened(float *factor, float *mean, bool &pad_reverse,
                               bool &keep_aspect_ratio, bool &use_model_threshold) {
  return CVI_SUCCESS;
}

int Core::vpssPreprocess(const VIDEO_FRAME_INFO_S *srcFrame, VIDEO_FRAME_INFO_S *dstFrame) {
  if (!m_use_vpss_crop) {
    mp_vpss_inst->sendFrame(srcFrame, &m_vpss_chn_attr[0], 1);
  } else {
    mp_vpss_inst->sendCropChnFrame(srcFrame, &m_crop_attr, &m_vpss_chn_attr[0], 1);
  }

  return mp_vpss_inst->getFrame(dstFrame, 0);
}

int Core::run(VIDEO_FRAME_INFO_S *srcFrame) {
  TRACE_EVENT("cviai_core", "Core::run");
  int ret = CVI_SUCCESS;
  if (mp_config->input_mem_type == 2) {
    if (m_skip_vpss_preprocess) {
      ret |= runVideoForward(srcFrame);
    } else {
      VIDEO_FRAME_INFO_S stDstFrame;
      ret |= vpssPreprocess(srcFrame, &stDstFrame);
      ret |= runVideoForward(&stDstFrame);
      ret |= mp_vpss_inst->releaseFrame(&stDstFrame, 0);
    }
  } else {
    if (int rcret = CVI_NN_Forward(mp_model_handle, mp_input_tensors, m_input_num,  // NOLINT
                                   mp_output_tensors, m_output_num) != CVI_RC_SUCCESS) {
      LOGE("NN forward failed: %d\n", rcret);
      ret |= CVI_FAILURE;
    }
  }

  return ret;
}

int Core::runVideoForward(VIDEO_FRAME_INFO_S *srcFrame) {
  // FIXME: Need to support multi-input and different fmt
  int ret = CVI_SUCCESS;
  CVI_TENSOR *input = getInputTensor(0);
  CVI_VIDEO_FRAME_INFO info;
  info.type = CVI_FRAME_PLANAR;
  info.shape.dim_size = input->shape.dim_size;
  info.shape.dim[0] = input->shape.dim[0];
  info.shape.dim[1] = input->shape.dim[1];
  info.shape.dim[2] = input->shape.dim[2];
  info.shape.dim[3] = input->shape.dim[3];
  info.fmt = CVI_FMT_INT8;
  for (size_t i = 0; i < 3; ++i) {
    if (m_reverse_device_mem) {
      info.stride[i] = srcFrame->stVFrame.u32Stride[2 - i];
      info.pyaddr[i] = srcFrame->stVFrame.u64PhyAddr[2 - i];
    } else {
      info.stride[i] = srcFrame->stVFrame.u32Stride[i];
      info.pyaddr[i] = srcFrame->stVFrame.u64PhyAddr[i];
    }
  }
  if (int ret =
          CVI_NN_SetTensorWithVideoFrame(mp_model_handle, mp_input_tensors, &info) !=  // NOLINT
          CVI_RC_SUCCESS) {
    LOGE("NN set tensor with vi failed: %d\n", ret);
    return CVI_FAILURE;
  }
  if (int rcret = CVI_NN_Forward(mp_model_handle, mp_input_tensors, m_input_num,  // NOLINT
                                 mp_output_tensors, m_output_num) != CVI_RC_SUCCESS) {
    LOGE("NN forward failed: %d\n", rcret);
    ret |= CVI_FAILURE;
  }
  return ret;
}

}  // namespace cviai