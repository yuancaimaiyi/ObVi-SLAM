//
// Created by amanda on 2/10/23.
//

#ifndef UT_VSLAM_LONG_TERM_MAP_EXTRACTION_TUNABLE_PARAMS_H
#define UT_VSLAM_LONG_TERM_MAP_EXTRACTION_TUNABLE_PARAMS_H
namespace vslam_types_refactor {

// static const double kFarFeatureThreshold = 75;

struct LongTermMapExtractionTunableParams {
  double far_feature_threshold_ = 75;  // Default overwritten by config

  bool operator==(const LongTermMapExtractionTunableParams &rhs) const {
    return (far_feature_threshold_ == rhs.far_feature_threshold_);
  }

  bool operator!=(const LongTermMapExtractionTunableParams &rhs) const {
    return !operator==(rhs);
  }
};
}  // namespace vslam_types_refactor

#endif  // UT_VSLAM_LONG_TERM_MAP_EXTRACTION_TUNABLE_PARAMS_H