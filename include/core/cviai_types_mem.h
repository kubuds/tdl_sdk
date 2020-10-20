#ifndef _CVIAI_TYPES_FREE_H_
#define _CVIAI_TYPES_FREE_H_
#include "core/core/cvai_core_types.h"
#include "core/face/cvai_face_types.h"
#include "core/object/cvai_object_types.h"

#ifdef __cplusplus
DLL_EXPORT void CVI_AI_FreeCpp(cvai_feature_t *feature);
DLL_EXPORT void CVI_AI_FreeCpp(cvai_pts_t *pts);
DLL_EXPORT void CVI_AI_FreeCpp(cvai_tracker_t *tracker);
DLL_EXPORT void CVI_AI_FreeCpp(cvai_face_info_t *face_info);
DLL_EXPORT void CVI_AI_FreeCpp(cvai_face_t *face);
DLL_EXPORT void CVI_AI_FreeCpp(cvai_object_info_t *obj_info);
DLL_EXPORT void CVI_AI_FreeCpp(cvai_object_t *obj);
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLL_EXPORT void CVI_AI_FreeFeature(cvai_feature_t *feature);
DLL_EXPORT void CVI_AI_FreePts(cvai_pts_t *pts);

DLL_EXPORT void CVI_AI_FreeTracker(cvai_tracker_t *tracker);

DLL_EXPORT void CVI_AI_FreeFaceInfo(cvai_face_info_t *face_info);
DLL_EXPORT void CVI_AI_FreeFace(cvai_face_t *face);

DLL_EXPORT void CVI_AI_FreeObjectInfo(cvai_object_info_t *obj_info);
DLL_EXPORT void CVI_AI_FreeObject(cvai_object_t *obj);

#ifdef __cplusplus
}
#endif
#endif