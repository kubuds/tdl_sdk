#include "lfw.hpp"

#include <cvi_type.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int compare(const void *arg1, const void *arg2) {
  return static_cast<int>(*(float *)arg1 < *(float *)arg2);
}

namespace cviai {
namespace evaluation {

lfwEval::lfwEval(const char *fiilepath) { getEvalData(fiilepath); }

int lfwEval::getEvalData(const char *fiilepath) {
  FILE *fp;
  if ((fp = fopen(fiilepath, "r")) == NULL) {
    printf("file open error: %s!\n", fiilepath);
    return CVI_FAILURE;
  }
  m_data.clear();
  m_eval_label.clear();
  m_eval_score.clear();
  char line[1024];
  while (fscanf(fp, "%[^\n]", line) != EOF) {
    fgetc(fp);

    const char *delim = " ";
    char *label = strtok(line, delim);
    char *name1 = strtok(NULL, delim);
    char *name2 = strtok(NULL, delim);
    m_data.push_back({atoi(label), name1, name2});
  }
  m_eval_label.resize(m_data.size());
  m_eval_score.resize(m_data.size());
  fclose(fp);
  return CVI_SUCCESS;
}

uint32_t lfwEval::getTotalImage() { return m_data.size(); }

void lfwEval::getImageLabelPair(const int index, std::string *path1, std::string *path2,
                                int *label) {
  *path1 = m_data[index].path1;
  *path2 = m_data[index].path2;
  *label = m_data[index].label;
}

void lfwEval::insertFaceData(const int index, const int label, const cvai_face_t *face1,
                             const cvai_face_t *face2) {
  int face_idx1 = getMaxFace(face1);
  int face_idx2 = getMaxFace(face2);

  float feature_diff =
      evalDifference(&face1->info[face_idx1].face_feature, &face2->info[face_idx2].face_feature);
  float score = 1.0 - (0.5 * feature_diff);

  m_eval_label[index] = label;
  m_eval_score[index] = score;
}

void lfwEval::resetData() { m_data.clear(); }

void lfwEval::resetEvalData() {
  m_eval_label.clear();
  m_eval_score.clear();
  m_eval_label.resize(m_data.size());
  m_eval_score.resize(m_data.size());
}

void lfwEval::saveEval2File(const char *filepath) {
  evalAUC(m_eval_label, m_eval_score, m_data.size(), filepath);
}

int lfwEval::getMaxFace(const cvai_face_t *face) {
  int face_idx = 0;
  float max_area = 0;
  for (uint32_t i = 0; i < face->size; i++) {
    cvai_bbox_t bbox = face->info[i].bbox;
    float curr_area = (bbox.x2 - bbox.x1) * (bbox.y2 - bbox.y1);
    if (curr_area > max_area) {
      max_area = curr_area;
      face_idx = i;
    }
  }

  return face_idx;
}

float lfwEval::evalDifference(const cvai_feature_t *features1, const cvai_feature_t *features2) {
  int32_t value1 = 0, value2 = 0, value3 = 0;
  for (uint32_t i = 0; i < features1->size; i++) {
    value1 += (short)features1->ptr[i] * features1->ptr[i];
    value2 += (short)features2->ptr[i] * features2->ptr[i];
    value3 += (short)features1->ptr[i] * features2->ptr[i];
  }

  return 1 - ((float)value3 / (sqrt((double)value1) * sqrt((double)value2)));
}

void lfwEval::evalAUC(const std::vector<int> &y, std::vector<float> &pred, const uint32_t data_size,
                      const char *filepath) {
  int pos = 0;
  int neg = 0;
  float pred_sort[data_size];
  float max_value = 0;
  for (uint32_t i = 0; i < data_size; i++) {
    if (y[i] == 1) pos++;
    if (y[i] == 0) neg++;
    pred_sort[i] = pred[i];
    if (max_value < pred[i]) max_value = pred[i];
  }

  qsort((void *)pred_sort, data_size, sizeof(float), compare);
  printf("%f, %f\n", pred_sort[0], max_value);

  FILE *fp;
  if ((fp = fopen(filepath, "w")) == NULL) {
    printf("LFW result file open error!");
    return;
  }

  for (uint32_t i = 0; i < data_size; i++) {
    float thr = pred_sort[i];

    uint32_t tpn = 0, fpn = 0;
    for (uint32_t j = 0; j < data_size; j++) {
      if (pred[j] >= thr) {
        if (y[j] == 1) {
          tpn++;
        } else {
          fpn++;
        }
      }
    }

    float tpr = (float)tpn / pos;
    float fpr = (float)fpn / neg;
    fprintf(fp, "thr: %f, tpr: %f, fpr: %f\n", thr, tpr, fpr);
  }

  fclose(fp);
}
}  // namespace evaluation
}  // namespace cviai