#include "liveness.hpp"
#include "template_matching.hpp"

#include "core/cviai_types_mem.h"
#include "core_utils.hpp"
#include "face_utils.hpp"

#include "cvi_sys.h"
#include "opencv2/opencv.hpp"

#define RESIZE_SIZE 112
#define LIVENESS_SCALE (1 / 255.0)
#define LIVENESS_N 1
#define LIVENESS_C 6
#define LIVENESS_WIDTH 32
#define LIVENESS_HEIGHT 32
#define CROP_NUM 9
#define MIN_FACE_WIDTH 25
#define MIN_FACE_HEIGHT 25
#define MATCH_IOU_THRESHOLD 0.2
#define OUTPUT_NAME "fc2_dequant"

using namespace std;

namespace cviai {

static vector<vector<cv::Mat>> image_preprocess(VIDEO_FRAME_INFO_S *frame,
                                                VIDEO_FRAME_INFO_S *sink_buffer,
                                                cvai_face_t *rgb_meta, cvai_face_t *ir_meta) {
  frame->stVFrame.pu8VirAddr[0] =
      (CVI_U8 *)CVI_SYS_MmapCache(frame->stVFrame.u64PhyAddr[0], frame->stVFrame.u32Length[0]);
  cv::Mat rgb_frame(frame->stVFrame.u32Height, frame->stVFrame.u32Width, CV_8UC3,
                    frame->stVFrame.pu8VirAddr[0], frame->stVFrame.u32Stride[0]);
  if (rgb_frame.data == nullptr) {
    LOGE("src image is empty!\n");
    return vector<vector<cv::Mat>>{};
  }

  sink_buffer->stVFrame.pu8VirAddr[0] = (CVI_U8 *)CVI_SYS_MmapCache(
      sink_buffer->stVFrame.u64PhyAddr[0], sink_buffer->stVFrame.u32Length[0]);
  cv::Mat ir_frame(sink_buffer->stVFrame.u32Height, sink_buffer->stVFrame.u32Width, CV_8UC3,
                   sink_buffer->stVFrame.pu8VirAddr[0], sink_buffer->stVFrame.u32Stride[0]);
  if (ir_frame.data == nullptr) {
    LOGE("sink image is empty!\n");
    return vector<vector<cv::Mat>>{};
  }

  vector<cv::Rect> rgb_boxs;
  vector<cv::Rect> ir_boxs;

  for (uint32_t i = 0; i < rgb_meta->size; i++) {
    cvai_face_info_t rgb_face_info =
        info_rescale_c(frame->stVFrame.u32Width, frame->stVFrame.u32Height, *rgb_meta, i);
    cv::Rect rgb_box;
    rgb_box.x = rgb_face_info.bbox.x1;
    rgb_box.y = rgb_face_info.bbox.y1;
    rgb_box.width = rgb_face_info.bbox.x2 - rgb_box.x;
    rgb_box.height = rgb_face_info.bbox.y2 - rgb_box.y;
    rgb_boxs.push_back(rgb_box);
    CVI_AI_FreeCpp(&rgb_face_info);
  }

  for (uint32_t i = 0; i < ir_meta->size; i++) {
    // cvai_face_info_t ir_face_info = rgb_face_info;
    cvai_face_info_t ir_face_info = info_rescale_c(sink_buffer->stVFrame.u32Width,
                                                   sink_buffer->stVFrame.u32Height, *ir_meta, i);

    cv::Rect ir_box;
    ir_box.x = ir_face_info.bbox.x1;
    ir_box.y = ir_face_info.bbox.y1;
    ir_box.width = ir_face_info.bbox.x2 - ir_box.x;
    ir_box.height = ir_face_info.bbox.y2 - ir_box.y;
    ir_boxs.push_back(ir_box);
    CVI_AI_FreeCpp(&ir_face_info);
  }

  vector<pair<cv::Rect, cv::Rect>> match_result;
  for (uint32_t i = 0; i < rgb_boxs.size(); i++) {
    vector<float> iou_result;
    for (uint32_t j = 0; j < ir_boxs.size(); j++) {
      cv::Rect rect_uni = rgb_boxs[i] | ir_boxs[j];
      cv::Rect rect_int = rgb_boxs[i] & ir_boxs[j];
      // IOU for each pairs
      float iou = rect_int.area() * 1.0 / rect_uni.area();
      iou_result.push_back(iou);
    }
    if (iou_result.size() > 0) {
      float maxElement = *max_element(iou_result.begin(), iou_result.end());
      if (maxElement > MATCH_IOU_THRESHOLD) {
        int match_index = max_element(iou_result.begin(), iou_result.end()) - iou_result.begin();
        match_result.push_back({rgb_boxs[i], ir_boxs[match_index]});
        ir_boxs.erase(ir_boxs.begin() + match_index);
      }
    }
  }

  vector<vector<cv::Mat>> input_mat(match_result.size(), vector<cv::Mat>());
  for (uint32_t i = 0; i < match_result.size(); i++) {
    cv::Rect rgb_box = match_result[i].first;
    cv::Rect ir_box = match_result[i].second;

    if (rgb_box.width <= MIN_FACE_WIDTH || rgb_box.height <= MIN_FACE_HEIGHT ||
        ir_box.width <= MIN_FACE_WIDTH || ir_box.height <= MIN_FACE_HEIGHT)
      continue;
    cv::Mat crop_rgb_frame = rgb_frame(rgb_box);
    cv::Mat crop_ir_frame = ir_frame(ir_box);

    cv::Mat color, ir;
    cv::resize(crop_rgb_frame, color, cv::Size(RESIZE_SIZE, RESIZE_SIZE));
    cv::resize(crop_ir_frame, ir, cv::Size(RESIZE_SIZE, RESIZE_SIZE));

    vector<cv::Mat> colors = TTA_9_cropps(color);
    vector<cv::Mat> irs = TTA_9_cropps(ir);

    vector<cv::Mat> input_v;
    for (size_t i = 0; i < colors.size(); i++) {
      cv::Mat temp;
      cv::merge(vector<cv::Mat>{colors[i], irs[i]}, temp);
      input_v.push_back(temp);
    }
    input_mat[i] = input_v;
  }

  CVI_SYS_Munmap((void *)frame->stVFrame.pu8VirAddr[0], frame->stVFrame.u32Length[0]);
  frame->stVFrame.pu8VirAddr[0] = NULL;
  CVI_SYS_Munmap((void *)sink_buffer->stVFrame.pu8VirAddr[0], sink_buffer->stVFrame.u32Length[0]);
  sink_buffer->stVFrame.pu8VirAddr[0] = NULL;

  return input_mat;
}

Liveness::Liveness() {
  mp_mi = std::make_unique<CvimodelInfo>();
  mp_mi->conf.batch_size = 9;
}

int Liveness::inference(VIDEO_FRAME_INFO_S *rgbFrame, VIDEO_FRAME_INFO_S *irFrame,
                        cvai_face_t *rgb_meta, cvai_face_t *ir_meta) {
  if (rgb_meta->size <= 0) {
    cout << "rgb_meta->size <= 0" << endl;
    return CVI_FAILURE;
  }

  vector<vector<cv::Mat>> input_mats = image_preprocess(rgbFrame, irFrame, rgb_meta, ir_meta);
  if (input_mats.empty()) {
    cout << "input_mat.empty" << endl;
    return CVI_FAILURE;
  }

  for (uint32_t i = 0; i < input_mats.size(); i++) {
    float conf0 = 0.0;
    float conf1 = 0.0;

    vector<cv::Mat> input = input_mats[i];
    if (input.empty()) continue;

    prepareInputTensor(input);

    std::vector<VIDEO_FRAME_INFO_S *> frames = {rgbFrame};
    run(frames);

    CVI_TENSOR *out = CVI_NN_GetTensorByName(OUTPUT_NAME, mp_mi->out.tensors, mp_mi->out.num);
    float *out_data = (float *)CVI_NN_TensorPtr(out);
    for (int j = 0; j < CROP_NUM; j++) {
      conf0 += out_data[j * 2];
      conf1 += out_data[(j * 2) + 1];
    }

    conf0 /= input.size();
    conf1 /= input.size();

    float max = std::max(conf0, conf1);
    float f0 = std::exp(conf0 - max);
    float f1 = std::exp(conf1 - max);
    float score = f1 / (f0 + f1);

    rgb_meta->info[i].liveness_score = score;
    // cout << "Face[" << i << "] liveness score: " << score << endl;
  }

  return CVI_SUCCESS;
}

void Liveness::prepareInputTensor(vector<cv::Mat> &input_mat) {
  CVI_TENSOR *input =
      CVI_NN_GetTensorByName(CVI_NN_DEFAULT_TENSOR, mp_mi->in.tensors, mp_mi->in.num);
  int8_t *input_ptr = (int8_t *)CVI_NN_TensorPtr(input);
  float quant_scale = CVI_NN_TensorQuantScale(input);

  for (int j = 0; j < CROP_NUM; j++) {
    cv::Mat tmpchannels[LIVENESS_C];
    cv::split(input_mat[j], tmpchannels);

    for (int c = 0; c < LIVENESS_C; ++c) {
      tmpchannels[c].convertTo(tmpchannels[c], CV_8SC1, LIVENESS_SCALE * quant_scale, 0);

      int size = tmpchannels[c].rows * tmpchannels[c].cols;
      for (int r = 0; r < tmpchannels[c].rows; ++r) {
        memcpy(input_ptr + size * c + tmpchannels[c].cols * r, tmpchannels[c].ptr(r, 0),
               tmpchannels[c].cols);
      }
    }
    input_ptr += CVI_NN_TensorCount(input) / CROP_NUM;
  }
}

}  // namespace cviai