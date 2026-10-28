// SPDX-License-Identifier: MIT
#include "FileDialog.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

namespace ak {
namespace {

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Runs a command and returns its stdout; exitCode is the process exit status (-1 if it could not start).
std::string run(const std::string& cmd, int& exitCode) {
  exitCode = -1;
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return {};
  std::string out;
  char buf[512];
  while (size_t n = fread(buf, 1, sizeof buf, f)) out.append(buf, n);
  int st = pclose(f);
#ifdef _WIN32
  exitCode = st;
#else
  exitCode = (st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
#endif
  return trim(out);
}

DialogResult finish(const std::string& out, int code, std::string& path) {
  if (!out.empty()) { path = out; return DialogResult::Ok; }
  if (code == -1 || code == 127) return DialogResult::Unavailable; // tool missing
  return DialogResult::Cancelled;
}

bool folderExists(const std::string& f) {
  std::error_code ec;
  return !f.empty() && std::filesystem::is_directory(std::filesystem::u8path(f), ec);
}

#if defined(__APPLE__)
// AppleScript string literal
std::string as(const std::string& s) {
  std::string o = "\"";
  for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
  return o + "\"";
}
// single-quoted shell argument
std::string sh(const std::string& s) {
  std::string o = "'";
  for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; }
  return o + "'";
}
std::string osascript(const std::string& script, int& code) {
  return run("/usr/bin/osascript -e " + sh(script) + " 2>/dev/null", code);
}
#elif defined(_WIN32)
// PowerShell single-quoted string
std::string ps(const std::string& s) {
  std::string o = "'";
  for (char c : s) { if (c == '\'') o += "''"; else o += c; }
  return o + "'";
}
std::string powershell(const std::string& script, int& code) {
  std::string cmd = "powershell.exe -NoProfile -NonInteractive -STA -Command \"[Console]::OutputEncoding=[Text.Encoding]::UTF8; "
                    "Add-Type -AssemblyName System.Windows.Forms; " + script + "\"";
  return run(cmd, code);
}
#else
std::string sh(const std::string& s) {
  std::string o = "'";
  for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; }
  return o + "'";
}
#endif

// Headless sessions (render nodes, CI) or AK_NO_DIALOGS=1: never try to open a window.
bool dialogsDisabled() {
  const char* off = std::getenv("AK_NO_DIALOGS");
  if (off && *off && std::string(off) != "0") return true;
#if !defined(__APPLE__) && !defined(_WIN32)
  const char* x = std::getenv("DISPLAY");
  const char* w = std::getenv("WAYLAND_DISPLAY");
  if ((!x || !*x) && (!w || !*w)) return true;
#endif
  return false;
}

} // namespace

DialogResult openFileDialog(const std::string& title, const std::string& folder, std::string& path) {
  if (dialogsDisabled()) return DialogResult::Unavailable;
  int code = -1;
  std::string out;
#if defined(__APPLE__)
  std::string s = "activate\nset f to choose file with prompt " + as(title);
  if (folderExists(folder)) s += " default location (POSIX file " + as(folder) + ")";
  s += "\nPOSIX path of f";
  out = osascript(s, code);
#elif defined(_WIN32)
  std::string s = "$d=New-Object System.Windows.Forms.OpenFileDialog; $d.Title=" + ps(title) +
                  "; $d.Filter='AndyKit preset (*.andykit)|*.andykit|All files (*.*)|*.*'";
  if (folderExists(folder)) s += "; $d.InitialDirectory=" + ps(folder);
  s += "; if($d.ShowDialog() -eq 'OK'){ $d.FileName }";
  out = powershell(s, code);
#else
  std::string start = folderExists(folder) ? folder + "/" : std::string();
  out = run("zenity --file-selection --title=" + sh(title) + " --file-filter='AndyKit preset | *.andykit' --file-filter='All files | *'" +
            (start.empty() ? "" : " --filename=" + sh(start)) + " 2>/dev/null", code);
  if (code == 127 || code == -1)
    out = run("kdialog --getopenfilename " + sh(start.empty() ? "." : start) + " '*.andykit' --title " + sh(title) + " 2>/dev/null", code);
#endif
  return finish(out, code, path);
}

DialogResult saveFileDialog(const std::string& title, const std::string& folder, const std::string& fileName, std::string& path) {
  if (dialogsDisabled()) return DialogResult::Unavailable;
  int code = -1;
  std::string out;
#if defined(__APPLE__)
  std::string s = "activate\nset f to choose file name with prompt " + as(title) + " default name " + as(fileName);
  if (folderExists(folder)) s += " default location (POSIX file " + as(folder) + ")";
  s += "\nPOSIX path of f";
  out = osascript(s, code);
#elif defined(_WIN32)
  std::string s = "$d=New-Object System.Windows.Forms.SaveFileDialog; $d.Title=" + ps(title) +
                  "; $d.Filter='AndyKit preset (*.andykit)|*.andykit'; $d.DefaultExt='andykit'; $d.FileName=" + ps(fileName);
  if (folderExists(folder)) s += "; $d.InitialDirectory=" + ps(folder);
  s += "; if($d.ShowDialog() -eq 'OK'){ $d.FileName }";
  out = powershell(s, code);
#else
  std::string start = (folderExists(folder) ? folder + "/" : std::string()) + fileName;
  out = run("zenity --file-selection --save --confirm-overwrite --title=" + sh(title) + " --filename=" + sh(start) + " 2>/dev/null", code);
  if (code == 127 || code == -1)
    out = run("kdialog --getsavefilename " + sh(start) + " '*.andykit' --title " + sh(title) + " 2>/dev/null", code);
#endif
  return finish(out, code, path);
}

} // namespace ak
