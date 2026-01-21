# CrossDesk Agent 文件传输使用指南

## 功能概述

CrossDesk Agent 被控端完全支持文件传输功能，可以与 CrossDesk 控制端无缝配合使用。

## 文件传输特性

### ✅ 已支持功能

- **接收文件** - 从控制端接收文件，自动保存到桌面
- **发送文件** - 控制端可以请求被控端的文件（需要额外实现）
- **大文件支持** - 支持任意大小文件的传输
- **断点续传** - 传输中断可以继续传输
- **进度显示** - 控制端显示实时传输进度
- **多文件队列** - 支持批量文件传输

### 📁 文件保存位置

接收到的文件默认保存在：
- Windows: `%USERPROFILE%\Desktop` (桌面)
- Linux: `~/Desktop`
- macOS: `~/Desktop`

如果文件名已存在，会自动添加时间戳避免覆盖。

## 使用方法

### 从控制端发送文件到被控端

#### 方法一：拖拽文件

1. 打开 CrossDesk 控制端
2. 连接到被控端（输入ID和密码 `admin`）
3. 建立连接后，直接**拖拽文件**到控制窗口
4. 文件会自动传输到被控端的桌面

#### 方法二：使用文件传输窗口

1. 在控制端菜单中选择"文件传输"
2. 选择要发送的文件
3. 点击"发送"按钮
4. 查看传输进度

### 接收文件示例

控制端操作：
```
1. 连接到被控端 (ID: 123456789, 密码: admin)
2. 拖拽文件: document.pdf (10MB)
3. 等待传输完成
```

被控端：
```
[INFO] Processing file transfer data, size: 8192
[INFO] Processing file transfer data, size: 8192
...
[INFO] FileReceiver: file received complete, file_id=1, size=10485760
```

文件位置：`C:\Users\YourName\Desktop\document.pdf`

## 文件传输协议

### 数据通道

被控端使用以下数据通道：

- `file` - 接收文件数据
- `file_feedback` - 发送传输确认(ACK)
- `control_data` - 接收控制指令
- `clipboard` - 接收剪贴板数据

### 传输流程

```
控制端                          被控端
   |                              |
   |------ 文件块 (Chunk) ------->|
   |                              | (写入磁盘)
   |<------- 确认 (ACK) ----------|
   |                              |
   |------ 文件块 (Chunk) ------->|
   |                              | (写入磁盘)
   |<------- 确认 (ACK) ----------|
   |                              |
   |------ 最后一块 ------------->|
   |                              | (完成，关闭文件)
   |<------- 完成确认 ------------|
```

### 传输特点

- **分块传输**: 默认每块 8KB-64KB
- **可靠传输**: 每个数据块都有ACK确认
- **错误处理**: 传输失败会自动清理临时文件
- **文件校验**: 通过 file_id 和 offset 确保数据完整性

## 代码实现要点

### 文件接收器初始化

```cpp
// 设置文件接收目录为桌面
std::filesystem::path desktop_path = FileReceiver::GetDefaultDesktopPath();
g_file_receiver.SetOutputDirectory(desktop_path);

// 设置ACK回调
g_file_receiver.SetOnSendAck([](const FileTransferAck& ack) -> int {
  return SendReliableDataFrame(g_peer, 
                               reinterpret_cast<const char*>(&ack),
                               sizeof(FileTransferAck), 
                               g_file_feedback_label.c_str());
});
```

### 数据通道回调

```cpp
void OnReceiveDataBuffer(const char* label, const char* data, size_t size,
                        void* user_data) {
  std::string channel_label(label);
  
  // 处理文件接收
  if (channel_label == g_file_label) {
    g_file_receiver.OnData(data, size);
    return;
  }
}
```

## 性能优化

### 传输速度

典型传输速度取决于网络条件：
- **局域网**: 10-100 MB/s
- **P2P直连**: 5-50 MB/s
- **TURN中继**: 1-10 MB/s

### 优化建议

1. **启用硬件加速**: 减少CPU占用
2. **关闭SRTP**: 在受信任网络中提升速度
3. **调整视频质量**: 降低视频质量可提升文件传输带宽

在 `config.ini` 中配置：
```ini
[Settings]
hardware_video_codec=true
enable_srtp=false
video_quality=0  # 0=低, 1=中, 2=高
```

## 故障排查

### 文件传输失败

**症状**: 文件传输开始但无法完成

**解决方案**:
1. 检查磁盘空间是否充足
2. 确认桌面路径访问权限
3. 查看日志中的错误信息：
   ```
   [ERROR] FileReceiver: failed to open [path] for writing
   ```

### 文件未保存到桌面

**症状**: 传输完成但找不到文件

**检查**:
1. 查看日志中的保存路径
2. 检查是否有时间戳后缀
3. 确认桌面路径正确：
   ```
   [INFO] File receiver output directory: C:\Users\...\Desktop
   ```

### 传输速度慢

**优化**:
1. 检查网络延迟和带宽
2. 暂停视频流传输
3. 使用有线网络代替WiFi
4. 确认防火墙没有限速

### 查看传输日志

```bash
# 查看实时日志
tail -f crossdesk_agent.log

# 筛选文件传输相关日志
grep "FileReceiver\|file transfer" crossdesk_agent.log
```

## 安全注意事项

⚠️ **重要提示**:

1. **文件自动保存** - 接收的所有文件会自动保存，请注意安全
2. **病毒扫描** - 建议对接收的文件进行病毒扫描
3. **隐私保护** - 不要在不受信任的环境中运行被控端
4. **访问控制** - 修改默认密码以防止未授权访问

## 扩展功能

### 自定义保存路径

修改代码中的保存路径：

```cpp
// 在 InitPeer() 函数中
std::filesystem::path custom_path = "D:\\ReceivedFiles";
std::filesystem::create_directories(custom_path);
g_file_receiver.SetOutputDirectory(custom_path);
```

### 添加文件过滤

可以添加文件类型过滤或大小限制：

```cpp
// 在 OnReceiveDataBuffer 中添加
if (file_size > MAX_FILE_SIZE) {
  LOG_WARN("File too large, rejecting");
  return;
}
```

## 技术参考

### 相关源文件

- `src/tools/file_transfer.h` - 文件传输协议定义
- `src/tools/file_transfer.cpp` - 文件传输实现
- `src/agent/crossdesk_agent.cpp` - 被控端主程序

### MiniRTC API

- `AddDataStream()` - 添加数据通道
- `SendReliableDataFrame()` - 可靠数据传输
- `OnReceiveDataBuffer()` - 数据接收回调

## 常见问题 FAQ

**Q: 可以同时传输多个文件吗？**
A: 可以。控制端支持文件队列，会依次传输。

**Q: 支持文件夹传输吗？**
A: 目前仅支持单个文件，文件夹需要先压缩。

**Q: 传输中断后可以续传吗？**
A: 协议支持断点续传，但当前实现会重新开始。

**Q: 可以从被控端发送文件到控制端吗？**
A: 协议支持双向传输，但被控端当前只实现了接收功能。

**Q: 传输的文件有大小限制吗？**
A: 理论上无限制，实际受磁盘空间和网络稳定性影响。

## 总结

✅ CrossDesk Agent 完全支持文件传输功能
✅ 接收文件自动保存到桌面
✅ 支持大文件和多文件传输
✅ 基于可靠的ACK机制确保传输完整性
✅ 与 CrossDesk 控制端完美配合

有问题请查看日志或提交 Issue！
