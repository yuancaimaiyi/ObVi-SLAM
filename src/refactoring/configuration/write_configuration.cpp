//
// Created by amanda on 10/10/22.
//

#include <file_io/cv_file_storage/config_file_storage_io.h>
#include <file_io/file_access_utils.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <refactoring/configuration/full_ov_slam_config.h>

DEFINE_string(config_directory, "config/", "Directory for configuration files");
DEFINE_string(
    config_identifier,
    "",
    "Recommended: Leave this blank -- if blank, the version number embedded in "
    "this class will be used. Update the verion number in code each time you "
    "update the config. Allowing override via command line for flexibility, "
    "but this is not recommended. Description: Identifier for the"
    " configuration to write. Used as file name and included in config file.");

using namespace vslam_types_refactor;

ShapeDimensionPriors constructShapeDimPriorConfiguration() {
  ShapeDimensionPriors shape_dim_priors;
  std::unordered_map<std::string,
                     std::pair<ObjectDim<double>, ObjectDim<double>>>
      shape_mean_and_std_devs_by_semantic_class;
  Eigen::Vector3d chair_mean(0.62, 0.62, 0.975);
  Eigen::Vector3d chair_std_dev(0.05, 0.05, 0.05);
  std::string chair_class = "chair";
  shape_mean_and_std_devs_by_semantic_class[chair_class] =
      std::make_pair(chair_mean, chair_std_dev);

  Eigen::Vector3d cone_mean(0.29, 0.29, 0.48);
  Eigen::Vector3d cone_std_dev(0.001, 0.001, 0.01);
  std::string cone_class = "roadblock";
  shape_mean_and_std_devs_by_semantic_class[cone_class] =
      std::make_pair(cone_mean, cone_std_dev);

  Eigen::Vector3d tree_mean(.40, .40, 2);
  Eigen::Vector3d tree_cov(0.2, 0.2, 3);
  std::string tree_class = "treetrunk";
  shape_mean_and_std_devs_by_semantic_class[tree_class] =
      std::make_pair(tree_mean, tree_cov);

  Eigen::Vector3d lamppost_mean(.3, .3, 4);
  Eigen::Vector3d lamppost_cov(0.15, 0.15, 3);
  std::string lamppost_class = "lamppost";
  shape_mean_and_std_devs_by_semantic_class[lamppost_class] =
      std::make_pair(lamppost_mean, lamppost_cov);

  Eigen::Vector3d bench_mean(1, 2.5, 1.5);
  Eigen::Vector3d bench_cov(1.5, 2, 1.5);
  std::string bench_class = "bench";
  shape_mean_and_std_devs_by_semantic_class[bench_class] =
      std::make_pair(bench_mean, bench_cov);

  Eigen::Vector3d trashcan_mean(1, 1, 1.5);
  Eigen::Vector3d trashcan_cov(1, 1, 1.5);
  std::string trashcan_class = "trashcan";
  shape_mean_and_std_devs_by_semantic_class[trashcan_class] =
      std::make_pair(trashcan_mean, trashcan_cov);
  for (const auto &shape_mean_and_std_dev_for_class :
       shape_mean_and_std_devs_by_semantic_class) {
    shape_dim_priors.mean_and_cov_by_semantic_class_
        [shape_mean_and_std_dev_for_class.first] =
        std::make_pair(shape_mean_and_std_dev_for_class.second.first,
                       createDiagCovFromStdDevs(
                           shape_mean_and_std_dev_for_class.second.second));
  }
  return shape_dim_priors;
}

BoundingBoxFrontEndParams generateBoundingBoxFrontEndParams(
    const pose_graph_optimization::ObjectResidualParams
        &baseline_obj_residual_params,
    const pose_graph_optimization::OptimizationSolverParams
        &baseline_solver_params) {
  BoundingBoxFrontEndParams params;
  params.geometric_similarity_scorer_params_.max_merge_distance_ = 4;
  params.geometric_similarity_scorer_params_.x_y_only_merge_ = true;

  PendingObjectEstimatorParams pending_obj_estimator_params;
  pending_obj_estimator_params.object_residual_params_ =
      baseline_obj_residual_params;
  pending_obj_estimator_params.solver_params_ = baseline_solver_params;
  pending_obj_estimator_params.solver_params_.max_num_iterations_ = 500;

  FeatureBasedBbAssociationParams bb_assoc_front_end_params;
  bb_assoc_front_end_params.discard_candidate_after_num_frames_ = 40;
  bb_assoc_front_end_params.feature_validity_window_ = 20;

  // TODO tune/override
  bb_assoc_front_end_params.min_observations_for_local_est_ = 3;
  bb_assoc_front_end_params.min_observations_ = 10;
  bb_assoc_front_end_params.min_bb_confidence_ = 0.2;
  bb_assoc_front_end_params.required_min_conf_for_initialization_ = 0;
  bb_assoc_front_end_params.min_overlapping_features_for_match_ = 3;
  bb_assoc_front_end_params.bounding_box_inflation_size_ = 10;
  bb_assoc_front_end_params.pending_obj_estimator_params_ =
      pending_obj_estimator_params;

  params.feature_based_bb_association_params_ = bb_assoc_front_end_params;

  // Post session merge
  params.post_session_object_merge_params_.max_merge_distance_ = -1;
  params.post_session_object_merge_params_.x_y_only_merge_ = true;

  return params;
}

BoundingBoxCovGenParams generateCovGenParams() {
  BoundingBoxCovGenParams cov_gen_params;

  // TODO tune these
  double bb_corner_std_dev = 30;
  Eigen::Vector4d bb_corner_std_devs =
      bb_corner_std_dev * Eigen::Vector4d::Ones();
  cov_gen_params.bounding_box_cov_ =
      createDiagCovFromStdDevs(bb_corner_std_devs);
  cov_gen_params.near_edge_threshold_ = 25;
  cov_gen_params.image_boundary_variance_ = pow(200.0, 2.0);

  return cov_gen_params;
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, false);

  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;

  if (FLAGS_config_directory.empty()) {
    LOG(INFO) << "Need directory to write config value to";
    exit(1);
  }

  std::string config_identifier = FLAGS_config_identifier;
  if (config_identifier.empty()) {
    // TODO INCREMENT IF YOU CHANGE VALUES/STRUCTURE FOR CONFIG
    int config_version_number = 12;

    config_identifier = std::to_string(config_version_number);
  }

  FullOVSLAMConfig configuration;
  configuration.config_version_id_ = config_identifier;

  // Set the visual feature params
  configuration.visual_feature_params_.reprojection_error_std_dev_ = 2;
  configuration.visual_feature_params_
      .min_visual_feature_parallax_pixel_requirement_ = 5.0;
  configuration.visual_feature_params_
      .min_visual_feature_parallax_robot_transl_requirement_ = 0.1;
  configuration.visual_feature_params_
      .min_visual_feature_parallax_robot_orient_requirement_ = 0.05;
  configuration.visual_feature_params_.enforce_min_pixel_parallax_requirement_ =
      true;
  configuration.visual_feature_params_
      .enforce_min_robot_pose_parallax_requirement_ = true;
  configuration.visual_feature_params_.inlier_epipolar_err_thresh_ = 8.0;
  configuration.visual_feature_params_.check_past_n_frames_for_epipolar_err_ =
      5;
  configuration.visual_feature_params_.enforce_epipolar_error_requirement_ =
      true;

  // Set up defaults for the different types of optimization that can be
  // overridden later
  pose_graph_optimization::OptimizationSolverParams base_solver_params;
  pose_graph_optimization::OptimizationIterationParams base_iteration_params;
  base_solver_params.max_num_iterations_ = 100;
  base_solver_params.allow_non_monotonic_steps_ = true;
  base_solver_params.function_tolerance_ = 1e-6;          // Ceres default
  base_solver_params.gradient_tolerance_ = 1e-10;         // Ceres default
  base_solver_params.parameter_tolerance_ = 1e-8;         // Ceres default
  base_solver_params.initial_trust_region_radius_ = 1e4;  // Ceres default
  base_solver_params.max_trust_region_radius_ = 1e16;     // Ceres default
  base_iteration_params.phase_one_opt_params_ = base_solver_params;
  base_iteration_params.phase_two_opt_params_ = base_solver_params;
  base_iteration_params.feature_outlier_percentage_ = .1;
  base_iteration_params.allow_reversion_after_detecting_jumps_ = true;
  base_iteration_params.consecutive_pose_transl_tol_ = 1.0;
  base_iteration_params.consecutive_pose_orient_tol_ = M_PI;

  // TODO modify convergence thresholds
  pose_graph_optimization::OptimizationSolverParams local_ba_solver_params =
      base_solver_params;
  local_ba_solver_params.max_num_iterations_ = 200;
  local_ba_solver_params.initial_trust_region_radius_ = 1e2;
  local_ba_solver_params.max_trust_region_radius_ = 1e4;
  pose_graph_optimization::OptimizationIterationParams
      local_ba_iteration_params = base_iteration_params;
  local_ba_iteration_params.phase_one_opt_params_ = local_ba_solver_params;
  local_ba_iteration_params.phase_two_opt_params_ = local_ba_solver_params;

  pose_graph_optimization::OptimizationSolverParams global_ba_solver_params =
      base_solver_params;
  global_ba_solver_params.max_num_iterations_ = 250;
  global_ba_solver_params.initial_trust_region_radius_ = 1e2;
  global_ba_solver_params.max_trust_region_radius_ = 1e4;
  pose_graph_optimization::OptimizationIterationParams
      global_ba_iteration_params = base_iteration_params;
  global_ba_iteration_params.phase_one_opt_params_ = global_ba_solver_params;
  global_ba_iteration_params.phase_two_opt_params_ = global_ba_solver_params;

  pose_graph_optimization::OptimizationSolverParams final_opt_solver_params =
      base_solver_params;
  final_opt_solver_params.max_num_iterations_ = 300;
  final_opt_solver_params.initial_trust_region_radius_ = 1e2;
  final_opt_solver_params.max_trust_region_radius_ = 1e4;
  pose_graph_optimization::OptimizationIterationParams
      final_ba_iteration_params = base_iteration_params;
  final_ba_iteration_params.phase_one_opt_params_ = final_opt_solver_params;
  final_ba_iteration_params.phase_two_opt_params_ = final_opt_solver_params;

  configuration.local_ba_iteration_params_ = local_ba_iteration_params;
  configuration.global_ba_iteration_params_ = global_ba_iteration_params;
  configuration.final_ba_iteration_params_ = final_ba_iteration_params;

  configuration.pgo_solver_params_.relative_pose_factor_huber_loss_ = 5;
  configuration.pgo_solver_params_.pgo_optimization_solver_params_ =
      global_ba_solver_params;
  configuration.pgo_solver_params_.final_pgo_optimization_solver_params_ =
      final_opt_solver_params;
  pose_graph_optimization::RelativePoseCovarianceOdomModelParams
      relative_pose_cov_params;
  relative_pose_cov_params.transl_error_mult_for_transl_error_ = 0.1;
  relative_pose_cov_params.transl_error_mult_for_rot_error_ = 0.1;
  relative_pose_cov_params.rot_error_mult_for_transl_error_ = 0.1;
  relative_pose_cov_params.rot_error_mult_for_rot_error_ = 0.1;
  configuration.pgo_solver_params_.relative_pose_cov_params_ =
      relative_pose_cov_params;
  configuration.pgo_solver_params_.enable_visual_feats_only_opt_post_pgo_ =
      false;
  configuration.pgo_solver_params_
      .enable_visual_non_opt_feature_adjustment_post_pgo_ = false;

  configuration.ltm_tunable_params_.far_feature_threshold_ = 75;
  configuration.ltm_tunable_params_.min_col_norm_ = 5e-9;
  configuration.ltm_solver_params_ =
      base_solver_params;  // TODO is this the one we want?
  configuration.ltm_tunable_params_.fallback_to_prev_for_failed_extraction_ = true;
  configuration.ltm_solver_params_.max_num_iterations_ = 300;

  pose_graph_optimization::ObjectVisualPoseGraphResidualParams residual_params;
  residual_params.object_residual_params_.object_observation_huber_loss_param_ =
      0.5;
  residual_params.object_residual_params_
      .shape_dim_prior_factor_huber_loss_param_ = 10;
  residual_params.object_residual_params_.invalid_ellipsoid_error_val_ = 1e3;
  residual_params.visual_residual_params_.reprojection_error_huber_loss_param_ =
      1;
  residual_params.long_term_map_params_.pair_huber_loss_param_ = 1;

  configuration.ltm_solver_residual_params_ = residual_params;

  residual_params.relative_pose_factor_huber_loss_ = 1.0;
  residual_params.relative_pose_cov_params_
      .transl_error_mult_for_transl_error_ = 0.025;
  residual_params.relative_pose_cov_params_.transl_error_mult_for_rot_error_ =
      0.025;
  residual_params.relative_pose_cov_params_.rot_error_mult_for_transl_error_ =
      0.025;
  residual_params.relative_pose_cov_params_.rot_error_mult_for_rot_error_ =
      0.025;
  configuration.object_visual_pose_graph_residual_params_ = residual_params;

  configuration.shape_dimension_priors_ = constructShapeDimPriorConfiguration();

  configuration.camera_info_.camera_topic_to_camera_id_ = {
      {"/zed/zed_node/left/image_rect_color/compressed", 1},
      {"/zed/zed_node/left/image_rect_color", 1},
      {"/zed/zed_node/right/image_rect_color/compressed", 2},
      {"/zed/zed_node/right/image_rect_color", 2},
      {"/zed2i/zed_node/left/image_rect_color/compressed", 1},
      {"/zed2i/zed_node/left/image_rect_color", 1},
      {"/zed2i/zed_node/right/image_rect_color/compressed", 2},
      {"/zed2i/zed_node/right/image_rect_color", 2}};

  configuration.bounding_box_front_end_params_ =
      generateBoundingBoxFrontEndParams(residual_params.object_residual_params_,
                                        base_solver_params);

  configuration.bounding_box_covariance_generator_params_ =
      generateCovGenParams();

  configuration.sliding_window_params_.local_ba_window_size_ = 50;
  configuration.sliding_window_params_.global_ba_frequency_ = 30;

  configuration.sparsifier_params_.max_pose_inc_threshold_transl_ = 0.2;
  configuration.sparsifier_params_.max_pose_inc_threshold_rot_ = 0.1;

  pose_graph_optimizer::OptimizationFactorsEnabledParams
      optimization_factors_enabled_params;

  optimization_factors_enabled_params
      .min_low_level_feature_observations_per_frame_ = 50;

  optimization_factors_enabled_params.use_pom_ = false;
  optimization_factors_enabled_params.include_visual_factors_ = true;
  //  optimization_factors_enabled_params.fix_poses_ = true;
  optimization_factors_enabled_params.fix_poses_ = false;
  optimization_factors_enabled_params.fix_visual_features_ = false;
  optimization_factors_enabled_params.fix_objects_ = false;
  optimization_factors_enabled_params.poses_prior_to_window_to_keep_constant_ =
      5;
  // adding larger min_low_level_feature_observations_ for stereo camera
  optimization_factors_enabled_params.min_low_level_feature_observations_ = 5;
  optimization_factors_enabled_params.min_object_observations_ = 10;
  optimization_factors_enabled_params.use_visual_features_on_global_ba_ = false;
  optimization_factors_enabled_params.use_pose_graph_on_global_ba_ = true;
  optimization_factors_enabled_params.use_pose_graph_on_final_global_ba_ =
      optimization_factors_enabled_params.use_pose_graph_on_global_ba_;
  optimization_factors_enabled_params.use_visual_features_on_final_global_ba_ =
      true;

  configuration.optimization_factors_enabled_params_ =
      optimization_factors_enabled_params;

  // These should only be modified when debugging -- should not be used in
  // normal operation
  configuration.limit_traj_eval_params_.should_limit_trajectory_evaluation_ =
      false;
  configuration.limit_traj_eval_params_.max_frame_id_ = 1;

  // TODO consider logging warning if there is already a config with this
  // identifer in the directory

  std::string config_file_name =
      file_io::ensureDirectoryPathEndsWithSlash(FLAGS_config_directory) +
      config_identifier + file_io::kJsonExtension;
  writeConfiguration(config_file_name, configuration);
}