# CrossDesk Agent - 静默被控端

## 简介

CrossDesk Agent 是 CrossDesk 项目的静默被控端程序，专为无人值守的远程控制场景设计。

## 特性

- ✅ **静默运行** - 无GUI界面，后台运行
- ✅ **开机自启** - 自动添加到Windows启动项或注册为系统服务
- ✅ **自动连接** - 启动后自动连接到CrossDesk服务器
- ✅ **ID保存** - 自动保存设备ID到文本文件
- ✅ **默认密码** - 预设密码为 `admin`
- ✅ **屏幕共享** - 自动捕获并传输屏幕画面
- ✅ **音频传输** - 支持系统音频传输（可选）
- ✅ **文件传输** - 支持双向文件传输，接收的文件保存到桌面
- ✅ **设备控制** - 支持远程鼠标和键盘控制

## 编译

### 前置条件

- Windows 10/11 (64位)
- xmake 构建工具
- Visual Studio 2019 或更高版本

### 编译步骤

```bash
# 1. 克隆仓库（如果尚未克隆）
git clone https://github.com/kunkundi/crossdesk.git
cd crossdesk

# 2. 更新子模块
git submodule update --init --recursive

# 3. 编译Agent
xmake f -m release
xmake build crossdesk_agent

# 编译输出位置
# build/windows/x64/release/crossdesk_agent.exe
```

## 使用方法

### 方法一：双击运行（推荐）

1. 直接运行 `crossdesk_agent.exe`
2. 程序会自动：
   - 添加到Windows启动项
   - 连接到CrossDesk服务器
   - 获取设备ID
   - 保存ID到程序目录下的 `device_id.txt` 文件
   - 文件默认保存到桌面

### 方法二：指定文件接收目录

使用命令行参数指定文件保存位置：

```bash
# 指定接收目录
crossdesk_agent.exe --receive-dir "D:\ReceivedFiles"

# 或使用相对路径
crossdesk_agent.exe --receive-dir ".\Downloads"

# 查看帮助
crossdesk_agent.exe --help
```

### 方法三：通过配置文件指定

编辑配置文件 `%APPDATA%\CrossDeskAgent\config.ini`：

```ini
[Settings]
file_receive_directory=D:\ReceivedFiles
```

**优先级**: 命令行参数 > 配置文件 > 默认桌面

### 方法四：安装为Windows服务

以管理员权限运行命令行：

```bash
# 安装服务
crossdesk_agent.exe --install-service

# 卸载服务（需要单独实现）
# sc delete CrossDeskAgent
```

### 查看设备ID

运行后，在程序所在目录会生成 `device_id.txt` 文件：

```
CrossDesk Agent Device ID
==========================
Device ID: 123456789
Password: admin
==========================
Generated at: 1234567890
```

## 连接到被控端

### 使用PC客户端

1. 运行 CrossDesk 主程序
2. 在"对端ID"输入框输入设备ID
3. 输入密码 `admin`
4. 点击连接按钮

**文件传输**：
- 连接成功后，可以直接拖拽文件到控制窗口进行传输
- 被控端接收的文件会自动保存到桌面（Desktop）
- 支持大文件传输，有进度显示
- 支持多文件队列传输

### 使用Web客户端

1. 访问 CrossDesk Web 客户端
2. 输入设备ID和密码 `admin`
3. 建立连接

**注意**: Web客户端文件传输功能可能受浏览器限制

## 配置说明

### 修改默认密码

编辑 `crossdesk_agent.cpp` 文件：

```cpp
// 修改此处的密码
constexpr const char* DEFAULT_PASSWO
- 接收文件: 保存到桌面（`%USERPROFILE%\Desktop`）RD = "your_password";
```

重新编译即可。

### 配置文件位置

- Windows: `%APPDATA%\CrossDeskAgent\config.ini`
- 日志文件: 程序目录下的 `crossdesk_agent.log`

### 配置项

可以通过编辑 `config.ini` 修改以下配置：

```ini
[Settings]
language=0                    # 0=中文, 1=英文
video_quality=1               # 0=低, 1=中, 2=高
video_frame_rate=1            # 0=30fps, 1=60fps
video_encode_format=0         # 0=H264, 1=AV1
hardware_video_codec=false    # 是否使用硬件编解码
enable_turn=true              # 是否启用TURN中继
enable_srtp=false             # 是否启用SRTP加密
file_receive_directory=       # 文件接收目录（空则使用桌面）
```

## 系统要求

| 系统 | 最低版本 |
|------|---------|
| Windows | Windows 10 及以上 (64位) |

## 注意事项

⚠️ **安全提示**：

1. 默认密码为 `admin`，建议在生产环境中修改
2. 确保只在受信任的网络环境中运行
3. 定期检查 `device_id.txt` 文件的安全性
4. 建议为不同的设备设置不同的密码

⚠️ **权限要求**：

- 首次运行会请求添加到启动项的权限
- 安装为服务需要管理员权限
- 屏幕捕获可能需要相应的系统权限

## 卸载

### 移除启动项

1. 按 `Win + R`，输入 `shell:startup`
2. 删除 CrossDeskAgent 快捷方式

或使用注册表：

```
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
删除 CrossDeskAgent 键值
```

### 卸载服务

以管理员权限运行：

```bash
sc stop CrossDeskAgent
sc delete CrossDeskAgent
```

## 故障排查

### 问题：程序启动后无反应

- 检查 `crossdesk_agent.log` 日志文件
- 确认网络连接正常
- 检查防火墙设置

### 问题：无法连接

- 确认设备ID正确
- 检查密码是否为 `admin`
- 查看日志文件中的错误信息
- 确认服务器地址可达

### 问题：文件保存位置找不到

- 检查配置文件中的 `file_receive_directory` 设置
- 查看日志中显示的实际保存路径：
  ```
  [INFO] File receiver output directory set to: D:\ReceivedFiles
  ```
- 确认目录有写入权限
- 检查磁盘空间是否充足

### 问题：屏幕共享不工作

- 检查屏幕捕获权限
- Windows 10/11 可能需要在"隐私设置"中允许屏幕录制
- 检查日志中是否有屏幕捕获相关错误

### 查看日志

日志文件位置：
- 程序目录: `crossdesk_agent.log`
- AppData目录: `%APPDATA%\CrossDeskAgent\logs\`

## 技术支持

- 项目主页: https://github.com/kunkundi/crossdesk
- 问题反馈: https://github.com/kunkundi/crossdesk/issues

## 许可证

LGPL-3.0 License

Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
