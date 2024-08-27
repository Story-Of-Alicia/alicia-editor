#include "util.hpp"

#include <vector>

#include <commdlg.h>
#include <ShlObj_core.h>
#include <windows.h>

namespace util
{

  std::wstring win32_widen(const std::string_view& narrow_str)
  {
    if (narrow_str.empty())
      return L"";

    std::wstring wide_str;
    wide_str.resize(narrow_str.size());
    MultiByteToWideChar(
        CP_UTF8,
        0,
        narrow_str.data(),
        static_cast<int32_t>(narrow_str.size()),
        wide_str.data(),
        static_cast<int32_t>(wide_str.size()));
    return wide_str;
  }

  std::string win32_narrow(const std::wstring_view& wide_str)
  {
    if (wide_str.empty())
      return "";

    // Determine the required size of the narrow UTF8 string.
    const auto size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide_str.data(),
        static_cast<int32_t>(wide_str.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    // Convert the unicode string to UTF8 string.
    std::string narrow_str;
    narrow_str.resize(size);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide_str.data(),
        static_cast<int32_t>(wide_str.size()),
        narrow_str.data(),
        size,
        nullptr,
        nullptr);

    return narrow_str;
  }

  std::string win32_narrow(const std::u16string_view& wide_str)
  {
    if (wide_str.empty())
      return "";

    // Determine the required size of the narrow UTF8 string.
    const auto size = WideCharToMultiByte(
        CP_UTF8,
        0,
        reinterpret_cast<const wchar_t*>(wide_str.data()),
        static_cast<int32_t>(wide_str.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    // Convert the unicode string to UTF8 string.
    std::string narrow_str;
    narrow_str.resize(size);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        reinterpret_cast<const wchar_t*>(wide_str.data()),
        static_cast<int32_t>(wide_str.size()),
        narrow_str.data(),
        size,
        nullptr,
        nullptr);

    return narrow_str;
  }

  std::string win32_prompt_for_file(
      std::string_view prompt_title,
      const std::span<const std::string_view>& type_filter,
      uint32_t flags)
  {

    std::wstring path;
    path.resize(MAX_PATH);

    std::vector<wchar_t> filter;
    for (auto& entry : type_filter)
    {
      for (wchar_t value : win32_widen(entry))
      {
        filter.emplace_back(value);
      }
      filter.emplace_back('\0');
    }
    filter.emplace_back('\0');

    const auto title = win32_widen(prompt_title);
    OPENFILENAMEW of{};
    of.lStructSize = sizeof(OPENFILENAME);
    of.lpstrFilter = filter.data();
    of.lpstrFile = path.data();
    of.lpstrTitle = title.c_str();
    of.nMaxFile = MAX_PATH;
    of.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ENABLESIZING | OFN_NOCHANGEDIR | flags;
    of.lpstrDefExt = L"";

    GetOpenFileNameW(&of);
    return win32_narrow(path);
  }

  std::string win32_prompt_for_folder(std::string_view prompt_title, int32_t flags)
  {
    std::wstring path;
    path.resize(MAX_PATH);

    const auto title = win32_widen(prompt_title);
    BROWSEINFOW bi{};
    bi.pidlRoot = nullptr;
    bi.pszDisplayName = path.data();
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX | flags;
    bi.lpfn = NULL;
    bi.lParam = 0;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != nullptr)
      SHGetPathFromIDListW(pidl, path.data());

    return win32_narrow(path);
  }

} // namespace util