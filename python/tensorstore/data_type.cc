// Copyright 2020 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "python/tensorstore/numpy.h"
// numpy.h must be included first.
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "numpy/npy_common.h"

// Other headers must be included after pybind11 to ensure header-order
// inclusion constraints are satisfied.

#include "python/tensorstore/data_type.h"

// Other headers
#include <stddef.h>

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "python/tensorstore/json_type_caster.h"
#include "python/tensorstore/serialization.h"
#include "python/tensorstore/tensorstore_module_components.h"
#include "tensorstore/data_type.h"
#include "tensorstore/internal/global_initializer.h"
#include "tensorstore/util/executor.h"
#include "tensorstore/util/quote_string.h"
#include "tensorstore/util/str_cat.h"

namespace tensorstore {
namespace internal_python {

// Validate NPY_ enum alias reference a type assigned in
// kDataTypeIdForNumpyTypeNum.
static_assert(kDataTypeIdForNumpyTypeNum[NPY_INT8] != DataTypeId::custom);
static_assert(kDataTypeIdForNumpyTypeNum[NPY_INT16] != DataTypeId::custom);
static_assert(kDataTypeIdForNumpyTypeNum[NPY_INT32] != DataTypeId::custom);
static_assert(kDataTypeIdForNumpyTypeNum[NPY_INT64] != DataTypeId::custom);
static_assert(kDataTypeIdForNumpyTypeNum[NPY_FLOAT16] != DataTypeId::custom);
static_assert(kDataTypeIdForNumpyTypeNum[NPY_FLOAT32] != DataTypeId::custom);
static_assert(kDataTypeIdForNumpyTypeNum[NPY_FLOAT64] != DataTypeId::custom);

namespace py = ::pybind11;

namespace {

class CustomDTypes {
 public:
  CustomDTypes() = delete;

  static void Initialize() {
    // this function should be called from the global initialization
    py::module ml_dtypes = py::module::import("ml_dtypes");

    *datatype_to_numpy_map_ = {
        {DataTypeId::bfloat16_t,
         py::dtype::from_args(ml_dtypes.attr("bfloat16")).num()},
    };

#define TENSORSTORE_INTERNAL_DO_ADD_TO_MAP(T)                           \
  {                                                                     \
    std::string type_name(                                              \
        internal_data_type::GetTypeName<tensorstore::dtypes::T>());     \
    datatype_to_numpy_map_->emplace(                                    \
        DataTypeId::T,                                                  \
        py::dtype::from_args(ml_dtypes.attr(type_name.c_str())).num()); \
  }                                                                     \
  /**/
    TENSORSTORE_FOR_EACH_FLOAT8_DATA_TYPE(TENSORSTORE_INTERNAL_DO_ADD_TO_MAP)
    TENSORSTORE_FOR_EACH_LOW_PRECISION_INT_DATA_TYPE(
        TENSORSTORE_INTERNAL_DO_ADD_TO_MAP)
#undef TENSORSTORE_INTERNAL_DO_ADD_TO_MAP

    for (auto [k, v] : *datatype_to_numpy_map_) {
      numpy_to_datatype_map_->emplace(v, k);
    }
  }

  static int GetNumpyTypeNum(DataTypeId id) {
    auto it = datatype_to_numpy_map_->find(id);
    assert(it != datatype_to_numpy_map_->end());
    return it->second;
  }

  static DataTypeId GetDataTypeId(int num) {
    if (auto it = numpy_to_datatype_map_->find(num);
        it != numpy_to_datatype_map_->end()) {
      return it->second;
    } else {
      return DataTypeId::num_ids;
    }
  }

 private:
  static absl::NoDestructor<absl::flat_hash_map<DataTypeId, int>>
      datatype_to_numpy_map_;
  static absl::NoDestructor<absl::flat_hash_map<int, DataTypeId>>
      numpy_to_datatype_map_;
};

absl::NoDestructor<absl::flat_hash_map<DataTypeId, int>>
    CustomDTypes::datatype_to_numpy_map_;
absl::NoDestructor<absl::flat_hash_map<int, DataTypeId>>
    CustomDTypes::numpy_to_datatype_map_;

};  // namespace

pybind11::dtype GetNumpyDtype(int type_num) {
  if (auto* obj = PyArray_DescrFromType(type_num)) {
    return py::reinterpret_borrow<py::dtype>(reinterpret_cast<PyObject*>(obj));
  }
  throw py::error_already_set();
}

DataType GetDataTypeOrThrow(std::string_view name) {
  auto d = GetDataType(name);
  if (!d.valid()) {
    throw py::value_error(tensorstore::StrCat(
        "No TensorStore data type with name: ", QuoteString(name)));
  }
  return d;
}

int GetNumpyTypeNum(DataType dtype) {
  const DataTypeId id = dtype.id();
  switch (id) {
    case DataTypeId::custom:
      return -1;
#define TENSORSTORE_INTERNAL_DO_GET_NPY_TYPE_NUM_CASE(T) \
  case DataTypeId::T:                                    \
    return CustomDTypes::GetNumpyTypeNum(id);            \
    /**/
      TENSORSTORE_INTERNAL_DO_GET_NPY_TYPE_NUM_CASE(bfloat16_t)
      TENSORSTORE_FOR_EACH_FLOAT8_DATA_TYPE(
          TENSORSTORE_INTERNAL_DO_GET_NPY_TYPE_NUM_CASE)
      TENSORSTORE_FOR_EACH_LOW_PRECISION_INT_DATA_TYPE(
          TENSORSTORE_INTERNAL_DO_GET_NPY_TYPE_NUM_CASE)
#undef TENSORSTORE_INTERNAL_DO_GET_NPY_TYPE_NUM_CASE
    default:
      return kNumpyTypeNumForDataTypeId[static_cast<size_t>(id)];
  }
}

py::dtype GetNumpyDtypeOrThrow(DataType dtype) {
  int type_num = GetNumpyTypeNum(dtype);
  if (type_num != -1) {
    return GetNumpyDtype(type_num);
  }
  throw py::value_error(tensorstore::StrCat(
      "No NumPy dtype corresponding to TensorStore data type: ",
      QuoteString(dtype.name())));
}

DataType GetDataType(pybind11::dtype dt) {
  const int type_num = py::detail::array_descriptor_proxy(dt.ptr())->type_num;

  if (DataTypeId type_id = CustomDTypes::GetDataTypeId(type_num);
      type_id != DataTypeId::num_ids) {
    return kDataTypes[static_cast<size_t>(type_id)];
  }

  if (type_num < 0 || type_num > TENSORSTORE_NPY_NTYPES) {
    return DataType();
  }
  const DataTypeId id = kDataTypeIdForNumpyTypeNum[type_num];
  if (id == DataTypeId::custom) return DataType();
  return kDataTypes[static_cast<size_t>(id)];
}

DataType GetDataTypeOrThrow(py::dtype dt) {
  auto dtype = GetDataType(dt);
  if (dtype.valid()) return dtype;
  throw py::value_error(tensorstore::StrCat(
      "No TensorStore data type corresponding to NumPy dtype: ",
      py::cast<std::string>(py::repr(dt))));
}

py::object GetTypeObjectOrThrow(DataType dtype) {
  switch (dtype.id()) {
    case DataTypeId::ustring_t:
      return py::reinterpret_borrow<py::object>(
          reinterpret_cast<PyObject*>(&PyUnicode_Type));
    case DataTypeId::string_t:
      return py::reinterpret_borrow<py::object>(
          reinterpret_cast<PyObject*>(&PyBytes_Type));
    default:
      break;
  }
  auto numpy_dtype = GetNumpyDtypeOrThrow(dtype);
  return py::reinterpret_borrow<py::object>(
      py::detail::array_descriptor_proxy(numpy_dtype.ptr())->typeobj);
}

namespace {
using DataTypeCls = py::class_<DataType>;

auto MakeDataTypeClass(py::module m) {
  return DataTypeCls(m, "dtype", R"(
TensorStore data type representation.

Group:
  Data types
)");
}

void DefineDataTypeAttributes(DataTypeCls& cls) {
  cls.def(py::init([](std::string name) { return GetDataTypeOrThrow(name); }),
          R"(
Construct by name.

Overload:
  name
)",
          py::arg("name"));

  cls.def(py::init([](DataTypeLike dtype) { return dtype.value; }),
          R"(
Construct from an existing TensorStore or NumPy data type.

Overload:
  dtype
)",
          py::arg("dtype"));

  cls.def_property_readonly(
      "name", [](DataType self) { return std::string(self.name()); });

  cls.def("__repr__", [](DataType self) {
    return tensorstore::StrCat("dtype(", QuoteString(self.name()), ")");
  });

  EnablePicklingFromSerialization(cls);

  cls.def("to_json", [](DataType self) { return std::string(self.name()); });

  cls.def_property_readonly(
      "numpy_dtype", [](DataType self) { return GetNumpyDtypeOrThrow(self); });

  cls.def("__hash__", [](DataType self) {
    absl::Hash<DataType> h;
    return h(self);
  });

  cls.def_property_readonly("type", [](DataType self) -> py::object {
    return GetTypeObjectOrThrow(self);
  });

  cls.def(
      "__call__",
      [](DataType self, py::object arg) -> py::object {
        if (self.id() == DataTypeId::json_t) {
          return py::cast(PyObjectToJson(arg));
        }
        return GetTypeObjectOrThrow(self)(std::move(arg));
      },
      "Construct a scalar instance of this data type");

  cls.def(
      "__eq__",
      [](DataType self, DataTypeLike other) { return self == other.value; },
      py::arg("other"));
}

void RegisterDataTypeBindings(pybind11::module m, Executor defer) {
  CustomDTypes::Initialize();

  defer([cls = MakeDataTypeClass(m)]() mutable {
    DefineDataTypeAttributes(cls);
  });

  // Like NumPy and Tensorflow, define `tensorstore.<dtype>` constants for each
  // supported data type.
  for (const DataType dtype : kDataTypes) {
    m.attr(std::string(dtype.name()).c_str()) = dtype;
  }
}

TENSORSTORE_GLOBAL_INITIALIZER {
  RegisterPythonComponent(RegisterDataTypeBindings, /*priority=*/-800);
}
}  // namespace
}  // namespace internal_python
}  // namespace tensorstore

namespace pybind11 {
namespace detail {

bool type_caster<tensorstore::internal_python::DataTypeLike>::load(
    handle src, bool convert) {
  using tensorstore::DataType;
  using tensorstore::dtype_v;
  // Handle the case that `src` is already a Python-wrapped
  // `tensorstore::DataType`.
  if (pybind11::isinstance<tensorstore::DataType>(src)) {
    value.value = pybind11::cast<tensorstore::DataType>(src);
    return true;
  }
  if (src.is_none()) return false;
  if (!convert) return false;
  if (src.ptr() == reinterpret_cast<PyObject*>(&PyUnicode_Type)) {
    value.value = dtype_v<tensorstore::dtypes::ustring_t>;
    return true;
  }
  if (src.ptr() == reinterpret_cast<PyObject*>(&PyBytes_Type)) {
    value.value = dtype_v<tensorstore::dtypes::string_t>;
    return true;
  }
  PyArray_Descr* ptr = nullptr;
  if (!PyArray_DescrConverter(
          pybind11::reinterpret_borrow<pybind11::object>(src).release().ptr(),
          &ptr) ||
      !ptr) {
    PyErr_Clear();
    return false;
  }
  value.value = tensorstore::internal_python::GetDataTypeOrThrow(
      pybind11::reinterpret_steal<pybind11::dtype>(
          reinterpret_cast<PyObject*>(ptr)));
  return true;
}

handle type_caster<tensorstore::internal_python::DataTypeLike>::cast(
    tensorstore::internal_python::DataTypeLike value,
    return_value_policy policy, handle parent) {
  return pybind11::cast(std::move(value.value));
}

}  // namespace detail
}  // namespace pybind11
