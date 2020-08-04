#include "cviai.h"

#include <string.h>

int main(void) {
  CVI_S32 ret = CVI_SUCCESS;
  cviai_handle_t handle;
  // Create handle
  if ((ret = CVI_AI_CreateHandle(&handle)) != CVI_SUCCESS) {
    printf("Handle create failed\n");
    return ret;
  }
  // This is a fake path. CVI_AI_SetModelPath will not actually read a model, it'll just
  // cache the path to the handle. By setting a fake path, we can see if the handle is
  // created successfully.
  const char fake_path[] = "face_quality.cvimodel";
  if ((ret = CVI_AI_SetModelPath(handle, CVI_AI_SUPPORTED_MODEL_FACEQUALITY, fake_path)) !=
      CVI_SUCCESS) {
    printf("Set model path failed.\n");
    return ret;
  }
  char *path = NULL;
  if ((ret = CVI_AI_GetModelPath(handle, CVI_AI_SUPPORTED_MODEL_FACEQUALITY, &path)) !=
      CVI_SUCCESS) {
    printf("Get model path failed.\n");
    return ret;
  }
  // Check if the two path are the same.
  if (strcmp(path, fake_path) != 0) {
    ret = CVI_FAILURE;
  }
  // Free the pointer created from CVI_AI_GetModelPath.
  free(path);
  // Destroy handle.
  ret = CVI_AI_DestroyHandle(handle);
  return ret;
}