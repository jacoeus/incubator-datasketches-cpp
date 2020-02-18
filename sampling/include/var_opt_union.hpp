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

#ifndef _VAR_OPT_UNION_HPP_
#define _VAR_OPT_UNION_HPP_

#include "var_opt_sketch.hpp"
#include "serde.hpp"

#include <vector>

namespace datasketches {

template<typename A> using AllocU8 = typename std::allocator_traits<A>::template rebind_alloc<uint8_t>;

/**
 * author Kevin Lang 
 * author Jon Malkin
 */

template <typename T, typename S = serde<T>, typename A = std::allocator<T>>
class var_opt_union {

public:
  static const uint32_t MAX_K = ((uint32_t) 1 << 31) - 2;

  explicit var_opt_union(uint32_t max_k);
  var_opt_union(const var_opt_union& other);
  var_opt_union(var_opt_union&& other) noexcept;
  static var_opt_union deserialize(std::istream& is);
  static var_opt_union deserialize(const void* bytes, size_t size);
    
  ~var_opt_union();

  var_opt_union& operator=(const var_opt_union& other);
  var_opt_union& operator=(var_opt_union&& other);

  void update(const var_opt_sketch<T,S,A>& sk);
  //void update(var_opt_sketch<T,S,A>&& sk);

  void reset();

  /**
   * Gets the varopt sketch resulting from the union of any input sketches.
   *
   * @return A varopt sketch
   */
  var_opt_sketch<T,S,A> get_result() const;
  
  size_t get_serialized_size_bytes() const;
  void serialize(std::ostream& os) const;
  std::vector<uint8_t, AllocU8<A>> serialize(unsigned header_size_bytes = 0) const;

  std::ostream& to_stream(std::ostream& os) const;
  std::string to_string() const;


private:
  typedef typename std::allocator_traits<A>::template rebind_alloc<var_opt_sketch<T,S,A>> AllocSketch;

  static const uint8_t PREAMBLE_LONGS_EMPTY = 1;
  static const uint8_t PREAMBLE_LONGS_NON_EMPTY = 4;
  static const uint8_t SER_VER = 2;
  static const uint8_t FAMILY_ID = 14;
  static const uint8_t EMPTY_FLAG_MASK  = 4;

  uint64_t n_; // cumulative over all input sketches

  // outer tau is the largest tau of any input sketch
  double outer_tau_numer_; // total weight of all input R-zones where tau = outer_tau

  // total cardinality of the same R-zones, or zero if no input sketch was in estimation mode
  uint64_t outer_tau_denom_;

  uint32_t max_k_;

  var_opt_sketch<T,S,A> gadget_;

  var_opt_union(uint64_t n, double outer_tau_numer, uint64_t outer_tau_denom,
                uint32_t max_k, var_opt_sketch<T,S,A>&& gadget);

  /*
   IMPORTANT NOTE: the "gadget" in the union object appears to be a varopt sketch,
   but in fact is NOT because it doesn't satisfy the mathematical definition
   of a varopt sketch of the concatenated input streams. Therefore it could be different
   from a true varopt sketch with that value of K, in which case it could easily provide
   worse estimation accuracy for subset-sum queries.

   This should not surprise you; the approximation guarantees of varopt sketches
   do not apply to things that merely resemble varopt sketches.

   However, even though the gadget is not a varopt sketch, the result
   of the unioning process IS a varopt sketch. It is constructed by a
   somewhat complicated "resolution" process which determines the largest K
   that a valid varopt sketch could have given the available information,
   then constructs a varopt sketch of that size and returns it.

   However, the gadget itself is not touched during the resolution process,
   and additional sketches could subsequently be merged into the union,
   at which point a varopt result could again be requested.
   */

  /*
   Explanation of "marked items" in the union's gadget:

   The boolean value "true" in an pair indicates that the item
   came from an input sketch's R zone, so it is already the result of sampling.

   Therefore it must not wind up in the H zone of the final result, because
   that would imply that the item is "exact".

   However, it is okay for a marked item to hang out in the gadget's H zone for a while.

   And once the item has moved to the gadget's R zone, the mark is never checked again,
   so no effort is made to ensure that its value is preserved or even makes sense.
   */

  /*
   Note: if the computer could perform exact real-valued arithmetic, the union could finalize
   its result by reducing k until inner_tau > outer_tau. [Due to the vagaries of floating point
   arithmetic, we won't attempt to detect and specially handle the inner_tau = outer_tau special
   case.]

   In fact, we won't even look at tau while while reducing k. Instead the logic will be based
   on the more robust integer quantity num_marks_in_h_ in the gadget. It is conceivable that due
   to round-off error we could end up with inner_tau slightly less than outer_tau, but that should
   be fairly harmless since we will have achieved our goal of getting the marked items out of H.

   Also, you might be wondering why we are bothering to maintain the numerator and denominator
   separately instead of just having a single variable outer_tau. This allows us (in certain
   cases) to add an input's entire R-zone weight into the result sketch, as opposed to subdividing
   it then adding it back up. That would be a source of numerical inaccuracy. And even
   more importantly, this design choice allows us to exactly re-construct the input sketch
   when there is only one of them.
   */
  void merge_into(const var_opt_sketch<T,S,A>& sk);

  double get_outer_tau() const;

  var_opt_sketch<T,S,A> simple_gadget_coercer() const;

  bool there_exist_unmarked_h_items_lighter_than_target(double threshold) const;
  bool detect_and_handle_subcase_of_pseudo_exact(var_opt_sketch<T,S,A>& sk) const;
  void mark_moving_gadget_coercer(var_opt_sketch<T,S,A>& sk) const;
  void migrate_marked_items_by_decreasing_k(var_opt_sketch<T,S,A>& sk) const;

  static void check_preamble_longs(uint8_t preamble_longs, uint8_t flags);
  static void check_family_and_serialization_version(uint8_t family_id, uint8_t ser_ver);
};

}

#include "var_opt_union_impl.hpp"

#endif // _VAR_OPT_UNION_HPP_