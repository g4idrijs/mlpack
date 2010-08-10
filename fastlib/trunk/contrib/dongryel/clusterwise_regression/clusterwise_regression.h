/** @file clusterwise_regression.h
 *
 *  @brief A prototype of clusterwise regression via the EM algorithm.
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef ML_CLUSTERWISE_REGRESSION_CLUSTERWISE_REGRESSION_H
#define ML_CLUSTERWISE_REGRESSION_CLUSTERWISE_REGRESSION_H

#include "fastlib/fastlib.h"

namespace ml {
class ClusterwiseRegressionResult {
  private:

    /** @brief The number of clusters.
     */
    int num_clusters_;

    /** @brief The mixing probabilities for each cluster.
     */
    Vector membership_probabilities_;

    /** @brief The trained linear model for each cluster. The
     *         dimensionality is $D + 1$ by the number of clusters.
     */
    Matrix coefficients_;

    /** @brief The bandwidths per each cluster.
     */
    std::vector<double> bandwidths_;

  private:
    double Diameter_(const Matrix &dataset) const;

  public:

    double Predict(const Vector &datapoint, int cluster_number) const;

    double Predict(const Vector &datapoint) const;

    double Predict(
      const Vector &datapoint, double target, double *squared_error) const;

    ClusterwiseRegressionResult();
};

class ClusterwiseRegression {
  private:
    const Matrix *dataset_;

  private:
    void EStep_(ClusterwiseRegressionResult &result_out);

    void MStep_(ClusterwiseRegressionResult &result_out);

  public:

    ClusterwiseRegression();

    void Init(const Matrix &dataset_in);

    void Compute(
      int num_clusters_in,
      ClusterwiseRegressionResult *result_out);
};
};

#endif
