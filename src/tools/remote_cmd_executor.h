/*
 * Remote Command Executor
 * 远程CMD执行器 - 支持管理员权限执行命令
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _REMOTE_CMD_EXECUTOR_H_
#define _REMOTE_CMD_EXECUTOR_H_

#include <functional>
#include <string>

namespace crossdesk {

// 命令执行结果
struct CmdExecuteResult {
  int exit_code = 0;           // 退出代码
  std::string output;          // 标准输出
  std::string error;           // 错误输出
  bool success = false;        // 是否成功执行
  bool elevated = false;       // 是否以管理员权限执行
};

// 输出回调函数类型
using CmdOutputCallback = std::function<void(const std::string& output, bool is_error)>;

class RemoteCmdExecutor {
 public:
  RemoteCmdExecutor();
  ~RemoteCmdExecutor();

  // 执行命令（同步）
  CmdExecuteResult Execute(const std::string& command, bool as_admin = true);

  // 执行命令（异步，带实时输出）
  void ExecuteAsync(const std::string& command, 
                   CmdOutputCallback callback,
                   bool as_admin = true);

  // 检查是否有管理员权限
  static bool IsRunningAsAdmin();

  // 以管理员权限重新启动
  static bool RestartAsAdmin(const std::string& args = "");

  // 设置命令超时（毫秒）
  void SetTimeout(int timeout_ms);

  // 取消当前执行
  void Cancel();

 private:
  int timeout_ms_ = 30000;  // 默认30秒超时
  bool cancelled_ = false;

#ifdef _WIN32
  CmdExecuteResult ExecuteWindows(const std::string& command, bool as_admin);
  std::string ReadPipeOutput(void* pipe_handle);
#else
  CmdExecuteResult ExecuteUnix(const std::string& command, bool as_admin);
#endif
};

}  // namespace crossdesk

#endif  // _REMOTE_CMD_EXECUTOR_H_
