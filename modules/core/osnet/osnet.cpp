#include "osnet.hpp"

#include "core/cviai_types_mem.h"
#include "core/cviai_types_mem_internal.h"
#include "core_utils.hpp"

#include "opencv2/opencv.hpp"

#define STD_R (255.f * 0.229f)
#define STD_G (255.f * 0.224f)
#define STD_B (255.f * 0.225f)
#define MODEL_MEAN_R (0.485f * 255.f)
#define MODEL_MEAN_G (0.456f * 255.f)
#define MODEL_MEAN_B (0.406f * 255.f)

#define OSNET_OUT_NAME "feature"

namespace cviai {

OSNet::OSNet() {
  mp_mi = std::make_unique<CvimodelInfo>();
  mp_mi->conf.skip_postprocess = true;
  mp_mi->conf.input_mem_type = CVI_MEM_DEVICE;
}

int OSNet::initAfterModelOpened(std::vector<initSetup> *data) {
  if (data->size() != 1) {
    LOGE("OSNet only has 1 input.\n");
    return CVI_FAILURE;
  }
  (*data)[0].factor[0] = 1 / STD_R;
  (*data)[0].factor[1] = 1 / STD_G;
  (*data)[0].factor[2] = 1 / STD_B;
  (*data)[0].mean[0] = MODEL_MEAN_R / STD_R;
  (*data)[0].mean[1] = MODEL_MEAN_G / STD_G;
  (*data)[0].mean[2] = MODEL_MEAN_B / STD_B;
  (*data)[0].keep_aspect_ratio = false;
  (*data)[0].use_quantize_scale = true;
  (*data)[0].use_crop = true;
  return 0;
}

int OSNet::inference(VIDEO_FRAME_INFO_S *stOutFrame, cvai_object_t *meta, int obj_idx) {
  for (uint32_t i = 0; i < meta->size; ++i) {
    if (obj_idx != -1 && i != (uint32_t)obj_idx) continue;
    cvai_bbox_t box =
        box_rescale(stOutFrame->stVFrame.u32Width, stOutFrame->stVFrame.u32Height, meta->width,
                    meta->height, meta->info[i].bbox, meta_rescale_type_e::RESCALE_CENTER);
    m_vpss_config[0].crop_attr.enCropCoordinate = VPSS_CROP_ABS_COOR;
    m_vpss_config[0].crop_attr.stCropRect = {
        (int32_t)box.x1, (int32_t)box.y1, (uint32_t)(box.x2 - box.x1), (uint32_t)(box.y2 - box.y1)};
    std::vector<VIDEO_FRAME_INFO_S *> frames = {stOutFrame};
    run(frames);

    // feature
    CVI_TENSOR *out = CVI_NN_GetTensorByName(OSNET_OUT_NAME, mp_mi->out.tensors, mp_mi->out.num);
    int8_t *feature_blob = (int8_t *)CVI_NN_TensorPtr(out);
    size_t feature_size = CVI_NN_TensorCount(out);
    // Create feature
    CVI_AI_MemAlloc(sizeof(int8_t), feature_size, TYPE_INT8, &meta->info[i].feature);
    memcpy(meta->info[i].feature.ptr, feature_blob, feature_size);
  }
  return CVI_SUCCESS;
}

}  // namespace cviai