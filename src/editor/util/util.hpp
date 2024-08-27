
#ifndef ALICIA_EDITOR_UTIL_HPP
#define ALICIA_EDITOR_UTIL_HPP

#include <span>
#include <string>

namespace util
{

  //! Widens a narrow UTF8 string.
  //!
  //! @param narrow_str UTF8 string.
  //! @returns Wide unicode string.
  std::wstring win32_widen(const std::string_view& narrow_str);

  //! Narrows a UTF16 unicode string.
  //!
  //! @param wide_str UTF16 string.
  //! @returns Narrow UTF8 string.
  std::string win32_narrow(const std::wstring_view& wide_str);

  //! Narrows a UTF16 unicode string.
  //!
  //! @param wide_str UTF16 string.
  //! @returns Narrow UTF8 string.
  std::string win32_narrow(const std::u16string_view& wide_str);

  //! Prompts user for a file selection.
  //!
  //! @param prompt_title Title of the prompt dialog.
  //! @param type_filter
  //! @param flags
  //! @returns Path to the file.
  std::string win32_prompt_for_file(
      std::string_view prompt_title,
      const std::span<const std::string_view>& type_filter,
      uint32_t flags = 0);

  //! Prompts user for a folder selection.
  //!
  //! @param prompt_title Title of the prompt dialog.
  //! @param flags
  std::string win32_prompt_for_folder(std::string_view prompt_title, int32_t flags = 0);

} // namespace util

#endif // ALICIA_EDITOR_UTIL_HPP