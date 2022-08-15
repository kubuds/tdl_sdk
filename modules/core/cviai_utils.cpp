#include "core/cviai_utils.h"
#include "cviai_core_internal.hpp"

#include "core/cviai_core.h"
#include "core/cviai_types_mem_internal.h"
#include "core/error_msg.hpp"
#include "core/utils/vpss_helper.h"
#include "utils/core_utils.hpp"
#include "utils/face_utils.hpp"
#include "utils/image_utils.hpp"

#include <string.h>

/** NOTE: If turn on DO_ALIGN_STRIDE, we can not copy the data from cv::Mat directly. */
/** TODO: If ALIGN is not necessary in AI SDK, remove it in the future. */
// #define DO_ALIGN_STRIDE
#ifdef DO_ALIGN_STRIDE
#define GET_AI_IMAGE_STRIDE(x) (ALIGN((x), DEFAULT_ALIGN))
#else
#define GET_AI_IMAGE_STRIDE(x) (x)
#endif

CVI_S32 CVI_AI_SQPreprocessRaw(cviai_handle_t handle, const VIDEO_FRAME_INFO_S *frame,
                               VIDEO_FRAME_INFO_S *output, const float quantized_factor,
                               const float quantized_mean, const uint32_t thread,
                               uint32_t timeout) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  uint32_t vpss_thread;
  int ret = CVI_AI_AddVpssEngineThread(thread, -1, 0, &vpss_thread, &ctx->vec_vpss_engine);
  if (ret != CVIAI_SUCCESS) {
    return ret;
  }

  const float factor[] = {quantized_factor, quantized_factor, quantized_factor};
  const float mean[] = {quantized_mean, quantized_mean, quantized_mean};
  VPSS_CHN_ATTR_S chn_attr;
  VPSS_CHN_SQ_HELPER(&chn_attr, frame->stVFrame.u32Width, frame->stVFrame.u32Height,
                     frame->stVFrame.enPixelFormat, factor, mean, false);
  auto &vpss_inst = ctx->vec_vpss_engine[vpss_thread];
  ret = vpss_inst->sendFrame(frame, &chn_attr, 1);
  if (ret != CVI_SUCCESS) {
    LOGE("Send frame failed: %s\n", cviai::get_vpss_error_msg(ret));
    return CVIAI_ERR_VPSS_SEND_FRAME;
  }

  ret = vpss_inst->getFrame(output, 0, timeout);
  if (ret != CVI_SUCCESS) {
    LOGE("Get frame failed: %s\n", cviai::get_vpss_error_msg(ret));
    return CVIAI_ERR_VPSS_GET_FRAME;
  }
  return CVIAI_SUCCESS;
}

CVI_S32 CVI_AI_SQPreprocess(cviai_handle_t handle, const VIDEO_FRAME_INFO_S *frame,
                            VIDEO_FRAME_INFO_S *output, const float factor, const float mean,
                            const float quantize_threshold, const uint32_t thread,
                            uint32_t timeout) {
  float quantized_factor = factor * 128 / quantize_threshold;
  float quantized_mean = (-1) * mean * 128 / quantize_threshold;
  return CVI_AI_SQPreprocessRaw(handle, frame, output, quantized_factor, quantized_mean, thread,
                                timeout);
}

CVI_S32 CVI_AI_Dequantize(const int8_t *quantizedData, float *data, const uint32_t bufferSize,
                          const float dequantizeThreshold) {
  cviai::Dequantize(quantizedData, data, dequantizeThreshold, bufferSize);
  return CVIAI_SUCCESS;
}
CVI_S32 CVI_AI_SoftMax(const float *inputBuffer, float *outputBuffer, const uint32_t bufferSize) {
  cviai::SoftMaxForBuffer(inputBuffer, outputBuffer, bufferSize);
  return CVIAI_SUCCESS;
}

template <typename T, typename U>
inline CVI_S32 CVI_AI_NMS(const T *input, T *nms, const float threshold, const char method) {
  if (method != 'u' && method != 'm') {
    LOGE("Unsupported NMS method. Only supports u or m");
    return CVIAI_FAILURE;
  }
  std::vector<U> bboxes;
  std::vector<U> bboxes_nms;
  for (uint32_t i = 0; i < input->size; i++) {
    bboxes.push_back(input->info[i]);
  }
  cviai::NonMaximumSuppression(bboxes, bboxes_nms, threshold, method);
  CVI_AI_Free(nms);
  nms->size = bboxes.size();
  nms->width = input->width;
  nms->height = input->height;
  nms->info = (U *)malloc(nms->size * sizeof(U));
  for (unsigned int i = 0; i < nms->size; i++) {
    CVI_AI_CopyInfoCpp(&bboxes_nms[i], &nms->info[i]);
  }
  return CVIAI_SUCCESS;
}

CVI_S32 CVI_AI_FaceNMS(const cvai_face_t *face, cvai_face_t *faceNMS, const float threshold,
                       const char method) {
  return CVI_AI_NMS<cvai_face_t, cvai_face_info_t>(face, faceNMS, threshold, method);
}

CVI_S32 CVI_AI_ObjectNMS(const cvai_object_t *obj, cvai_object_t *objNMS, const float threshold,
                         const char method) {
  return CVI_AI_NMS<cvai_object_t, cvai_object_info_t>(obj, objNMS, threshold, method);
}

CVI_S32 CVI_AI_FaceAlignment(VIDEO_FRAME_INFO_S *inFrame, const uint32_t metaWidth,
                             const uint32_t metaHeight, const cvai_face_info_t *info,
                             VIDEO_FRAME_INFO_S *outFrame, const bool enableGDC) {
  if (enableGDC) {
    if (inFrame->stVFrame.enPixelFormat != PIXEL_FORMAT_RGB_888_PLANAR &&
        inFrame->stVFrame.enPixelFormat != PIXEL_FORMAT_YUV_PLANAR_420) {
      LOGE(
          "Supported format are PIXEL_FORMAT_RGB_888_PLANAR, PIXEL_FORMAT_YUV_PLANAR_420. Current: "
          "%x\n",
          inFrame->stVFrame.enPixelFormat);
      return CVIAI_FAILURE;
    }
    cvai_face_info_t face_info = cviai::info_rescale_c(
        metaWidth, metaHeight, inFrame->stVFrame.u32Width, inFrame->stVFrame.u32Height, *info);
    cviai::face_align_gdc(inFrame, outFrame, face_info);
  } else {
    if (inFrame->stVFrame.enPixelFormat != PIXEL_FORMAT_RGB_888) {
      LOGE("Supported format is PIXEL_FORMAT_RGB_888. Current: %x\n",
           inFrame->stVFrame.enPixelFormat);
      return CVIAI_FAILURE;
    }
    bool do_unmap_in = false, do_unmap_out = false;
    if (inFrame->stVFrame.pu8VirAddr[0] == NULL) {
      inFrame->stVFrame.pu8VirAddr[0] = (CVI_U8 *)CVI_SYS_MmapCache(inFrame->stVFrame.u64PhyAddr[0],
                                                                    inFrame->stVFrame.u32Length[0]);
      do_unmap_in = true;
    }
    if (outFrame->stVFrame.pu8VirAddr[0] == NULL) {
      outFrame->stVFrame.pu8VirAddr[0] = (CVI_U8 *)CVI_SYS_MmapCache(
          outFrame->stVFrame.u64PhyAddr[0], outFrame->stVFrame.u32Length[0]);
      do_unmap_out = true;
    }
    cv::Mat image(cv::Size(inFrame->stVFrame.u32Width, inFrame->stVFrame.u32Height), CV_8UC3,
                  inFrame->stVFrame.pu8VirAddr[0], inFrame->stVFrame.u32Stride[0]);
    cv::Mat warp_image(cv::Size(outFrame->stVFrame.u32Width, outFrame->stVFrame.u32Height),
                       image.type(), outFrame->stVFrame.pu8VirAddr[0],
                       outFrame->stVFrame.u32Stride[0]);
    cvai_face_info_t face_info = cviai::info_rescale_c(
        metaWidth, metaHeight, inFrame->stVFrame.u32Width, inFrame->stVFrame.u32Height, *info);
    cviai::face_align(image, warp_image, face_info);
    CVI_SYS_IonFlushCache(outFrame->stVFrame.u64PhyAddr[0], outFrame->stVFrame.pu8VirAddr[0],
                          outFrame->stVFrame.u32Length[0]);
    if (do_unmap_in) {
      CVI_SYS_Munmap((void *)inFrame->stVFrame.pu8VirAddr[0], inFrame->stVFrame.u32Length[0]);
      inFrame->stVFrame.pu8VirAddr[0] = NULL;
    }
    if (do_unmap_out) {
      CVI_SYS_Munmap((void *)outFrame->stVFrame.pu8VirAddr[0], outFrame->stVFrame.u32Length[0]);
      outFrame->stVFrame.pu8VirAddr[0] = NULL;
    }
  }
  return CVIAI_SUCCESS;
}

CVI_S32 CVI_AI_Face_Quality_Laplacian(VIDEO_FRAME_INFO_S *srcFrame, cvai_face_info_t *face_info,
                                      float *score) {
  return cviai::face_quality_laplacian(srcFrame, face_info, score);
}

CVI_S32 CVI_AI_CreateImage(cvai_image_t *image, uint32_t height, uint32_t width,
                           PIXEL_FORMAT_E fmt) {
  if (fmt != PIXEL_FORMAT_RGB_888 && fmt != PIXEL_FORMAT_RGB_888_PLANAR &&
      fmt != PIXEL_FORMAT_NV21 && fmt != PIXEL_FORMAT_YUV_PLANAR_420) {
    LOGE("Pixel format [%d] is not supported.\n", fmt);
    return CVIAI_ERR_INVALID_ARGS;
  }
  if (image->pix[0] != NULL) {
    LOGE("destination image is not empty.");
    return CVIAI_ERR_INVALID_ARGS;
  }
  image->pix_format = fmt;
  image->height = height;
  image->width = width;

  /* NOTE: Refer to vpss_helper.h*/
  switch (fmt) {
    case PIXEL_FORMAT_RGB_888: {
      image->stride[0] = GET_AI_IMAGE_STRIDE(image->width * 3);
      image->stride[1] = 0;
      image->stride[2] = 0;
      image->length[0] = image->stride[0] * image->height;
      image->length[1] = 0;
      image->length[2] = 0;
    } break;
    case PIXEL_FORMAT_RGB_888_PLANAR: {
      image->stride[0] = GET_AI_IMAGE_STRIDE(image->width);
      image->stride[1] = GET_AI_IMAGE_STRIDE(image->width);
      image->stride[2] = GET_AI_IMAGE_STRIDE(image->width);
      image->length[0] = image->stride[0] * image->height;
      image->length[1] = image->stride[1] * image->height;
      image->length[2] = image->stride[2] * image->height;
    } break;
    case PIXEL_FORMAT_NV21: {
      image->stride[0] = GET_AI_IMAGE_STRIDE(image->width);
      image->stride[1] = GET_AI_IMAGE_STRIDE(image->width);
      image->stride[2] = 0;
      image->length[0] = image->stride[0] * image->height;
      image->length[1] = image->stride[0] * (image->height >> 1);
      image->length[2] = 0;
    } break;
    case PIXEL_FORMAT_YUV_PLANAR_420: {
      image->stride[0] = GET_AI_IMAGE_STRIDE(image->width);
      image->stride[1] = GET_AI_IMAGE_STRIDE(image->width >> 1);
      image->stride[2] = GET_AI_IMAGE_STRIDE(image->width >> 1);
      image->length[0] = image->stride[0] * image->height;
      image->length[1] = image->stride[1] * (image->height >> 1);
      image->length[2] = image->stride[2] * (image->height >> 1);
    } break;
    default:
      LOGE("Currently unsupported format %u\n", fmt);
      return CVIAI_ERR_INVALID_ARGS;
  }

  uint32_t image_size = image->length[0] + image->length[1] + image->length[2];
  image->pix[0] = (uint8_t *)malloc(image_size);
  memset(image->pix[0], 0, image_size);
  switch (fmt) {
    case PIXEL_FORMAT_RGB_888: {
      image->pix[1] = NULL;
      image->pix[2] = NULL;
    } break;
    case PIXEL_FORMAT_RGB_888_PLANAR: {
      image->pix[1] = image->pix[0] + image->length[0];
      image->pix[2] = image->pix[1] + image->length[1];
    } break;
    case PIXEL_FORMAT_NV21: {
      image->pix[1] = image->pix[0] + image->length[0];
      image->pix[2] = NULL;
    } break;
    case PIXEL_FORMAT_YUV_PLANAR_420: {
      image->pix[1] = image->pix[0] + image->length[0];
      image->pix[2] = image->pix[1] + image->length[1];
    } break;
    default:
      LOGE("Currently unsupported format %u\n", fmt);
      return CVIAI_ERR_INVALID_ARGS;
  }

#if 0
  LOGI("[create image] format[%d], height[%u], width[%u], stride[%u][%u][%u], length[%u][%u][%u]\n",
       image->pix_format, image->height, image->width, image->stride[0], image->stride[1],
       image->stride[2], image->length[0], image->length[1], image->length[2]);
#endif

  return CVIAI_SUCCESS;
}

CVI_S32 CVI_AI_EstimateImageSize(uint64_t *size, uint32_t height, uint32_t width,
                                 PIXEL_FORMAT_E fmt) {
  *size = 0;
  switch (fmt) {
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_RGB_888_PLANAR: {
      *size += GET_AI_IMAGE_STRIDE(width * 3) * height;
    } break;
    case PIXEL_FORMAT_YUV_PLANAR_420:
    case PIXEL_FORMAT_NV21: {
      *size += GET_AI_IMAGE_STRIDE(width) * height;
      *size += GET_AI_IMAGE_STRIDE(width) * (height >> 1);
    } break;
    default:
      LOGE("Currently unsupported format %u\n", fmt);
      return CVIAI_ERR_INVALID_ARGS;
  }
  return CVIAI_SUCCESS;
}