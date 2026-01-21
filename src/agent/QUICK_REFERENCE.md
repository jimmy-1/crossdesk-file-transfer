# CrossDesk Agent 文件接收目录 - 快速参考

## 🚀 快速开始

### 默认使用（桌面）
```bash
crossdesk_agent.exe
```
📁 文件保存到：`%USERPROFILE%\Desktop`

---

### 自定义目录（临时）
```bash
crossdesk_agent.exe --receive-dir "D:\ReceivedFiles"
```
📁 文件保存到：`D:\ReceivedFiles`  
💾 配置自动保存到 config.ini

---

### 自定义目录（永久）
编辑 `%APPDATA%\CrossDeskAgent\config.ini`：
```ini
[Settings]
file_receive_directory=D:\ReceivedFiles
```
📁 每次启动都使用此目录

---

## 📋 支持的路径格式

| 格式 | 示例 | 说明 |
|------|------|------|
| **绝对路径** | `D:\Files` | ✅ 推荐 |
| **相对路径** | `.\Downloads` | 相对程序目录 |
| **网络路径** | `\\SERVER\Share` | UNC路径 |
| **空格路径** | `"C:\My Files"` | 需要引号 |

---

## ⚙️ 配置优先级

```
命令行参数 > 配置文件 > 默认桌面
```

---

## 🔍 验证配置

查看日志 `crossdesk_agent.log`：
```
[INFO] File receiver output directory set to: D:\ReceivedFiles
```

---

## 💡 常用命令

```bash
# 查看帮助
crossdesk_agent.exe --help

# 保存到D盘
crossdesk_agent.exe --receive-dir "D:\Files"

# 保存到下载文件夹
crossdesk_agent.exe --receive-dir "%USERPROFILE%\Downloads"

# 保存到程序目录下的Received文件夹
crossdesk_agent.exe --receive-dir ".\Received"
```

---

## 📝 配置示例

### 保存到非系统盘
```ini
file_receive_directory=D:\ReceivedFiles
```

### 保存到用户文档
```ini
file_receive_directory=C:\Users\YourName\Documents\Received
```

### 保存到网络共享
```ini
file_receive_directory=\\192.168.1.100\Share\Files
```

---

## ✅ 检查清单

- [ ] 确认目录有写入权限
- [ ] 确认磁盘空间充足
- [ ] 查看日志确认配置生效
- [ ] 测试文件传输功能
- [ ] 检查接收目录中的文件

---

## 🆘 常见问题

**Q: 配置后不生效？**  
A: 需要重启程序

**Q: 目录不存在？**  
A: 程序会自动创建

**Q: 权限不足？**  
A: 以管理员身份运行，或选择有权限的目录

**Q: 如何恢复默认？**  
A: 删除配置文件中的 `file_receive_directory` 行

---

## 📚 详细文档

- [README.md](README.md) - 完整使用说明
- [FILE_RECEIVE_CONFIG.md](FILE_RECEIVE_CONFIG.md) - 详细配置指南
- [FILE_TRANSFER_GUIDE.md](FILE_TRANSFER_GUIDE.md) - 文件传输指南

---

*更新时间: 2026年1月21日*
