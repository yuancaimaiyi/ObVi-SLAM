//
// Created by amanda on 2/10/23.
//

#ifndef UT_VSLAM_OPTIMIZATION_FACTORS_ENABLED_PARAMS_H
#define UT_VSLAM_OPTIMIZATION_FACTORS_ENABLED_PARAMS_H

#include <refactoring/optimization/object_pose_graph.h>
#include <refactoring/types/vslam_basic_types_refactor.h>

namespace pose_graph_optimizer {
struct OptimizationFactorsEnabledParams {
  // NOTE: If this structure is modified, increment the
  // kCurrentConfigSchemaVersion number in FullOVSLAMConfig.h NOTE: If the
  // default values here are modified, make sure any changes are reflected in
  // the config and if necessary, regenerate the config with a new
  // config_version_id_
  bool include_object_factors_ = true;
  bool include_visual_factors_ = true;
  bool fix_poses_ = true;
  bool fix_objects_ = true;
  bool fix_visual_features_ = true;
  bool fix_ltm_objects_ = false;
  bool use_pom_ = false;
  uint32_t poses_prior_to_window_to_keep_constant_ = 1;

  uint32_t min_object_observations_ = 1;
  uint32_t min_low_level_feature_observations_ = 3;

  bool operator==(const OptimizationFactorsEnabledParams &rhs) const {
    return (include_object_factors_ == rhs.include_object_factors_) &&
           (include_visual_factors_ == rhs.include_visual_factors_) &&
           (fix_poses_ == rhs.fix_poses_) &&
           (fix_objects_ == rhs.fix_objects_) &&
           (fix_visual_features_ == rhs.fix_visual_features_) &&
           (fix_ltm_objects_ == rhs.fix_ltm_objects_) &&
           (use_pom_ == rhs.use_pom_) &&
           (poses_prior_to_window_to_keep_constant_ ==
            rhs.poses_prior_to_window_to_keep_constant_) &&
           (min_object_observations_ == rhs.min_object_observations_) &&
           (min_low_level_feature_observations_ ==
            rhs.min_low_level_feature_observations_);
  }

  bool operator!=(const OptimizationFactorsEnabledParams &rhs) const {
    return !operator==(rhs);
  }
};

struct OptimizationScopeParams {
  bool include_object_factors_;
  bool include_visual_factors_;
  bool fix_poses_;
  bool fix_objects_;
  bool fix_visual_features_;
  bool use_pom_;          // Effectively false if fix_objects_ is true
  bool fix_ltm_objects_;  // Effectively true if fix_objects_ is true
  uint32_t poses_prior_to_window_to_keep_constant_ =
      1;  // Should be min of 1, default to 1

  uint32_t min_object_observations_ = 1;
  uint32_t min_low_level_feature_observations_ = 3;

  // This will only filter out factors. A factor could still be excluded even if
  // not in this list if one of the other flags excludes it (ex.
  // include_visual_features)
  // TODO peraps include object factors/include visual factors should be merged
  // with this?
  std::unordered_set<vslam_types_refactor::FactorType>
      factor_types_to_exclude;  // Should be true for LTM extraction, false
  // otherwise.
  vslam_types_refactor::FrameId min_frame_id_;
  vslam_types_refactor::FrameId max_frame_id_;
  bool force_include_ltm_objs_ = false;
  // TODO consider adding set of nodes to optimize -- for now, we'll just assume
  // that we have min_frame_id (held constant) and all poses above that
};
}  // namespace pose_graph_optimizer

#endif  // UT_VSLAM_OPTIMIZATION_FACTORS_ENABLED_PARAMS_H