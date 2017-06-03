// Copyright 2012-2017 Glyn Matthews.
// Copyright 2012 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <locale>
#include <algorithm>
#include <functional>
#include "network/uri/whatwg/url.hpp"
#include "detail/uri_parse.hpp"
#include "detail/uri_advance_parts.hpp"
#include "detail/uri_percent_encode.hpp"
#include "detail/uri_normalize.hpp"
#include "detail/uri_resolve.hpp"
#include "detail/algorithm.hpp"

namespace network {
namespace whatwg {
namespace {
// With the parser, we use string_views, which are mutable. However,
// there are times (e.g. during normalization), when we want a part
// to be mutable. This function returns a pair of
// std::string::iterators in the same range as the URL part.
//
inline std::pair<std::string::iterator, std::string::iterator> mutable_part(
    std::string &str, ::network::detail::uri_part part) {
  auto view = string_view(str);

  auto first_index = std::distance(std::begin(view), std::begin(part));
  auto first = std::begin(str);
  std::advance(first, first_index);

  auto last_index = std::distance(std::begin(view), std::end(part));
  auto last = std::begin(str);
  std::advance(last, last_index);

  return std::make_pair(first, last);
}

// This is a convenience function that converts a part of a
// std::string to a string_view.
inline string_view to_string_view(const std::string &url,
                                  ::network::detail::uri_part part) {
  if (!part.empty()) {
    const char *c_str = url.c_str();
    const char *part_begin = &(*(std::begin(part)));
    std::advance(c_str, std::distance(c_str, part_begin));
    return string_view(c_str, std::distance(std::begin(part), std::end(part)));
  }
  return string_view();
}

inline optional<std::string> make_arg(optional<string_view> view) {
  if (view) {
    return view->to_string();
  }
  return nullopt;
}

template <class T>
inline void ignore(T) {}
}  // namespace

url::query_iterator::query_iterator() : query_{}, kvp_{} {}

url::query_iterator::query_iterator(optional<::network::detail::uri_part> query)
  : query_(query)
  , kvp_{} {
  if (query_ && query_->empty()) {
    query_ = nullopt;
  }
  else {
    assign_kvp();
  }
}

url::query_iterator::query_iterator(const query_iterator &other)
  : query_(other.query_)
  , kvp_(other.kvp_) {

}

url::query_iterator &url::query_iterator::operator = (const query_iterator &other) {
  auto tmp(other);
  swap(tmp);
  return *this;
}

url::query_iterator::reference url::query_iterator::operator ++ () noexcept {
  increment();
  return kvp_;
}

url::query_iterator::value_type url::query_iterator::operator ++ (int) noexcept {
  auto original = kvp_;
  increment();
  return original;
}

url::query_iterator::reference url::query_iterator::operator * () const noexcept {
  return kvp_;
}

url::query_iterator::pointer url::query_iterator::operator -> () const noexcept {
  return std::addressof(kvp_);
}

bool url::query_iterator::operator==(const query_iterator &other) const noexcept {
  if (!query_ && !other.query_) {
    return true;
  }
  else if (query_ && other.query_) {
    // since we're comparing substrings, the address of the first
    // element in each iterator must be the same
    return std::addressof(kvp_.first) == std::addressof(other.kvp_.first);
  }
  return false;
}

void url::query_iterator::swap(query_iterator &other) noexcept {
  std::swap(query_, other.query_);
  std::swap(kvp_, other.kvp_);
}

void url::query_iterator::advance_to_next_kvp() noexcept {
  auto first = std::begin(*query_), last = std::end(*query_);

  auto sep_it = std::find_if(
      first, last, [](char c) -> bool { return c == '&' || c == ';'; });

  if (sep_it != last) {
    ++sep_it; // skip next separator
  }

  // reassign query to the next element
  query_ = ::network::detail::uri_part(sep_it, last);
}

void url::query_iterator::assign_kvp() noexcept {
  auto first = std::begin(*query_), last = std::end(*query_);

  auto sep_it = std::find_if(
      first, last, [](char c) -> bool { return c == '&' || c == ';'; });
  auto eq_it =
      std::find_if(first, sep_it, [](char c) -> bool { return c == '='; });

  kvp_.first = string_view(std::addressof(*first), std::distance(first, eq_it));
  if (eq_it != sep_it) {
    ++eq_it; // skip '=' symbol
  }
  kvp_.second = string_view(std::addressof(*eq_it), std::distance(eq_it, sep_it));
}

void url::query_iterator::increment() noexcept {
  assert(query_);

  if (!query_->empty()) {
    advance_to_next_kvp();
    assign_kvp();
  }

  if (query_->empty()) {
    query_ = nullopt;
  }
}

url::url() : url_view_(url_), url_parts_() {}

url::url(const url &other) : url_(other.url_), url_view_(url_), url_parts_() {
  ::network::detail::advance_parts(url_view_, url_parts_, other.url_parts_);
}

url::url(url &&other) noexcept : url_(std::move(other.url_)),
                                 url_view_(url_),
                                 url_parts_(std::move(other.url_parts_)) {
  ::network::detail::advance_parts(url_view_, url_parts_, other.url_parts_);
  other.url_.clear();
  other.url_view_ = string_view(other.url_);
  other.url_parts_ = ::network::detail::uri_parts();
}

url::~url() {}

url &url::operator=(url other) {
  other.swap(*this);
  return *this;
}

void url::swap(url &other) noexcept {
  auto parts = url_parts_;
  advance_parts(other.url_view_, url_parts_, other.url_parts_);
  url_.swap(other.url_);
  url_view_.swap(other.url_view_);
  advance_parts(other.url_view_, other.url_parts_, parts);
}

url::const_iterator url::begin() const noexcept { return url_view_.begin(); }

url::const_iterator url::end() const noexcept { return url_view_.end(); }

bool url::has_scheme() const noexcept {
  return static_cast<bool>(url_parts_.scheme);
}

url::string_view url::scheme() const noexcept {
  return has_scheme() ? to_string_view(url_, *url_parts_.scheme)
                      : string_view{};
}

bool url::has_user_info() const noexcept {
  return static_cast<bool>(url_parts_.user_info);
}

url::string_view url::user_info() const noexcept {
  return has_user_info()
             ? to_string_view(url_, *url_parts_.user_info)
             : string_view{};
}

bool url::has_host() const noexcept {
  return static_cast<bool>(url_parts_.host);
}

url::string_view url::host() const noexcept {
  return has_host() ? to_string_view(url_, *url_parts_.host)
                    : string_view{};
}

bool url::has_port() const noexcept {
  return static_cast<bool>(url_parts_.port);
}

url::string_view url::port() const noexcept {
  return has_port() ? to_string_view(url_, *url_parts_.port)
                    : string_view{};
}

bool url::has_path() const noexcept {
  return static_cast<bool>(url_parts_.path);
}

url::string_view url::path() const noexcept {
  return has_path() ? to_string_view(url_, *url_parts_.path)
                    : string_view{};
}

bool url::has_query() const noexcept {
  return static_cast<bool>(url_parts_.query);
}

url::string_view url::query() const noexcept {
  return has_query() ? to_string_view(url_, *url_parts_.query) : string_view{};
}

url::query_iterator url::query_begin() const noexcept {
  return has_query()? url::query_iterator{url_parts_.query} : url::query_iterator{};
}

url::query_iterator url::query_end() const noexcept {
  return url::query_iterator{};
}

bool url::has_fragment() const noexcept {
  return static_cast<bool>(url_parts_.fragment);
}

url::string_view url::fragment() const noexcept {
  return has_fragment() ? to_string_view(url_, *url_parts_.fragment)
                        : string_view{};
}

bool url::has_authority() const noexcept {
  return has_host();
}

url::string_view url::authority() const noexcept {
  if (!has_host()) {
    return string_view{};
  }

  auto host = this->host();

  auto user_info = string_view{};
  if (has_user_info()) {
    user_info = this->user_info();
  }

  auto port = string_view{};
  if (has_port()) {
    port = this->port();
  }

  auto first = std::begin(host), last = std::end(host);
  if (has_user_info() && !user_info.empty()) {
    first = std::begin(user_info);
  }
  else if (host.empty() && has_port() && !port.empty()) {
    first = std::begin(port);
    --first; // include ':' before port
  }

  if (host.empty()) {
    if (has_port() && !port.empty()) {
      last = std::end(port);
    }
    else if (has_user_info() && !user_info.empty()) {
      last = std::end(user_info);
      ++last; // include '@'
    }
  }
  else if (has_port()) {
    if (port.empty()) {
      ++last; // include ':' after host
    }
    else {
      last = std::end(port);
    }
  }

  return string_view(first, std::distance(first, last));
}

std::string url::string() const { return url_; }

std::wstring url::wstring() const {
  return std::wstring(std::begin(*this), std::end(*this));
}

std::u16string url::u16string() const {
  return std::u16string(std::begin(*this), std::end(*this));
}

std::u32string url::u32string() const {
  return std::u32string(std::begin(*this), std::end(*this));
}

bool url::empty() const noexcept { return url_.empty(); }

bool url::is_absolute() const noexcept { return has_scheme(); }

bool url::is_opaque() const noexcept {
  return (is_absolute() && !has_authority());
}

url url::normalize() const {
  string_type normalized(url_);
  string_view normalized_view(normalized);
  ::network::detail::uri_parts parts;
  ::network::detail::advance_parts(normalized_view, parts, url_parts_);

  // All alphabetic characters in the scheme and host are
  // lower-case...
  if (parts.scheme) {
    std::string::iterator first, last;
    std::tie(first, last) = mutable_part(normalized, *parts.scheme);
    std::transform(first, last, first,
                   [](char ch) { return std::tolower(ch, std::locale()); });
  }

  if (parts.host) {
    std::string::iterator first, last;
    std::tie(first, last) = mutable_part(normalized, *parts.host);
    std::transform(first, last, first,
                   [](char ch) { return std::tolower(ch, std::locale()); });
  }

  // ...except when used in percent encoding
  ::network::detail::for_each(
      normalized, ::network::detail::percent_encoded_to_upper<std::string>());

  // parts are invalidated here
  // there's got to be a better way of doing this that doesn't
  // mean parsing again (twice!)
  normalized.erase(::network::detail::decode_encoded_unreserved_chars(
                       std::begin(normalized), std::end(normalized)),
                   std::end(normalized));
  normalized_view = string_view(normalized);

  // need to parse the parts again as the underlying string has changed
  const_iterator it = std::begin(normalized_view), last = std::end(normalized_view);
  bool is_valid = ::network::detail::parse(it, last, parts);
  ignore(is_valid);
  assert(is_valid);

  if (parts.path) {
    url::string_type path = ::network::detail::normalize_path_segments(
        to_string_view(normalized, *parts.path));

    // put the normalized path back into the url
    optional<string_type> query, fragment;
    if (parts.query) {
      query = parts.query->to_string();
    }

    if (parts.fragment) {
      fragment = parts.fragment->to_string();
    }

    auto path_begin = std::begin(normalized);
    auto path_range = mutable_part(normalized, *parts.path);
    std::advance(path_begin, std::distance(path_begin, path_range.first));
    normalized.erase(path_begin, std::end(normalized));
    normalized.append(path);

    if (query) {
      normalized.append("?");
      normalized.append(*query);
    }

    if (fragment) {
      normalized.append("#");
      normalized.append(*fragment);
    }
  }

  return url(normalized);
}

int url::compare(const url &other) const noexcept {
  // if both URLs are empty, then we should define them as equal
  // even though they're still invalid.
  if (empty() && other.empty()) {
    return 0;
  }

  if (empty()) {
    return -1;
  }

  if (other.empty()) {
    return 1;
  }

  return normalize().url_.compare(other.normalize().url_);
}

bool url::initialize(const string_type &url) {
  url_ = ::network::detail::trim_copy(url);
  if (!url_.empty()) {
    url_view_ = string_view(url_);
    const_iterator it = std::begin(url_view_), last = std::end(url_view_);
    bool is_valid = ::network::detail::parse(it, last, url_parts_);
    return is_valid;
  }
  return true;
}

void swap(url &lhs, url &rhs) noexcept { lhs.swap(rhs); }

bool operator==(const url &lhs, const url &rhs) noexcept {
  return lhs.compare(rhs) == 0;
}

bool operator==(const url &lhs, const char *rhs) noexcept {
  if (std::strlen(rhs) !=
      std::size_t(std::distance(std::begin(lhs), std::end(lhs)))) {
    return false;
  }
  return std::equal(std::begin(lhs), std::end(lhs), rhs);
}

bool operator<(const url &lhs, const url &rhs) noexcept {
  return lhs.compare(rhs) < 0;
}
}  // namespace whatwg
}  // namespace network