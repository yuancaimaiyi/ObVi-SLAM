#ifndef UT_VSLAM_REFACTORING_REPROJECTION_COST_FUNCTOR_H
#define UT_VSLAM_REFACTORING_REPROJECTION_COST_FUNCTOR_H

#include <analysis/cumulative_timer_constants.h>
#include <analysis/cumulative_timer_factory.h>
#include <ceres/autodiff_cost_function.h>
#include <refactoring/types/vslam_basic_types_refactor.h>
#include <refactoring/types/vslam_math_util.h>

#include <eigen3/Eigen/Dense>

namespace vslam_types_refactor {

/**
 * Cost functor that adds a residual for Gaussian-distributed reprojection
 * error.
 */
class ReprojectionCostFunctor {
 public:
  /**
   * Constructor.
   *
   * @param image_feature               Pixel location of the feature in image
   * @param intrinsics                  Camera intrinsics.
   * @param extrinsics                  Camera extrinsics (pose of the camera
   *                                    relative to the robot).
   * @param reprojection_error_std_dev  Standard deviation of the reprojection
   *                                    error
   */
  ReprojectionCostFunctor(
      const vslam_types_refactor::PixelCoord<double> &image_feature,
      const vslam_types_refactor::CameraIntrinsicsMat<double> &intrinsics,
      const vslam_types_refactor::CameraExtrinsics<double> &extrinsics,
      const double &reprojection_error_std_dev);

  /**
   * Compute the reprojection error
   *
   * @tparam T                  Type that the cost functor is evaluating.
   * @param pose[in]            Robot's pose in the world frame corresponding to
   *                            the location of where the feature was imaged.
   *                            This is a 6 entry array with the first 3 entries
   *                            corresponding to the translation and the second
   *                            3 entries containing the axis-angle
   *                            representation (with angle given by the
   *                            magnitude of the vector).
   * @param point[in]           3D position of the imaged feature in the world
   *                            frame
   * @param residual[out]       Residual giving the error. Contains 2 entries.
   *                            The first is the x direction reprojection error
   *                            and the second is the y direction reprojection
   *                            error
   *
   * @return True if the residual was computed successfully, false otherwise.
   */
  template <typename T>
  bool runOperator(const T *pose, const T *point, T *residual) const {
    //    // Transform from world to current robot pose
    //    Eigen::Transform<T, 3, Eigen::Affine> world_to_robot_current =
    //        vslam_types_refactor::PoseArrayToAffine(&(pose[3]), &(pose[0]))
    //            .inverse();
    //
    //    // Point in world frame
    //    Eigen::Matrix<T, 3, 1> point_world(point[0], point[1], point[2]);
    //
    //    // Transform the point from global coordinates to frame of current
    //    pose Eigen::Matrix<T, 3, 1> point_current =
    //        cam_to_robot_tf_.inverse().cast<T>() * world_to_robot_current *
    //        point_world;
    //
    //    // Project the 3D point into the current image.
    //    T p_x = T(intrinsics_(0, 0)) * point_current.x() / point_current.z() +
    //            T(intrinsics_(0, 2));
    //    T p_y = T(intrinsics_(1, 1)) * point_current.y() / point_current.z() +
    //            T(intrinsics_(1, 2));
    //
    //    // Compute the residual.
    //    residual[0] = (p_x - T(image_feature_.x())) /
    //    reprojection_error_std_dev_; residual[1] = (p_y -
    //    T(image_feature_.y())) / reprojection_error_std_dev_;

    Eigen::Matrix<T, 2, 1> projected_pixel;
    getProjectedPixelLocationRectified<T>(
        pose, point, cam_to_robot_tf_inv_.cast<T>(), projected_pixel);
    //    LOG(INFO) << "Projected pixel location " << projected_pixel.x() << ",
    //    " << projected_pixel.y();

    // Compute the residual.
    residual[0] = T(rectified_error_multiplier_x_) *
                  (projected_pixel.x() - T(rect_feature_x_));
    residual[1] = T(rectified_error_multiplier_y_) *
                  (projected_pixel.y() - T(rect_feature_y_));

    return true;
  }

  bool operator()(const double *pose,
                  const double *point,
                  double *residual) const {
    //#ifdef RUN_TIMERS
    //    CumulativeFunctionTimer::Invocation invoc(
    //        CumulativeTimerFactory::getInstance()
    //            .getOrCreateFunctionTimer(
    //                kTimerNameFactorReprojectionCostFunctorDouble)
    //            .get());
    //#endif
    return runOperator<double>(pose, point, residual);
  }

  template <int JetDim>
  bool operator()(const ceres::Jet<double, JetDim> *pose,
                  const ceres::Jet<double, JetDim> *point,
                  ceres::Jet<double, JetDim> *residual) const {
    //#ifdef RUN_TIMERS
    //    CumulativeFunctionTimer::Invocation invoc(
    //        CumulativeTimerFactory::getInstance()
    //            .getOrCreateFunctionTimer(
    //                kTimerNameFactorReprojectionCostFunctorJacobian)
    //            .get());
    //#endif
    return runOperator<ceres::Jet<double, JetDim>>(pose, point, residual);
  }

  /**
   * Create the autodiff cost function with this cost functor.
   *
   * @param intrinsics                  Camera intrinsics.
   * @param extrinsics                  Camera extrinsics (provide camera pose
   *                                    relative to robot).
   * @param feature_pixel               Pixel location of the feature.
   * @param reprojection_error_std_dev  Standard deviation of the reprojection
   *                                    error (assuming this is normally
   *                                    distributed).
   *
   * @return Ceres cost function.
   */
  static ceres::AutoDiffCostFunction<ReprojectionCostFunctor, 2, 6, 3> *create(
      const vslam_types_refactor::CameraIntrinsicsMat<double> &intrinsics,
      const vslam_types_refactor::CameraExtrinsics<double> &extrinsics,
      const vslam_types_refactor::PixelCoord<double> &feature_pixel,
      const double &reprojection_error_std_dev) {
    ReprojectionCostFunctor *residual = new ReprojectionCostFunctor(
        feature_pixel, intrinsics, extrinsics, reprojection_error_std_dev);
    return new ceres::AutoDiffCostFunction<ReprojectionCostFunctor, 2, 6, 3>(
        residual);
  }

 private:
  double rect_feature_x_;

  double rect_feature_y_;

  double rectified_error_multiplier_x_;

  double rectified_error_multiplier_y_;

  /**
   * Transform that provides the camera position in the robot's frame.
   */
  Eigen::Affine3d cam_to_robot_tf_inv_;
};
}  // namespace vslam_types_refactor

#endif  // UT_VSLAM_REFACTORING_REPROJECTION_COST_FUNCTOR_H
