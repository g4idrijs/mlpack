#ifndef NNSVM_IMPL_H
#define NNSVM_IMPL_H

#include <fastlib/fx/io.h>
/**
* NNSVM initialization
*
* @param: labeled training set
* @param: number of classes (different labels) in the data set
* @param: module name
*/
template<typename TKernel>
void NNSVM<TKernel>::Init(const arma::mat& dataset, index_t n_classes)
{
  param_.kernel_.Init();
  param_.kernel_.GetName(param_.kernelname_);
  param_.kerneltypeid_ = param_.kernel_.GetTypeId();
  // c; default:10
  param_.c_ = mlpack::IO::GetParam<double>("nnsvm/c");
  // budget parameter, contorls # of support vectors; default: # of data samples
  if(!mlpack::IO::HasParam("nnsvm/b"))
    mlpack::IO::GetParam<double>("nnsvm/b") = dataset.n_rows;

  param_.b_ = mlpack::IO::GetParam<double>("nnsvm/b");
  // tolerance: eps, default: 1.0e-6
  param_.eps_ = mlpack::IO::GetParam<double>("nnsvm/eps");
  //max iterations: max_iter, default: 1000
  param_.max_iter_ = mlpack::IO::GetParam<double>("nnsvm/max_iter");
  fprintf(stderr, "c=%f, eps=%g, max_iter=%"LI" \n", param_.c_, param_.eps_, param_.max_iter_);
}

/**
* Initialization(data dependent) and training for NNSVM Classifier
*
* @param: labeled training set
* @param: number of classes (different labels) in the training set
* @param: module name
*/
template<typename TKernel>
void NNSVM<TKernel>::InitTrain(
    const arma::mat& dataset, index_t n_classes)
{
  Init(dataset, n_classes);
  /* # of features = # of rows in data matrix - 1, as last row is for labels*/
  num_features_ = dataset.n_rows - 1;
  DEBUG_ASSERT_MSG(n_classes == 2, "SVM is only a binary classifier");
  mlpack::IO::GetParam<std::string>("kernel_type") = typeid(TKernel).name();

  /* Initialize parameters c_, budget_, eps_, max_iter_, VTA_, alpha_, error_, thresh_ */
  NNSMO<Kernel> nnsmo;
  nnsmo.Init(dataset, param_.c_, param_.b_, param_.eps_, param_.max_iter_);
  nnsmo.kernel().Copy(param_.kernel_);

  /* 2-classes NNSVM training using NNSMO */
  mlpack::IO::StartTimer("nnsvm/nnsvm_train");
  nnsmo.Train();
  mlpack::IO::StopTimer("nnsvm/nnsvm_train");

  /* Get the trained bi-class model */
  nnsmo.GetNNSVM(support_vectors_, model_.sv_coef_, model_.w_);
  DEBUG_ASSERT(model_.sv_coef_.n_elem != 0);
  model_.num_sv_ = support_vectors_.n_cols;
  model_.thresh_ = nnsmo.threshold();
  DEBUG_ONLY(fprintf(stderr, "THRESHOLD: %f\n", model_.thresh_));

  /* Save models to file "nnsvm_model" */
  SaveModel("nnsvm_model"); // TODO: param_req
}

/**
* Save the NNSVM model to a text file
*
* @param: name of the model file
*/
template<typename TKernel>
void NNSVM<TKernel>::SaveModel(std::string modelfilename)
{
  FILE *fp = fopen(modelfilename.c_str(), "w");
  if (fp == NULL)
  {
    fprintf(stderr, "Cannot save trained model to file!");
    return;
  }

  fprintf(fp, "svm_type svm_c\n"); // TODO: svm-mu, svm-regression...
  fprintf(fp, "kernel_name %s\n", param_.kernelname_.c_str());
  fprintf(fp, "kernel_typeid %"LI"\n", param_.kerneltypeid_);
  // save kernel parameters
  param_.kernel_.SaveParam(fp);
  fprintf(fp, "total_num_sv %"LI"\n", model_.num_sv_);
  fprintf(fp, "threshold %g\n", model_.thresh_);
  fprintf(fp, "weights");
  index_t len = model_.w_.n_elem;
  for(index_t s = 0; s < len; s++)
    fprintf(fp, " %f", model_.w_[s]);
  fprintf(fp, "\nsvs\n");
  for(index_t i=0; i < model_.num_sv_; i++)
  {
     fprintf(fp, "%f ", model_.sv_coef_[i]);
     for(index_t s=0; s < num_features_; s++)
     {
       fprintf(fp, "%f ", support_vectors_(s, i));
     }
     fprintf(fp, "\n");
  }
  fclose(fp);
}

/**
* Load NNSVM model file
*
* @param: name of the model file
*/
// TODO: use XML
template<typename TKernel>
void NNSVM<TKernel>::LoadModel(arma::mat& testset, std::string modelfilename)
{
  /* Init */
  //fprintf(stderr, "modelfilename= %s\n", modelfilename.c_str());
  num_features_ = testset.n_cols - 1;

  model_.w_.set_size(num_features_);
  /* load model file */
  FILE *fp = fopen(modelfilename.c_str(), "r");
  if (fp == NULL)
  {
    fprintf(stderr, "Cannot open NNSVM model file!");
    return;
  }
  char cmd[80];
  index_t i, j;
  double temp_f;
  char kernel_name[1024];
  while (1)
  {
    fscanf(fp, "%80s", cmd);
    if(strcmp(cmd,"svm_type") == 0)
    {
      fscanf(fp, "%80s", cmd);
      if(strcmp(cmd, "svm_c") == 0)
      {
        fprintf(stderr, "SVM_C\n");
      }
    }
    else if (strcmp(cmd, "kernel_name") == 0)
    {
      fscanf(fp, "%80s", &kernel_name[0]);
      param_.kernelname_ = std::string(kernel_name);
    }
    else if (strcmp(cmd, "kernel_typeid") == 0)
    {
      fscanf(fp, "%"LI, &param_.kerneltypeid_);
    }
    else if (strcmp(cmd, "total_num_sv") == 0)
    {
      fscanf(fp, "%"LI, &model_.num_sv_);
    }
    else if (strcmp(cmd, "threshold") == 0)
    {
      fscanf(fp, "%lf", &model_.thresh_);
    }
    else if (strcmp(cmd, "weights")==0)
    {
      for (index_t s= 0; s < num_features_; s++)
      {
        fscanf(fp, "%lf", &temp_f);
        model_.w_[s] = temp_f;
      }
      break;
    }
  }
  support_vectors_.set_size(num_features_, model_.num_sv_);
  model_.sv_coef_.set_size(model_.num_sv_);

  while (1)
  {
    fscanf(fp, "%80s", cmd);
    if (strcmp(cmd, "svs") == 0)
    {
      for (i = 0; i < model_.num_sv_; i++)
      {
        fscanf(fp, "%lf", &temp_f);
        model_.sv_coef_[i] = temp_f;
        for (j = 0; j < num_features_; j++)
        {
          fscanf(fp, "%lf", &temp_f);
          support_vectors_(j, i) = temp_f;
        }
      }
      break;
    }
  }
  fclose(fp);
}

/**
* NNSVM classification for one testing vector
*
* @param: testing vector
*
* @return: a label (integer)
*/

template<typename TKernel>
index_t NNSVM<TKernel>::Classify(const arma::vec& datum)
{
  double summation = dot(model_.w_, datum);

  VERBOSE_MSG(0, "summation=%f, thresh_=%f", summation, model_.thresh_);

  return (summation - model_.thresh_ > 0.0) ? 1 : 0;

  return 0;
}

/**
* Online batch classification for multiple testing vectors. No need to load model file,
* since models are already in RAM.
*
* Note: for test set, if no true test labels provided, just put some dummy labels
* (e.g. all -1) in the last row of testset
*
* @param: testing set
* @param: file name of the testing data
*/
template<typename TKernel>
void NNSVM<TKernel>::BatchClassify(arma::mat& testset, std::string testlablefilename)
{
  FILE *fp = fopen(testlablefilename.c_str(), "w");
  if (fp == NULL)
  {
    fprintf(stderr, "Cannot save test labels to file!");
    return;
  }
  num_features_ = testset.n_cols - 1;
  for (index_t i = 0; i < testset.n_rows; i++)
  {
    arma::vec testvec(num_features_);
    for(index_t j = 0; j < num_features_; j++)
    {
      testvec[j] = testset(j, i);
    }
    index_t testlabel = Classify(testvec);
    fprintf(fp, "%"LI"\n", testlabel);
  }
  fclose(fp);
}

/**
* Load models from a file, and perform offline batch classification for multiple testing vectors
*
* @param: testing set
* @param: name of the model file
* @param: name of the file to store classified labels
*/
template<typename TKernel>
void NNSVM<TKernel>::LoadModelBatchClassify(arma::mat& testset, std::string modelfilename, std::string testlabelfilename)
{
  LoadModel(testset, modelfilename);
  BatchClassify(testset, testlabelfilename);
}
#endif