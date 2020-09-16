#include "thermal_face.hpp"
#include "core/cviai_types_mem.h"
#include "core/utils/vpss_helper.h"
#include "core_utils.hpp"

#include "cvi_sys.h"

#define SCALE_R (1.0 / (255.0 * 0.229)) * (128 / 2.64064478874)
#define SCALE_G (1.0 / (255.0 * 0.224)) * (128 / 2.64064478874)
#define SCALE_B (1.0 / (255.0 * 0.225)) * (128 / 2.64064478874)
#define MEAN_R (0.485 / 0.229) * (128 / 2.64064478874)
#define MEAN_G (0.456 / 0.224) * (128 / 2.64064478874)
#define MEAN_B (0.406 / 0.225) * (128 / 2.64064478874)
#define NAME_BBOX "regression_dequant"
#define NAME_SCORE "classification_dequant"

namespace cviai {

static std::vector<cvai_bbox_t> generate_anchors(int base_size, const std::vector<float> &ratios,
                                                 const std::vector<float> &scales) {
  int num_anchors = ratios.size() * scales.size();
  std::vector<cvai_bbox_t> anchors(num_anchors, cvai_bbox_t());
  std::vector<float> areas(num_anchors, 0);

  for (size_t i = 0; i < anchors.size(); i++) {
    anchors[i].x2 = base_size * scales[i % scales.size()];
    anchors[i].y2 = base_size * scales[i % scales.size()];
    areas[i] = anchors[i].x2 * anchors[i].y2;

    anchors[i].x2 = std::sqrt(areas[i] / ratios[i / scales.size()]);
    anchors[i].y2 = anchors[i].x2 * ratios[i / scales.size()];

    anchors[i].x1 -= anchors[i].x2 * 0.5;
    anchors[i].x2 -= anchors[i].x2 * 0.5;
    anchors[i].y1 -= anchors[i].y2 * 0.5;
    anchors[i].y2 -= anchors[i].y2 * 0.5;
  }

  return anchors;
}

static std::vector<cvai_bbox_t> shift(const std::vector<int> &shape, int stride,
                                      const std::vector<cvai_bbox_t> &anchors) {
  std::vector<int> shift_x(shape[0] * shape[1], 0);
  std::vector<int> shift_y(shape[0] * shape[1], 0);

  for (int i = 0; i < shape[0]; i++) {
    for (int j = 0; j < shape[1]; j++) {
      shift_x[i * shape[1] + j] = (j + 0.5) * stride;
    }
  }
  for (int i = 0; i < shape[0]; i++) {
    for (int j = 0; j < shape[1]; j++) {
      shift_y[i * shape[1] + j] = (i + 0.5) * stride;
    }
  }

  std::vector<cvai_bbox_t> shifts(shape[0] * shape[1], cvai_bbox_t());
  for (size_t i = 0; i < shifts.size(); i++) {
    shifts[i].x1 = shift_x[i];
    shifts[i].y1 = shift_y[i];
    shifts[i].x2 = shift_x[i];
    shifts[i].y2 = shift_y[i];
  }

  std::vector<cvai_bbox_t> all_anchors(anchors.size() * shifts.size(), cvai_bbox_t());
  for (size_t i = 0; i < shifts.size(); i++) {
    for (size_t j = 0; j < anchors.size(); j++) {
      all_anchors[i * anchors.size() + j].x1 = anchors[j].x1 + shifts[i].x1;
      all_anchors[i * anchors.size() + j].y1 = anchors[j].y1 + shifts[i].y1;
      all_anchors[i * anchors.size() + j].x2 = anchors[j].x2 + shifts[i].x2;
      all_anchors[i * anchors.size() + j].y2 = anchors[j].y2 + shifts[i].y2;
    }
  }

  return all_anchors;
}

static void bbox_pred(const cvai_bbox_t &anchor, cv::Vec4f regress, std::vector<float> std,
                      cvai_bbox_t &bbox) {
  float width = anchor.x2 - anchor.x1 + 1;
  float height = anchor.y2 - anchor.y1 + 1;
  float ctr_x = anchor.x1 + 0.5 * (width - 1.0);
  float ctr_y = anchor.y1 + 0.5 * (height - 1.0);

  regress[0] *= std[0];
  regress[1] *= std[1];
  regress[2] *= std[2];
  regress[3] *= std[3];

  float pred_ctr_x = regress[0] * width + ctr_x;
  float pred_ctr_y = regress[1] * height + ctr_y;
  float pred_w = FastExp(regress[2]) * width;
  float pred_h = FastExp(regress[3]) * height;

  bbox.x1 = (pred_ctr_x - 0.5 * (pred_w - 1.0));
  bbox.y1 = (pred_ctr_y - 0.5 * (pred_h - 1.0));
  bbox.x2 = (pred_ctr_x + 0.5 * (pred_w - 1.0));
  bbox.y2 = (pred_ctr_y + 0.5 * (pred_h - 1.0));
}

ThermalFace::ThermalFace() {
  mp_config = std::make_unique<ModelConfig>();
  mp_config->skip_preprocess = true;
  mp_config->input_mem_type = CVI_MEM_DEVICE;
}

ThermalFace::~ThermalFace() {}

int ThermalFace::initAfterModelOpened() {
  std::vector<int> pyramid_levels = {3, 4, 5, 6, 7};
  std::vector<int> strides = {8, 16, 32, 64, 128};
  std::vector<int> sizes = {24, 48, 96, 192, 384};
  std::vector<float> ratios = {1, 2};
  std::vector<float> scales = {1, 1.25992105, 1.58740105};

  CVI_TENSOR *input = getInputTensor(0);
  std::vector<std::vector<int>> image_shapes;
  image_shapes.reserve(strides.size());
  for (int s : strides) {
    image_shapes.push_back({(input->shape.dim[2] + s - 1) / s, (input->shape.dim[3] + s - 1) / s});
  }

  for (size_t i = 0; i < sizes.size(); i++) {
    std::vector<cvai_bbox_t> anchors = generate_anchors(sizes[i], ratios, scales);
    std::vector<cvai_bbox_t> shifted_anchors = shift(image_shapes[i], strides[i], anchors);
    m_all_anchors.insert(m_all_anchors.end(), shifted_anchors.begin(), shifted_anchors.end());
  }

  return CVI_SUCCESS;
}

int ThermalFace::vpssPreprocess(const VIDEO_FRAME_INFO_S *srcFrame, VIDEO_FRAME_INFO_S *dstFrame) {
  CVI_TENSOR *input = CVI_NN_GetTensorByName(CVI_NN_DEFAULT_TENSOR, mp_input_tensors, m_input_num);
  VPSS_CHN_ATTR_S vpssChnAttr;
  const float factor[] = {SCALE_R, SCALE_G, SCALE_B};
  const float mean[] = {MEAN_R, MEAN_G, MEAN_B};
  VPSS_CHN_SQ_RB_HELPER(&vpssChnAttr, srcFrame->stVFrame.u32Width, srcFrame->stVFrame.u32Height,
                        input->shape.dim[3], input->shape.dim[2], PIXEL_FORMAT_RGB_888_PLANAR,
                        factor, mean, true);
  mp_vpss_inst->sendFrame(srcFrame, &vpssChnAttr, 1);
  return mp_vpss_inst->getFrame(dstFrame, 0);
}

int ThermalFace::inference(VIDEO_FRAME_INFO_S *srcFrame, cvai_face_t *meta) {
  CVI_TENSOR *input = CVI_NN_GetTensorByName(CVI_NN_DEFAULT_TENSOR, mp_input_tensors, m_input_num);

  int ret = CVI_SUCCESS;
  ret = run(srcFrame);

  std::vector<cvai_face_info_t> faceList;
  outputParser(input->shape.dim[3], input->shape.dim[2], &faceList);

  initFaceMeta(meta, faceList.size());
  meta->width = input->shape.dim[3];
  meta->height = input->shape.dim[2];

  for (uint32_t i = 0; i < meta->size; ++i) {
    meta->info[i].bbox.x1 = faceList[i].bbox.x1;
    meta->info[i].bbox.x2 = faceList[i].bbox.x2;
    meta->info[i].bbox.y1 = faceList[i].bbox.y1;
    meta->info[i].bbox.y2 = faceList[i].bbox.y2;
    meta->info[i].bbox.score = faceList[i].bbox.score;
  }

  return ret;
}

void ThermalFace::outputParser(int image_width, int image_height,
                               std::vector<cvai_face_info_t> *bboxes_nms) {
  std::vector<cvai_face_info_t> BBoxes;

  std::string score_str = NAME_SCORE;
  CVI_TENSOR *out = CVI_NN_GetTensorByName(score_str.c_str(), mp_output_tensors, m_output_num);
  float *score_blob = (float *)CVI_NN_TensorPtr(out);

  std::string bbox_str = NAME_BBOX;
  out = CVI_NN_GetTensorByName(bbox_str.c_str(), mp_output_tensors, m_output_num);
  float *bbox_blob = (float *)CVI_NN_TensorPtr(out);

  for (size_t i = 0; i < m_all_anchors.size(); i++) {
    cvai_face_info_t box;

    float conf = score_blob[i];
    if (conf <= m_model_threshold) {
      continue;
    }
    box.bbox.score = conf;

    cv::Vec4f regress;
    float dx = bbox_blob[0 + i * 4];
    float dy = bbox_blob[1 + i * 4];
    float dw = bbox_blob[2 + i * 4];
    float dh = bbox_blob[3 + i * 4];
    regress = cv::Vec4f(dx, dy, dw, dh);
    bbox_pred(m_all_anchors[i], regress, {0.1, 0.1, 0.2, 0.2}, box.bbox);

    BBoxes.push_back(box);
  }

  NonMaximumSuppression(BBoxes, *bboxes_nms, 0.5, 'u');

  for (auto &box : *bboxes_nms) {
    clip_boxes(image_width, image_height, box.bbox);
  }
}

void ThermalFace::initFaceMeta(cvai_face_t *meta, int size) {
  meta->size = size;
  if (meta->size == 0) return;

  meta->info = (cvai_face_info_t *)malloc(sizeof(cvai_face_info_t) * meta->size);

  memset(meta->info, 0, sizeof(cvai_face_info_t) * meta->size);

  for (uint32_t i = 0; i < meta->size; ++i) {
    meta->info[i].bbox.x1 = -1;
    meta->info[i].bbox.x2 = -1;
    meta->info[i].bbox.y1 = -1;
    meta->info[i].bbox.y2 = -1;

    meta->info[i].name[0] = '\0';
    meta->info[i].emotion = EMOTION_UNKNOWN;
    meta->info[i].gender = GENDER_UNKNOWN;
    meta->info[i].race = RACE_UNKNOWN;
    meta->info[i].age = -1;
    meta->info[i].liveness_score = -1;
    meta->info[i].mask_score = -1;
  }
}

}  // namespace cviai
