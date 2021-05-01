#include <ceres/ceres.h>
#include <pairwise_2d_feature_cost_functor.h>
#include <reprojection_cost_functor.h>
#include <slam_backend_solver.h>

namespace vslam_solver {

void RobotPosesToSLAMNodes(
    const std::vector<vslam_types::RobotPose> &robot_poses,
    std::vector<vslam_types::SLAMNode> &nodes) {
  for (const vslam_types::RobotPose &robot_pose : robot_poses) {
    nodes.emplace_back(FromRobotPose(robot_pose));
  }
}

void SLAMNodesToRobotPoses(const std::vector<vslam_types::SLAMNode> &slam_nodes,
                           std::vector<vslam_types::RobotPose> &updated_poses) {
  updated_poses.clear();
  for (const vslam_types::SLAMNode &node : slam_nodes) {
    updated_poses.emplace_back(FromSLAMNode(node));
  }
}

vslam_types::SLAMNode FromRobotPose(const vslam_types::RobotPose &robot_pose) {
  return vslam_types::SLAMNode(
      robot_pose.frame_idx, robot_pose.loc, robot_pose.angle);
}

vslam_types::RobotPose FromSLAMNode(const vslam_types::SLAMNode &slam_node) {
  Eigen::Vector3f rotation_axis(
      slam_node.pose[3], slam_node.pose[4], slam_node.pose[5]);

  Eigen::AngleAxisf rotation_aa = vslam_types::VectorToAxisAngle(rotation_axis);

  Eigen::Vector3f transl(
      slam_node.pose[0], slam_node.pose[1], slam_node.pose[2]);
  return vslam_types::RobotPose(slam_node.node_idx, transl, rotation_aa);
}

// TODO make all class methods lower case ?
template <typename FeatureTrackType, typename ProblemParams>
bool SLAMSolver::SolveSLAM(
    const vslam_types::CameraIntrinsics &intrinsics,
    const vslam_types::CameraExtrinsics &extrinsics,
    const std::function<void(const vslam_types::CameraIntrinsics &,
                             const vslam_types::CameraExtrinsics &,
                             const ProblemParams &,
                             vslam_types::UTSLAMProblem<FeatureTrackType> &,
                             ceres::Problem &,
                             std::vector<vslam_types::SLAMNode> *)>
        vision_constraint_adder,
    const std::function<std::shared_ptr<ceres::IterationCallback>(
        const vslam_types::CameraIntrinsics &,
        const vslam_types::CameraExtrinsics &,
        const vslam_types::UTSLAMProblem<FeatureTrackType> &,
        std::vector<vslam_types::SLAMNode> *)> callback_creator,
    const ProblemParams &problem_params,
    vslam_types::UTSLAMProblem<FeatureTrackType> &slam_problem,
    std::vector<vslam_types::RobotPose> &updated_robot_poses) {
  ceres::Problem problem;
  ceres::Solver::Options options;
  ceres::Solver::Summary summary;

  // TODO configure options

  options.max_num_iterations = solver_optimization_params_.max_iterations;
  options.minimizer_type = solver_optimization_params_.minimizer_type;

  std::vector<vslam_types::SLAMNode> slam_nodes;
  RobotPosesToSLAMNodes(updated_robot_poses, slam_nodes);

  vision_constraint_adder(intrinsics,
                          extrinsics,
                          problem_params,
                          slam_problem,
                          problem,
                          &slam_nodes);

  std::shared_ptr<ceres::IterationCallback> viz_callback =
      callback_creator(intrinsics, extrinsics, slam_problem, &slam_nodes);

  if (viz_callback != nullptr) {
    options.callbacks.emplace_back(viz_callback.get());
    options.update_state_every_iteration = true;
  }

  ceres::Solve(options, &problem, &summary);
  std::cout << summary.FullReport() << "\n";

  SLAMNodesToRobotPoses(slam_nodes, updated_robot_poses);

  return (summary.termination_type == ceres::CONVERGENCE ||
          summary.termination_type == ceres::USER_SUCCESS);
}

void AddStructurelessVisionFactors(
    const vslam_types::CameraIntrinsics &intrinsics,
    const vslam_types::CameraExtrinsics &extrinsics,
    const StructurelessSlamProblemParams &solver_optimization_params,
    vslam_types::UTSLAMProblem<vslam_types::VisionFeatureTrack> &slam_problem,
    ceres::Problem &ceres_problem,
    std::vector<vslam_types::SLAMNode> *updated_solved_nodes) {
  std::vector<vslam_types::SLAMNode> &solution = *updated_solved_nodes;

  for (const auto &feature_track_by_id : slam_problem.tracks) {
    for (int i = 0; i < feature_track_by_id.second.track.size() - 1; i++) {
      vslam_types::VisionFeature f1 = feature_track_by_id.second.track[i];
      for (int j = i + 1; j < feature_track_by_id.second.track.size(); j++) {
        vslam_types::VisionFeature f2 = feature_track_by_id.second.track[j];

        double *initial_pose_block = solution[f1.frame_idx].pose;
        double *curr_pose_block = solution[f2.frame_idx].pose;

        ceres_problem.AddResidualBlock(
            Pairwise2dFeatureCostFunctor::create(
                intrinsics,
                extrinsics,
                f1,
                f2,
                solver_optimization_params.epipolar_error_std_dev),
            nullptr,
            initial_pose_block,
            curr_pose_block);
      }
    }
  }
}

void AddStructuredVisionFactors(
    const vslam_types::CameraIntrinsics &intrinsics,
    const vslam_types::CameraExtrinsics &extrinsics,
    const StructuredSlamProblemParams &solver_optimization_params,
    vslam_types::UTSLAMProblem<vslam_types::StructuredVisionFeatureTrack>
        &slam_problem,
    ceres::Problem &ceres_problem,
    std::vector<vslam_types::SLAMNode> *updated_solved_nodes) {
  std::vector<vslam_types::SLAMNode> &solution = *updated_solved_nodes;

  for (auto &feature_track_by_id : slam_problem.tracks) {
    double *feature_position_block = feature_track_by_id.second.point.data();
    for (const vslam_types::VisionFeature &feature :
         feature_track_by_id.second.feature_track.track) {
      double *pose_block = solution[feature.frame_idx].pose;

      ceres_problem.AddResidualBlock(
          ReprojectionCostFunctor::create(
              intrinsics,
              extrinsics,
              feature,
              solver_optimization_params.reprojection_error_std_dev),
          nullptr,
          pose_block,
          feature_position_block);
    }
  }
}

template bool SLAMSolver::SolveSLAM<vslam_types::VisionFeatureTrack,
                                    StructurelessSlamProblemParams>(
    const vslam_types::CameraIntrinsics &intrinsics,
    const vslam_types::CameraExtrinsics &extrinsics,
    const std::function<
        void(const vslam_types::CameraIntrinsics &,
             const vslam_types::CameraExtrinsics &,
             const StructurelessSlamProblemParams &,
             vslam_types::UTSLAMProblem<vslam_types::VisionFeatureTrack> &,
             ceres::Problem &,
             std::vector<vslam_types::SLAMNode> *)> vision_constraint_adder,
    const std::function<std::shared_ptr<ceres::IterationCallback>(
        const vslam_types::CameraIntrinsics &,
        const vslam_types::CameraExtrinsics &,
        const vslam_types::UTSLAMProblem<vslam_types::VisionFeatureTrack> &,
        std::vector<vslam_types::SLAMNode> *)> callback_creator,
    const StructurelessSlamProblemParams &problem_params,
    vslam_types::UTSLAMProblem<vslam_types::VisionFeatureTrack> &slam_problem,
    std::vector<vslam_types::RobotPose> &updated_robot_poses);

template bool SLAMSolver::SolveSLAM<vslam_types::StructuredVisionFeatureTrack,
                                    StructuredSlamProblemParams>(
    const vslam_types::CameraIntrinsics &intrinsics,
    const vslam_types::CameraExtrinsics &extrinsics,
    const std::function<void(
        const vslam_types::CameraIntrinsics &,
        const vslam_types::CameraExtrinsics &,
        const StructuredSlamProblemParams &,
        vslam_types::UTSLAMProblem<vslam_types::StructuredVisionFeatureTrack> &,
        ceres::Problem &,
        std::vector<vslam_types::SLAMNode> *)> vision_constraint_adder,
    const std::function<std::shared_ptr<ceres::IterationCallback>(
        const vslam_types::CameraIntrinsics &,
        const vslam_types::CameraExtrinsics &,
        const vslam_types::UTSLAMProblem<
            vslam_types::StructuredVisionFeatureTrack> &,
        std::vector<vslam_types::SLAMNode> *)> callback_creator,
    const StructuredSlamProblemParams &problem_params,
    vslam_types::UTSLAMProblem<vslam_types::StructuredVisionFeatureTrack>
        &slam_problem,
    std::vector<vslam_types::RobotPose> &updated_robot_poses);
}  // namespace vslam_solver