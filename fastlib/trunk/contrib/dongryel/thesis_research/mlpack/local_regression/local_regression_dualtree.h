/** @file local_regression_dualtree.h
 *
 *  The template stub filled out for computing the local regression
 *  estimate using a dual-tree algorithm.
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef MLPACK_LOCAL_REGRESSION_LOCAL_REGRESSION_DUALTREE_H
#define MLPACK_LOCAL_REGRESSION_LOCAL_REGRESSION_DUALTREE_H

#include <boost/math/distributions/normal.hpp>
#include <boost/mpi.hpp>
#include <boost/scoped_array.hpp>
#include <boost/serialization/serialization.hpp>
#include <deque>
#include "core/monte_carlo/mean_variance_pair.h"
#include "core/monte_carlo/mean_variance_pair_matrix.h"
#include "core/metric_kernels/kernel.h"
#include "core/tree/statistic.h"
#include "core/table/table.h"

namespace mlpack {
namespace local_regression {

/** @brief The postponed quantities for local regression.
 */
class LocalRegressionPostponed {

  private:

    // For boost serialization.
    friend class boost::serialization::access;

  public:

    /** @brief The lower bound on the postponed quantities for the
     *         left hand side.
     */
    core::monte_carlo::MeanVariancePairMatrix left_hand_side_l_;

    /** @brief The finite-difference postponed quantities for the left
     *         hand side.
     */
    core::monte_carlo::MeanVariancePairMatrix left_hand_side_e_;

    /** @brief The upper bound on the postponed quantities for the
     *         left hand side.
     */
    core::monte_carlo::MeanVariancePairMatrix left_hand_side_u_;

    /** @brief The lower bound on the postponed quantities for the
     *         right hand side.
     */
    core::monte_carlo::MeanVariancePairVector right_hand_side_l_;

    /** @brief The finite-difference postponed quantities for the left
     *         right side.
     */
    core::monte_carlo::MeanVariancePairVector right_hand_side_e_;

    /** @brief The upper bound on the postponed quantities for the
     *         right hand side.
     */
    core::monte_carlo::MeanVariancePairVector right_hand_side_u_;

    /** @brief The amount of pruned quantities.
     */
    double pruned_;

    /** @brief The upper bound on the used error.
     */
    double used_error_;

    /** @brief Serialize the postponed quantities.
     */
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
      ar & left_hand_side_l_;
      ar & left_hand_side_e_;
      ar & left_hand_side_u_;
      ar & right_hand_side_l_;
      ar & right_hand_side_e_;
      ar & right_hand_side_u_;
      ar & pruned_;
      ar & used_error_;
    }

    /** @brief The default constructor.
     */
    LocalRegressionPostponed() {
      SetZero();
    }

    /** @brief Copies another postponed object.
     */
    void Copy(const LocalRegressionPostponed &postponed_in) {
      left_hand_side_l_.CopyValues(postponed_in.left_hand_side_l_);
      left_hand_side_e_.CopyValues(postponed_in.left_hand_side_e_);
      left_hand_side_u_.CopyValues(postponed_in.left_hand_side_u_);
      right_hand_side_l_.CopyValues(postponed_in.right_hand_side_l_);
      right_hand_side_e_.CopyValues(postponed_in.right_hand_side_e_);
      right_hand_side_u_.CopyValues(postponed_in.right_hand_side_u_);
      pruned_ = postponed_in.pruned_;
      used_error_ = postponed_in.used_error_;
    }

    /** @brief Initializes the postponed quantities.
     */
    void Init() {
      SetZero();
    }

    /** @brief Initializes the postponed quantities given a global
     *         object and a query reference pair.
     */
    template<typename GlobalType, typename TreeType>
    void Init(const GlobalType &global_in, TreeType *qnode, TreeType *rnode) {
      left_hand_side_l_.SetZero();
      left_hand_side_e_.SetZero();
      left_hand_side_u_.SetZero();
      right_hand_side_l_.SetZero();
      right_hand_side_e_.SetZero();
      right_hand_side_u_.SetZero();

      // Set the total number of terms.
      left_hand_side_l_.set_total_num_terms(rnode->count());
      left_hand_side_e_.set_total_num_terms(rnode->count());
      left_hand_side_u_.set_total_num_terms(rnode->count());
      right_hand_side_l_.set_total_num_terms(rnode->count());
      right_hand_side_e_.set_total_num_terms(rnode->count());
      right_hand_side_u_.set_total_num_terms(rnode->count());
      pruned_ = static_cast<double>(rnode->count());

      // Used error is zero.
      used_error_ = 0;
    }

    /** @brief Applies the incoming delta contribution to the
     *         postponed quantities, optionally to the query results
     *         as well.
     */
    template < typename TreeType, typename GlobalType,
             typename LocalRegressionDelta, typename ResultType >
    void ApplyDelta(
      TreeType *qnode, TreeType *rnode,
      const GlobalType &global, const LocalRegressionDelta &delta_in,
      ResultType *query_results) {

      // Combine the delta.
      left_hand_side_l_.CombineWith(delta_in.left_hand_side_l_);
      left_hand_side_e_.CombineWith(delta_in.left_hand_side_e_);
      left_hand_side_u_.CombineWith(delta_in.left_hand_side_u_);
      right_hand_side_l_.CombineWith(delta_in.right_hand_side_l_);
      right_hand_side_e_.CombineWith(delta_in.right_hand_side_e_);
      right_hand_side_u_.CombineWith(delta_in.right_hand_side_u_);

      // Add the pruned and used error quantities.
      pruned_ = pruned_ + delta_in.pruned_;
      used_error_ = used_error_ + delta_in.used_error_;
    }

    /** @brief Applies the incoming postponed contribution.
     */
    void ApplyPostponed(const LocalRegressionPostponed &other_postponed) {

      // Combine the postponed quantities.
      left_hand_side_l_.CombineWith(other_postponed.left_hand_side_l_);
      left_hand_side_e_.CombineWith(other_postponed.left_hand_side_e_);
      left_hand_side_u_.CombineWith(other_postponed.left_hand_side_u_);
      right_hand_side_l_.CombineWith(other_postponed.right_hand_side_l_);
      right_hand_side_e_.CombineWith(other_postponed.right_hand_side_e_);
      right_hand_side_u_.CombineWith(other_postponed.right_hand_side_u_);

      // Add the pruned and used error quantities.
      pruned_ = pruned_ + other_postponed.pruned_;
      used_error_ = used_error_ + other_postponed.used_error_;
    }

    /** @brief Applies the incoming postponed contribution during the
     *         postprocessing stage.
     */
    template<typename GlobalType>
    void FinalApplyPostponed(
      const GlobalType &global, LocalRegressionPostponed &other_postponed) {

      ApplyPostponed(other_postponed);
    }

    /** @brief Called from an exact pairwise evaluation method
     *         (i.e. the base case) which incurs no error.
     */
    template<typename GlobalType, typename MetricType>
    void ApplyContribution(
      const GlobalType &global,
      const MetricType &metric,
      const arma::vec &query_point,
      double query_weight,
      const arma::vec &reference_point,
      double reference_weight) {

      double distsq = metric.DistanceSq(query_point, reference_point);
      double kernel_value = global.kernel().EvalUnnormOnSq(distsq);
      left_hand_side_l_.get(0, 0).push_back(kernel_value);
      left_hand_side_e_.get(0, 0).push_back(kernel_value);
      left_hand_side_u_.get(0, 0).push_back(kernel_value);
      right_hand_side_l_[0].push_back(kernel_value * reference_weight);
      right_hand_side_e_[0].push_back(kernel_value * reference_weight);
      right_hand_side_u_[0].push_back(kernel_value * reference_weight);
      for(unsigned int j = 1; j <= reference_point.n_elem; j++) {

        // The row update for the left hand side.
        double left_hand_side_increment = kernel_value * reference_point[j - 1];
        left_hand_side_l_.get(0, j - 1).push_back(left_hand_side_increment);
        left_hand_side_e_.get(0, j - 1).push_back(left_hand_side_increment);
        left_hand_side_u_.get(0, j - 1).push_back(left_hand_side_increment);

        // The column update for the left hand side.
        left_hand_side_l_.get(j - 1, 0).push_back(left_hand_side_increment);
        left_hand_side_e_.get(j - 1, 0).push_back(left_hand_side_increment);
        left_hand_side_u_.get(j - 1, 0).push_back(left_hand_side_increment);

        // The right hand side.
        double right_hand_side_increment =
          kernel_value * reference_weight * reference_point[j - 1];
        right_hand_side_l_[j - 1].push_back(right_hand_side_increment);
        right_hand_side_e_[j - 1].push_back(right_hand_side_increment);
        right_hand_side_u_[j - 1].push_back(right_hand_side_increment);

        for(unsigned int i = 1; i <= reference_point.n_elem; i++) {

          double inner_increment = kernel_value * reference_point[i - 1] *
                                   reference_point[j - 1];
          left_hand_side_l_.get(i, j).push_back(inner_increment);
          left_hand_side_e_.get(i, j).push_back(inner_increment);
          left_hand_side_u_.get(i, j).push_back(inner_increment);
        }
      }
    }

    /** @brief Sets everything to zero.
     */
    void SetZero() {
      left_hand_side_l_.SetZero();
      left_hand_side_e_.SetZero();
      left_hand_side_u_.SetZero();
      right_hand_side_l_.SetZero();
      right_hand_side_e_.SetZero();
      right_hand_side_u_.SetZero();
      pruned_ = 0;
      used_error_ = 0;
    }

    /** @brief Sets everything to zero in the post-processing step.
     */
    void FinalSetZero() {
      this->SetZero();
    }
};

template<typename KernelType>
class ConsiderExtrinsicPruneTrait {
  public:
    static bool Compute(
      const KernelType &kernel_aux_in,
      const core::math::Range &squared_distance_range_in) {
      return false;
    }
};

template<>
class ConsiderExtrinsicPruneTrait <
    core::metric_kernels::EpanKernel > {
  public:
    static bool Compute(
      const core::metric_kernels::EpanKernel &kernel_in,
      const core::math::Range &squared_distance_range_in) {

      return
        kernel_in.bandwidth_sq() <= squared_distance_range_in.lo;
    }
};

/** @brief The global constant struct passed around for local
 *         regression computation.
 */
template<typename IncomingTableType, typename IncomingKernelType>
class LocalRegressionGlobal {

  public:
    typedef IncomingTableType TableType;

    typedef IncomingKernelType KernelType;

  private:

    /** @brief The absolute error approximation level.
     */
    double absolute_error_;

    /** @brief The relative error approximation level.
     */
    double relative_error_;

    /** @brief For the probabilistic approximation.
     */
    double probability_;

    /** @brief The kernel type.
     */
    KernelType kernel_;

    /** @brief The effective number of reference points used for
     *         normalization.
     */
    double effective_num_reference_points_;

    /** @brief The query table.
     */
    TableType *query_table_;

    /** @brief The reference table.
     */
    TableType *reference_table_;

    /** @brief Whether the computation is monochromatic or not.
     */
    bool is_monochromatic_;

  public:

    /** @brief Tells whether the given squared distance range is
     *         sufficient for pruning for any pair of query/reference
     *         pair that satisfies the range.
     */
    bool ConsiderExtrinsicPrune(
      const core::math::Range &squared_distance_range) const {

      return
        ConsiderExtrinsicPruneTrait<KernelType>::Compute(
          kernel_, squared_distance_range);
    }

    /** @brief Returns whether the computation is monochromatic or
     *         not.
     */
    bool is_monochromatic() const {
      return is_monochromatic_;
    }

    /** @brief Returns the effective number of reference points.
     */
    double effective_num_reference_points() const {
      return effective_num_reference_points_;
    }

    /** @brief Sets the effective number of reference points given a
     *         pair of distributed table of points.
     */
    template<typename DistributedTableType>
    void set_effective_num_reference_points(
      boost::mpi::communicator &comm,
      DistributedTableType *reference_table_in,
      DistributedTableType *query_table_in) {

      double total_sum = 0;
      for(int i = 0; i < comm.size(); i++) {
        total_sum += reference_table_in->local_n_entries(i);
      }
      effective_num_reference_points_ =
        (reference_table_in == query_table_in) ?
        (total_sum - 1.0) : total_sum;
    }

    /** @brief The constructor.
     */
    LocalRegressionGlobal() {
      absolute_error_ = 0.0;
      relative_error_ = 0.0;
      probability_ = 1.0;
      effective_num_reference_points_ = 0.0;
      query_table_ = NULL;
      reference_table_ = NULL;
      is_monochromatic_ = true;
    }

    /** @brief Returns the query table.
     */
    TableType *query_table() {
      return query_table_;
    }

    /** @brief Returns the query table.
     */
    const TableType *query_table() const {
      return query_table_;
    }

    /** @brief Returns the reference table.
     */
    TableType *reference_table() {
      return reference_table_;
    }

    /** @brief Returns the reference table.
     */
    const TableType *reference_table() const {
      return reference_table_;
    }

    /** @brief Returns the absolute error.
     */
    double absolute_error() const {
      return absolute_error_;
    }

    /** @brief Returns the relative error.
     */
    double relative_error() const {
      return relative_error_;
    }

    /** @brief Returns the probability.
     */
    double probability() const {
      return probability_;
    }

    /** @brief Returns the bandwidth value being used.
     */
    double bandwidth() const {
      return sqrt(kernel_.bandwidth_sq());
    }

    /** @brief Sets the bandwidth.
     */
    void set_bandwidth(double bandwidth_in) {
      kernel_.Init(bandwidth_in);
    }

    /** @brief Returns the kernel.
     */
    const KernelType &kernel() const {
      return kernel_;
    }

    /** @brief Initializes the local regression global object.
     */
    void Init(
      TableType *reference_table_in,
      TableType *query_table_in,
      double effective_num_reference_points_in,
      double bandwidth_in,
      const bool is_monochromatic,
      double relative_error_in,
      double absolute_error_in,
      double probability_in) {

      effective_num_reference_points_ = effective_num_reference_points_in;

      // Initialize the kernel.
      kernel_.Init(bandwidth_in);

      relative_error_ = relative_error_in;
      absolute_error_ = absolute_error_in;
      probability_ = probability_in;
      query_table_ = query_table_in;
      reference_table_ = reference_table_in;

      // Set the monochromatic flag.
      is_monochromatic_ = is_monochromatic;
    }
};

/** @brief Represents the storage of local regression computation
 *         results.
 */
class LocalRegressionResult {
  private:

    // For BOOST serialization.
    friend class boost::serialization::access;

  public:

    /** @brief The number of query points.
     */
    int num_query_points_;

    /** @brief The flag that tells whether the self contribution has
     *         been subtracted or not.
     */
    boost::scoped_array<bool> self_contribution_subtracted_;

    /** @brief The lower bound on the left hand side.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairMatrix > left_hand_side_l_;

    /** @brief The estimated left hand side.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairMatrix > left_hand_side_e_;

    /** @brief The upper bound on the left hand side.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairMatrix > left_hand_side_u_;

    /** @brief The lower bound on the right hand side.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairVector > right_hand_side_l_;

    /** @brief The estimated right hand side.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairVector > right_hand_side_e_;

    /** @brief The upper bound on the left hand side.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairVector > right_hand_side_u_;

    /** @brief The number of points pruned per each query.
     */
    boost::scoped_array<double> pruned_;

    /** @brief The amount of maximum error incurred per each query.
     */
    boost::scoped_array<double> used_error_;

    /** @brief Saves the local regression result object.
     */
    template<class Archive>
    void save(Archive &ar, const unsigned int version) const {
      ar & num_query_points_;
      for(unsigned int i = 0; i < num_query_points_; i++) {
        ar & self_contribution_subtracted_[i];
        ar & left_hand_side_l_[i];
        ar & left_hand_side_e_[i];
        ar & left_hand_side_u_[i];
        ar & right_hand_side_l_[i];
        ar & right_hand_side_e_[i];
        ar & right_hand_side_u_[i];
        ar & pruned_[i];
        ar & used_error_[i];
      }
    }

    /** @brief Loads the local regression result object.
     */
    template<class Archive>
    void load(Archive &ar, const unsigned int version) {

      // Load the number of points.
      ar & num_query_points_;

      // Initialize the array.
      this->Init(num_query_points_);

      // Load.
      for(int i = 0; i < num_query_points_; i++) {
        ar & self_contribution_subtracted_[i];
        ar & left_hand_side_l_[i];
        ar & left_hand_side_e_[i];
        ar & left_hand_side_u_[i];
        ar & right_hand_side_l_[i];
        ar & right_hand_side_e_[i];
        ar & right_hand_side_u_[i];
        ar & pruned_[i];
        ar & used_error_[i];
      }
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()

    void Seed(int qpoint_index, double initial_pruned_in) {
      pruned_[qpoint_index] = initial_pruned_in;
    }

    /** @brief The default constructor.
     */
    LocalRegressionResult() {
      num_query_points_ = 0;
      SetZero();
    }

    template<typename MetricType, typename GlobalType>
    void PostProcess(
      const MetricType &metric,
      int q_index,
      const GlobalType &global,
      const bool is_monochromatic) {

    }

    void Print(const std::string &file_name) const {
    }

    void Init(int num_points) {

      boost::scoped_array<bool> tmp_self_contribution_subtracted(
        new bool[num_query_points_]);
      self_contribution_subtracted_.swap(tmp_self_contribution_subtracted);

      boost::scoped_array <
      core::monte_carlo::MeanVariancePairMatrix >
      tmp_left_hand_side_l(
        new core::monte_carlo::MeanVariancePairMatrix[num_query_points_]);
      left_hand_side_l_.swap(tmp_left_hand_side_l);

      boost::scoped_array <
      core::monte_carlo::MeanVariancePairMatrix >
      tmp_left_hand_side_e(
        new core::monte_carlo::MeanVariancePairMatrix[num_query_points_]);
      left_hand_side_e_.swap(tmp_left_hand_side_e);

      boost::scoped_array <
      core::monte_carlo::MeanVariancePairMatrix >
      tmp_left_hand_side_u(
        new core::monte_carlo::MeanVariancePairMatrix[num_query_points_]);
      left_hand_side_u_.swap(tmp_left_hand_side_u);

      boost::scoped_array <
      core::monte_carlo::MeanVariancePairVector >
      tmp_right_hand_side_l(
        new core::monte_carlo::MeanVariancePairVector[num_query_points_]);
      right_hand_side_l_.swap(tmp_right_hand_side_l);

      boost::scoped_array <
      core::monte_carlo::MeanVariancePairVector >
      tmp_right_hand_side_e(
        new core::monte_carlo::MeanVariancePairVector[num_query_points_]);
      right_hand_side_e_.swap(tmp_right_hand_side_e);

      boost::scoped_array <
      core::monte_carlo::MeanVariancePairVector >
      tmp_right_hand_side_u(
        new core::monte_carlo::MeanVariancePairVector[num_query_points_]);
      right_hand_side_u_.swap(tmp_right_hand_side_u);

      // Set everything to zero.
      SetZero();
    }

    void SetZero() {
      for(int i = 0; i < num_query_points_; i++) {
        self_contribution_subtracted_[i] = false;
        left_hand_side_l_[i].SetZero();
        left_hand_side_e_[i].SetZero();
        left_hand_side_u_[i].SetZero();
        right_hand_side_l_[i].SetZero();
        right_hand_side_e_[i].SetZero();
        right_hand_side_u_[i].SetZero();
        pruned_[i] = 0;
        used_error_[i] = 0;
      }
    }

    template<typename GlobalType, typename TreeType, typename DeltaType>
    void ApplyProbabilisticDelta(
      GlobalType &global, TreeType *qnode, double failure_probability,
      const DeltaType &delta_in) {

    }

    /** @brief Apply postponed contributions.
     */
    template<typename LocalRegressionPostponedType>
    void ApplyPostponed(
      int q_index, const LocalRegressionPostponedType &postponed_in) {
      left_hand_side_l_[q_index].CombineWith(postponed_in.left_hand_side_l_);
      left_hand_side_e_[q_index].CombineWith(postponed_in.left_hand_side_e_);
      left_hand_side_u_[q_index].CombineWith(postponed_in.left_hand_side_u_);
      right_hand_side_l_[q_index].CombineWith(postponed_in.right_hand_side_l_);
      right_hand_side_e_[q_index].CombineWith(postponed_in.right_hand_side_e_);
      right_hand_side_u_[q_index].CombineWith(postponed_in.right_hand_side_u_);
      pruned_[q_index] = pruned_[q_index] + postponed_in.pruned_;
      used_error_[q_index] = used_error_[q_index] + postponed_in.used_error_;
    }

    /** @brief Apply the postponed quantities to the query results
     *         during the final postprocessing stage.
     */
    template<typename GlobalType, typename LocalRegressionPostponedType>
    void FinalApplyPostponed(
      const GlobalType &global,
      const core::table::DensePoint &qpoint,
      int q_index,
      const LocalRegressionPostponedType &postponed_in) {

      // Apply postponed.
      ApplyPostponed(q_index, postponed_in);
    }
};

class LocalRegressionDelta {

  public:

    core::monte_carlo::MeanVariancePairMatrix left_hand_side_l_;

    core::monte_carlo::MeanVariancePairMatrix left_hand_side_e_;

    core::monte_carlo::MeanVariancePairMatrix left_hand_side_u_;

    core::monte_carlo::MeanVariancePairVector right_hand_side_l_;

    core::monte_carlo::MeanVariancePairVector right_hand_side_e_;

    core::monte_carlo::MeanVariancePairVector right_hand_side_u_;

    double pruned_;

    double used_error_;

    LocalRegressionDelta() {
      SetZero();
    }

    void SetZero() {
      left_hand_side_l_.SetZero();
      left_hand_side_e_.SetZero();
      left_hand_side_u_.SetZero();
      right_hand_side_l_.SetZero();
      right_hand_side_e_.SetZero();
      right_hand_side_u_.SetZero();
      pruned_ = used_error_ = 0.0;
    }

    template<typename MetricType, typename GlobalType, typename TreeType>
    void DeterministicCompute(
      const MetricType &metric,
      const GlobalType &global, TreeType *qnode, TreeType *rnode,
      const core::math::Range &squared_distance_range) {

      // The maximum deviation between the lower and the upper
      // estimated quantities for the left hand side and the right
      // hand side.
      double max_deviation = 0;

      // Lower and upper bound on the kernels.
      double lower_kernel_value =
        global.kernel().EvalUnnormOnSq(squared_distance_range.hi);
      double upper_kernel_value =
        global.kernel().EvalUnnormOnSq(squared_distance_range.lo);

      // Initialize the left hand side lower bound.
      left_hand_side_l_.Init(
        global.reference_table()->n_attributes() + 1,
        global.reference_table()->n_attributes() + 1);
      left_hand_side_l_.set_total_num_terms(rnode->count());
      for(int j = 0; j <= global.reference_table()->n_attributes(); j++) {
        for(int i = 0; i <= global.reference_table()->n_attributes(); i++) {
          left_hand_side_l_.get(i, j).push_back(
            lower_kernel_value *
            rnode->stat().average_info_.get(i, j).sample_mean());
        }
      }

      // Initialize the left hand side estimate.
      left_hand_side_e_.Init(
        global.reference_table()->n_attributes() + 1,
        global.reference_table()->n_attributes() + 1);
      left_hand_side_e_.set_total_num_terms(rnode->count());
      for(int j = 0; j <= global.reference_table()->n_attributes(); j++) {
        for(int i = 0; i <= global.reference_table()->n_attributes(); i++) {
          left_hand_side_e_.get(i, j).push_back(
            0.5 *(lower_kernel_value + upper_kernel_value) *
            rnode->stat().average_info_.get(i, j).sample_mean());
        }
      }

      // Initialize the left hand side upper bound.
      left_hand_side_u_.Init(
        global.reference_table()->n_attributes() + 1,
        global.reference_table()->n_attributes() + 1);
      left_hand_side_u_.set_total_num_terms(rnode->count());
      for(int j = 0; j <= global.reference_table()->n_attributes(); j++) {
        for(int i = 0; i <= global.reference_table()->n_attributes(); i++) {
          left_hand_side_u_.get(i, j).push_back(
            upper_kernel_value *
            rnode->stat().average_info_.get(i, j).sample_mean());
        }
      }

      // Initialize the right hand side lower bound.
      right_hand_side_l_.Init(global.reference_table()->n_attributes() + 1);
      right_hand_side_l_.set_total_num_terms(rnode->count());
      for(int j = 0; j <= global.reference_table()->n_attributes(); j++) {
        right_hand_side_l_[j].push_back(
          lower_kernel_value *
          rnode->stat().weighted_average_info_[j].sample_mean());
      }

      // Initialize the right hand side estimate.
      right_hand_side_e_.Init(global.reference_table()->n_attributes() + 1);
      right_hand_side_e_.set_total_num_terms(rnode->count());
      for(int j = 0; j <= global.reference_table()->n_attributes(); j++) {
        right_hand_side_e_[j].push_back(
          0.5 *(lower_kernel_value + upper_kernel_value) *
          rnode->stat().weighted_average_info_[j].sample_mean());
      }

      // Initialize the right hand side upper bound.
      right_hand_side_u_.Init(global.reference_table()->n_attributes() + 1);
      right_hand_side_u_.set_total_num_terms(rnode->count());
      for(int j = 0; j <= global.reference_table()->n_attributes(); j++) {
        right_hand_side_u_[j].push_back(
          upper_kernel_value *
          rnode->stat().weighted_average_info_[j].sample_mean());
      }

      // Compute the maximum deviation.
      for(int j = 0; j < left_hand_side_l_.n_cols(); j++) {
        max_deviation =
          std::max(
            max_deviation,
            right_hand_side_u_[j].sample_mean() -
            right_hand_side_l_[j].sample_mean());
        for(int i = 0; i < left_hand_side_l_.n_rows(); i++) {
          max_deviation =
            std::max(
              max_deviation,
              left_hand_side_u_.get(i, j).sample_mean() -
              left_hand_side_l_.get(i, j).sample_mean());
        }
      }

      int rnode_count = rnode->count();
      pruned_ = static_cast<double>(rnode_count);
      used_error_ = 0.5 * max_deviation;
    }
};

class LocalRegressionSummary {

  private:

    // For Boost serialization.
    friend class boost::serialization::access;

  public:

    arma::mat left_hand_side_l_;

    arma::mat left_hand_side_u_;

    arma::vec right_hand_side_l_;

    arma::vec right_hand_side_u_;

    double pruned_l_;

    double used_error_u_;

    void Seed(double initial_pruned_in) {
      this->SetZero();
      pruned_l_ = initial_pruned_in;
    }

    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
      ar & left_hand_side_l_;
      ar & left_hand_side_u_;
      ar & right_hand_side_l_;
      ar & right_hand_side_u_;
      ar & pruned_l_;
      ar & used_error_u_;
    }

    void Copy(const LocalRegressionSummary &summary_in) {
      left_hand_side_l_ = summary_in.left_hand_side_l_;
      left_hand_side_u_ = summary_in.left_hand_side_u_;
      right_hand_side_l_ = summary_in.right_hand_side_l_;
      right_hand_side_u_ = summary_in.right_hand_side_u_;
      pruned_l_ = summary_in.pruned_l_;
      used_error_u_ = summary_in.used_error_u_;
    }

    LocalRegressionSummary() {
      this->SetZero();
    }

    LocalRegressionSummary(const LocalRegressionSummary &summary_in) {
      this->Copy(summary_in);
    }

    template < typename MetricType, typename GlobalType,
             typename PostponedType, typename DeltaType,
             typename TreeType, typename ResultType >
    bool CanProbabilisticSummarize(
      const MetricType &metric,
      GlobalType &global,
      const PostponedType &postponed, DeltaType &delta,
      const core::math::Range &squared_distance_range,
      TreeType *qnode, TreeType *rnode,
      double failure_probability, ResultType *query_results) const {

      return false;
    }

    template < typename GlobalType, typename DeltaType, typename TreeType,
             typename ResultType >
    bool CanSummarize(
      const GlobalType &global, DeltaType &delta,
      const core::math::Range &squared_distance_range,
      TreeType *qnode, TreeType *rnode, ResultType *query_results) const {

      double left_hand_side = delta.used_error_;
      double lower_bound_l1_norm = 0.0;
      for(unsigned int j = 0; j < left_hand_side_l_.n_cols; j++) {
        lower_bound_l1_norm += right_hand_side_l_[j];
        for(unsigned int i = 0; i < left_hand_side_l_.n_rows; i++) {
          lower_bound_l1_norm += left_hand_side_l_.at(i, j);
        }
      }

      double right_hand_side =
        rnode->count() * (
          global.relative_error() * lower_bound_l1_norm +
          global.effective_num_reference_points() * global.absolute_error() -
          used_error_u_) /
        static_cast<double>(
          global.effective_num_reference_points() - pruned_l_);

      // Prunable by finite-difference.
      return left_hand_side <= right_hand_side;
    }

    void SetZero() {
      left_hand_side_l_.zeros();
      left_hand_side_u_.zeros();
      right_hand_side_l_.zeros();
      right_hand_side_u_.zeros();
      pruned_l_ = 0;
      used_error_u_ = 0;
    }

    void Init() {
      SetZero();
    }

    void StartReaccumulate() {
      left_hand_side_l_.fill(std::numeric_limits<double>::max());
      left_hand_side_u_.zeros();
      right_hand_side_l_.fill(std::numeric_limits<double>::max());
      right_hand_side_u_.zeros();
      pruned_l_ = std::numeric_limits<double>::max();
      used_error_u_ = 0;
    }

    template<typename GlobalType, typename ResultType>
    void Accumulate(
      const GlobalType &global, const ResultType &results, int q_index) {

      for(unsigned int j = 0; j < left_hand_side_l_.n_cols; j++) {
        right_hand_side_l_[j] =
          std::min(
            right_hand_side_l_[j],
            results.right_hand_side_l_[q_index][j].sample_mean() *
            results.pruned_[q_index]);
        right_hand_side_u_[j] =
          std::max(
            right_hand_side_u_[j],
            results.right_hand_side_u_[q_index][j].sample_mean() *
            results.pruned_[q_index]);
        for(unsigned int i = 0; i < left_hand_side_l_.n_rows; i++) {
          left_hand_side_l_.at(i, j) =
            std::min(
              left_hand_side_l_.at(i, j),
              results.left_hand_side_l_[q_index].get(i, j).sample_mean() *
              results.pruned_[q_index]);
          left_hand_side_u_.at(i, j) =
            std::max(
              left_hand_side_u_.at(i, j),
              results.left_hand_side_u_[q_index].get(i, j).sample_mean() *
              results.pruned_[q_index]);
        }
      }
      pruned_l_ = std::min(pruned_l_, results.pruned_[q_index]);
      used_error_u_ = std::max(used_error_u_, results.used_error_[q_index]);
    }

    template<typename GlobalType, typename LocalRegressionPostponedType>
    void Accumulate(
      const GlobalType &global, const LocalRegressionSummary &summary_in,
      const LocalRegressionPostponedType &postponed_in) {

      for(unsigned int j = 0; j < left_hand_side_l_.n_cols; j++) {
        right_hand_side_l_[j] =
          std::min(
            right_hand_side_l_[j],
            summary_in.right_hand_side_l_[j] +
            postponed_in.right_hand_side_l_[j].sample_mean() *
            postponed_in.pruned_);
        right_hand_side_u_[j] =
          std::max(
            right_hand_side_u_[j],
            summary_in.right_hand_side_u_[j] +
            postponed_in.right_hand_side_u_[j].sample_mean() *
            postponed_in.pruned_);
        for(unsigned int i = 0; i < left_hand_side_l_.n_rows; i++) {
          left_hand_side_l_.at(i, j) =
            std::min(
              left_hand_side_l_.at(i, j),
              summary_in.left_hand_side_l_.at(i, j) +
              postponed_in.left_hand_side_l_.get(i, j).sample_mean() *
              postponed_in.pruned_);
          left_hand_side_u_.at(i, j) =
            std::max(
              left_hand_side_u_.at(i, j),
              summary_in.left_hand_side_u_.at(i, j) +
              postponed_in.left_hand_side_u_.get(i, j).sample_mean() *
              postponed_in.pruned_);
        }
      }
      pruned_l_ = std::min(
                    pruned_l_, summary_in.pruned_l_ + postponed_in.pruned_);
      used_error_u_ = std::max(
                        used_error_u_,
                        summary_in.used_error_u_ + postponed_in.used_error_);
    }

    void ApplyDelta(const LocalRegressionDelta &delta_in) {
      for(unsigned int j = 0; j < left_hand_side_l_.n_cols; j++) {
        right_hand_side_l_[j] +=
          delta_in.right_hand_side_l_[j].sample_mean() * delta_in.pruned_;
        right_hand_side_u_[j] +=
          delta_in.right_hand_side_u_[j].sample_mean() * delta_in.pruned_;
        for(unsigned int i = 0; i < left_hand_side_l_.n_rows; i++) {
          left_hand_side_l_.at(i, j) +=
            delta_in.left_hand_side_l_.get(i, j).sample_mean() *
            delta_in.pruned_;
          left_hand_side_u_.at(i, j) +=
            delta_in.left_hand_side_u_.get(i, j).sample_mean() *
            delta_in.pruned_;
        }
      }
    }

    template<typename LocalRegressionPostponedType>
    void ApplyPostponed(const LocalRegressionPostponedType &postponed_in) {
      for(unsigned int j = 0; j < left_hand_side_l_.n_cols; j++) {
        right_hand_side_l_[j] +=
          postponed_in.right_hand_side_l_[j].sample_mean() *
          postponed_in.pruned_;
        right_hand_side_u_[j] +=
          postponed_in.right_hand_side_u_[j].sample_mean() *
          postponed_in.pruned_;
        for(unsigned int i = 0; i < left_hand_side_l_.n_rows; i++) {
          left_hand_side_l_.at(i, j) +=
            postponed_in.left_hand_side_l_.get(i, j).sample_mean() *
            postponed_in.pruned_;
          left_hand_side_u_.at(i, j) +=
            postponed_in.left_hand_side_u_.get(i, j).sample_mean() *
            postponed_in.pruned_;
        }
      }
      pruned_l_ = pruned_l_ + postponed_in.pruned_;
      used_error_u_ = used_error_u_ + postponed_in.used_error_;
    }
};

class LocalRegressionStatistic {

  private:

    // For Boost serialization.
    friend class boost::serialization::access;

  public:

    core::monte_carlo::MeanVariancePairMatrix average_info_;

    core::monte_carlo::MeanVariancePairVector weighted_average_info_;

    mlpack::local_regression::LocalRegressionPostponed postponed_;

    mlpack::local_regression::LocalRegressionSummary summary_;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
      ar & postponed_;
      ar & summary_;
    }

    /** @brief Copies another local regression statistic.
     */
    void Copy(const LocalRegressionStatistic &stat_in) {
      postponed_.Copy(stat_in.postponed_);
      summary_.Copy(stat_in.summary_);
    }

    /** @brief The default constructor.
     */
    LocalRegressionStatistic() {
      SetZero();
    }

    /** @brief Sets the postponed and the summary statistics to zero.
     */
    void SetZero() {
      postponed_.SetZero();
      summary_.SetZero();
    }

    void Seed(double initial_pruned_in) {
      postponed_.SetZero();
      summary_.Seed(initial_pruned_in);
    }

    /** @brief Initializes by taking statistics on raw data.
     */
    template<typename GlobalType, typename TreeType>
    void Init(const GlobalType &global, TreeType *node) {

      average_info_.Init(
        global.reference_table()->n_attributes() + 1,
        global.reference_table()->n_attributes() + 1);
      average_info_.set_total_num_terms(node->count());
      weighted_average_info_.Init(
        global.reference_table()->n_attributes() + 1);
      weighted_average_info_.set_total_num_terms(node->count());

      // Accumulate from the raw data.
      typename GlobalType::TableType::TreeIterator node_it =
        const_cast<GlobalType &>(global).
        reference_table()->get_node_iterator(node);
      while(node_it.HasNext()) {

        // Point ID and its weight.
        int point_id;
        double point_weight;

        // Get each point.
        arma::vec point;
        node_it.Next(&point, &point_id, &point_weight);

        // Push the contribution of each point.
        average_info_.get(0, 0).push_back(1.0);
        weighted_average_info_[0].push_back(point_weight);
        for(int j = 1; j <= global.reference_table()->n_attributes(); j++) {
          average_info_.get(0, j).push_back(point[j - 1]);
          average_info_.get(j, 0).push_back(point[j - 1]);
          weighted_average_info_[j].push_back(point_weight * point[j - 1]);
          for(int i = 1; i <= global.reference_table()->n_attributes(); i++) {
            average_info_.get(i, j).push_back(point[i - 1] * point[j - 1]);
          }
        }
      }

      // Sets the postponed quantities and summary statistics to zero.
      SetZero();
    }

    /** @brief Initializes by combining statistics of two partitions.
     *
     * This lets you build fast bottom-up statistics when building trees.
     */
    template<typename GlobalType, typename TreeType>
    void Init(
      const GlobalType &global,
      TreeType *node,
      const LocalRegressionStatistic &left_stat,
      const LocalRegressionStatistic &right_stat) {

      // Initialize the average information.
      average_info_.Init(
        global.reference_table()->n_attributes() + 1,
        global.reference_table()->n_attributes() + 1);
      weighted_average_info_.Init(
        global.reference_table()->n_attributes() + 1);

      // Form the average information by combining from the children
      // information.
      average_info_.CombineWith(left_stat.average_info_);
      average_info_.CombineWith(right_stat.average_info_);
      weighted_average_info_.CombineWith(left_stat.weighted_average_info_);
      weighted_average_info_.CombineWith(right_stat.weighted_average_info_);

      // Sets the postponed quantities and summary statistics to zero.
      SetZero();
    }
};
}
}

#endif