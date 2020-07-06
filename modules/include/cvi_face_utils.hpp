#ifndef _CVI_FACE_UTILS_H_
#define _CVI_FACE_UTILS_H_

#include <vector>
#include "cvi_comm_video.h"
#include "cvi_face_types.hpp"

template <int DIM>
class FeatureVector final {
    public:
    std::vector<float> features;

    FeatureVector() : features(DIM, 0) {}
    FeatureVector(const FeatureVector &other) { *this = other; }

    inline size_t size() { return DIM; }

    FeatureVector &operator=(const FeatureVector &other) {
        features = std::vector<float>(other.features.begin(), other.features.end());
        return *this;
    }

    FeatureVector &operator=(FeatureVector &&other) {
        features.swap(other.features);
        return *this;
    }

    FeatureVector &operator+=(const FeatureVector &other) {
        for (size_t i = 0; i < DIM; i++) {
            features[i] += other.features[i];
        }
        return *this;
    }

    FeatureVector &operator-=(const FeatureVector &other) {
        for (size_t i = 0; i < DIM; i++) {
            features[i] -= other.features[i];
        }
        return *this;
    }

    FeatureVector &operator*=(const FeatureVector &other) {
        for (size_t i = 0; i < DIM; i++) {
            features[i] *= other.features[i];
        }
        return *this;
    }

    FeatureVector &operator/=(const float num) {
        for (size_t i = 0; i < DIM; i++) {
            features[i] /= num;
        }
        return *this;
    }

    inline bool operator==(const FeatureVector &rhs) const {
        for (int i = 0; i < DIM; i++) {
            if (this->features[i] != rhs.features[i]) {
                return false;
            }
        }
        return true;
    }

    inline bool operator!=(const FeatureVector &rhs) const { return !(*this == rhs); }
    inline float &operator[](int x) { return features[x]; }

    int max_idx() const {
        float tmp = features[0];
        int res = 0;
        for (int i = 0; i < DIM; i++) {
            if (features[i] > tmp) {
                tmp = features[i];
                res = i;
            }
        }
        return res;
    }
};

typedef FeatureVector<NUM_FACE_FEATURE_DIM> FaceFeature;
typedef FeatureVector<NUM_EMOTION_FEATURE_DIM> EmotionFeature;
typedef FeatureVector<NUM_GENDER_FEATURE_DIM> GenderFeature;
typedef FeatureVector<NUM_RACE_FEATURE_DIM> RaceFeature;
typedef FeatureVector<NUM_AGE_FEATURE_DIM> AgeFeature;

class FaceAttributeInfo final {
    public:
    FaceAttributeInfo() = default;
    FaceAttributeInfo(FaceAttributeInfo &&other) { *this = std::move(other); }

    FaceAttributeInfo(const FaceAttributeInfo &other) { *this = other; }

    FaceAttributeInfo &operator=(const FaceAttributeInfo &other) {
        this->face_feature = other.face_feature;
        this->emotion_prob = other.emotion_prob;
        this->gender_prob = other.gender_prob;
        this->race_prob = other.race_prob;
        this->age_prob = other.age_prob;
        this->emotion = other.emotion;
        this->gender = other.gender;
        this->race = other.race;
        this->age = other.age;
        return *this;
    }

    FaceAttributeInfo &operator=(FaceAttributeInfo &&other) {
        this->face_feature = std::move(other.face_feature);
        this->emotion_prob = std::move(other.emotion_prob);
        this->gender_prob = std::move(other.gender_prob);
        this->race_prob = std::move(other.race_prob);
        this->age_prob = std::move(other.age_prob);
        this->emotion = other.emotion;
        this->gender = other.gender;
        this->race = other.race;
        this->age = other.age;
        return *this;
    }

    inline bool operator==(const FaceAttributeInfo &rhs) const {
        if (this->face_feature != rhs.face_feature || this->emotion_prob != rhs.emotion_prob ||
            this->gender_prob != rhs.gender_prob || this->race_prob != rhs.race_prob ||
            this->age_prob != rhs.age_prob || this->emotion != rhs.emotion ||
            this->gender != rhs.gender || this->race != rhs.race || this->age != rhs.age) {
            return false;
        }
        return true;
    }

    inline bool operator!=(const FaceAttributeInfo &rhs) const { return !(*this == rhs); }

    FaceFeature face_feature;
    EmotionFeature emotion_prob;
    GenderFeature gender_prob;
    RaceFeature race_prob;
    AgeFeature age_prob;
    FaceEmotion emotion = EMOTION_UNKNOWN;
    FaceGender gender = GENDER_UNKNOWN;
    FaceRace race = RACE_UNKNOWN;
    float age = -1.0;
};

inline std::string GetEmotionString(FaceEmotion emotion) {
    switch (emotion) {
        case EMOTION_UNKNOWN:
            return std::string("Unknown");
        case EMOTION_HAPPY:
            return std::string("Happy");
        case EMOTION_SURPRISE:
            return std::string("Surprise");
        case EMOTION_FEAR:
            return std::string("Fear");
        case EMOTION_DISGUST:
            return std::string("Disgust");
        case EMOTION_SAD:
            return std::string("Sad");
        case EMOTION_ANGER:
            return std::string("Anger");
        case EMOTION_NEUTRAL:
            return std::string("Neutral");
        default:
            return std::string("Unknown");
    }
}

inline std::string GetGenderString(FaceGender gender) {
    switch (gender) {
        case GENDER_UNKNOWN:
            return std::string("Unknown");
        case GENDER_MALE:
            return std::string("Male");
        case GENDER_FEMALE:
            return std::string("Female");
        default:
            return std::string("Unknown");
    }
}

inline std::string GetRaceString(FaceRace race) {
    switch (race) {
        case RACE_UNKNOWN:
            return std::string("Unknown");
        case RACE_CAUCASIAN:
            return std::string("Caucasian");
        case RACE_BLACK:
            return std::string("Black");
        case RACE_ASIAN:
            return std::string("Asian");
        default:
            return std::string("Unknown");
    }
}

cvi_face_info_t bbox_rescale(VIDEO_FRAME_INFO_S *frame, cvi_face_t *face_meta, int face_idx);
int face_align(const cv::Mat &image, cv::Mat &aligned, const cvi_face_info_t &face_info, int width,
               int height);
void SoftMaxForBuffer(float *src, float *dst, size_t size);
void Dequantize(int8_t *q_data, float *data, float threshold, size_t size);
void NonMaximumSuppression(std::vector<cvi_face_info_t> &bboxes, std::vector<cvi_face_info_t> &bboxes_nms,
                           float threshold, char method);
void clip_boxes(int width, int height, cvi_face_detect_rect_t &box);

inline float FastExp(float x) {
    union {
        unsigned int i;
        float f;
    } v;

    v.i = (1 << 23) * (1.4426950409 * x + 126.93490512f);

    return v.f;
}

#endif