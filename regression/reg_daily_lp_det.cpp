#include <gtest.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include "core/utils/vpss_helper.h"
#include "cvi_tdl.h"
#include "cvi_tdl_evaluation.h"
#include "cvi_tdl_media.h"
#include "cvi_tdl_test.hpp"
#include "json.hpp"
#include "raii.hpp"
#include "regression_utils.hpp"

namespace fs = std::experimental::filesystem;
namespace cvitdl {
namespace unitest {

class LicensePlateDetectionV2TestSuite : public CVI_TDLModelTestSuite {
 public:
  LicensePlateDetectionV2TestSuite()
      : CVI_TDLModelTestSuite("reg_daily_licenseplate.json", "reg_daily_lpd") {}

  virtual ~LicensePlateDetectionV2TestSuite() = default;

  std::string m_model_path;

 protected:
  virtual void SetUp() {
    ASSERT_EQ(CVI_TDL_CreateHandle2(&m_tdl_handle, 1, 0), CVI_TDL_SUCCESS);
    ASSERT_EQ(CVI_TDL_SetVpssTimeout(m_tdl_handle, 1000), CVI_TDL_SUCCESS);
  }

  virtual void TearDown() {
    CVI_TDL_DestroyHandle(m_tdl_handle);
    m_tdl_handle = NULL;
  }
};

TEST_F(LicensePlateDetectionV2TestSuite, open_close_model) {
  for (size_t test_index = 0; test_index < m_json_object.size(); test_index++) {
    std::string model_name = std::string(m_json_object[test_index]["model"]);
    m_model_path = (m_model_dir / fs::path(model_name)).string();

    TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_LP_DETECTION,
                             m_model_path.c_str(), false);
    ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

    const char *model_path_get =
        CVI_TDL_GetModelPath(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_LP_DETECTION);

    EXPECT_PRED2([](auto s1, auto s2) { return s1 == s2; }, m_model_path,
                 std::string(model_path_get));
  }
}

TEST_F(LicensePlateDetectionV2TestSuite, inference) {
  for (size_t test_index = 0; test_index < m_json_object.size(); test_index++) {
    std::string model_name = std::string(m_json_object[test_index]["model"]);
    m_model_path = (m_model_dir / fs::path(model_name)).string();

    TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_LP_DETECTION,
                             m_model_path.c_str(), false);
    ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

    for (int img_idx = 0; img_idx < 1; img_idx++) {
      // select image_0 for test
      std::string image_path =
          (m_image_dir / std::string(m_json_object[test_index]["test_images"][img_idx])).string();

      {
        Image frame(image_path, PIXEL_FORMAT_RGB_888_PLANAR);
        ASSERT_TRUE(frame.open());

        cvtdl_object_t obj_meta;
        memset(&obj_meta, 0, sizeof(cvtdl_object_t));
        EXPECT_EQ(CVI_TDL_License_Plate_Detectionv2(m_tdl_handle, frame.getFrame(), &obj_meta),
                  CVI_TDL_SUCCESS);
      }

      {
        Image frame(image_path, PIXEL_FORMAT_BGR_888);
        ASSERT_TRUE(frame.open());

        cvtdl_object_t obj_meta;
        memset(&obj_meta, 0, sizeof(cvtdl_object_t));
        EXPECT_EQ(CVI_TDL_License_Plate_Detectionv2(m_tdl_handle, frame.getFrame(), &obj_meta),
                  CVI_TDL_SUCCESS);
      }
    }
  }
}

TEST_F(LicensePlateDetectionV2TestSuite, accruacy) {
  for (size_t test_index = 0; test_index < m_json_object.size(); test_index++) {
    std::string model_name = std::string(m_json_object[test_index]["model"]);
    m_model_path = (m_model_dir / fs::path(model_name)).string();

    TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_LP_DETECTION,
                             m_model_path.c_str(), false);
    ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

    int img_num = int(m_json_object[test_index]["test_images"].size());
    float threshold = float(m_json_object[test_index]["threshold"]);

    for (int img_idx = 0; img_idx < img_num; img_idx++) {
      std::string image_path =
          (m_image_dir / std::string(m_json_object[test_index]["test_images"][img_idx])).string();

      Image frame(image_path, PIXEL_FORMAT_BGR_888);
      ASSERT_TRUE(frame.open());
      VIDEO_FRAME_INFO_S *vframe = frame.getFrame();

      TDLObject<cvtdl_object_t> obj_meta;
      init_obj_meta(obj_meta, 1, vframe->stVFrame.u32Height, vframe->stVFrame.u32Width, 0);
      EXPECT_EQ(CVI_TDL_License_Plate_Detectionv2(m_tdl_handle, vframe, obj_meta), CVI_TDL_SUCCESS);

#if 0
      int expected_value =
        int(m_json_object[test_index]["expected_results"][img_idx][0]);
      EXPECT_EQ(obj_meta->size, expected_value);

      for (uint32_t i = 0; i < obj_meta->size; i++) {
        float expected_res_x1 =
            float(m_json_object[test_index]["expected_results"][img_idx][1][i][0]);
        float expected_res_y1 =
            float(m_json_object[test_index]["expected_results"][img_idx][1][i][1]);
        float expected_res_x2 =
            float(m_json_object[test_index]["expected_results"][img_idx][1][i][2]);
        float expected_res_y2 =
            float(m_json_object[test_index]["expected_results"][img_idx][1][i][3]);

        cvtdl_bbox_t expected_bbox = {
            .x1 = expected_res_x1,
            .y1 = expected_res_y1,
            .x2 = expected_res_x2,
            .y2 = expected_res_y2,
        };

        auto comp = [=](cvtdl_face_info_t &pred, cvtdl_bbox_t &expected) {
          if (iou(pred.bbox, expected) >= threshold) {
            return true;
          }
          return false;
        };

        bool matched = match_dets(*obj_meta, expected_bbox, comp);
        auto &bbox = obj_meta->info[i].bbox;
        EXPECT_TRUE(matched) << "image path: " << image_path << "\n"
                             << "model path: " << m_model_path << "\n"
                             << "infer bbox: (" << bbox.x1 << ", " << bbox.y1
                             << bbox.x2 << ", " << bbox.y2 <<")\n"
                             << "expected bbox: (" << expected_bbox.x1 << ", " << expected_bbox.y1
                             << ", " << expected_bbox.x2 << ", " << expected_bbox.y2 << ")\n";
      }
#endif
      CVI_TDL_FreeCpp(obj_meta);
    }
  }
}

}  // namespace unitest
}  // namespace cvitdl
