#ifndef _CVIAI_APP_FACE_CAPTURE_TYPE_H_
#define _CVIAI_APP_FACE_CAPTURE_TYPE_H_

#include "capture_type.h"
#include "core/cviai_core.h"

typedef struct {
  cvai_face_info_t info;
  tracker_state_e state;
  uint32_t miss_counter;
  cvai_image_t image;
  cvai_bbox_t crop_box;  // box used to crop image
  bool _capture;
  uint64_t _timestamp;  // output timestamp
  uint32_t _out_counter;
  uint64_t cap_timestamp;       // frame id of the captured image
  uint64_t last_cap_timestamp;  // frame id of the last captured image
  int matched_gallery_idx;      // used for face recognition

} face_cpt_data_t;

typedef struct {
  int thr_size_min;
  int thr_size_max;
  int qa_method;  // 0:use posture (default),1:laplacian,2:posture+laplacian
  float thr_quality;
  float thr_yaw;
  float thr_pitch;
  float thr_roll;
  float thr_laplacian;
  uint32_t miss_time_limit;  // missed time limit,when reach this limit the track would be destroyed
  uint32_t m_interval;     // frame interval between current frame and last captured frame,if reach
                           // this val would send captured image out
  uint32_t m_capture_num;  // max capture times
  bool auto_m_fast_cap;    // if true,would send first image out in auto mode
  uint8_t venc_channel_id;
  int img_capture_flag;         // 0:capture extended face,1:capture whole frame
  bool store_feature;           // if true,would export face feature ,should set FR
  float eye_dist_thresh;        // default 20
  float landmark_score_thresh;  // default 0.5
} face_capture_config_t;

typedef struct {
  capture_mode_e mode;
  face_capture_config_t cfg;

  uint32_t size;
  face_cpt_data_t *data;
  cvai_face_t last_faces;
  cvai_object_t last_objects;  // if fuse with PD,would have PD result stored here
  cvai_tracker_t last_trackers;
  CVI_AI_SUPPORTED_MODEL_E fd_model;
  CVI_AI_SUPPORTED_MODEL_E od_model;
  CVI_AI_SUPPORTED_MODEL_E fl_model;

  int fr_flag;  // 0:only face detect and track,no fr,1:reid,2:no reid but fr for captured face
                // image,used for later face_recognition

  bool use_FQNet; /* don't set manually */

  int (*fd_inference)(cviai_handle_t, VIDEO_FRAME_INFO_S *, cvai_face_t *);
  int (*fr_inference)(cviai_handle_t, VIDEO_FRAME_INFO_S *, cvai_face_t *);
  bool *_output;   // output signal (# = .size)
  uint64_t _time;  // timer
  uint32_t _m_limit;

  CVI_U64 tmp_buf_physic_addr;
  CVI_VOID *p_tmp_buf_addr;
} face_capture_t;

#endif  // End of _CVIAI_APP_FACE_CAPTURE_TYPE_H_
