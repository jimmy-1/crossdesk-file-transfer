# 远程CMD功能使用指南

## 功能概述

CrossDesk Agent 被控端现已支持**远程CMD命令执行**功能，允许控制端远程执行命令行指令。

### ✨ 核心特性

- ✅ **管理员权限执行** - 以管理员权限运行CMD命令
- ✅ **实时输出反馈** - 命令输出实时返回控制端
- ✅ **错误处理** - 分离标准输出和错误输出
- ✅ **超时保护** - 默认30秒超时，防止命令hang住
- ✅ **异步执行** - 不阻塞其他操作
- ✅ **跨平台支持** - Windows/Linux/macOS

## 工作原理

```
控制端                                被控端
   |                                    |
   |--- 发送CMD命令 (remote_cmd) ------>|
   |                                    | 检查管理员权限
   |                                    | 执行命令
   |                                    | (cmd.exe /c <command>)
   |<--- 返回输出 (cmd_output) ---------|
   |<--- 返回错误 (cmd_output) ---------|
   |<--- 返回退出码 (cmd_output) -------|
```

### 数据通道

- **remote_cmd** - 接收要执行的命令
- **cmd_output** - 返回命令输出和错误

## 使用方法

### 被控端配置

#### 1. 以管理员权限运行（推荐）

```bash
# 右键以管理员身份运行
crossdesk_agent.exe

# 或通过命令行
runas /user:Administrator crossdesk_agent.exe
```

**日志输出**：
```
[INFO] Running with ADMINISTRATOR privileges
[INFO] Remote CMD channels initialized
```

#### 2. 普通权限运行（受限）

```bash
crossdesk_agent.exe
```

**日志输出**：
```
[WARN] Running with NORMAL USER privileges
[WARN] Remote CMD commands will require administrator privileges
[WARN] Consider running as administrator for full functionality
```

### 控制端使用（需要实现）

控制端需要实现发送命令的功能，示例代码：

```cpp
// 发送CMD命令到被控端
void SendRemoteCommand(PeerPtr peer, const std::string& command) {
  SendReliableDataFrame(peer, command.c_str(), command.length(), "remote_cmd");
  LOG_INFO("Sent remote command: {}", command);
}

// 接收CMD输出
void OnReceiveCmdOutput(const char* data, size_t size) {
  std::string output(data, size);
  
  if (output.find("[OUTPUT]") == 0) {
    // 标准输出
    std::string result = output.substr(9);  // 去掉 "[OUTPUT] "
    LOG_INFO("CMD Output: {}", result);
  } else if (output.find("[ERROR]") == 0) {
    // 错误输出
    std::string error = output.substr(8);  // 去掉 "[ERROR] "
    LOG_ERROR("CMD Error: {}", error);
  }
}
```

## 命令示例

### Windows系统命令

#### 基础命令
```cmd
# 查看系统信息
systeminfo

# 查看IP配置
ipconfig /all

# 列出目录内容
dir C:\

# 查看进程列表
tasklist

# 查看服务状态
sc query
```

#### 管理员权限命令
```cmd
# 创建用户
net user testuser password123 /add

# 安装软件
msiexec /i software.msi /quiet

# 修改服务
sc config servicename start=auto

# 修改注册表
reg add "HKLM\Software\Key" /v Value /d Data

# 防火墙配置
netsh advfirewall firewall add rule name="Test" dir=in action=allow
```

#### 文件操作
```cmd
# 复制文件
copy C:\source\file.txt D:\dest\

# 删除文件
del C:\temp\file.txt

# 创建目录
mkdir C:\NewFolder

# 查看文件内容
type C:\file.txt
```

### Linux/macOS命令

```bash
# 系统信息
uname -a

# 磁盘使用
df -h

# 内存使用
free -m

# 进程列表
ps aux

# 网络状态
netstat -an
```

## 权限说明

### Windows权限要求

| 命令类型 | 普通用户 | 管理员 | 说明 |
|---------|---------|--------|------|
| dir, type, echo | ✅ | ✅ | 基础命令 |
| ipconfig, systeminfo | ✅ | ✅ | 查询命令 |
| net user, sc config | ❌ | ✅ | 需要管理员 |
| reg add, reg delete | ❌ | ✅ | 注册表操作 |
| 服务管理 (sc, net) | ❌ | ✅ | 需要管理员 |

### 检查权限

被控端启动时会显示权限状态：

```
[INFO] Running with ADMINISTRATOR privileges  # 管理员权限
# 或
[WARN] Running with NORMAL USER privileges    # 普通权限
```

## 安全考虑

⚠️ **重要安全提示**：

### 1. 访问控制
- 修改默认密码（不使用 `admin`）
- 限制网络访问（防火墙规则）
- 仅在受信任的网络中使用

### 2. 命令审计
所有命令都会记录在日志中：
```
[INFO] Received remote CMD command: <command>
[INFO] Executing command with admin privileges
```

### 3. 命令白名单（建议实现）
可以在代码中添加命令白名单：

```cpp
// 在 OnReceiveDataBuffer 中添加
static const std::vector<std::string> allowed_commands = {
  "ipconfig", "systeminfo", "dir", "tasklist"
};

bool IsCommandAllowed(const std::string& command) {
  for (const auto& allowed : allowed_commands) {
    if (command.find(allowed) == 0) {
      return true;
    }
  }
  return false;
}

// 使用
if (!IsCommandAllowed(command)) {
  LOG_WARN("Command not in whitelist: {}", command);
  return;
}
```

### 4. 禁用危险命令
避免执行以下危险命令：
- `format` - 格式化磁盘
- `del /f /s /q C:\` - 递归删除
- `shutdown /s` - 关机
- `reg delete` - 删除注册表

## 输出格式

### 标准输出
```
[OUTPUT] <命令输出内容>
```

### 错误输出
```
[ERROR] <错误信息>
```

### 退出代码
```
[ERROR] Exit code: <code>
```

## 超时设置

默认命令超时时间为30秒，可以修改：

```cpp
if (!g_cmd_executor) {
  g_cmd_executor = std::make_unique<RemoteCmdExecutor>();
  g_cmd_executor->SetTimeout(60000);  // 设置为60秒
}
```

## 故障排查

### 命令无响应

**症状**：发送命令后没有输出

**检查**：
1. 查看日志确认命令已接收：
   ```
   [INFO] Received remote CMD command: <command>
   ```
2. 检查是否超时（默认30秒）
3. 测试简单命令如 `echo test`

### 权限不足错误

**症状**：
```
[ERROR] Not running as administrator. Command requires elevated privileges.
```

**解决**：
1. 以管理员身份重新运行被控端
2. 或使用不需要管理员权限的命令

### 命令执行失败

**症状**：
```
[ERROR] Exit code: 1
```

**检查**：
1. 命令语法是否正确
2. 文件路径是否存在
3. 查看错误输出了解具体原因

## 日志示例

### 成功执行
```
[INFO] Received remote CMD command: ipconfig
[INFO] Executing command with admin privileges
[INFO] Command executed, exit code: 0, output length: 1234
[INFO] Sent CMD output: 1234 bytes
```

### 权限不足
```
[INFO] Received remote CMD command: net user testuser /add
[WARN] Command requires admin privileges but not elevated: net user testuser /add
[ERROR] Not running as administrator. Command requires elevated privileges.
```

### 超时
```
[INFO] Received remote CMD command: ping 8.8.8.8 -t
[INFO] Executing command with admin privileges
[WARN] Command execution timeout: ping 8.8.8.8 -t
```

## API参考

### RemoteCmdExecutor类

#### 构造函数
```cpp
RemoteCmdExecutor();
```

#### 同步执行
```cpp
CmdExecuteResult Execute(const std::string& command, bool as_admin = true);
```

#### 异步执行
```cpp
void ExecuteAsync(const std::string& command, 
                 CmdOutputCallback callback,
                 bool as_admin = true);
```

#### 权限检查
```cpp
static bool IsRunningAsAdmin();
```

#### 设置超时
```cpp
void SetTimeout(int timeout_ms);
```

#### 取消执行
```cpp
void Cancel();
```

### CmdExecuteResult结构

```cpp
struct CmdExecuteResult {
  int exit_code;           // 退出代码
  std::string output;      // 标准输出
  std::string error;       // 错误输出
  bool success;            // 是否成功
  bool elevated;           // 是否以管理员权限执行
};
```

## 扩展功能

### 1. 交互式Shell（高级）

可以实现持久的CMD会话：

```cpp
// 保持CMD进程运行
// 发送命令到stdin
// 读取stdout/stderr
```

### 2. PowerShell支持

修改命令执行器以支持PowerShell：

```cpp
std::string full_command = "powershell.exe -Command " + command;
```

### 3. 命令历史

记录执行过的命令：

```cpp
static std::vector<std::string> command_history;
command_history.push_back(command);
```

## 最佳实践

### 1. 命令测试
在生产环境前先测试命令：
```cmd
# 安全命令
echo test
dir
ipconfig

# 避免危险命令
# del /f /s /q C:\*
# format C:
```

### 2. 输出限制
对于输出很多的命令，考虑添加限制：
```cmd
# 限制行数
dir C:\ | more

# 限制文件数
dir C:\ /b | findstr /n "^" | findstr /r "^[1-9][0-9]:"
```

### 3. 错误处理
始终检查返回的错误信息和退出码

### 4. 日志监控
定期检查日志中的命令执行记录

## 总结

✅ **远程CMD功能已完全实现**
✅ **支持管理员权限执行**
✅ **实时输出反馈**
✅ **完整的错误处理**
✅ **安全的超时机制**

**快速开始**：
1. 以管理员身份运行被控端
2. 控制端发送命令到 `remote_cmd` 通道
3. 接收 `cmd_output` 通道的输出

**安全提醒**：
- 仅在受信任环境使用
- 修改默认密码
- 审查命令日志
- 考虑实现命令白名单

---

*创建时间: 2026年1月21日*
