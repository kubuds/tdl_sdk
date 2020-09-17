#include "core/cviai_core.h"

#include "core/utils/vpss_helper.h"
#include "opencv2/opencv.hpp"

#include <syslog.h>

inline void BufferRGBPackedCopy(const uint8_t *buffer, uint32_t width, uint32_t height,
                                uint32_t stride, VIDEO_FRAME_INFO_S *frame, bool invert) {
  VIDEO_FRAME_S *vFrame = &frame->stVFrame;
  if (invert) {
    for (uint32_t j = 0; j < height; j++) {
      const uint8_t *ptr = buffer + j * stride;
      for (uint32_t i = 0; i < width; i++) {
        uint32_t offset = i * 3 + j * vFrame->u32Stride[0];
        const uint8_t *ptr_pxl = i * 3 + ptr;
        vFrame->pu8VirAddr[0][offset] = ptr_pxl[2];
        vFrame->pu8VirAddr[0][offset + 1] = ptr_pxl[1];
        vFrame->pu8VirAddr[0][offset + 2] = ptr_pxl[0];
      }
    }
  } else {
    for (uint32_t j = 0; j < height; j++) {
      const uint8_t *ptr = buffer + j * stride;
      for (uint32_t i = 0; i < width; i++) {
        uint32_t offset = i * 3 + j * vFrame->u32Stride[0];
        const uint8_t *ptr_pxl = i * 3 + ptr;
        vFrame->pu8VirAddr[0][offset] = ptr_pxl[0];
        vFrame->pu8VirAddr[0][offset + 1] = ptr_pxl[1];
        vFrame->pu8VirAddr[0][offset + 2] = ptr_pxl[2];
      }
    }
  }
}

inline void BufferRGBPacked2PlanarCopy(const uint8_t *buffer, uint32_t width, uint32_t height,
                                       uint32_t stride, VIDEO_FRAME_INFO_S *frame, bool invert) {
  VIDEO_FRAME_S *vFrame = &frame->stVFrame;
  if (invert) {
    for (uint32_t j = 0; j < height; j++) {
      const uint8_t *ptr = buffer + j * stride;
      for (uint32_t i = 0; i < width; i++) {
        const uint8_t *ptr_pxl = i * 3 + ptr;
        vFrame->pu8VirAddr[0][i + j * vFrame->u32Stride[0]] = ptr_pxl[0];
        vFrame->pu8VirAddr[1][i + j * vFrame->u32Stride[1]] = ptr_pxl[1];
        vFrame->pu8VirAddr[2][i + j * vFrame->u32Stride[2]] = ptr_pxl[2];
      }
    }
  } else {
    for (uint32_t j = 0; j < height; j++) {
      const uint8_t *ptr = buffer + j * stride;
      for (uint32_t i = 0; i < width; i++) {
        const uint8_t *ptr_pxl = i * 3 + ptr;
        vFrame->pu8VirAddr[0][i + j * vFrame->u32Stride[0]] = ptr_pxl[2];
        vFrame->pu8VirAddr[1][i + j * vFrame->u32Stride[1]] = ptr_pxl[1];
        vFrame->pu8VirAddr[2][i + j * vFrame->u32Stride[2]] = ptr_pxl[0];
      }
    }
  }
}

int CVI_AI_Buffer2VBFrame(const uint8_t *buffer, uint32_t width, uint32_t height, uint32_t stride,
                          const PIXEL_FORMAT_E inFormat, VB_BLK *blk, VIDEO_FRAME_INFO_S *frame,
                          const PIXEL_FORMAT_E outFormat) {
  if (CREATE_VBFRAME_HELPER(blk, frame, width, height, outFormat) != CVI_SUCCESS) {
    syslog(LOG_ERR, "Create VBFrame failed.\n");
    return CVI_FAILURE;
  }

  int ret = CVI_SUCCESS;
  if ((inFormat == PIXEL_FORMAT_RGB_888 && outFormat == PIXEL_FORMAT_BGR_888) ||
      (inFormat == PIXEL_FORMAT_BGR_888 && outFormat == PIXEL_FORMAT_RGB_888)) {
    BufferRGBPackedCopy(buffer, width, height, stride, frame, true);
  } else if ((inFormat == PIXEL_FORMAT_RGB_888 && outFormat == PIXEL_FORMAT_RGB_888) ||
             (inFormat == PIXEL_FORMAT_BGR_888 && outFormat == PIXEL_FORMAT_BGR_888)) {
    BufferRGBPackedCopy(buffer, width, height, stride, frame, false);
  } else if (inFormat == PIXEL_FORMAT_BGR_888 && outFormat == PIXEL_FORMAT_RGB_888_PLANAR) {
    BufferRGBPacked2PlanarCopy(buffer, width, height, stride, frame, true);
  } else if (inFormat == PIXEL_FORMAT_RGB_888 && outFormat == PIXEL_FORMAT_RGB_888_PLANAR) {
    BufferRGBPacked2PlanarCopy(buffer, width, height, stride, frame, false);
  } else {
    syslog(LOG_ERR, "Unsupported convert format: %u -> %u.\n", inFormat, outFormat);
    ret = CVI_FAILURE;
  }
  CACHED_VBFRAME_FLUSH_UNMAP(frame);
  return ret;
}

int CVI_AI_ReadImage(const char *filepath, VB_BLK *blk, VIDEO_FRAME_INFO_S *frame,
                     PIXEL_FORMAT_E format) {
  cv::Mat img = cv::imread(filepath);
  if (CREATE_VBFRAME_HELPER(blk, frame, img.cols, img.rows, format) != CVI_SUCCESS) {
    syslog(LOG_ERR, "Create VBFrame failed.\n");
    return CVI_FAILURE;
  }

  int ret = CVI_SUCCESS;
  switch (format) {
    case PIXEL_FORMAT_RGB_888: {
      BufferRGBPackedCopy(img.data, img.cols, img.rows, img.step, frame, true);
    } break;
    case PIXEL_FORMAT_BGR_888: {
      BufferRGBPackedCopy(img.data, img.cols, img.rows, img.step, frame, false);
    } break;
    case PIXEL_FORMAT_RGB_888_PLANAR: {
      BufferRGBPacked2PlanarCopy(img.data, img.cols, img.rows, img.step, frame, true);
    } break;
    default:
      syslog(LOG_ERR, "Unsupported format: %u.\n", format);
      ret = CVI_FAILURE;
      break;
  }
  CACHED_VBFRAME_FLUSH_UNMAP(frame);
  return ret;
}