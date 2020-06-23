/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/c/experimental/filesystem/plugins/gcs/gcs_filesystem.h"

#include "tensorflow/c/tf_status_helper.h"
#include "tensorflow/core/platform/path.h"
#include "tensorflow/core/platform/stacktrace_handler.h"
#include "tensorflow/core/platform/test.h"

#define ASSERT_TF_OK(x) ASSERT_EQ(TF_OK, TF_GetCode(x)) << TF_Message(x)

static const char* content = "abcdefghijklmnopqrstuvwxyz1234567890";  // 36

namespace gcs = google::cloud::storage;

namespace tensorflow {
namespace {

class GCSFilesystemTest : public ::testing::Test {
 public:
  void SetUp() override {
    root_dir_ = io::JoinPath(
        tmp_dir_,
        ::testing::UnitTest::GetInstance()->current_test_info()->name());
    status_ = TF_NewStatus();
    filesystem_ = new TF_Filesystem;
    tf_gcs_filesystem::Init(filesystem_, status_);
    ASSERT_TF_OK(status_) << "Could not initialize filesystem. "
                          << TF_Message(status_);
  }
  void TearDown() override {
    TF_DeleteStatus(status_);
    tf_gcs_filesystem::Cleanup(filesystem_);
    delete filesystem_;
  }

  static bool InitializeTmpDir() {
    // This env should be something like `gs://bucket/path`
    const char* test_dir = getenv("GCS_TEST_TMPDIR");
    if (test_dir != nullptr) {
      // We add a random value into `test_dir` to ensures that two consecutive
      // runs are unlikely to clash.
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution;
      std::string rng_val = std::to_string(distribution(gen));
      tmp_dir_ = io::JoinPath(string(test_dir), rng_val);
      return true;
    } else {
      return false;
    }
  }

  std::string GetURIForPath(absl::string_view path) {
    const std::string translated_name =
        tensorflow::io::JoinPath(root_dir_, path);
    return translated_name;
  }

 protected:
  TF_Filesystem* filesystem_;
  TF_Status* status_;

 private:
  std::string root_dir_;
  static std::string tmp_dir_;
};
std::string GCSFilesystemTest::tmp_dir_;

TEST_F(GCSFilesystemTest, StandaloneRandomAccessFile) {
  // TODO: Put the code which creates file on the server to a seperate function
  // if needed.
  std::string filepath = GetURIForPath("a_file");
  char* bucket;
  char* object;
  ParseGCSPath(filepath, false, &bucket, &object, status_);
  ASSERT_TF_OK(status_);
  auto gcs_client = static_cast<gcs::Client*>(filesystem_->plugin_filesystem);
  auto writer = gcs_client->WriteObject(bucket, object);
  writer << content;
  writer.Close();
  ASSERT_TRUE(writer.metadata()) << writer.metadata().status().message();

  TF_RandomAccessFile* file = new TF_RandomAccessFile;
  tf_gcs_filesystem::NewRandomAccessFile(filesystem_, filepath.c_str(), file,
                                         status_);
  ASSERT_TF_OK(status_);

  char* result = new char[36];
  int64_t read = tf_random_access_file::Read(file, 0, 36, result, status_);
  ASSERT_TF_OK(status_);
  ASSERT_EQ(read, 36) << "Number of bytes read: " << read;
  ASSERT_EQ(absl::string_view(result).substr(0, read),
            absl::string_view(content))
      << "Result: " << absl::string_view(result).substr(0, read);
  delete result;

TEST_F(GCSFilesystemTest, ParseGCSPath) {
  std::string bucket, object;
  ParseGCSPath("gs://bucket/path/to/object", false, &bucket, &object, status_);
  ASSERT_TF_OK(status_);
  ASSERT_EQ(bucket, "bucket");
  ASSERT_EQ(object, "path/to/object");

  ParseGCSPath("gs://bucket/", true, &bucket, &object, status_);
  ASSERT_TF_OK(status_);
  ASSERT_EQ(bucket, "bucket");

  ParseGCSPath("bucket/path/to/object", false, &bucket, &object, status_);
  ASSERT_EQ(TF_GetCode(status_), TF_INVALID_ARGUMENT);

  // bucket name must end with "/"
  ParseGCSPath("gs://bucket", true, &bucket, &object, status_);
  ASSERT_EQ(TF_GetCode(status_), TF_INVALID_ARGUMENT);

  ParseGCSPath("gs://bucket/", false, &bucket, &object, status_);
  ASSERT_EQ(TF_GetCode(status_), TF_INVALID_ARGUMENT);
}

}  // namespace
}  // namespace tensorflow

GTEST_API_ int main(int argc, char** argv) {
  tensorflow::testing::InstallStacktraceHandler();
  if (!tensorflow::GCSFilesystemTest::InitializeTmpDir()) {
    std::cerr << "Could not read GCS_TEST_TMPDIR env";
    return -1;
  }
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
