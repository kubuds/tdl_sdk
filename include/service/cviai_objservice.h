#ifndef _CVIAI_OBJSERVICE_H_
#define _CVIAI_OBJSERVICE_H_
#include "service/cviai_service_types.h"

#include "core/cviai_core.h"
#include "core/object/cvai_object_types.h"

#include <cvi_sys.h>

/**
 * @addtogroup core_cviaiobjservice CVIAI OBJService
 * @ingroup core_cviaiservice
 */

/** @typedef cviai_objservice_handle_t
 *  @ingroup core_cviaiobjservice
 *  @brief A cviai objservice handle.
 */
typedef void *cviai_objservice_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a cviai_objservice_handle_t.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJ Service handle.
 * @param ai_handle A cviai handle.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_CreateHandle(cviai_objservice_handle_t *handle,
                                                  cviai_handle_t ai_handle);

/**
 * @brief Destroy a cviai_objservice_handle_t.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJ Service handle.
 * @return CVI_S32 Return CVI_SUCCESS if success to destroy handle.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_DestroyHandle(cviai_objservice_handle_t handle);

/**
 * @brief Register a feature array to OBJ Service.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJ Service handle.
 * @param featureArray Input registered feature array.
 * @param method Set feature matching method.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_RegisterFeatureArray(
    cviai_objservice_handle_t handle, const cvai_service_feature_array_t featureArray,
    const cvai_service_feature_matching_e method);

/**
 * @brief Do a single cvai_object_info_t feature matching with registed feature array.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJ Service handle.
 * @param object_info The cvai_object_info_t from NN output with feature data.
 * @param k Output top k results.
 * @param index Output top k index.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_ObjectInfoMatching(cviai_objservice_handle_t handle,
                                                        const cvai_object_info_t *object_info,
                                                        const uint32_t k, uint32_t **index);

/**
 * @brief Do a single raw data with registed feature array.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJ Service handle.
 * @param feature Raw feature vector.
 * @param type The data type of the feature vector.
 * @param k Output top k results.
 * @param index Output top k index.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_RawMatching(cviai_objservice_handle_t handle,
                                                 const uint8_t *feature, const feature_type_e type,
                                                 const uint32_t k, uint32_t **index);
/**
 * @brief Zoom in to the largest face from the output of object detection results.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJ Service handle.
 * @param inFrame Input frame.
 * @param meta THe result from face detection.
 * @param obj_skip_ratio Skip the objects that are too small comparing to the area of the image.
 * Default is 0.05.
 * @param trans_ratio Change to zoom in ratio. Default is 0.1.
 * @param outFrame Output result image, will keep aspect ratio.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_DigitalZoom(
    cviai_objservice_handle_t handle, const VIDEO_FRAME_INFO_S *inFrame, const cvai_object_t *meta,
    const float obj_skip_ratio, const float trans_ratio, VIDEO_FRAME_INFO_S *outFrame);

/**
 * @brief Draw rect to YUV frame with given object meta.
 * @ingroup core_cviaiobjservice
 *
 * @param meta meta structure.
 * @param frame In/ out YUV frame.
 * @param drawText Choose to draw name of the object.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_DrawRect(const cvai_object_t *meta, VIDEO_FRAME_INFO_S *frame,
                                              const bool drawText);

/**
 * @brief Set intersect area for detection.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJService handle.
 * @param pts Intersect area or line. (pts must larger than 2 or more.)
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_SetIntersect(cviai_objservice_handle_t handle,
                                                  const cvai_pts_t *pts);

/**
 * @brief Check if the object intersected with the set area or line.
 * @ingroup core_cviaiobjservice
 *
 * @param handle An OBJService handle.
 * @param frame Input frame.
 * @param obj_meta Object meta structure.
 * @param status Output status of each object.
 * @return CVI_S32 Return CVI_SUCCESS if succeed.
 */
DLL_EXPORT CVI_S32 CVI_AI_OBJService_DetectIntersect(cviai_objservice_handle_t handle,
                                                     const VIDEO_FRAME_INFO_S *frame,
                                                     const cvai_object_t *obj_meta,
                                                     cvai_area_detect_e **status);
#ifdef __cplusplus
}
#endif

#endif  // End of _CVIAI_OBJSERVICE_H_