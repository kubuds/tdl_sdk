#ifndef _CVIAI_APP_FACE_CAPTURE_TYPE_H_
#define _CVIAI_APP_FACE_CAPTURE_TYPE_H_

#include "capture_type.h"
#include "core/cviai_core.h"

typedef struct {
  cvai_face_info_t info;
  tracker_state_e state;
  uint32_t miss_counter;
  cvai_image_t image;

  bool _capture;
  uint64_t _timestamp;  // output timestamp
  uint32_t _out_counter;
  uint64_t cap_timestamp;   // frame id of the captured image
  int matched_gallery_idx;  // used for face recognition
} face_cpt_data_t;

typedef struct {
  int thr_size_min;
  int thr_size_max;
  int qa_method;
  float thr_quality;
  float thr_quality_high;
  float thr_yaw;
  float thr_pitch;
  float thr_roll;
  float thr_laplacian;
  uint32_t miss_time_limit;
  uint32_t fast_m_interval;
  uint32_t fast_m_capture_num;
  uint32_t cycle_m_interval;
  uint32_t auto_m_time_limit;
  bool auto_m_fast_cap;

  int img_capture_flag;  // 0:capture extended face,1:capture whole frame
  bool store_feature;
  bool store_RGB888;  // whether to crop image as RGB_PACKED
  bool store_frame;   // whether to store whole frame, if not
} face_capture_config_t;

typedef struct {
  capture_mode_e mode;
  face_capture_config_t cfg;

  uint32_t size;
  face_cpt_data_t *data;
  cvai_face_t last_faces;
  cvai_tracker_t last_trackers;
  CVI_AI_SUPPORTED_MODEL_E fd_model;
  int fr_flag;  // 0:only face detect and track,no fr,1:reid,2:no reid but fr for captured face
                // image,used for later face_recognition
  // bool do_FR;     /* don't set manually */
  bool use_FQNet; /* don't set manually */

  int (*fd_inference)(cviai_handle_t, VIDEO_FRAME_INFO_S *, cvai_face_t *);
  int (*fr_inference)(cviai_handle_t, VIDEO_FRAME_INFO_S *, cvai_face_t *);
  bool *_output;   // output signal (# = .size)
  uint64_t _time;  // timer
  uint32_t _m_limit;
} face_capture_t;

#endif  // End of _CVIAI_APP_FACE_CAPTURE_TYPE_H_
