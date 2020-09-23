# Service Module

Service module is consist of FRService and OBJService.

## Feature Matching

Available: FRService, OBJService

FRService, OBJService provide feature matching tool to analyze the result from model that generates feature such as Face Attribute, OSNet. First, use ``RegisterFeatureArray`` to register a feature array for output comparison.

```c
CVI_S32 CVI_AI_FRService_RegisterFeatureArray(cviai_frservice_handle_t handle,
                                              const cvai_service_feature_array_t featureArray);

CVI_S32 CVI_AI_OBJService_RegisterFeatureArray(cviai_objservice_handle_t handle,
                                               const cvai_service_feature_array_t featureArray);
```

Second, use ``FaceInfoMatching`` or ``ObjectInfoMatching`` to match to output result with the feature array. The length of the top ``index`` equals to ``k``.

```c
CVI_S32 CVI_AI_FRService_FaceInfoMatching(cviai_frservice_handle_t handle, const cvai_face_t *face,
                                          const uint32_t k, uint32_t **index);

CVI_S32 CVI_AI_OBJService_ObjectInfoMatching(cviai_objservice_handle_t handle,
                                             const cvai_object_info_t *object_info, const uint32_t k,
                                             uint32_t **index);
```

The tool also provides raw feature comparison with the registered feature array. Currently only supports ``TYPE_INT8`` and ``TYPE_UINT8`` comparison.

```c
CVI_S32 CVI_AI_FRService_RawMatching(cviai_frservice_handle_t handle, const uint8_t *feature,
                                     const feature_type_e type, const uint32_t k, uint32_t **index);

CVI_S32 CVI_AI_OBJService_RawMatching(cviai_objservice_handle_t handle, const uint8_t *feature,
                                      const feature_type_e type, const uint32_t k,
                                      uint32_t **index);
```

## Digital Zoom

Available: FRService, OBJService

Digital Zoom is an effect tool that zooms in to the largest detected bounding box in a frame. Users can create zoom-in effect easily with this tool.

``face_skip_ratio`` or ``obj_skip_ratio`` is a threshold that skips the bounding box area smaller than the ``image_size * face_skip_ratio``. ``trans_ratio`` is a value that controls the zoom-in speed. The recommended initial value for these two parameters are ``0.05f`` and ``0.1f``.

```c
CVI_S32 CVI_AI_FRService_DigitalZoom(cviai_frservice_handle_t handle,
                                     const VIDEO_FRAME_INFO_S *inFrame, const cvai_face_t *meta,
                                     const float face_skip_ratio, const float trans_ratio,
                                     VIDEO_FRAME_INFO_S *outFrame);

CVI_S32 CVI_AI_OBJService_DigitalZoom(cviai_objservice_handle_t handle,
                                      const VIDEO_FRAME_INFO_S *inFrame, const cvai_object_t *meta,
                                      const float obj_skip_ratio, const float trans_ratio,
                                      VIDEO_FRAME_INFO_S *outFrame);
```

Related sample codes: ``sample_read_dt.c``

## Draw Rect

Available: FRService, OBJService

``DrawRect`` is a function that draws all the bounding boxes and their tag names on the frame.

```c
CVI_S32 CVI_AI_FRService_DrawRect(const cvai_face_t *meta, VIDEO_FRAME_INFO_S *frame);

CVI_S32 CVI_AI_OBJService_DrawRect(const cvai_object_t *meta, VIDEO_FRAME_INFO_S *frame);
```

Related sample codes: ``sample_vi_fd.c``, ``sample_vi_fq.c``, ``sample_vi_mask_fr.c``


## Object Intersect

Available: OBJService

The function provides user to draw a line or a polygon to see if the given sequental path interacts with the set region. If a polygon is given, the status will return if the input point is **on line**, **inside** or **outside** the polygon. If a line is given, the status will return if a vector is **no intersect**, **on line**, **towards negative**, or **towards positive**.

The status enumeration.

```c
typedef enum {
  UNKNOWN = 0,
  NO_INTERSECT,
  ON_LINE,
  CROSS_LINE_POS,
  CROSS_LINE_NEG,
  INSIDE_POLYGON,
  OUTSIDE_POLYGON
} cvai_area_detect_e;
```

### Setting Detect Region

The region is set using ``cvai_pts_t``. Note that the size of ``pts->size`` must larger than 2. The API will make the lines into a close loop if 3 more more points are found. The coordinate of the points cannot exceed the size of the given frame.

```c
CVI_S32 CVI_AI_OBJService_SetIntersect(cviai_objservice_handle_t handle,
                                       const VIDEO_FRAME_INFO_S *frame, const cvai_pts_t *pts);
```

The user has to give an object an unique id and a coordinate (x, y). The tracker inside the API will handle the rest.


```c
typedef struct {
  uint64_t unique_id;
  float x;
  float y;
} area_detect_pts_t;
```

The output is an ``cvai_area_detect_e`` array. Its size is equal to the ``input_length``. You'll need to free ``status`` after use to prevent memory leaks.

```c
CVI_S32 CVI_AI_OBJService_DetectIntersect(cviai_objservice_handle_t handle,
                                          const VIDEO_FRAME_INFO_S *frame,
                                          const area_detect_pts_t *input,
                                          const uint32_t input_length, cvai_area_detect_e **status);
```