/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <shims/filesystem>

#include <nlohmann/json.hpp>

#include <concepts>
#include <string>

namespace OpenKneeboard {

/// Translation marker which requires a literal string
#ifdef _
#undef _
#endif
template <class T, size_t N>
  requires std::same_as<T, typename std::char_traits<T>::char_type>
constexpr auto _(const T (&str)[N]) {
  return str;
}

// Allow construction of std::string and std::string_view
constexpr std::string to_utf8(const std::string& in) {
  return in;
}
constexpr std::string_view to_utf8(std::string_view in) {
  return in;
}
constexpr std::string_view to_utf8(const char* in) {
  return in;
}

template <size_t N>
constexpr std::string_view to_utf8(const char (&in)[N]) {
  return {in, N};
}

std::string to_utf8(const std::wstring&);
std::string to_utf8(std::wstring_view);
std::string to_utf8(const wchar_t*);
std::string to_utf8(const std::filesystem::path&);

template <size_t N>
constexpr std::string to_utf8(const wchar_t (&in)[N]) {
  return to_utf8(std::wstring_view {in, N});
}

}// namespace OpenKneeboard

NLOHMANN_JSON_NAMESPACE_BEGIN

template <>
struct adl_serializer<std::filesystem::path> {
  static void to_json(nlohmann::json&, const std::filesystem::path&);
  static void from_json(const nlohmann::json&, std::filesystem::path&);
};

template <>
struct adl_serializer<std::wstring> {
  static void from_json(const nlohmann::json&, std::wstring&) = delete;
  static void to_json(nlohmann::json&, const std::wstring&) = delete;
};

template <>
struct adl_serializer<std::wstring_view> {
  static void from_json(const nlohmann::json&, std::wstring_view&) = delete;
  static void to_json(nlohmann::json&, const std::wstring_view&) = delete;
};
template <>
struct adl_serializer<wchar_t*> {
  static void from_json(const nlohmann::json&, wchar_t*) = delete;
  static void to_json(nlohmann::json&, const wchar_t*) = delete;
};

NLOHMANN_JSON_NAMESPACE_END
