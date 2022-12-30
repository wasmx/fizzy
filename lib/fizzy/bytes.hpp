// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <string>

#include <stdexcept>

namespace fizzy
{

template <class CharT, class Traits = std::char_traits<CharT>>
class basic_string_view {

//      static_assert(!is_array_v<CharT>);
//      static_assert(is_trivial_v<_CharT> && is_standard_layout_v<_CharT>);
//      static_assert(is_same_v<_CharT, typename _Traits::char_type>);

public:
  // Types.
  using traits_type = Traits;
  using value_type = CharT;
  using const_pointer = const value_type*;
  using const_reference = const value_type&;
  using const_iterator = const value_type*;
  using size_type = size_t;
  static constexpr size_type npos = size_type(-1);

  // Construction.
  constexpr basic_string_view() noexcept : data_(nullptr), len_(0) {}
//  constexpr basic_string_view(const basic_string_view& other) noexcept =
//      default;
  constexpr basic_string_view(const_pointer str)
      : data_(str), len_(internal_strlen(str)) {}
  constexpr basic_string_view(const_pointer str, size_type len)
      : data_(str), len_(len) {}
//  basic_string_view(const std::string& str)
//      : data_(str.data()), len_(str.size()) {}
  constexpr basic_string_view(std::basic_string<CharT> /*str*/) : data_(nullptr), len_(0) {}

//  basic_string_view& operator=(const basic_string_view&) noexcept = default;

//  constexpr basic_string_view() noexcept : data_(nullptr), len_(0) {}
//  constexpr basic_string_view(const_pointer str, size_type len)
//      : data_(str), len_(len) {}
//  constexpr basic_string_view(const_pointer str)
//      : data_(str), len_(internal_strlen(str)) {}
//  basic_string_view(const std::string& str)
//      : data_(str.data()), len_(str.size()) {}
//  template<class It, class End>
//  constexpr basic_string_view(It first, End last) {}
//  template< class R >
//  explicit constexpr basic_string_view( R&& r ) {}

  // Iterator support.
  constexpr const_iterator begin() const noexcept { return data_; }
  constexpr const_iterator end() const noexcept { return data_ + len_; }

  // Element access.
  constexpr const_pointer data() const noexcept { return data_; }

  // Capacity.
  constexpr size_type size() const noexcept { return len_; }

  constexpr const_reference operator[](size_type pos) const {
    return data_[pos];
  }

  // Modifiers.
  void remove_prefix(size_type n) {
    data_ = data_ + n;
    len_ -= n;
  }

  // Operations.

  // Returns a view of the substring [pos, pos + rcount), where rcount is the
  // smaller of count and size() - pos.
  constexpr basic_string_view substr(size_type pos = 0, size_type count = npos) const {
    if (pos > len_) {
      throw std::out_of_range("Out of range");
    }

    const size_type rcount = std::min(count, len_ - pos);
    if (rcount > 0) {
      return basic_string_view(data_ + pos, rcount);
    }
    return basic_string_view();
  }

  // Compares two character sequences.
  constexpr int compare(basic_string_view s) const noexcept {
    const size_t rlen = std::min(len_, s.len_);
    const int comparison = traits_type::compare(data_, s.data_, rlen);
    if (comparison != 0) return comparison;
    if (len_ == s.len_) return 0;
    return len_ < s.len_ ? -1 : 1;
  }
  // Compare substring(pos1, count1) with s.
  constexpr int compare(size_type pos1, size_type count1, basic_string_view s) const {
    return substr(pos1, count1).compare(s);
  }
  // Compare substring(pos1, count1) with s.substring(pos2, count2).
  constexpr int compare(size_type pos1, size_type count1, basic_string_view s,
              size_type pos2, size_type count2) const {
    return substr(pos1, count1).compare(s.substr(pos2, count2));
  }
  constexpr int compare(const_pointer s) const { return compare(basic_string_view(s)); }
  constexpr int compare(size_type pos1, size_type count1, const_pointer s) const {
    return substr(pos1, count1).compare(basic_string_view(s));
  }
  constexpr int compare(size_type pos1, size_type count1, const_pointer s,
              size_type count2) const {
    return substr(pos1, count1).compare(basic_string_view(s, count2));
  }

// operator!=(const basic_string<_CharT, _Traits, _Allocator>& __lhs,

//  constexpr bool operator!=(basic_string_view<CharT> x, const std::basic_string<CharT> y) const {
//    return true;
//  }

private:
  constexpr static size_type internal_strlen(const_pointer str) {
    return str ? traits_type::length(str) : 0;
  }

  const_pointer data_;
  size_type len_;
};

using bytes_view = basic_string_view<uint8_t>;

using bytes = std::basic_string<uint8_t>;
//using bytes_view = std::basic_string_view<uint8_t>;

}  // namespace fizzy
