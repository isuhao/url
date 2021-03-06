// Copyright 2006-2016 Nemanja Trifunovic

/*
Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef UTF8_FOR_CPP_CHECKED_H_2675DCD0_9480_4c0c_B92A_CC14C027B731
#define UTF8_FOR_CPP_CHECKED_H_2675DCD0_9480_4c0c_B92A_CC14C027B731

#include <stdexcept>
#include <algorithm>
#include <skyr/unicode.hpp>

namespace skyr {
namespace utf8 {
// Helper code - not intended to be directly called by the library users. May be
// changed at any time
namespace details {
namespace constants {
// Unicode constants
// Leading (high) surrogates: 0xd800 - 0xdbff
// Trailing (low) surrogates: 0xdc00 - 0xdfff
const char16_t lead_surrogate_min = 0xd800u;
const char16_t lead_surrogate_max = 0xdbffu;
const char16_t trail_surrogate_min = 0xdc00u;
const char16_t trail_surrogate_max = 0xdfffu;
const char16_t lead_offset = lead_surrogate_min - (0x10000 >> 10);
const char32_t surrogate_offset =
    0x10000u - (lead_surrogate_min << 10) - trail_surrogate_min;

// Maximum valid value for a Unicode code point
const char32_t code_point_max = 0x0010ffffu;
}  // namespace constants

template <typename OctetType>
inline uint8_t mask8(OctetType octet) {
  return static_cast<uint8_t>(0xff & octet);
}

template <typename U16Type>
inline char16_t mask16(U16Type octet) {
  return static_cast<char16_t>(0xffff & octet);
}
template <typename OctetType>
inline bool is_trail(OctetType octet) {
  return ((details::mask8(octet) >> 6) == 0x2);
}

inline bool is_lead_surrogate(char16_t code_point) {
  return
    (code_point >= constants::lead_surrogate_min) &&
    (code_point <= constants::lead_surrogate_max);
}

inline bool is_trail_surrogate(char16_t code_point) {
  return
  (code_point >= constants::trail_surrogate_min) &&
  (code_point <= constants::trail_surrogate_max);
}

inline bool is_surrogate(char16_t code_point) {
  return
    (code_point >= constants::lead_surrogate_min) &&
    (code_point <= constants::trail_surrogate_max);
}

inline bool is_code_point_valid(char32_t code_point) {
  return
    (code_point <= constants::code_point_max) &&
    !is_surrogate(static_cast<char16_t>(code_point));
}

template <typename OctetIterator>
inline std::ptrdiff_t sequence_length(
    OctetIterator lead_it) {
  auto lead = details::mask8(*lead_it);
  if (lead < 0x80) {
    return 1;
  } else if ((lead >> 5) == 0x6) {
    return 2;
  } else if ((lead >> 4) == 0xe) {
    return 3;
  } else if ((lead >> 3) == 0x1e) {
    return 4;
  }
  return 0;
}

inline bool is_overlong_sequence(
    char32_t code_point,
    std::ptrdiff_t length) {
  bool result = false;
  result &= (code_point < 0x80) && (length != 1);
  result &= (code_point < 0x800) && (length != 2);
  result &= (code_point < 0x10000) && (length != 3);
  return result;
}

/// Helper for get_sequence_x
template <typename OctetIterator>
expected<OctetIterator, unicode_errc> increment(
    OctetIterator &it,
    OctetIterator end) {
  if (++it == end) {
    return make_unexpected(unicode_errc::overflow);
  }

  if (!is_trail(*it)) {
    return make_unexpected(unicode_errc::illegal_byte_sequence);
  }

  return it;
}

/// get_sequence_x functions decode utf-8 sequences of the length x
template <typename OctetIterator>
expected<char32_t, unicode_errc> get_sequence_1(
    OctetIterator& it,
    OctetIterator end) {
  if (it == end) {
    return make_unexpected(unicode_errc::overflow);
  }
  return mask8(*it);
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> get_sequence_2(
    OctetIterator& it,
    OctetIterator last) {
  if (it == last) {
    return make_unexpected(unicode_errc::overflow);
  }

  auto code_point = static_cast<char32_t>(mask8(*it));
  auto result = increment(it, last);
  if (!result) {
    return make_unexpected(std::move(result.error()));
  }
  return ((code_point << 6) & 0x7ff) + (*it & 0x3f);
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> get_sequence_3(
    OctetIterator& it,
    OctetIterator last) {
  if (it == last) {
    return make_unexpected(unicode_errc::overflow);
  }

  auto code_point = static_cast<char32_t>(mask8(*it));

  auto result = increment(it, last);
  if (!result) {
    return make_unexpected(std::move(result.error()));
  }

  code_point = ((code_point << 12) & 0xffff) +
               ((mask8(*it) << 6) & 0xfff);

  result = increment(it, last);
  if (!result) {
    return make_unexpected(std::move(result.error()));
  }

  return code_point + (*it & 0x3f);
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> get_sequence_4(
    OctetIterator& it,
    OctetIterator last) {
  if (it == last) {
    return make_unexpected(unicode_errc::overflow);
  }

  auto code_point = static_cast<char32_t>(mask8(*it));

  auto result = increment(it, last);
  if (!result) {
    return make_unexpected(std::move(result.error()));
  }

  code_point = ((code_point << 18) & 0x1fffff) +
               ((mask8(*it) << 12) & 0x3ffff);

  result = increment(it, last);
  if (!result) {
    return make_unexpected(std::move(result.error()));
  }

  code_point += (mask8(*it) << 6) & 0xfff;

  result = increment(it, last);
  if (!result) {
    return make_unexpected(std::move(result.error()));
  }

  return code_point + (*it & 0x3f);
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> validate_next(
    OctetIterator& it,
    OctetIterator last) {
  if (it == last) {
    return make_unexpected(unicode_errc::overflow);
  }

  const auto length = sequence_length(it);
  auto code_point =
      (length == 1)? get_sequence_1(it, last) :
      (length == 2)? get_sequence_2(it, last) :
      (length == 3)? get_sequence_3(it, last) :
      (length == 4)? get_sequence_4(it, last) : make_unexpected(unicode_errc::overflow)
      ;
  if (code_point) {
    if (is_code_point_valid(code_point.value())) {
      if (is_overlong_sequence(code_point.value(), length)) {
        return make_unexpected(unicode_errc::illegal_byte_sequence);
      }
    } else {
      return make_unexpected(unicode_errc::invalid_code_point);
    }

    ++it;
  }
  return code_point;
}
}  // namespace details

template <typename OctetIterator>
OctetIterator find_invalid(
    OctetIterator first,
    OctetIterator last) {
  auto it = first;
  while (it != last) {
    auto err_code = details::validate_next(it, last);
    if (!err_code) {
      return it;
    }
  }
  return it;
}

template <typename OctetIterator>
inline bool is_valid(
    OctetIterator first,
    OctetIterator last) {
  return (utf8::find_invalid(first, last) == last);
}

template <typename OctetIterator>
expected<OctetIterator, unicode_errc> append(
    char32_t code_point,
    OctetIterator result) {
  if (!details::is_code_point_valid(code_point)) {
    return make_unexpected(unicode_errc::invalid_code_point);
  }

  if (code_point < 0x80) { // one octet
    *(result++) = static_cast<uint8_t>(code_point);
  }
  else if (code_point < 0x800) {  // two octets
    *(result++) = static_cast<uint8_t>((code_point >> 6) | 0xc0);
    *(result++) = static_cast<uint8_t>((code_point & 0x3f) | 0x80);
  } else if (code_point < 0x10000) {  // three octets
    *(result++) = static_cast<uint8_t>((code_point >> 12) | 0xe0);
    *(result++) = static_cast<uint8_t>(((code_point >> 6) & 0x3f) | 0x80);
    *(result++) = static_cast<uint8_t>((code_point & 0x3f) | 0x80);
  } else {  // four octets
    *(result++) = static_cast<uint8_t>((code_point >> 18) | 0xf0);
    *(result++) = static_cast<uint8_t>(((code_point >> 12) & 0x3f) | 0x80);
    *(result++) = static_cast<uint8_t>(((code_point >> 6) & 0x3f) | 0x80);
    *(result++) = static_cast<uint8_t>((code_point & 0x3f) | 0x80);
  }
  return result;
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> next(
    OctetIterator& it,
    OctetIterator last) {
  return details::validate_next(it, last);
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> peek_next(
    OctetIterator it,
    OctetIterator last) {
  return next(it, last);
}

template <typename OctetIterator>
expected<char32_t, unicode_errc> prior(
    OctetIterator& it,
    OctetIterator first) {
  if (it == first) {
    return make_unexpected(unicode_errc::overflow);
  }

  auto last = it;
  // Go back until we hit either a lead octet or start
  --it;
  while (details::is_trail(*it)) {
    if (it == first) {
      return make_unexpected(unicode_errc::invalid_code_point);
    }
    --it;
  }
  return peek_next(it, last);
}

template <typename OctetIterator>
expected<void, unicode_errc> advance(
    OctetIterator& it,
    std::ptrdiff_t n,
    OctetIterator end) {
  while (n != 0) {
    auto result = next(it, end);
    if (!result) {
      return make_unexpected(std::move(result.error()));
    }
    --n;
  }
}

template <typename OctetIterator>
expected<std::ptrdiff_t, unicode_errc> distance(
    OctetIterator first,
    OctetIterator last) {
  std::ptrdiff_t dist = 0;
  auto it = first;
  while (it != last) {
    auto result = next(it, last);
    if (!result) {
      return make_unexpected(std::move(result.error()));
    }
    ++dist;
  }
  return dist;
}

template <typename U16BitIterator, typename OctetIterator>
expected<OctetIterator, unicode_errc> utf16to8(
    U16BitIterator first,
    U16BitIterator last,
    OctetIterator result) {
  auto it = first;
  while (it != last) {
    auto code_point = static_cast<std::uint32_t>(details::mask16(*it));
    ++it;

    // Take care of surrogate pairs first
    if (details::is_lead_surrogate(code_point)) {
      if (it != last) {
        auto trail_surrogate = details::mask16(*it);
        ++it;
        if (details::is_trail_surrogate(trail_surrogate)) {
          code_point = (code_point << 10) + trail_surrogate + details::constants::surrogate_offset;
        }
        else {
          return make_unexpected(unicode_errc::invalid_code_point);
        }
      } else {
        return make_unexpected(unicode_errc::invalid_code_point);
      }
    }
    else if (details::is_trail_surrogate(code_point)) {
      return make_unexpected(unicode_errc::invalid_code_point);
    }

    auto result_it = utf8::append(code_point, result);
    if (!result_it) {
      return make_unexpected(std::move(result_it.error()));
    }
  }
  return result;
}

template <typename U16BitIterator, typename OctetIterator>
expected<U16BitIterator, unicode_errc> utf8to16(
    OctetIterator first,
    OctetIterator last,
    U16BitIterator result) {
  auto it = first;
  while (it != last) {
    auto code_point = utf8::next(it, last);
    if (!code_point) {
      return make_unexpected(std::move(code_point.error()));
    }

    if (code_point.value() > 0xffff) {  // make a surrogate pair
      *result++ =
          static_cast<char16_t>((code_point.value() >> 10) +
            details::constants::lead_offset);
      *result++ =
          static_cast<char16_t>((code_point.value() & 0x3ff) +
            details::constants::trail_surrogate_min);
    } else {
      *result++ = static_cast<char16_t>(code_point.value());
    }
  }
  return result;
}

template <typename OctetIterator, typename U32BitIterator>
expected<OctetIterator, unicode_errc> utf32to8(
    U32BitIterator first,
    U32BitIterator last,
    OctetIterator result) {
  auto it = first;
  while (it != last) {
    auto result_it = utf8::append(*it, result);
    if (!result_it) {
      return make_unexpected(std::move(result_it.error()));
    }
    result = result_it.value();
    ++it;
  }
  return result;
}

template <typename OctetIterator, typename U32BitIterator>
expected<U32BitIterator, unicode_errc> utf8to32(
    OctetIterator first,
    OctetIterator last,
    U32BitIterator result) {
  auto it = first;
  while (it != last) {
    auto next_it = next(it, last);
    if (!next_it) {
      return make_unexpected(std::move(next_it.error()));
    }
    (*result++) = next_it.value();
  }
  return result;
}
}  // namespace utf8
}  // namespace skyr

#endif  // header guard
