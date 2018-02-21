//
// Created by skama on 1/23/18.
//

#ifndef TENSORFLOW_CONTRIB_TENSORRT_RESOURCES_TRTRESOURCES_H_

#define TENSORFLOW_CONTRIB_TENSORRT_RESOURCES_TRTRESOURCES_H_

#include <string>
#include <sstream>
#include "tensorrt/include/NvInfer.h"
#include <vector>
#include <list>
#include <thread>
#include "tensorflow/contrib/tensorrt/log/trt_logger.h"
#include "tensorflow/contrib/tensorrt/resources/TRTInt8Calibrator.h"
#include "tensorflow/core/framework/resource_mgr.h"

namespace tensorflow {
namespace trt {
struct TRTCalibrationResource : public tensorflow::ResourceBase {
  TRTCalibrationResource()
      : calibrator(nullptr),
        builder(nullptr),
        network(nullptr),
        engine(nullptr),
        logger(nullptr),
        thr(nullptr)
        {}
  string DebugString() override {
    std::stringstream oss;
#define VALID_OR_NULL(ptr) (!ptr ? "nullptr" : std::hex<<(void)ptr<<std::dec<<std::endl)
    oss<<" Calibrator = "<<std::hex<<calibrator<<std::dec<<std::endl
       <<" Builder    = "<<std::hex<<builder<<std::dec<<std::endl
       <<" Network    = "<<std::hex<<network<<std::dec<<std::endl
       <<" Engine     = "<<std::hex<<engine<<std::dec<<std::endl
       <<" Logger     = "<<std::hex<<logger<<std::dec<<std::endl
       <<" Thread     = "<<std::hex<<thr<<std::dec<<std::endl;
    return oss.str();
#undef VALID_OR_NULL
  }
  ~TRTCalibrationResource(){
    VLOG(0)<<"Destroying Calibration Resource "<<std::endl<<DebugString();
  }
  TRTInt8Calibrator* calibrator;
  nvinfer1::IBuilder* builder;
  nvinfer1::INetworkDefinition* network;
  nvinfer1::ICudaEngine* engine;
  tensorflow::tensorrt::Logger* logger;
  std::thread* thr;
};
struct TRTWeightStore : public tensorflow::ResourceBase{
  TRTWeightStore(){}
  std::list<std::vector<uint8_t>> store_;
  string DebugString() override {
    std::stringstream oss;
    size_t lenBytes = 0;
    for(const auto& v:store_){
      lenBytes += v.size()*sizeof(uint8_t);
    }
    oss<<" Number of entries     = "<<store_.size()<<std::endl
       <<" Total number of bytes = "<<store_.size()*sizeof(std::vector<uint8_t>)+lenBytes
       <<std::endl;
    return oss.str();
  }
  virtual ~TRTWeightStore(){
    VLOG(1)<<"Destroying store"<<DebugString();
  }
};

struct TRTEngineResource : public tensorflow::ResourceBase {
  TRTEngineResource() : runtime(nullptr), ctx(nullptr){};
  string DebugString() override {return string("");}
  nvinfer1::IRuntime* runtime;
  nvinfer1::IExecutionContext* ctx;
};

}  // namespace trt
}  // namespace tensorflow
#endif  // TENSORFLOW_CONTRIB_TENSORRT_RESOURCEMGR_TRTRESOURCES_H_
