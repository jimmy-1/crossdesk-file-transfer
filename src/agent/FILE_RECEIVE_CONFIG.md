# 文件接收目录配置指南

## 概述

CrossDesk Agent 支持自定义文件接收目录，提供三种配置方式：

1. **命令行参数** - 临时指定，优先级最高
2. **配置文件** - 永久保存，推荐使用
3. **默认桌面** - 无配置时使用

## 配置方法

### 方法1: 命令行参数（临时）

适用于测试或临时修改接收目录。

```bash
# 基本用法
crossdesk_agent.exe --receive-dir "D:\ReceivedFiles"

# 使用相对路径
crossdesk_agent.exe --receive-dir ".\Downloads"

# 使用网络路径
crossdesk_agent.exe --receive-dir "\\SERVER\Share\Files"

# 带空格的路径需要引号
crossdesk_agent.exe --receive-dir "C:\My Files\Received"
```

**特点**：
- ✅ 立即生效
- ✅ 会自动保存到配置文件
- ✅ 适合快速测试

### 方法2: 配置文件（推荐）

永久配置，程序每次启动都使用此目录。

#### 步骤：

1. 找到配置文件位置：
   ```
   %APPDATA%\CrossDeskAgent\config.ini
   
   # 完整路径示例
   C:\Users\YourName\AppData\Roaming\CrossDeskAgent\config.ini
   ```

2. 使用记事本打开 `config.ini`

3. 在 `[Settings]` 部分添加或修改：
   ```ini
   [Settings]
   file_receive_directory=D:\ReceivedFiles
   ```

4. 保存文件并重启程序

#### 配置示例：

```ini
[Settings]
language=0
video_quality=1
video_frame_rate=1
video_encode_format=0
hardware_video_codec=false
enable_turn=true
enable_srtp=false
file_receive_directory=D:\ReceivedFiles
```

**路径格式**：

| 类型 | 示例 | 说明 |
|------|------|------|
| 绝对路径 | `D:\ReceivedFiles` | 推荐，明确指定位置 |
| 相对路径 | `.\Downloads` | 相对于程序目录 |
| 网络路径 | `\\SERVER\Share\Files` | UNC路径 |
| 用户目录 | `C:\Users\YourName\Documents\Received` | 用户文档文件夹 |

### 方法3: 默认行为

如果未配置，文件将保存到桌面：

- Windows: `C:\Users\YourName\Desktop`
- Linux: `~/Desktop`
- macOS: `~/Desktop`

## 优先级说明

当同时存在多种配置时，优先级为：

```
命令行参数 > 配置文件 > 默认桌面
```

**示例场景**：

```ini
# config.ini 中配置
file_receive_directory=D:\Files

# 但使用命令行参数
crossdesk_agent.exe --receive-dir "E:\Downloads"

# 实际使用: E:\Downloads （命令行优先）
```

## 目录创建和权限

### 自动创建

程序会自动创建指定的目录（如果不存在）：

```
[INFO] File receiver output directory set to: D:\ReceivedFiles
```

### 权限检查

确保程序对目录有写入权限：

```bash
# Windows 检查权限
icacls "D:\ReceivedFiles"

# 添加写入权限（以管理员身份）
icacls "D:\ReceivedFiles" /grant Users:(OI)(CI)F
```

### 错误处理

如果目录创建失败，会回退到当前目录：

```
[ERROR] Failed to create directory D:\ReceivedFiles: Access denied
[WARN] Falling back to current directory
```

## 实际使用示例

### 示例1: 保存到独立分区

避免占用C盘空间：

```bash
# 命令行
crossdesk_agent.exe --receive-dir "D:\CrossDeskFiles"

# 或在 config.ini
file_receive_directory=D:\CrossDeskFiles
```

### 示例2: 按日期组织文件夹

可以使用批处理脚本自动创建日期文件夹：

```batch
@echo off
set TODAY=%DATE:~0,10%
set RECEIVE_DIR=D:\ReceivedFiles\%TODAY%
mkdir "%RECEIVE_DIR%" 2>nul
crossdesk_agent.exe --receive-dir "%RECEIVE_DIR%"
```

### 示例3: 网络共享目录

保存到局域网共享文件夹：

```ini
[Settings]
file_receive_directory=\\192.168.1.100\SharedFiles\Received
```

**注意**：需要确保网络连接稳定且有访问权限。

### 示例4: 多用户环境

为不同用户配置不同接收目录：

```ini
# 用户A的配置
file_receive_directory=C:\Users\UserA\ReceivedFiles

# 用户B的配置
file_receive_directory=C:\Users\UserB\ReceivedFiles
```

## 验证配置

### 查看日志确认

运行程序后，查看日志文件 `crossdesk_agent.log`：

```
[INFO] Using configured file receive directory: D:\ReceivedFiles
[INFO] File receiver output directory set to: D:\ReceivedFiles
```

或默认情况：

```
[INFO] Using default desktop directory: C:\Users\YourName\Desktop
[INFO] File receiver output directory set to: C:\Users\YourName\Desktop
```

### 测试文件传输

1. 启动被控端
2. 从控制端发送测试文件
3. 检查指定目录中是否出现文件

## 常见问题

### Q1: 路径中的反斜杠怎么写？

**A**: Windows路径使用单反斜杠即可：

```ini
# 正确
file_receive_directory=D:\ReceivedFiles

# 也正确（双反斜杠）
file_receive_directory=D:\\ReceivedFiles

# 也正确（正斜杠，Windows兼容）
file_receive_directory=D:/ReceivedFiles
```

### Q2: 修改配置后需要重启吗？

**A**: 
- 修改 config.ini 文件后，需要重启程序
- 使用 `--receive-dir` 参数会立即生效并保存

### Q3: 可以使用环境变量吗？

**A**: 目前不支持环境变量展开，需要使用完整路径：

```ini
# 不支持
file_receive_directory=%USERPROFILE%\Documents\Received

# 请使用完整路径
file_receive_directory=C:\Users\YourName\Documents\Received
```

### Q4: 目录不存在会怎样？

**A**: 程序会自动创建目录：

```
[INFO] Creating directory: D:\ReceivedFiles
[INFO] File receiver output directory set to: D:\ReceivedFiles
```

如果创建失败（权限问题），会回退到当前目录。

### Q5: 网络路径连接断开怎么办？

**A**: 文件传输会失败，建议：
- 使用本地路径提高稳定性
- 确保网络连接稳定
- 检查日志中的错误信息

### Q6: 可以同时保存到多个目录吗？

**A**: 目前只支持一个接收目录。如需分发，可以：
- 使用文件同步工具（如 FreeFileSync）
- 编写后处理脚本复制文件

## 高级技巧

### 技巧1: 快捷方式创建

为不同接收目录创建快捷方式：

```batch
REM 桌面接收.bat
crossdesk_agent.exe --receive-dir "%USERPROFILE%\Desktop"

REM 下载接收.bat  
crossdesk_agent.exe --receive-dir "%USERPROFILE%\Downloads"

REM 文档接收.bat
crossdesk_agent.exe --receive-dir "%USERPROFILE%\Documents"
```

### 技巧2: 自动清理脚本

定期清理旧文件：

```batch
@echo off
REM 删除30天前的文件
forfiles /P "D:\ReceivedFiles" /S /M *.* /D -30 /C "cmd /c del @path"
```

### 技巧3: 文件监控

监控接收目录的新文件：

```powershell
# PowerShell 监控脚本
$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = "D:\ReceivedFiles"
$watcher.Filter = "*.*"
$watcher.EnableRaisingEvents = $true

Register-ObjectEvent $watcher "Created" -Action {
    Write-Host "New file received: $($Event.SourceEventArgs.Name)"
}
```

## 安全建议

⚠️ **重要提示**：

1. **权限控制** - 接收目录应限制访问权限
2. **病毒扫描** - 对接收文件进行实时扫描
3. **磁盘配额** - 设置目录大小限制避免磁盘占满
4. **审计日志** - 定期检查接收的文件
5. **备份策略** - 重要文件及时备份

### 推荐配置（安全）

```ini
[Settings]
file_receive_directory=D:\Quarantine\ReceivedFiles
# 建议配合杀毒软件实时扫描此目录
```

## 性能优化

### 使用SSD

接收大文件时，使用SSD可提升性能：

```ini
file_receive_directory=C:\ReceivedFiles  # C盘通常是SSD
```

### 避免网络路径

网络路径会降低传输速度：

```
本地磁盘: 100+ MB/s
网络共享: 10-50 MB/s（取决于网络速度）
```

### 磁盘空间监控

```powershell
# 检查剩余空间
Get-PSDrive D | Select-Object Used,Free
```

## 总结

✅ **三种配置方式**：命令行 > 配置文件 > 默认桌面
✅ **自动创建目录**：程序会自动创建不存在的目录
✅ **灵活配置**：支持绝对路径、相对路径、网络路径
✅ **持久化保存**：配置保存在 config.ini 中
✅ **实时生效**：命令行参数立即生效

**快速开始**：

```bash
# 临时测试
crossdesk_agent.exe --receive-dir "D:\Test"

# 永久配置
# 编辑 %APPDATA%\CrossDeskAgent\config.ini
# 添加: file_receive_directory=D:\ReceivedFiles
```

---

*最后更新: 2026年1月21日*
