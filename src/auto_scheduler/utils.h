/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file auto_scheduler/utils.h
 * \brief Common utilities.
 */

#ifndef TVM_AUTO_SCHEDULER_UTILS_H_
#define TVM_AUTO_SCHEDULER_UTILS_H_

#include <dmlc/common.h>
#include <tvm/tir/expr.h>

#include <algorithm>
#include <deque>
#include <exception>
#include <future>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace std {

/*! \brief Hash function for std::pair */
template <typename T1, typename T2>
struct hash<std::pair<T1, T2>> {
  std::size_t operator()(const std::pair<T1, T2>& k) const {
    return ::dmlc::HashCombine(std::hash<T1>()(k.first), std::hash<T2>()(k.second));
  }
};

/*! \brief Hash function for std::tuple */
template <typename T1, typename T2, typename T3>
struct hash<std::tuple<T1, T2, T3>> {
  std::size_t operator()(const std::tuple<T1, T2, T3>& k) const {
    return ::dmlc::HashCombine(
        ::dmlc::HashCombine(std::hash<T1>()(std::get<0>(k)), std::hash<T2>()(std::get<1>(k))),
        std::hash<T3>()(std::get<2>(k)));
  }
};

}  // namespace std

namespace tvm {
namespace auto_scheduler {

/********** Utilities for Array, std::vector, std::string **********/
/*! \brief Get the first appearance index of elements in an Array */
template <typename T>
inline void GetIndices(const Array<T>& array, const Array<T>& to_locate, Array<Integer>* indices) {
  for (const auto& v : to_locate) {
    auto it = std::find(array.begin(), array.end(), v);
    if (it != array.end()) {
      indices->push_back(it - array.begin());
    } else {
      LOG(FATAL) << "Cannot find the item";
    }
  }
}

/*! \brief Get the first appearance index of an element in an Array */
template <typename T>
inline int GetIndex(const Array<T>& array, const T& to_locate) {
  for (size_t i = 0; i < array.size(); ++i) {
    if (array[i] == to_locate) {
      return i;
    }
  }
  LOG(FATAL) << "Cannot find the item";
  return -1;
}

/*! \brief Delete the item in a std::vector if it exists. */
template <typename T>
inline void FindAndDeleteItem(std::vector<T>* array, const T& to_delete) {
  auto iter = std::find(array->begin(), array->end(), to_delete);
  if (iter != array->end()) {
    array->erase(iter);
  }
}

/*! \brief Replace a sub-string to another sub-string in a string */
inline void StrReplace(std::string* base, const std::string& from, const std::string& to) {
  auto pos = base->find(from);
  while (pos != std::string::npos) {
    base->replace(pos, from.size(), to);
    pos = base->find(from, pos + to.size());
  }
}

/*! \brief Convert a Array<Integer> to std::vector<int>. */
inline std::vector<int> IntArrayToVector(const ::tvm::Array<::tvm::Integer>& data) {
  std::vector<int> out;
  for (const auto& x : data) {
    CHECK(x.defined());
    out.push_back(x);
  }
  return out;
}

/*! \brief Convert a Array<Optional<Integer>> to std::vector<int>. */
inline std::vector<int> IntArrayToVector(
    const ::tvm::Array<::tvm::Optional<::tvm::Integer>>& data) {
  std::vector<int> out;
  for (const auto& x : data) {
    CHECK(x);
    out.push_back(x.value());
  }
  return out;
}

/*! \brief Return whether two int arrays are elementwise-equal */
inline bool IntArrayEqual(const Array<PrimExpr>& arr1, const Array<PrimExpr>& arr2) {
  if (arr1.size() != arr2.size()) {
    return false;
  }

  for (size_t i = 0; i < arr1.size(); ++i) {
    auto int1 = arr1[i].as<IntImmNode>();
    auto int2 = arr2[i].as<IntImmNode>();
    CHECK(int1 != nullptr);
    CHECK(int2 != nullptr);
    if (int1->value != int2->value) {
      return false;
    }
  }
  return true;
}

/********** Utilities for TVM Containers / ByteArray **********/
/*! \brief Compute mean of a FloatImm array */
inline double FloatArrayMean(const Array<PrimExpr>& float_array) {
  double sum = 0;
  if (float_array.empty()) {
    return 0.0;
  }

  for (const auto& x : float_array) {
    auto floatimm = x.as<tir::FloatImmNode>();
    CHECK(floatimm != nullptr);
    sum += floatimm->value;
  }
  return sum / float_array.size();
}

/********** Other Utilities **********/
/*! \brief Get an int value from an Expr */
inline int64_t GetIntImm(const PrimExpr& expr) {
  auto pint = expr.as<IntImmNode>();
  CHECK(pint != nullptr);
  return pint->value;
}

/*! \brief Compute the product of the lengths of axes */
inline int64_t AxisLengthProd(const Array<tir::IterVar>& axes) {
  int64_t ret = 1.0;
  for (const auto& x : axes) {
    if (const IntImmNode* imm = x->dom->extent.as<IntImmNode>()) {
      ret *= imm->value;
    } else {
      return -1.0;
    }
  }
  return ret;
}

/*!
 * \brief Clean the name of an iterator to make it valid in python code.
 * \param str The original name.
 * \return The cleaned name.
 */
inline std::string CleanName(const std::string& str) {
  std::string ret = str;
  StrReplace(&ret, ".", "_");
  StrReplace(&ret, "@", "_");
  StrReplace(&ret, "outer", "o");
  StrReplace(&ret, "inner", "i");
  return ret;
}

/*! \brief An empty output stream */
class NullStream : public std::ostream {
 public:
  NullStream() : std::ostream(nullptr) {}
  NullStream(const NullStream&) : std::ostream(nullptr) {}
  static NullStream& Global();
};

template <class T>
NullStream& operator<<(NullStream& os, const T& value) {
  return os;
}

/*! \brief Get std cout with verbose control */
inline std::ostream& StdCout(int verbose, int setting = 1) {
  return verbose >= setting ? std::cout : NullStream::Global();
}

/*! \brief Print multiple chars */
inline std::string Chars(const char& str, int times) {
  std::stringstream ret;
  for (int i = 0; i < times; ++i) {
    ret << str;
  }
  return ret.str();
}

/*! \brief Print a title */
inline void PrintTitle(const std::string& title, int verbose) {
  StdCout(verbose) << Chars('-', 60) << "\n"
                   << Chars('-', 25) << "  [ " << title << " ]\n"
                   << Chars('-', 60) << std::endl;
}

}  // namespace auto_scheduler
}  // namespace tvm

#endif  // TVM_AUTO_SCHEDULER_UTILS_H_
