// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "arrow/array/builder_binary.h"
#include "arrow/array/builder_primitive.h"
#include "arrow/array/builder_time.h"
#include "arrow/result.h"
#include "arrow/status.h"
#include "arrow/testing/gtest_compat.h"
#include "arrow/testing/util.h"
#include "arrow/testing/visibility.h"
#include "arrow/type_fwd.h"
#include "arrow/type_traits.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/macros.h"
#include "arrow/util/string_builder.h"
#include "arrow/util/type_fwd.h"

// NOTE: failing must be inline in the macros below, to get correct file / line number
// reporting on test failures.

// NOTE: using a for loop for this macro allows extra failure messages to be
// appended with operator<<
#define ASSERT_RAISES(ENUM, expr)                                                 \
  for (::arrow::Status _st = ::arrow::internal::GenericToStatus((expr));          \
       !_st.Is##ENUM();)                                                          \
  FAIL() << "Expected '" ARROW_STRINGIFY(expr) "' to fail with " ARROW_STRINGIFY( \
                ENUM) ", but got "                                                \
         << _st.ToString()

#define ASSERT_RAISES_WITH_MESSAGE(ENUM, message, expr)                               \
  do {                                                                                \
    auto _res = (expr);                                                               \
    ::arrow::Status _st = ::arrow::internal::GenericToStatus(_res);                   \
    if (!_st.Is##ENUM()) {                                                            \
      FAIL() << "Expected '" ARROW_STRINGIFY(expr) "' to fail with " ARROW_STRINGIFY( \
                    ENUM) ", but got "                                                \
             << _st.ToString();                                                       \
    }                                                                                 \
    ASSERT_EQ((message), _st.ToString());                                             \
  } while (false)

#define EXPECT_RAISES_WITH_MESSAGE_THAT(ENUM, matcher, expr)                             \
  do {                                                                                   \
    auto _res = (expr);                                                                  \
    ::arrow::Status _st = ::arrow::internal::GenericToStatus(_res);                      \
    EXPECT_TRUE(_st.Is##ENUM()) << "Expected '" ARROW_STRINGIFY(expr) "' to fail with "  \
                                << ARROW_STRINGIFY(ENUM) ", but got " << _st.ToString(); \
    EXPECT_THAT(_st.ToString(), (matcher));                                              \
  } while (false)

#define EXPECT_RAISES_WITH_CODE_AND_MESSAGE_THAT(code, matcher, expr) \
  do {                                                                \
    auto _res = (expr);                                               \
    ::arrow::Status _st = ::arrow::internal::GenericToStatus(_res);   \
    EXPECT_EQ(_st.CodeAsString(), Status::CodeAsString(code));        \
    EXPECT_THAT(_st.ToString(), (matcher));                           \
  } while (false)

#define ASSERT_OK(expr)                                                              \
  for (::arrow::Status _st = ::arrow::internal::GenericToStatus((expr)); !_st.ok();) \
  FAIL() << "'" ARROW_STRINGIFY(expr) "' failed with " << _st.ToString()

#define ASSERT_OK_NO_THROW(expr) ASSERT_NO_THROW(ASSERT_OK(expr))

#define ARROW_EXPECT_OK(expr)                                           \
  do {                                                                  \
    auto _res = (expr);                                                 \
    ::arrow::Status _st = ::arrow::internal::GenericToStatus(_res);     \
    EXPECT_TRUE(_st.ok()) << "'" ARROW_STRINGIFY(expr) "' failed with " \
                          << _st.ToString();                            \
  } while (false)

#define ASSERT_NOT_OK(expr)                                                         \
  for (::arrow::Status _st = ::arrow::internal::GenericToStatus((expr)); _st.ok();) \
  FAIL() << "'" ARROW_STRINGIFY(expr) "' did not failed" << _st.ToString()

#define ABORT_NOT_OK(expr)                                          \
  do {                                                              \
    auto _res = (expr);                                             \
    ::arrow::Status _st = ::arrow::internal::GenericToStatus(_res); \
    if (ARROW_PREDICT_FALSE(!_st.ok())) {                           \
      _st.Abort();                                                  \
    }                                                               \
  } while (false);

#define ASSIGN_OR_HANDLE_ERROR_IMPL(handle_error, status_name, lhs, rexpr) \
  auto&& status_name = (rexpr);                                            \
  handle_error(status_name.status());                                      \
  lhs = std::move(status_name).ValueOrDie();

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSIGN_OR_HANDLE_ERROR_IMPL(           \
      ASSERT_OK, ARROW_ASSIGN_OR_RAISE_NAME(_error_or_value, __COUNTER__), lhs, rexpr);

#define ASSIGN_OR_ABORT(lhs, rexpr)                                                     \
  ASSIGN_OR_HANDLE_ERROR_IMPL(ABORT_NOT_OK,                                             \
                              ARROW_ASSIGN_OR_RAISE_NAME(_error_or_value, __COUNTER__), \
                              lhs, rexpr);

#define EXPECT_OK_AND_ASSIGN(lhs, rexpr)                                                \
  ASSIGN_OR_HANDLE_ERROR_IMPL(ARROW_EXPECT_OK,                                          \
                              ARROW_ASSIGN_OR_RAISE_NAME(_error_or_value, __COUNTER__), \
                              lhs, rexpr);

#define ASSERT_OK_AND_EQ(expected, expr)        \
  do {                                          \
    ASSERT_OK_AND_ASSIGN(auto _actual, (expr)); \
    ASSERT_EQ(expected, _actual);               \
  } while (0)

// A generalized version of GTest's SCOPED_TRACE that takes arbitrary arguments.
//   ARROW_SCOPED_TRACE("some variable = ", some_variable, ...)

#define ARROW_SCOPED_TRACE(...) SCOPED_TRACE(::arrow::util::StringBuilder(__VA_ARGS__))

namespace arrow {

// ----------------------------------------------------------------------
// Useful testing::Types declarations

inline void PrintTo(StatusCode code, std::ostream* os) {
  *os << Status::CodeAsString(code);
}

using NumericArrowTypes =
    ::testing::Types<UInt8Type, UInt16Type, UInt32Type, UInt64Type, Int8Type, Int16Type,
                     Int32Type, Int64Type, FloatType, DoubleType>;

using RealArrowTypes = ::testing::Types<FloatType, DoubleType>;

using IntegralArrowTypes = ::testing::Types<UInt8Type, UInt16Type, UInt32Type, UInt64Type,
                                            Int8Type, Int16Type, Int32Type, Int64Type>;

using PhysicalIntegralArrowTypes =
    ::testing::Types<UInt8Type, UInt16Type, UInt32Type, UInt64Type, Int8Type, Int16Type,
                     Int32Type, Int64Type, Date32Type, Date64Type, Time32Type, Time64Type,
                     TimestampType, MonthIntervalType>;

using PrimitiveArrowTypes =
    ::testing::Types<BooleanType, Int8Type, UInt8Type, Int16Type, UInt16Type, Int32Type,
                     UInt32Type, Int64Type, UInt64Type, FloatType, DoubleType>;

using TemporalArrowTypes =
    ::testing::Types<Date32Type, Date64Type, TimestampType, Time32Type, Time64Type>;

using DecimalArrowTypes = ::testing::Types<Decimal128Type, Decimal256Type>;

using BinaryArrowTypes =
    ::testing::Types<BinaryType, LargeBinaryType, StringType, LargeStringType>;

using StringArrowTypes = ::testing::Types<StringType, LargeStringType>;

using ListArrowTypes = ::testing::Types<ListType, LargeListType>;

using UnionArrowTypes = ::testing::Types<SparseUnionType, DenseUnionType>;

class Array;
class ChunkedArray;
class RecordBatch;
class Table;
struct Datum;

ARROW_TESTING_EXPORT
std::vector<Type::type> AllTypeIds();

#define ASSERT_ARRAYS_EQUAL(lhs, rhs) AssertArraysEqual((lhs), (rhs))
#define ASSERT_BATCHES_EQUAL(lhs, rhs) AssertBatchesEqual((lhs), (rhs))
#define ASSERT_BATCHES_APPROX_EQUAL(lhs, rhs) AssertBatchesApproxEqual((lhs), (rhs))
#define ASSERT_TABLES_EQUAL(lhs, rhs) AssertTablesEqual((lhs), (rhs))

// If verbose is true, then the arrays will be pretty printed
ARROW_TESTING_EXPORT void AssertArraysEqual(const Array& expected, const Array& actual,
                                            bool verbose = false,
                                            const EqualOptions& options = {});
ARROW_TESTING_EXPORT void AssertArraysApproxEqual(const Array& expected,
                                                  const Array& actual,
                                                  bool verbose = false,
                                                  const EqualOptions& options = {});
// Returns true when values are both null
ARROW_TESTING_EXPORT void AssertScalarsEqual(
    const Scalar& expected, const Scalar& actual, bool verbose = false,
    const EqualOptions& options = EqualOptions::Defaults());
ARROW_TESTING_EXPORT void AssertScalarsApproxEqual(
    const Scalar& expected, const Scalar& actual, bool verbose = false,
    const EqualOptions& options = EqualOptions::Defaults());
ARROW_TESTING_EXPORT void AssertBatchesEqual(const RecordBatch& expected,
                                             const RecordBatch& actual,
                                             bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertBatchesApproxEqual(const RecordBatch& expected,
                                                   const RecordBatch& actual);
ARROW_TESTING_EXPORT void AssertChunkedEqual(const ChunkedArray& expected,
                                             const ChunkedArray& actual);
ARROW_TESTING_EXPORT void AssertChunkedEqual(const ChunkedArray& actual,
                                             const ArrayVector& expected);
// Like ChunkedEqual, but permits different chunk layout
ARROW_TESTING_EXPORT void AssertChunkedEquivalent(const ChunkedArray& expected,
                                                  const ChunkedArray& actual);
ARROW_TESTING_EXPORT void AssertChunkedApproxEquivalent(
    const ChunkedArray& expected, const ChunkedArray& actual,
    const EqualOptions& equal_options = EqualOptions::Defaults());
ARROW_TESTING_EXPORT void AssertBufferEqual(const Buffer& buffer,
                                            const std::vector<uint8_t>& expected);
ARROW_TESTING_EXPORT void AssertBufferEqual(const Buffer& buffer,
                                            const std::string& expected);
ARROW_TESTING_EXPORT void AssertBufferEqual(const Buffer& buffer, const Buffer& expected);

ARROW_TESTING_EXPORT void AssertTypeEqual(const DataType& lhs, const DataType& rhs,
                                          bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertTypeEqual(const std::shared_ptr<DataType>& lhs,
                                          const std::shared_ptr<DataType>& rhs,
                                          bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertFieldEqual(const Field& lhs, const Field& rhs,
                                           bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertFieldEqual(const std::shared_ptr<Field>& lhs,
                                           const std::shared_ptr<Field>& rhs,
                                           bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertSchemaEqual(const Schema& lhs, const Schema& rhs,
                                            bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertSchemaEqual(const std::shared_ptr<Schema>& lhs,
                                            const std::shared_ptr<Schema>& rhs,
                                            bool check_metadata = false);

ARROW_TESTING_EXPORT void AssertTypeNotEqual(const DataType& lhs, const DataType& rhs,
                                             bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertTypeNotEqual(const std::shared_ptr<DataType>& lhs,
                                             const std::shared_ptr<DataType>& rhs,
                                             bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertFieldNotEqual(const Field& lhs, const Field& rhs,
                                              bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertFieldNotEqual(const std::shared_ptr<Field>& lhs,
                                              const std::shared_ptr<Field>& rhs,
                                              bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertSchemaNotEqual(const Schema& lhs, const Schema& rhs,
                                               bool check_metadata = false);
ARROW_TESTING_EXPORT void AssertSchemaNotEqual(const std::shared_ptr<Schema>& lhs,
                                               const std::shared_ptr<Schema>& rhs,
                                               bool check_metadata = false);

ARROW_TESTING_EXPORT Result<util::optional<std::string>> PrintArrayDiff(
    const ChunkedArray& expected, const ChunkedArray& actual);

ARROW_TESTING_EXPORT void AssertTablesEqual(const Table& expected, const Table& actual,
                                            bool same_chunk_layout = true,
                                            bool flatten = false);

ARROW_TESTING_EXPORT void AssertDatumsEqual(const Datum& expected, const Datum& actual,
                                            bool verbose = false);
ARROW_TESTING_EXPORT void AssertDatumsApproxEqual(
    const Datum& expected, const Datum& actual, bool verbose = false,
    const EqualOptions& options = EqualOptions::Defaults());

template <typename C_TYPE>
void AssertNumericDataEqual(const C_TYPE* raw_data,
                            const std::vector<C_TYPE>& expected_values) {
  for (auto expected : expected_values) {
    ASSERT_EQ(expected, *raw_data);
    ++raw_data;
  }
}

ARROW_TESTING_EXPORT void CompareBatch(const RecordBatch& left, const RecordBatch& right,
                                       bool compare_metadata = true);

ARROW_TESTING_EXPORT void ApproxCompareBatch(const RecordBatch& left,
                                             const RecordBatch& right,
                                             bool compare_metadata = true);

// Check if the padding of the buffers of the array is zero.
// Also cause valgrind warnings if the padding bytes are uninitialized.
ARROW_TESTING_EXPORT void AssertZeroPadded(const Array& array);

// Check if the valid buffer bytes are initialized
// and cause valgrind warnings otherwise.
ARROW_TESTING_EXPORT void TestInitialized(const ArrayData& array);
ARROW_TESTING_EXPORT void TestInitialized(const Array& array);

template <typename BuilderType>
void FinishAndCheckPadding(BuilderType* builder, std::shared_ptr<Array>* out) {
  ASSERT_OK_AND_ASSIGN(*out, builder->Finish());
  AssertZeroPadded(**out);
  TestInitialized(**out);
}

#define DECL_T() typedef typename TestFixture::T T;

#define DECL_TYPE() typedef typename TestFixture::Type Type;

// ArrayFromJSON: construct an Array from a simple JSON representation

ARROW_TESTING_EXPORT
std::shared_ptr<Array> ArrayFromJSON(const std::shared_ptr<DataType>&,
                                     util::string_view json);

ARROW_TESTING_EXPORT
std::shared_ptr<Array> DictArrayFromJSON(const std::shared_ptr<DataType>& type,
                                         util::string_view indices_json,
                                         util::string_view dictionary_json);

ARROW_TESTING_EXPORT
std::shared_ptr<RecordBatch> RecordBatchFromJSON(const std::shared_ptr<Schema>&,
                                                 util::string_view);

ARROW_TESTING_EXPORT
std::shared_ptr<ChunkedArray> ChunkedArrayFromJSON(const std::shared_ptr<DataType>&,
                                                   const std::vector<std::string>& json);

ARROW_TESTING_EXPORT
std::shared_ptr<Scalar> ScalarFromJSON(const std::shared_ptr<DataType>&,
                                       util::string_view json);

ARROW_TESTING_EXPORT
std::shared_ptr<Scalar> DictScalarFromJSON(const std::shared_ptr<DataType>&,
                                           util::string_view index_json,
                                           util::string_view dictionary_json);

ARROW_TESTING_EXPORT
std::shared_ptr<Table> TableFromJSON(const std::shared_ptr<Schema>&,
                                     const std::vector<std::string>& json);

// ArrayFromVector: construct an Array from vectors of C values

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ArrayFromVector(const std::shared_ptr<DataType>& type,
                     const std::vector<bool>& is_valid, const std::vector<C_TYPE>& values,
                     std::shared_ptr<Array>* out) {
  auto type_id = TYPE::type_id;
  ASSERT_EQ(type_id, type->id())
      << "template parameter and concrete DataType instance don't agree";

  std::unique_ptr<ArrayBuilder> builder_ptr;
  ASSERT_OK(MakeBuilder(default_memory_pool(), type, &builder_ptr));
  // Get the concrete builder class to access its Append() specializations
  auto& builder = dynamic_cast<typename TypeTraits<TYPE>::BuilderType&>(*builder_ptr);

  for (size_t i = 0; i < values.size(); ++i) {
    if (is_valid[i]) {
      ASSERT_OK(builder.Append(values[i]));
    } else {
      ASSERT_OK(builder.AppendNull());
    }
  }
  ASSERT_OK(builder.Finish(out));
}

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ArrayFromVector(const std::shared_ptr<DataType>& type,
                     const std::vector<C_TYPE>& values, std::shared_ptr<Array>* out) {
  auto type_id = TYPE::type_id;
  ASSERT_EQ(type_id, type->id())
      << "template parameter and concrete DataType instance don't agree";

  std::unique_ptr<ArrayBuilder> builder_ptr;
  ASSERT_OK(MakeBuilder(default_memory_pool(), type, &builder_ptr));
  // Get the concrete builder class to access its Append() specializations
  auto& builder = dynamic_cast<typename TypeTraits<TYPE>::BuilderType&>(*builder_ptr);

  for (size_t i = 0; i < values.size(); ++i) {
    ASSERT_OK(builder.Append(values[i]));
  }
  ASSERT_OK(builder.Finish(out));
}

// Overloads without a DataType argument, for parameterless types

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ArrayFromVector(const std::vector<bool>& is_valid, const std::vector<C_TYPE>& values,
                     std::shared_ptr<Array>* out) {
  auto type = TypeTraits<TYPE>::type_singleton();
  ArrayFromVector<TYPE, C_TYPE>(type, is_valid, values, out);
}

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ArrayFromVector(const std::vector<C_TYPE>& values, std::shared_ptr<Array>* out) {
  auto type = TypeTraits<TYPE>::type_singleton();
  ArrayFromVector<TYPE, C_TYPE>(type, values, out);
}

// ChunkedArrayFromVector: construct a ChunkedArray from vectors of C values

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ChunkedArrayFromVector(const std::shared_ptr<DataType>& type,
                            const std::vector<std::vector<bool>>& is_valid,
                            const std::vector<std::vector<C_TYPE>>& values,
                            std::shared_ptr<ChunkedArray>* out) {
  ArrayVector chunks;
  ASSERT_EQ(is_valid.size(), values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    std::shared_ptr<Array> array;
    ArrayFromVector<TYPE, C_TYPE>(type, is_valid[i], values[i], &array);
    chunks.push_back(array);
  }
  *out = std::make_shared<ChunkedArray>(chunks);
}

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ChunkedArrayFromVector(const std::shared_ptr<DataType>& type,
                            const std::vector<std::vector<C_TYPE>>& values,
                            std::shared_ptr<ChunkedArray>* out) {
  ArrayVector chunks;
  for (size_t i = 0; i < values.size(); ++i) {
    std::shared_ptr<Array> array;
    ArrayFromVector<TYPE, C_TYPE>(type, values[i], &array);
    chunks.push_back(array);
  }
  *out = std::make_shared<ChunkedArray>(chunks);
}

// Overloads without a DataType argument, for parameterless types

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ChunkedArrayFromVector(const std::vector<std::vector<bool>>& is_valid,
                            const std::vector<std::vector<C_TYPE>>& values,
                            std::shared_ptr<ChunkedArray>* out) {
  auto type = TypeTraits<TYPE>::type_singleton();
  ChunkedArrayFromVector<TYPE, C_TYPE>(type, is_valid, values, out);
}

template <typename TYPE, typename C_TYPE = typename TYPE::c_type>
void ChunkedArrayFromVector(const std::vector<std::vector<C_TYPE>>& values,
                            std::shared_ptr<ChunkedArray>* out) {
  auto type = TypeTraits<TYPE>::type_singleton();
  ChunkedArrayFromVector<TYPE, C_TYPE>(type, values, out);
}

template <typename T>
static inline Status GetBitmapFromVector(const std::vector<T>& is_valid,
                                         std::shared_ptr<Buffer>* result) {
  size_t length = is_valid.size();

  ARROW_ASSIGN_OR_RAISE(auto buffer, AllocateEmptyBitmap(length));

  uint8_t* bitmap = buffer->mutable_data();
  for (size_t i = 0; i < static_cast<size_t>(length); ++i) {
    if (is_valid[i]) {
      BitUtil::SetBit(bitmap, i);
    }
  }

  *result = buffer;
  return Status::OK();
}

template <typename T>
inline void BitmapFromVector(const std::vector<T>& is_valid,
                             std::shared_ptr<Buffer>* out) {
  ASSERT_OK(GetBitmapFromVector(is_valid, out));
}

// Given an array, return a new identical array except for one validity bit
// set to a new value.
// This is useful to force the underlying "value" of null entries to otherwise
// invalid data and check that errors don't get reported.
ARROW_TESTING_EXPORT
std::shared_ptr<Array> TweakValidityBit(const std::shared_ptr<Array>& array,
                                        int64_t index, bool validity);

ARROW_TESTING_EXPORT
void SleepFor(double seconds);

// Sleeps for a very small amount of time.  The thread will be yielded
// at least once ensuring that context switches could happen.  It is intended
// to be used for stress testing parallel code and shouldn't be assumed to do any
// reliable timing.
ARROW_TESTING_EXPORT
void SleepABit();

// Wait until predicate is true or timeout in seconds expires.
ARROW_TESTING_EXPORT
void BusyWait(double seconds, std::function<bool()> predicate);

ARROW_TESTING_EXPORT
Future<> SleepAsync(double seconds);

// \see SleepABit
ARROW_TESTING_EXPORT
Future<> SleepABitAsync();

template <typename T>
std::vector<T> IteratorToVector(Iterator<T> iterator) {
  EXPECT_OK_AND_ASSIGN(auto out, iterator.ToVector());
  return out;
}

ARROW_TESTING_EXPORT
bool LocaleExists(const char* locale);

// A RAII-style object that switches to a new locale, and switches back
// to the old locale when going out of scope.  Doesn't do anything if the
// new locale doesn't exist on the local machine.
// ATTENTION: may crash with an assertion failure on Windows debug builds.
// See ARROW-6108, also https://gerrit.libreoffice.org/#/c/54110/
class ARROW_TESTING_EXPORT LocaleGuard {
 public:
  explicit LocaleGuard(const char* new_locale);
  ~LocaleGuard();

 protected:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class ARROW_TESTING_EXPORT EnvVarGuard {
 public:
  EnvVarGuard(const std::string& name, const std::string& value);
  ~EnvVarGuard();

 protected:
  const std::string name_;
  std::string old_value_;
  bool was_set_;
};

namespace internal {
class SignalHandler;
}

class ARROW_TESTING_EXPORT SignalHandlerGuard {
 public:
  typedef void (*Callback)(int);

  SignalHandlerGuard(int signum, Callback cb);
  SignalHandlerGuard(int signum, const internal::SignalHandler& handler);
  ~SignalHandlerGuard();

 protected:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#ifndef ARROW_LARGE_MEMORY_TESTS
#define LARGE_MEMORY_TEST(name) DISABLED_##name
#else
#define LARGE_MEMORY_TEST(name) name
#endif

inline void PrintTo(const Status& st, std::ostream* os) { *os << st.ToString(); }

template <typename T>
void PrintTo(const Result<T>& result, std::ostream* os) {
  if (result.ok()) {
    ::testing::internal::UniversalPrint(result.ValueOrDie(), os);
  } else {
    *os << result.status();
  }
}

// A data type with only move constructors (no copy, no default).
struct MoveOnlyDataType {
  explicit MoveOnlyDataType(int x) : data(new int(x)) {}

  MoveOnlyDataType(const MoveOnlyDataType& other) = delete;
  MoveOnlyDataType& operator=(const MoveOnlyDataType& other) = delete;

  MoveOnlyDataType(MoveOnlyDataType&& other) { MoveFrom(&other); }
  MoveOnlyDataType& operator=(MoveOnlyDataType&& other) {
    MoveFrom(&other);
    return *this;
  }

  MoveOnlyDataType& operator=(int x) {
    if (data != nullptr) {
      delete data;
    }
    data = new int(x);
    return *this;
  }

  ~MoveOnlyDataType() { Destroy(); }

  void Destroy() {
    if (data != nullptr) {
      delete data;
      data = nullptr;
      moves = -1;
    }
  }

  void MoveFrom(MoveOnlyDataType* other) {
    Destroy();
    data = other->data;
    other->data = nullptr;
    moves = other->moves + 1;
  }

  int ToInt() const { return data == nullptr ? -42 : *data; }

  bool operator==(const MoveOnlyDataType& other) const {
    return data != nullptr && other.data != nullptr && *data == *other.data;
  }
  bool operator<(const MoveOnlyDataType& other) const {
    return data == nullptr || (other.data != nullptr && *data < *other.data);
  }

  bool operator==(int other) const { return data != nullptr && *data == other; }
  friend bool operator==(int left, const MoveOnlyDataType& right) {
    return right == left;
  }

  int* data = nullptr;
  int moves = 0;
};

// A task that blocks until unlocked.  Useful for timing tests.
class ARROW_TESTING_EXPORT GatingTask {
 public:
  explicit GatingTask(double timeout_seconds = 10);
  /// \brief During destruction we wait for all pending tasks to finish
  ~GatingTask();

  /// \brief Creates a new waiting task (presumably to spawn on a thread).  It will return
  /// invalid if the timeout arrived before the unlock.  The task will not complete until
  /// unlocked or timed out
  ///
  /// Note: The GatingTask must outlive any Task instances
  std::function<void()> Task();
  /// \brief Waits until at least count tasks are running.
  Status WaitForRunning(int count);
  /// \brief Unlocks all waiting tasks.  Returns an invalid status if any waiting task has
  /// timed out
  Status Unlock();

  static std::shared_ptr<GatingTask> Make(double timeout_seconds = 10);

 private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace arrow

namespace nonstd {
namespace sv_lite {

// Without this hint, GTest will print string_views as a container of char
template <class Char, class Traits = std::char_traits<Char>>
void PrintTo(const basic_string_view<Char, Traits>& view, std::ostream* os) {
  *os << view;
}

}  // namespace sv_lite

namespace optional_lite {

template <typename T>
void PrintTo(const optional<T>& opt, std::ostream* os) {
  if (opt.has_value()) {
    *os << "{";
    ::testing::internal::UniversalPrint(*opt, os);
    *os << "}";
  } else {
    *os << "nullopt";
  }
}

inline void PrintTo(const decltype(nullopt)&, std::ostream* os) { *os << "nullopt"; }

}  // namespace optional_lite
}  // namespace nonstd
