#include "tensorflow/core/kernels/initializable_lookup_table.h"

#include "tensorflow/core/lib/core/errors.h"

namespace tensorflow {
namespace lookup {

Status InitializableLookupTable::Find(const Tensor& keys, Tensor* values,
                                      const Tensor& default_value) {
  if (!is_initialized()) {
    return errors::FailedPrecondition("Table not initialized.");
  }
  TF_RETURN_IF_ERROR(CheckFindArguments(keys, *values, default_value));
  return DoFind(keys, values, default_value);
}

Status InitializableLookupTable::Initialize(InitTableIterator& iter) {
  if (!iter.Valid()) {
    return iter.status();
  }
  TF_RETURN_IF_ERROR(CheckKeyAndValueTensors(iter.keys(), iter.values()));

  mutex_lock l(mu_);
  if (is_initialized()) {
    return errors::FailedPrecondition("Table already initialized.");
  }

  TF_RETURN_IF_ERROR(DoPrepare(iter.total_size()));
  while (iter.Valid()) {
    TF_RETURN_IF_ERROR(DoInsert(iter.keys(), iter.values()));
    iter.Next();
  }
  if (!errors::IsOutOfRange(iter.status())) {
    return iter.status();
  }
  is_initialized_ = true;
  return Status::OK();
}

}  // namespace lookup
}  // namespace tensorflow
