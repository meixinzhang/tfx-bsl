// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TFX_BSL_CC_SKETCHES_MISRAGRIES_SKETCH_H_
#define TFX_BSL_CC_SKETCHES_MISRAGRIES_SKETCH_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "tfx_bsl/cc/sketches/sketches.pb.h"
#include "tfx_bsl/cc/util/status.h"
#include "arrow/api.h"

namespace tfx_bsl {
namespace sketches {

// The Misra-Gries (MG) sketch estimates the top k item counts (possibly
// weighted) in a data stream, where k is the number of buckets stored in the
// sketch.
//
// This implementation uses `delta_` to track the the number of the times that
// the values in `item_counts_` are decremented in the algorithm. It stores the
// lower bound estimate in `item_counts_` and returns the upper bound estimate,
// which is equal to the lower bound estimate + delta_, when queried. This
// algorithm guarantees for any item x with true count X:
//
//               item_counts_[x] <= X <= item_counts_[x] + delta_
//                         delta_ <= (m - m') / (k + 1)
//
// where m is the total weight of the items in the dataset and m' is the sum of
// the lower bound estimates in `item_counts_` [1].
//
// The sketch is mergeable, meaning it guarantees this error bound when merged
// with other MG sketches [2]. For datasets that are Zipfian distributed with
// parameter a, the algorithm provides an expected value of
// delta_ = m / (k ^ a) [3].
//
// This implementation is also designed to support both unweighted and weighted
// data streams [3]. In comparison to the Count-Min Sketch algorithm, the MG
// algorithm requires less space to achieve the same accuracy, but is slower
// [4]. The sketch can be stored on disk for usage later.
//
//    [1] http://www.cohenwang.com/edith/bigdataclass2013/lectures/lecture1.pdf
//    [2] https://www.cs.utah.edu/~jeffp/papers/merge-summ.pdf
//    [3] http://dimacs.rutgers.edu/~graham/pubs/papers/countersj.pdf
//    [4] http://www.cs.toronto.edu/~bor/2420f17/L11.pdf

class MisraGriesSketch {
 public:
  MisraGriesSketch(int num_buckets);
  // This class is copyable and movable.
  ~MisraGriesSketch() = default;
  // Adds an array of items.
  Status AddValues(const arrow::Array& items);
  // Adds an array of items with their associated weights. Raises an error if
  // the weights are not a FloatArray.
  Status AddValues(const arrow::Array& items, const arrow::Array& weights);
  // Merges another MisraGriesSketch into this sketch. Raises an error if the
  // sketches do not have the same number of buckets.
  Status Merge(MisraGriesSketch& other);
  // Returns the list of top-k items sorted in descending order of their counts.
  std::vector<std::pair<std::string, double>> GetCounts() const;
  // Creates a struct array <values, counts> of the top-k items.
  Status Estimate(std::shared_ptr<arrow::Array>* values_and_counts_array) const;
  // Serializes the sketch into a string.
  std::string Serialize() const;
  // Deserializes the string to a MisraGries object.
  static MisraGriesSketch Deserialize(absl::string_view encoded);
  // Gets delta_.
  double GetDelta() const;
  // Gets theoretical upper bound on delta (for testing purposes).
  double GetDeltaUpperBound(double global_weight) const;

 private:
  // Reduces size of the item_counts_ to num_buckets_ items.
  void Compress();
  // The number of items stored in item_counts_.
  int num_buckets_;
  // Tracks the maximum error due to subtractions.
  double delta_;
  // Dictionary containing lower bound estimates of the item counts.
  absl::flat_hash_map<std::string, double> item_counts_;
};

}  // namespace sketches
}  // namespace tfx_bsl

#endif  // TFX_BSL_CC_SKETCHES_MISRAGRIES_SKETCH_H_
