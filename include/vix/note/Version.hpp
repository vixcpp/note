/**
 *
 *  @file Version.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Version information for the Vix Note module.
 *
 *  Copyright 2026, Gaspard Kirira.
 *  All rights reserved.
 *  https://github.com/vixcpp/note
 *
 *  Use of this source code is governed by a MIT license
 *  that can be found in the LICENSE file.
 *
 *  Vix Note
 *
 */

#ifndef VIX_NOTE_VERSION_HPP
#define VIX_NOTE_VERSION_HPP

#include <string>
#include <string_view>

namespace vix::note
{
  /**
   * @brief Major version number.
   */
  inline constexpr int VIX_NOTE_VERSION_MAJOR = 0;

  /**
   * @brief Minor version number.
   */
  inline constexpr int VIX_NOTE_VERSION_MINOR = 1;

  /**
   * @brief Patch version number.
   */
  inline constexpr int VIX_NOTE_VERSION_PATCH = 0;

  /**
   * @brief Full semantic version string.
   */
  inline constexpr std::string_view VIX_NOTE_VERSION = "0.1.0";

  /**
   * @brief Module name.
   */
  inline constexpr std::string_view VIX_NOTE_NAME = "Vix Note";

  /**
   * @brief Module repository URL.
   */
  inline constexpr std::string_view VIX_NOTE_REPOSITORY = "https://github.com/vixcpp/note";

  /**
   * @brief Returns the Vix Note semantic version string.
   *
   * @return Version string.
   */
  inline constexpr std::string_view version() noexcept
  {
    return VIX_NOTE_VERSION;
  }

  /**
   * @brief Returns the Vix Note module name.
   *
   * @return Module name.
   */
  inline constexpr std::string_view name() noexcept
  {
    return VIX_NOTE_NAME;
  }

  /**
   * @brief Returns the Vix Note repository URL.
   *
   * @return Repository URL.
   */
  inline constexpr std::string_view repository() noexcept
  {
    return VIX_NOTE_REPOSITORY;
  }

  /**
   * @brief Builds the Vix Note version string.
   *
   * @return Version string as std::string.
   */
  inline std::string version_string()
  {
    return std::string(VIX_NOTE_VERSION);
  }
}

#endif // VIX_NOTE_VERSION_HPP
