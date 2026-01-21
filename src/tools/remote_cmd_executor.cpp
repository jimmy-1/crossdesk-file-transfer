/*
 * Remote Command Executor Implementation
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#include "remote_cmd_executor.h"

#include <chrono>
#include <thread>

#include "rd_log.h"

#ifdef _WIN32
#include <windows.h>
#include <sstream>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#endif

namespace crossdesk {

RemoteCmdExecutor::RemoteCmdExecutor() {}

RemoteCmdExecutor::~RemoteCmdExecutor() {
  Cancel();
}

void RemoteCmdExecutor::SetTimeout(int timeout_ms) {
  timeout_ms_ = timeout_ms;
}

void RemoteCmdExecutor::Cancel() {
  cancelled_ = true;
}

#ifdef _WIN32

bool RemoteCmdExecutor::IsRunningAsAdmin() {
  BOOL is_admin = FALSE;
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID admin_group = nullptr;

  if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &admin_group)) {
    CheckTokenMembership(NULL, admin_group, &is_admin);
    FreeSid(admin_group);
  }

  return is_admin == TRUE;
}

bool RemoteCmdExecutor::RestartAsAdmin(const std::string& args) {
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpVerb = "runas";
  sei.lpFile = exe_path;
  sei.lpParameters = args.empty() ? NULL : args.c_str();
  sei.hwnd = NULL;
  sei.nShow = SW_NORMAL;

  if (!ShellExecuteExA(&sei)) {
    DWORD error = GetLastError();
    if (error != ERROR_CANCELLED) {
      LOG_ERROR("Failed to restart as admin: {}", error);
      return false;
    }
  }

  return true;
}

std::string RemoteCmdExecutor::ReadPipeOutput(void* pipe_handle) {
  HANDLE hPipe = static_cast<HANDLE>(pipe_handle);
  std::string output;
  char buffer[4096];
  DWORD bytes_read = 0;

  while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
    buffer[bytes_read] = '\0';
    output += buffer;
  }

  return output;
}

CmdExecuteResult RemoteCmdExecutor::ExecuteWindows(const std::string& command, bool as_admin) {
  CmdExecuteResult result;
  result.elevated = IsRunningAsAdmin();

  if (as_admin && !result.elevated) {
    result.success = false;
    result.error = "Not running as administrator. Command requires elevated privileges.";
    LOG_WARN("Command requires admin privileges but not elevated: {}", command);
    return result;
  }

  LOG_INFO("Executing command{}: {}", result.elevated ? " (elevated)" : "", command);

  // 创建管道
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  HANDLE hStdoutRead = NULL;
  HANDLE hStdoutWrite = NULL;
  HANDLE hStderrRead = NULL;
  HANDLE hStderrWrite = NULL;

  if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
      !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
    result.error = "Failed to create pipes";
    LOG_ERROR("Failed to create pipes for command execution");
    return result;
  }

  // 确保读取句柄不被继承
  SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

  // 准备执行命令
  STARTUPINFOA si = {sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.hStdOutput = hStdoutWrite;
  si.hStdError = hStderrWrite;
  si.hStdInput = NULL;
  si.wShowWindow = SW_HIDE;

  PROCESS_INFORMATION pi = {0};

  // 构建完整命令行
  std::string full_command = "cmd.exe /c " + command;

  // 创建进程
  BOOL success = CreateProcessA(
      NULL,
      const_cast<char*>(full_command.c_str()),
      NULL,
      NULL,
      TRUE,
      CREATE_NO_WINDOW,
      NULL,
      NULL,
      &si,
      &pi);

  CloseHandle(hStdoutWrite);
  CloseHandle(hStderrWrite);

  if (!success) {
    result.error = "Failed to create process";
    result.exit_code = GetLastError();
    LOG_ERROR("Failed to create process for command: {}, error: {}", command, result.exit_code);
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    return result;
  }

  // 等待进程完成或超时
  DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms_);
  
  if (wait_result == WAIT_TIMEOUT) {
    TerminateProcess(pi.hProcess, 1);
    result.error = "Command execution timeout";
    LOG_WARN("Command execution timeout: {}", command);
  } else if (wait_result == WAIT_OBJECT_0) {
    // 读取输出
    result.output = ReadPipeOutput(hStdoutRead);
    result.error = ReadPipeOutput(hStderrRead);

    // 获取退出代码
    DWORD exit_code = 0;
    if (GetExitCodeProcess(pi.hProcess, &exit_code)) {
      result.exit_code = static_cast<int>(exit_code);
      result.success = (exit_code == 0);
    }

    LOG_INFO("Command executed, exit code: {}, output length: {}", 
             result.exit_code, result.output.length());
  }

  CloseHandle(hStdoutRead);
  CloseHandle(hStderrRead);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return result;
}

CmdExecuteResult RemoteCmdExecutor::Execute(const std::string& command, bool as_admin) {
  cancelled_ = false;
  return ExecuteWindows(command, as_admin);
}

void RemoteCmdExecutor::ExecuteAsync(const std::string& command,
                                    CmdOutputCallback callback,
                                    bool as_admin) {
  std::thread([this, command, callback, as_admin]() {
    auto result = Execute(command, as_admin);
    
    if (!result.output.empty()) {
      callback(result.output, false);
    }
    
    if (!result.error.empty()) {
      callback(result.error, true);
    }
    
    if (!result.success) {
      callback("Exit code: " + std::to_string(result.exit_code), true);
    }
  }).detach();
}

#else  // Linux/macOS

bool RemoteCmdExecutor::IsRunningAsAdmin() {
  return geteuid() == 0;
}

bool RemoteCmdExecutor::RestartAsAdmin(const std::string& args) {
  // Unix系统需要使用sudo
  LOG_INFO("Please restart with sudo for admin privileges");
  return false;
}

CmdExecuteResult RemoteCmdExecutor::ExecuteUnix(const std::string& command, bool as_admin) {
  CmdExecuteResult result;
  result.elevated = IsRunningAsAdmin();

  if (as_admin && !result.elevated) {
    result.success = false;
    result.error = "Not running as root. Command requires elevated privileges.";
    LOG_WARN("Command requires root privileges: {}", command);
    return result;
  }

  std::string full_command = command + " 2>&1";
  std::array<char, 256> buffer;
  FILE* pipe = popen(full_command.c_str(), "r");

  if (!pipe) {
    result.error = "Failed to execute command";
    LOG_ERROR("Failed to execute command: {}", command);
    return result;
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result.output += buffer.data();
  }

  int status = pclose(pipe);
  result.exit_code = WEXITSTATUS(status);
  result.success = (result.exit_code == 0);

  LOG_INFO("Command executed, exit code: {}", result.exit_code);
  return result;
}

CmdExecuteResult RemoteCmdExecutor::Execute(const std::string& command, bool as_admin) {
  cancelled_ = false;
  return ExecuteUnix(command, as_admin);
}

void RemoteCmdExecutor::ExecuteAsync(const std::string& command,
                                    CmdOutputCallback callback,
                                    bool as_admin) {
  std::thread([this, command, callback, as_admin]() {
    auto result = Execute(command, as_admin);
    
    if (!result.output.empty()) {
      callback(result.output, false);
    }
    
    if (!result.success && !result.error.empty()) {
      callback(result.error, true);
    }
  }).detach();
}

#endif

}  // namespace crossdesk
