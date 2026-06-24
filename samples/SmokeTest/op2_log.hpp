#pragma once
// op2_log.hpp - tiny append-only debug logger for the TitanAPI mission.
//
// Writes to  <Outpost2.exe dir>\OPU\logs\cTitanSmokeTest.log  (or <dir>\logs if the exe already sits in an OPU
// folder). Each line is opened+written+closed via raw Win32 (KERNEL32 only - no <windows.h>, so it does
// not clash with memory.hpp's GetModuleHandleA declaration; keeps the DLL importing only KERNEL32). The
// open/close-per-line cost buys a crucial property for crash debugging: the last line is flushed to disk
// before the next call runs, so if an engine call crashes the game, its "about to call" line is preserved.

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace op2::log {

// Minimal Win32 imports (all KERNEL32). extern "C" => global C linkage, so these resolve to the real
// imports despite living in this namespace.
extern "C" {
  __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
  __declspec(dllimport) void*         __stdcall CreateFileA(const char* name, unsigned long access, unsigned long share,
                                                            void* sa, unsigned long disp, unsigned long flags, void* tmpl);
  __declspec(dllimport) int           __stdcall WriteFile(void* h, const void* buf, unsigned long n,
                                                          unsigned long* written, void* ovl);
  __declspec(dllimport) int           __stdcall CloseHandle(void* h);
  __declspec(dllimport) int           __stdcall CreateDirectoryA(const char* path, void* sa);
  __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId();
  __declspec(dllimport) unsigned long __stdcall GetTickCount();
}

namespace detail {

inline char g_path[600] = { 0 };
inline bool g_ready     = false;

inline void* const kInvalidHandle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));

// Create every directory segment of an absolute path (e.g. D:\Outpost 2\OPU\logs). Drive-letter paths only.
inline void ensure_dirs(char* dir) {
  for (char* p = dir + 3; *p; ++p) {            // skip "X:\"
    if (*p == '\\') { *p = 0; CreateDirectoryA(dir, nullptr); *p = '\\'; }
  }
  CreateDirectoryA(dir, nullptr);
}

inline void resolve_path() {
  g_ready = true;
  char exe[600] = { 0 };
  const unsigned long n = GetModuleFileNameA(nullptr, exe, sizeof(exe));
  if (n == 0 || n >= sizeof(exe)) { std::strcpy(g_path, "cTitanSmokeTest.log"); return; }

  int slash = -1;                                // strip the exe filename -> directory
  for (int i = 0; exe[i]; ++i) if (exe[i] == '\\') slash = i;
  if (slash < 0) { std::strcpy(g_path, "cTitanSmokeTest.log"); return; }
  exe[slash] = 0;

  const int len = static_cast<int>(std::strlen(exe));
  const bool inOpu = (len >= 4) && (exe[len-4] == '\\') &&
                     (exe[len-3]=='O'||exe[len-3]=='o') && (exe[len-2]=='P'||exe[len-2]=='p') &&
                     (exe[len-1]=='U'||exe[len-1]=='u');
  char dir[600];
  std::snprintf(dir, sizeof(dir), inOpu ? "%s\\logs" : "%s\\OPU\\logs", exe);
  ensure_dirs(dir);
  std::snprintf(g_path, sizeof(g_path), "%s\\cTitanSmokeTest.log", dir);
}

inline void write_raw(const char* s, int len) {
  // FILE_APPEND_DATA=0x4, FILE_SHARE_READ|WRITE=0x3, OPEN_ALWAYS=4, FILE_ATTRIBUTE_NORMAL=0x80
  void* h = CreateFileA(g_path, 0x0004, 0x0003, nullptr, 4, 0x80, nullptr);
  if (h == kInvalidHandle || h == nullptr) return;
  unsigned long w = 0;
  WriteFile(h, s, static_cast<unsigned long>(len), &w, nullptr);
  CloseHandle(h);
}

} // namespace detail

/// Absolute path the log resolves to (also written into the log on the first line).
inline const char* path() { if (!detail::g_ready) detail::resolve_path(); return detail::g_path; }

/// The OS process id - logged once on DLL attach/detach, not on every line.
inline unsigned long pid() { return GetCurrentProcessId(); }

/// Append one line (with a tick prefix), flushed immediately.
inline void line(const char* msg) {
  if (!detail::g_ready) detail::resolve_path();
  char buf[1024];
  int n = std::snprintf(buf, sizeof(buf), "[t=%lu] %s\r\n",
                        static_cast<unsigned long>(GetTickCount()), msg);
  if (n < 0) return;
  if (n > static_cast<int>(sizeof(buf))) n = static_cast<int>(sizeof(buf));
  detail::write_raw(buf, n);
}

/// printf-style line.
inline void linef(const char* fmt, ...) {
  char msg[768];
  va_list ap; va_start(ap, fmt);
  std::vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  line(msg);
}

} // namespace op2::log
