#pragma once

#include "cviai_log.hpp"
#include "service/cviai_service_types.h"

#include <cvi_type.h>
#include <cvikernel/cvikernel.h>
#include <cvimath/cvimath.h>
#include <cviruntime_context.h>

namespace cviai {
namespace service {

typedef struct {
  CVI_RT_MEM rtmem = NULL;   // Set to NULL if not initialized
  uint64_t paddr = -1;       // Set to maximum of uint64_t if not initaulized
  uint8_t *vaddr = nullptr;  // Set to nullptr if not initualized
} rtinfo;

typedef struct {
  cvai_service_feature_array_t feature_array;
  float *feature_unit_length = nullptr;
  float *feature_array_buffer = nullptr;
} cvai_service_feature_array_ext_t;

typedef struct {
  uint32_t feature_length;
  uint32_t data_num;
  rtinfo feature_input;
  rtinfo feature_array;
  rtinfo buffer_array;
  size_t *slice_num = nullptr;
  float *feature_unit_length = nullptr;
  uint32_t *array_buffer_32 = nullptr;
  float *array_buffer_f = nullptr;
} cvai_service_feature_array_tpu_ext_t;

class FeatureMatching {
 public:
  ~FeatureMatching();
  int init();
  static int createHandle(CVI_RT_HANDLE *rt_handle, cvk_context_t **cvk_ctx);
  static int destroyHandle(CVI_RT_HANDLE rt_handle, cvk_context_t *cvk_ctx);
  int registerData(const cvai_service_feature_array_t &feature_array,
                   const cvai_service_feature_matching_e &matching_method);

  int run(const uint8_t *feature, const feature_type_e &type, const uint32_t k, uint32_t **index);

 private:
  int innerProductRegister(const cvai_service_feature_array_t &feature_array);
  int innerProductRun(const uint8_t *feature, const feature_type_e &type, const uint32_t k,
                      uint32_t *index);
  CVI_RT_HANDLE m_rt_handle;
  cvk_context_t *m_cvk_ctx = NULL;

  cvai_service_feature_matching_e m_matching_method;
  bool m_is_cpu = true;

  cvai_service_feature_array_tpu_ext_t m_tpu_ipfeature;
  cvai_service_feature_array_ext_t m_cpu_ipfeature;
};
}  // namespace service
}  // namespace cviai