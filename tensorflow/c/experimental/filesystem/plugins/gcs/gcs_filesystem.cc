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

#include <stdlib.h>
#include <string.h>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "google/cloud/storage/client.h"
#include "tensorflow/c/env.h"
#include "tensorflow/c/experimental/filesystem/plugins/gcs/gcs_helper.h"
#include "tensorflow/c/tf_status.h"

// Implementation of a filesystem for GCS environments.
// This filesystem will support `gs://` URI schemes.
namespace gcs = google::cloud::storage;

// The environment variable that overrides the block size for aligned reads from
// GCS. Specified in MB (e.g. "16" = 16 x 1024 x 1024 = 16777216 bytes).
constexpr char kBlockSize[] = "GCS_READ_CACHE_BLOCK_SIZE_MB";
constexpr size_t kDefaultBlockSize = 64 * 1024 * 1024;
// The environment variable that overrides the max size of the LRU cache of
// blocks read from GCS. Specified in MB.
constexpr char kMaxCacheSize[] = "GCS_READ_CACHE_MAX_SIZE_MB";
constexpr size_t kDefaultMaxCacheSize = 0;
// The environment variable that overrides the maximum staleness of cached file
// contents. Once any block of a file reaches this staleness, all cached blocks
// will be evicted on the next read.
constexpr char kMaxStaleness[] = "GCS_READ_CACHE_MAX_STALENESS";
constexpr uint64_t kDefaultMaxStaleness = 0;

constexpr char kStatCacheMaxAge[] = "GCS_STAT_CACHE_MAX_AGE";
constexpr uint64_t kStatCacheDefaultMaxAge = 5;
// The environment variable that overrides the maximum number of entries in the
// Stat cache.
constexpr char kStatCacheMaxEntries[] = "GCS_STAT_CACHE_MAX_ENTRIES";
constexpr size_t kStatCacheDefaultMaxEntries = 1024;

// How to upload new data when Flush() is called multiple times.
// By default the entire file is reuploaded.
constexpr char kAppendMode[] = "GCS_APPEND_MODE";
// If GCS_APPEND_MODE=compose then instead the new data is uploaded to a
// temporary object and composed with the original object. This is disabled by
// default as the multiple API calls required add a risk of stranding temporary
// objects.
constexpr char kComposeAppend[] = "compose";

// We can cast `google::cloud::StatusCode` to `TF_Code` because they have the
// same integer values. See
// https://github.com/googleapis/google-cloud-cpp/blob/6c09cbfa0160bc046e5509b4dd2ab4b872648b4a/google/cloud/status.h#L32-L52
static inline void TF_SetStatusFromGCSStatus(
    const google::cloud::Status& gcs_status, TF_Status* status) {
  TF_SetStatus(status, static_cast<TF_Code>(gcs_status.code()),
               gcs_status.message().c_str());
}

static void* plugin_memory_allocate(size_t size) { return calloc(1, size); }
static void plugin_memory_free(void* ptr) { free(ptr); }

void ParseGCSPath(const std::string& fname, bool object_empty_ok,
                  std::string* bucket, std::string* object, TF_Status* status) {
  size_t scheme_end = fname.find("://") + 2;
  if (fname.substr(0, scheme_end + 1) != "gs://") {
    TF_SetStatus(status, TF_INVALID_ARGUMENT,
                 "GCS path doesn't start with 'gs://'.");
    return;
  }

  size_t bucket_end = fname.find("/", scheme_end + 1);
  if (bucket_end == std::string::npos) {
    TF_SetStatus(status, TF_INVALID_ARGUMENT,
                 "GCS path doesn't contain a bucket name.");
    return;
  }

  *bucket = fname.substr(scheme_end + 1, bucket_end - scheme_end - 1);
  *object = fname.substr(bucket_end + 1);

  if (object->empty() && !object_empty_ok) {
    TF_SetStatus(status, TF_INVALID_ARGUMENT,
                 "GCS path doesn't contain an object name.");
  }
}

/// Appends a trailing slash if the name doesn't already have one.
static void MaybeAppendSlash(std::string* name) {
  if (name->empty())
    *name = "/";
  else if (name->back() != '/')
    name->push_back('/');
}

// A helper function to actually read the data from GCS.
static int64_t LoadBufferFromGCS(const std::string& path, size_t offset,
                                 size_t buffer_size, char* buffer,
                                 gcs::Client* gcs_client, TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return -1;
  auto stream = gcs_client->ReadObject(
      bucket, object, gcs::ReadRange(offset, offset + buffer_size));
  TF_SetStatusFromGCSStatus(stream.status(), status);
  if ((TF_GetCode(status) != TF_OK) &&
      (TF_GetCode(status) != TF_OUT_OF_RANGE)) {
    return -1;
  }
  int64_t read;
  if (!absl::SimpleAtoi(stream.headers().find("content-length")->second,
                        &read)) {
    TF_SetStatus(status, TF_UNKNOWN, "Could not get content-length header");
    return -1;
  }
  stream.read(buffer, read);
  return stream.gcount();
}

// SECTION 1. Implementation for `TF_RandomAccessFile`
// ----------------------------------------------------------------------------
namespace tf_random_access_file {
using ReadFn =
    std::function<int64_t(const std::string& path, uint64_t offset, size_t n,
                          char* buffer, TF_Status* status)>;
typedef struct GCSFile {
  const std::string path;
  const bool is_cache_enable;
  const uint64_t buffer_size;
  ReadFn read_fn;
  absl::Mutex buffer_mutex;
  uint64_t buffer_start ABSL_GUARDED_BY(buffer_mutex);
  bool buffer_end_is_past_eof ABSL_GUARDED_BY(buffer_mutex);
  std::string buffer ABSL_GUARDED_BY(buffer_mutex);
  GCSFile(std::string path, bool is_cache_enable, uint64_t buffer_size,
          ReadFn read_fn)
      : path(path),
        is_cache_enable(is_cache_enable),
        buffer_size(buffer_size),
        buffer_mutex(),
        buffer_start(0),
        buffer_end_is_past_eof(false),
        buffer(),
        read_fn(std::move(read_fn)) {}
} GCSFile;

void Cleanup(TF_RandomAccessFile* file) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  delete gcs_file;
}

static void FillBuffer(uint64_t start, GCSFile* gcs_file, TF_Status* status) {
  gcs_file->buffer_start = start;
  gcs_file->buffer.resize(gcs_file->buffer_size);
  auto read =
      gcs_file->read_fn(gcs_file->path, gcs_file->buffer_start,
                        gcs_file->buffer_size, &(gcs_file->buffer[0]), status);
  gcs_file->buffer_end_is_past_eof = (TF_GetCode(status) == TF_OUT_OF_RANGE);
  gcs_file->buffer.resize(read);
}

// `google-cloud-cpp` is working on a feature that we may want to use.
// See https://github.com/googleapis/google-cloud-cpp/issues/4013.
int64_t Read(const TF_RandomAccessFile* file, uint64_t offset, size_t n,
             char* buffer, TF_Status* status) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  if (gcs_file->is_cache_enable || n > gcs_file->buffer_size) {
    return gcs_file->read_fn(gcs_file->path, offset, n, buffer, status);
  } else {
    absl::MutexLock l(&gcs_file->buffer_mutex);
    size_t buffer_end = gcs_file->buffer_start + gcs_file->buffer.size();
    size_t copy_size = 0;
    if (offset < buffer_end && gcs_file->buffer_start) {
      copy_size = (std::min)(n, static_cast<size_t>(buffer_end - offset));
      memcpy(buffer,
             gcs_file->buffer.data() + (offset - gcs_file->buffer_start),
             copy_size);
    }
    bool consumed_buffer_to_eof =
        offset + copy_size >= buffer_end && gcs_file->buffer_end_is_past_eof;
    if (copy_size < n && !consumed_buffer_to_eof) {
      FillBuffer(offset + copy_size, gcs_file, status);
      if (TF_GetCode(status) != TF_OK &&
          TF_GetCode(status) != TF_OUT_OF_RANGE) {
        // Empty the buffer to avoid caching bad reads.
        gcs_file->buffer.resize(0);
        return -1;
      }
      size_t remaining_copy =
          (std::min)(n - copy_size, gcs_file->buffer.size());
      memcpy(buffer + copy_size, gcs_file->buffer.data(), remaining_copy);
      copy_size += remaining_copy;
    }
    return copy_size;
  }
}

}  // namespace tf_random_access_file

// SECTION 2. Implementation for `TF_WritableFile`
// ----------------------------------------------------------------------------
namespace tf_writable_file {
typedef struct GCSFile {
  const std::string bucket;
  const std::string object;
  gcs::Client* gcs_client;  // not owned
  TempFile outfile;
  bool sync_need;
  // `offset` tells us how many bytes of this file are already uploaded to
  // server. If `offset == -1`, we always upload the entire temporary file.
  int64_t offset;
} GCSFile;

static void SyncImpl(const std::string& bucket, const std::string& object,
                     int64_t* offset, TempFile* outfile,
                     gcs::Client* gcs_client, TF_Status* status) {
  outfile->flush();
  // `*offset == 0` means this file does not exist on the server.
  if (*offset == -1 || *offset == 0) {
    // UploadFile will automatically switch to resumable upload based on Client
    // configuration.
    auto metadata = gcs_client->UploadFile(outfile->getName(), bucket, object);
    if (!metadata) {
      TF_SetStatusFromGCSStatus(metadata.status(), status);
      return;
    }
    if (*offset == 0) {
      if (!outfile->truncate()) {
        TF_SetStatus(status, TF_INTERNAL,
                     "Could not truncate internal temporary file.");
        return;
      }
      *offset = static_cast<int64_t>(metadata->size());
    }
    outfile->clear();
    outfile->seekp(0, std::ios::end);
    TF_SetStatus(status, TF_OK, "");
  } else {
    std::string temporary_object =
        gcs::CreateRandomPrefixName("tf_writable_file_gcs");
    auto metadata =
        gcs_client->UploadFile(outfile->getName(), bucket, temporary_object);
    if (!metadata) {
      TF_SetStatusFromGCSStatus(metadata.status(), status);
      return;
    }
    const std::vector<gcs::ComposeSourceObject> source_objects = {
        {object, {}, {}}, {temporary_object, {}, {}}};
    metadata = gcs_client->ComposeObject(bucket, source_objects, object);
    if (!metadata) {
      TF_SetStatusFromGCSStatus(metadata.status(), status);
      return;
    }
    // We have to delete the temporary object after composing.
    auto delete_status = gcs_client->DeleteObject(bucket, temporary_object);
    if (!delete_status.ok()) {
      TF_SetStatusFromGCSStatus(delete_status, status);
      return;
    }
    // We truncate the data that are already uploaded.
    if (!outfile->truncate()) {
      TF_SetStatus(status, TF_INTERNAL,
                   "Could not truncate internal temporary file.");
      return;
    }
    *offset = static_cast<int64_t>(metadata->size());
    TF_SetStatus(status, TF_OK, "");
  }
}

void Cleanup(TF_WritableFile* file) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  delete gcs_file;
}

void Append(const TF_WritableFile* file, const char* buffer, size_t n,
            TF_Status* status) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  if (!gcs_file->outfile.is_open()) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION,
                 "The internal temporary file is not writable.");
    return;
  }
  gcs_file->sync_need = true;
  gcs_file->outfile.write(buffer, n);
  if (!gcs_file->outfile)
    TF_SetStatus(status, TF_INTERNAL,
                 "Could not append to the internal temporary file.");
  else
    TF_SetStatus(status, TF_OK, "");
}

int64_t Tell(const TF_WritableFile* file, TF_Status* status) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  int64_t position = int64_t(gcs_file->outfile.tellp());
  if (position == -1)
    TF_SetStatus(status, TF_INTERNAL,
                 "tellp on the internal temporary file failed");
  else
    TF_SetStatus(status, TF_OK, "");
  return position == -1
             ? -1
             : position + (gcs_file->offset == -1 ? 0 : gcs_file->offset);
}

void Flush(const TF_WritableFile* file, TF_Status* status) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  if (gcs_file->sync_need) {
    if (!gcs_file->outfile) {
      TF_SetStatus(status, TF_INTERNAL,
                   "Could not append to the internal temporary file.");
      return;
    }
    SyncImpl(gcs_file->bucket, gcs_file->object, &gcs_file->offset,
             &gcs_file->outfile, gcs_file->gcs_client, status);
    if (TF_GetCode(status) != TF_OK) return;
    gcs_file->sync_need = false;
  } else {
    TF_SetStatus(status, TF_OK, "");
  }
}

void Sync(const TF_WritableFile* file, TF_Status* status) {
  Flush(file, status);
}

void Close(const TF_WritableFile* file, TF_Status* status) {
  auto gcs_file = static_cast<GCSFile*>(file->plugin_file);
  if (gcs_file->sync_need) {
    Flush(file, status);
  }
  gcs_file->outfile.close();
}

}  // namespace tf_writable_file

// SECTION 3. Implementation for `TF_ReadOnlyMemoryRegion`
// ----------------------------------------------------------------------------
namespace tf_read_only_memory_region {
typedef struct GCSMemoryRegion {
  const void* const address;
  const uint64_t length;
} GCSMemoryRegion;

void Cleanup(TF_ReadOnlyMemoryRegion* region) {
  auto r = static_cast<GCSMemoryRegion*>(region->plugin_memory_region);
  plugin_memory_free(const_cast<void*>(r->address));
  delete r;
}

const void* Data(const TF_ReadOnlyMemoryRegion* region) {
  auto r = static_cast<GCSMemoryRegion*>(region->plugin_memory_region);
  return r->address;
}

uint64_t Length(const TF_ReadOnlyMemoryRegion* region) {
  auto r = static_cast<GCSMemoryRegion*>(region->plugin_memory_region);
  return r->length;
}

}  // namespace tf_read_only_memory_region

// SECTION 4. Implementation for `TF_Filesystem`, the actual filesystem
// ----------------------------------------------------------------------------
namespace tf_gcs_filesystem {
// TODO(vnvo2409): Use partial reponse for better performance.
// TODO(vnvo2409): We could do some cleanups like `return TF_SetStatus`.
// TODO(vnvo2409): Refactor the filesystem implementation when
// https://github.com/googleapis/google-cloud-cpp/issues/4482 is done.
GCSFile::GCSFile(google::cloud::storage::Client&& gcs_client)
    : gcs_client(gcs_client), block_cache_lock() {
  const char* append_mode = std::getenv(kAppendMode);
  compose = (append_mode != nullptr) && (!strcmp(kAppendMode, append_mode));

  uint64_t value;
  block_size = kDefaultBlockSize;
  size_t max_bytes = kDefaultMaxCacheSize;
  uint64_t max_staleness = kDefaultMaxStaleness;

  // Apply the overrides for the block size (MB), max bytes (MB), and max
  // staleness (seconds) if provided.
  if (absl::SimpleAtoi(std::getenv(kBlockSize), &value)) {
    block_size = value * 1024 * 1024;
  }
  if (absl::SimpleAtoi(std::getenv(kMaxCacheSize), &value)) {
    max_bytes = static_cast<size_t>(value * 1024 * 1024);
  }
  if (absl::SimpleAtoi(std::getenv(kMaxStaleness), &value)) {
    max_staleness = value;
  }

  auto gcs_client_ptr = &this->gcs_client;
  file_block_cache = std::make_unique<RamFileBlockCache>(
      block_size, max_bytes, max_staleness,
      [gcs_client_ptr](const std::string& filename, size_t offset,
                       size_t buffer_size, char* buffer, TF_Status* status) {
        return LoadBufferFromGCS(filename, offset, buffer_size, buffer,
                                 gcs_client_ptr, status);
      });

  uint64_t stat_cache_max_age = kStatCacheDefaultMaxAge;
  size_t stat_cache_max_entries = kStatCacheDefaultMaxEntries;
  if (absl::SimpleAtoi(std::getenv(kStatCacheMaxAge), &value)) {
    stat_cache_max_age = value;
  }
  if (absl::SimpleAtoi(std::getenv(kStatCacheMaxEntries), &value)) {
    stat_cache_max_entries = static_cast<size_t>(value);
  }
  stat_cache = std::make_unique<ExpiringLRUCache<GcsFileStat>>(
      stat_cache_max_age, stat_cache_max_entries);
}

void Init(TF_Filesystem* filesystem, TF_Status* status) {
  google::cloud::StatusOr<gcs::Client> client =
      gcs::Client::CreateDefaultClient();
  if (!client) {
    TF_SetStatusFromGCSStatus(client.status(), status);
    return;
  }

  filesystem->plugin_filesystem = new GCSFile(std::move(client.value()));
  TF_SetStatus(status, TF_OK, "");
}

void Cleanup(TF_Filesystem* filesystem) {
  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  delete gcs_file;
}

// TODO(vnvo2409): Implement later
void NewRandomAccessFile(const TF_Filesystem* filesystem, const char* path,
                         TF_RandomAccessFile* file, TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  bool is_cache_enabled;
  {
    absl::MutexLock l(&gcs_file->block_cache_lock);
    is_cache_enabled = gcs_file->file_block_cache->IsCacheEnabled();
  }
  auto read_fn = [gcs_file, is_cache_enabled](
                     const std::string& path, uint64_t offset, size_t n,
                     char* buffer, TF_Status* status) -> int64_t {
    // TODO(vnvo2409): Check for `stat_cache`.
    auto read =
        is_cache_enabled
            ? gcs_file->file_block_cache->Read(path, offset, n, buffer, status)
            : LoadBufferFromGCS(path, offset, n, buffer, &gcs_file->gcs_client,
                                status);
    if (TF_GetCode(status) != TF_OK) return -1;
    if (read < n)
      TF_SetStatus(status, TF_OUT_OF_RANGE, "Read less bytes than requested");
    else
      TF_SetStatus(status, TF_OK, "");
    return read;
  };
  file->plugin_file = new tf_random_access_file::GCSFile(
      std::move(path), is_cache_enabled, gcs_file->block_size, read_fn);
  TF_SetStatus(status, TF_OK, "");
}

void NewWritableFile(const TF_Filesystem* filesystem, const char* path,
                     TF_WritableFile* file, TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  char* temp_file_name = TF_GetTempFileName("");
  file->plugin_file = new tf_writable_file::GCSFile(
      {std::move(bucket), std::move(object), &gcs_file->gcs_client,
       TempFile(temp_file_name, std::ios::binary | std::ios::out), true,
       (gcs_file->compose ? 0 : -1)});
  // We are responsible for freeing the pointer returned by TF_GetTempFileName
  free(temp_file_name);
  TF_SetStatus(status, TF_OK, "");
}

void NewAppendableFile(const TF_Filesystem* filesystem, const char* path,
                       TF_WritableFile* file, TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  char* temp_file_name_c_str = TF_GetTempFileName("");
  std::string temp_file_name(temp_file_name_c_str);  // To prevent memory-leak
  free(temp_file_name_c_str);

  if (!gcs_file->compose) {
    auto gcs_status =
        gcs_file->gcs_client.DownloadToFile(bucket, object, temp_file_name);
    TF_SetStatusFromGCSStatus(gcs_status, status);
    auto status_code = TF_GetCode(status);
    if (status_code != TF_OK && status_code != TF_NOT_FOUND) return;
    // If this file does not exist on server, we will need to sync it.
    bool sync_need = (status_code == TF_NOT_FOUND);
    file->plugin_file = new tf_writable_file::GCSFile(
        {std::move(bucket), std::move(object), &gcs_file->gcs_client,
         TempFile(temp_file_name, std::ios::binary | std::ios::app), sync_need,
         -1});
  } else {
    // If compose is true, we do not download anything.
    // Instead we only check if this file exists on server or not.
    auto metadata = gcs_file->gcs_client.GetObjectMetadata(bucket, object);
    TF_SetStatusFromGCSStatus(metadata.status(), status);
    if (TF_GetCode(status) == TF_OK) {
      file->plugin_file = new tf_writable_file::GCSFile(
          {std::move(bucket), std::move(object), &gcs_file->gcs_client,
           TempFile(temp_file_name, std::ios::binary | std::ios::trunc), false,
           static_cast<int64_t>(metadata->size())});
    } else if (TF_GetCode(status) == TF_NOT_FOUND) {
      file->plugin_file = new tf_writable_file::GCSFile(
          {std::move(bucket), std::move(object), &gcs_file->gcs_client,
           TempFile(temp_file_name, std::ios::binary | std::ios::trunc), true,
           0});
    } else {
      return;
    }
  }

  TF_SetStatus(status, TF_OK, "");
}

// TODO(vnvo2409): We could download into a local temporary file and use
// memory-mapping.
void NewReadOnlyMemoryRegionFromFile(const TF_Filesystem* filesystem,
                                     const char* path,
                                     TF_ReadOnlyMemoryRegion* region,
                                     TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  auto metadata = gcs_file->gcs_client.GetObjectMetadata(bucket, object);
  if (!metadata) {
    TF_SetStatusFromGCSStatus(metadata.status(), status);
    return;
  }

  TF_RandomAccessFile reader;
  NewRandomAccessFile(filesystem, path, &reader, status);
  if (TF_GetCode(status) != TF_OK) return;
  char* buffer = static_cast<char*>(plugin_memory_allocate(metadata->size()));
  int64_t read =
      tf_random_access_file::Read(&reader, 0, metadata->size(), buffer, status);
  tf_random_access_file::Cleanup(&reader);
  if (TF_GetCode(status) != TF_OK) return;

  if (read > 0 && buffer) {
    region->plugin_memory_region =
        new tf_read_only_memory_region::GCSMemoryRegion(
            {buffer, static_cast<uint64_t>(read)});
    TF_SetStatus(status, TF_OK, "");
  } else if (read == 0) {
    TF_SetStatus(status, TF_INVALID_ARGUMENT, "File is empty");
  }
}

void CreateDir(const TF_Filesystem* filesystem, const char* path,
               TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, true, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;
  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  if (object.empty()) {
    auto bucket_metadata = gcs_file->gcs_client.GetBucketMetadata(bucket);
    TF_SetStatusFromGCSStatus(bucket_metadata.status(), status);
    return;
  }

  MaybeAppendSlash(&object);
  auto object_metadata = gcs_file->gcs_client.GetObjectMetadata(bucket, object);
  TF_SetStatusFromGCSStatus(object_metadata.status(), status);
  if (TF_GetCode(status) == TF_NOT_FOUND) {
    auto insert_metadata =
        gcs_file->gcs_client.InsertObject(bucket, object, "");
    TF_SetStatusFromGCSStatus(insert_metadata.status(), status);
  } else if (TF_GetCode(status) == TF_OK) {
    TF_SetStatus(status, TF_ALREADY_EXISTS, path);
  }
}

// TODO(vnvo2409): `RecursivelyCreateDir` should use `CreateDir` instead of the
// default implementation. Because we could create an empty object whose
// key is equal to the `path` and Google Cloud Console will automatically
// display it as a directory tree.

void DeleteFile(const TF_Filesystem* filesystem, const char* path,
                TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;
  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  auto gcs_status = gcs_file->gcs_client.DeleteObject(bucket, object);
  TF_SetStatusFromGCSStatus(gcs_status, status);
}

void DeleteDir(const TF_Filesystem* filesystem, const char* path,
               TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;
  MaybeAppendSlash(&object);
  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  int object_count = 0;
  for (auto&& metadata :
       gcs_file->gcs_client.ListObjects(bucket, gcs::Prefix(object))) {
    if (!metadata) {
      TF_SetStatusFromGCSStatus(metadata.status(), status);
      return;
    }
    ++object_count;
    // We consider a path is a non-empty directory in two cases:
    // - There are more than two objects whose keys start with the name of this
    // directory.
    // - There is one object whose key contains the name of this directory ( but
    // not equal ).
    if (object_count > 1 || metadata->name() != object) {
      TF_SetStatus(status, TF_FAILED_PRECONDITION,
                   "Cannot delete a non-empty directory.");
      return;
    }
  }
  auto gcs_status = gcs_file->gcs_client.DeleteObject(bucket, object);
  TF_SetStatusFromGCSStatus(gcs_status, status);
}

// TODO(vnvo2409): `DeleteRecursively` needs `GetChildrens` but there will be
// some differents compared to the default implementation. Will be refactored.
static void DeleteRecursively(const TF_Filesystem* filesystem, const char* path,
                              uint64_t* undeleted_files,
                              uint64_t* undeleted_dirs, TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, false, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  auto gcs_status = gcs::DeleteByPrefix(gcs_file->gcs_client, bucket, object);
  TF_SetStatusFromGCSStatus(gcs_status, status);
  if (TF_GetCode(status) != TF_OK) return;
  *undeleted_dirs = 0;
  *undeleted_files = 0;
}

// TODO(vnvo2409): `RewriteObjectBlocking` will set `status` to `TF_NOT_FOUND`
// if the object does not exist. In that case, we will have to check if the
// `src` is a directory or not to set the correspondent `status` (i.e
// `TF_NOT_FOUND` if path `src` does not exist, `TF_FAILED_PRECONDITION` if
// path `src` is a directory).
void RenameFile(const TF_Filesystem* filesystem, const char* src,
                const char* dst, TF_Status* status) {
  std::string bucket_src, object_src;
  ParseGCSPath(src, false, &bucket_src, &object_src, status);
  if (TF_GetCode(status) != TF_OK) return;

  std::string bucket_dst, object_dst;
  ParseGCSPath(dst, false, &bucket_dst, &object_dst, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  auto metadata = gcs_file->gcs_client.RewriteObjectBlocking(
      bucket_src, object_src, bucket_dst, object_dst);
  if (!metadata) {
    TF_SetStatusFromGCSStatus(metadata.status(), status);
    return;
  }
  auto gcs_status = gcs_file->gcs_client.DeleteObject(bucket_src, object_src);
  TF_SetStatusFromGCSStatus(gcs_status, status);
}

void CopyFile(const TF_Filesystem* filesystem, const char* src, const char* dst,
              TF_Status* status) {
  std::string bucket_src, object_src;
  ParseGCSPath(src, false, &bucket_src, &object_src, status);
  if (TF_GetCode(status) != TF_OK) return;

  std::string bucket_dst, object_dst;
  ParseGCSPath(dst, false, &bucket_dst, &object_dst, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  auto metadata = gcs_file->gcs_client.RewriteObjectBlocking(
      bucket_src, object_src, bucket_dst, object_dst);
  TF_SetStatusFromGCSStatus(metadata.status(), status);
}

// TODO(vnvo2409): This approach can cause a problem when our path is
// `path/to/dir` and there is an object with key `path/to/directory`. Will be
// fixed when refactoring.
void PathExists(const TF_Filesystem* filesystem, const char* path,
                TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, true, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  for (auto&& metadata :
       gcs_file->gcs_client.ListObjects(bucket, gcs::Prefix(object))) {
    if (!metadata) {
      TF_SetStatusFromGCSStatus(metadata.status(), status);
      return;
    }
    // We consider a path exists if there is at least one object whose key
    // contains the path.
    return TF_SetStatus(status, TF_OK, "");
  }
  return TF_SetStatus(
      status, TF_NOT_FOUND,
      absl::StrCat("The path ", path, " does not exist.").c_str());
}

bool IsDirectory(const TF_Filesystem* filesystem, const char* path,
                 TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, true, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return false;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  if (object.empty()) {
    auto bucket_metadata = gcs_file->gcs_client.GetBucketMetadata(bucket);
    TF_SetStatusFromGCSStatus(bucket_metadata.status(), status);
    if (TF_GetCode(status) == TF_OK)
      return true;
    else
      return false;
  }

  // We check if there is an object with this key on the GCS server.
  auto metadata = gcs_file->gcs_client.GetObjectMetadata(bucket, object);
  if (metadata) {
    TF_SetStatus(status, TF_OK, "");
    if (metadata->name().back() == '/')
      return true;
    else
      return false;
  }

  // If there is no object with this key on the GCS server. We check if there is
  // any object whose key contains that path.
  MaybeAppendSlash(&object);
  for (auto&& metadata :
       gcs_file->gcs_client.ListObjects(bucket, gcs::Prefix(object))) {
    if (!metadata) {
      TF_SetStatusFromGCSStatus(metadata.status(), status);
      return false;
    }
    TF_SetStatus(status, TF_OK, "");
    return true;
  }
  TF_SetStatus(status, TF_NOT_FOUND,
               absl::StrCat("The path ", path, " does not exist.").c_str());
  return false;
}

void Stat(const TF_Filesystem* filesystem, const char* path,
          TF_FileStatistics* stats, TF_Status* status) {
  std::string bucket, object;
  ParseGCSPath(path, true, &bucket, &object, status);
  if (TF_GetCode(status) != TF_OK) return;

  auto gcs_file = static_cast<GCSFile*>(filesystem->plugin_filesystem);
  if (object.empty()) {
    auto bucket_metadata = gcs_file->gcs_client.GetBucketMetadata(bucket);
    TF_SetStatusFromGCSStatus(bucket_metadata.status(), status);
    if (TF_GetCode(status) == TF_OK) {
      stats->is_directory = true;
      stats->length = 0;
      stats->mtime_nsec = 0;
    }
    return;
  }
  if (IsDirectory(filesystem, path, status)) {
    stats->is_directory = true;
    stats->length = 0;
    stats->mtime_nsec = 0;
    return TF_SetStatus(status, TF_OK, "");
  }
  if (TF_GetCode(status) == TF_OK) {
    auto metadata = gcs_file->gcs_client.GetObjectMetadata(bucket, object);
    if (metadata) {
      stats->is_directory = false;
      stats->length = metadata.value().size();
      stats->mtime_nsec = metadata.value()
                              .time_storage_class_updated()
                              .time_since_epoch()
                              .count();
    }
    TF_SetStatusFromGCSStatus(metadata.status(), status);
  }
}

}  // namespace tf_gcs_filesystem

static void ProvideFilesystemSupportFor(TF_FilesystemPluginOps* ops,
                                        const char* uri) {
  TF_SetFilesystemVersionMetadata(ops);
  ops->scheme = strdup(uri);

  ops->random_access_file_ops = static_cast<TF_RandomAccessFileOps*>(
      plugin_memory_allocate(TF_RANDOM_ACCESS_FILE_OPS_SIZE));
  ops->random_access_file_ops->cleanup = tf_random_access_file::Cleanup;
  ops->random_access_file_ops->read = tf_random_access_file::Read;

  ops->writable_file_ops = static_cast<TF_WritableFileOps*>(
      plugin_memory_allocate(TF_WRITABLE_FILE_OPS_SIZE));
  ops->writable_file_ops->cleanup = tf_writable_file::Cleanup;

  ops->filesystem_ops = static_cast<TF_FilesystemOps*>(
      plugin_memory_allocate(TF_FILESYSTEM_OPS_SIZE));
  ops->filesystem_ops->init = tf_gcs_filesystem::Init;
  ops->filesystem_ops->cleanup = tf_gcs_filesystem::Cleanup;
  ops->filesystem_ops->new_random_access_file =
      tf_gcs_filesystem::NewRandomAccessFile;
  ops->filesystem_ops->new_writable_file = tf_gcs_filesystem::NewWritableFile;
  ops->filesystem_ops->new_appendable_file =
      tf_gcs_filesystem::NewAppendableFile;
}

void TF_InitPlugin(TF_FilesystemPluginInfo* info) {
  info->plugin_memory_allocate = plugin_memory_allocate;
  info->plugin_memory_free = plugin_memory_free;
  info->num_schemes = 1;
  info->ops = static_cast<TF_FilesystemPluginOps*>(
      plugin_memory_allocate(info->num_schemes * sizeof(info->ops[0])));
  ProvideFilesystemSupportFor(&info->ops[0], "gs");
}
