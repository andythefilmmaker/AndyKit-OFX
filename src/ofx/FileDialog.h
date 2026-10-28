// Native open/save file dialogs for the preset buttons.
// OpenFX hosts (DaVinci Resolve included) don't always show a browse button for file-path
// parameters, so the plugin asks the operating system for the dialog through its standard
// scripting tool: osascript on macOS, PowerShell on Windows, zenity/kdialog on Linux.
// SPDX-License-Identifier: MIT
#pragma once
#include <string>

namespace ak {

enum class DialogResult { Ok, Cancelled, Unavailable };

DialogResult openFileDialog(const std::string& title, const std::string& folder, std::string& path);
DialogResult saveFileDialog(const std::string& title, const std::string& folder, const std::string& fileName, std::string& path);

} // namespace ak
