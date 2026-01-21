/*
 * CrossDesk Silent Agent - 被控端程序
 * 功能: 静默运行，自动开机启动，生成并保存设备ID
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifdef _WIN32
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include "config_center.h"
#include "device_controller_factory.h"
#include "file_transfer.h"
#include "minirtc.h"
#include "path_manager.h"
#include "rd_log.h"
#include "remote_cmd_executor.h"
#include "screen_capturer_factory.h"
#include "speaker_capturer_factory.h"

using namespace crossdesk;

// 默认密码
constexpr const char* DEFAULT_PASSWORD = "admin";

// 全局变量
static PeerPtr g_peer = nullptr;
static std::unique_ptr<ScreenCapturer> g_screen_capturer = nullptr;
static std::unique_ptr<SpeakerCapturer> g_speaker_capturer = nullptr;
static std::unique_ptr<MouseController> g_mouse_controller = nullptr;
static std::unique_ptr<KeyboardController> g_keyboard_controller = nullptr;
static std::unique_ptr<ConfigCenter> g_config_center = nullptr;
static std::unique_ptr<PathManager> g_path_manager = nullptr;
static FileReceiver g_file_receiver;
static bool g_running = true;
static std::string g_device_id;
static std::string g_exe_dir;
static std::string g_file_label = "file";
static std::string g_file_feedback_label = "file_feedback";
static std::string g_control_data_label = "control_data";
static std::string g_clipboard_label = "clipboard";
static std::string g_cmd_label = "remote_cmd";
static std::string g_cmd_output_label = "cmd_output";
static std::unique_ptr<RemoteCmdExecutor> g_cmd_executor = nullptr;

// 获取可执行文件目录
std::string GetExecutableDirectory() {
#ifdef _WIN32
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  std::string exe_path(path);
  size_t pos = exe_path.find_last_of("\\/");
  return (pos != std::string::npos) ? exe_path.substr(0, pos) : "";
#else
  char path[1024];
  ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (count != -1) {
    path[count] = '\0';
    std::string exe_path(path);
    size_t pos = exe_path.find_last_of("/");
    return (pos != std::string::npos) ? exe_path.substr(0, pos) : "";
  }
  return "";
#endif
}

// 保存设备ID到文件
bool SaveDeviceIDToFile(const std::string& device_id) {
  std::string id_file = g_exe_dir + "\\device_id.txt";
  std::ofstream file(id_file, std::ios::trunc);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open device_id.txt for writing");
    return false;
  }
  
  file << "CrossDesk Agent Device ID\n";
  file << "==========================\n";
  file << "Device ID: " << device_id << "\n";
  file << "Password: " << DEFAULT_PASSWORD << "\n";
  file << "==========================\n";
  file << "Generated at: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
  file.close();
  
  LOG_INFO("Device ID saved to: {}", id_file);
  return true;
}

#ifdef _WIN32
// 检查是否以管理员权限运行
bool IsRunAsAdmin() {
  BOOL is_admin = FALSE;
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID admin_group;
  
  if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &admin_group)) {
    CheckTokenMembership(NULL, admin_group, &is_admin);
    FreeSid(admin_group);
  }
  
  return is_admin == TRUE;
}

// 以管理员权限重启程序
bool RestartAsAdmin() {
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);
  
  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpVerb = "runas";
  sei.lpFile = exe_path;
  sei.lpParameters = "--install-service";
  sei.hwnd = NULL;
  sei.nShow = SW_HIDE;
  
  if (!ShellExecuteExA(&sei)) {
    DWORD error = GetLastError();
    if (error != ERROR_CANCELLED) {
      LOG_ERROR("Failed to restart as admin: {}", error);
      return false;
    }
  }
  
  return true;
}

// 安装Windows服务
bool InstallWindowsService() {
  if (!IsRunAsAdmin()) {
    LOG_INFO("Not running as admin, requesting elevation...");
    return RestartAsAdmin();
  }
  
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!scm) {
    LOG_ERROR("Failed to open SCManager: {}", GetLastError());
    return false;
  }
  
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);
  
  // 检查服务是否已存在
  SC_HANDLE service = OpenServiceA(scm, "CrossDeskAgent", SERVICE_ALL_ACCESS);
  if (service) {
    LOG_INFO("Service already exists, attempting to start...");
    StartServiceA(service, 0, NULL);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
  }
  
  // 创建服务
  service = CreateServiceA(
      scm,
      "CrossDeskAgent",
      "CrossDesk Remote Agent",
      SERVICE_ALL_ACCESS,
      SERVICE_WIN32_OWN_PROCESS,
      SERVICE_AUTO_START,
      SERVICE_ERROR_NORMAL,
      exe_path,
      NULL, NULL, NULL, NULL, NULL);
  
  if (!service) {
    DWORD error = GetLastError();
    LOG_ERROR("Failed to create service: {}", error);
    CloseServiceHandle(scm);
    return false;
  }
  
  LOG_INFO("Service installed successfully");
  
  // 设置服务描述
  SERVICE_DESCRIPTIONA desc;
  desc.lpDescription = (LPSTR)"CrossDesk remote desktop agent service";
  ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &desc);
  
  // 启动服务
  if (!StartServiceA(service, 0, NULL)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_ALREADY_RUNNING) {
      LOG_ERROR("Failed to start service: {}", error);
    }
  } else {
    LOG_INFO("Service started successfully");
  }
  
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

// 添加到启动项（备用方案）
bool AddToStartup() {
  HKEY hKey;
  const char* keyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  
  if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
    LOG_ERROR("Failed to open registry key");
    return false;
  }
  
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);
  
  LONG result = RegSetValueExA(hKey, "CrossDeskAgent", 0, REG_SZ,
                               (const BYTE*)exe_path, strlen(exe_path) + 1);
  RegCloseKey(hKey);
  
  if (result == ERROR_SUCCESS) {
    LOG_INFO("Added to startup registry");
    return true;
  } else {
    LOG_ERROR("Failed to add to startup registry: {}", result);
    return false;
  }
}

// 检查是否已在启动项
bool IsInStartup() {
  HKEY hKey;
  const char* keyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  
  if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
    return false;
  }
  
  char value[MAX_PATH];
  DWORD size = sizeof(value);
  LONG result = RegQueryValueExA(hKey, "CrossDeskAgent", NULL, NULL,
                                 (BYTE*)value, &size);
  RegCloseKey(hKey);
  
  return result == ERROR_SUCCESS;
}
#endif

// RTC回调函数
void OnSignalStatus(SignalStatus status, void* user_data) {
  LOG_INFO("Signal status: {}", static_cast<int>(status));
}

void OnConnectionStatus(const char* remote_id, ConnectionStatus status,
                       void* user_data) {
  LOG_INFO("Connection status with {}: {}", remote_id, static_cast<int>(status));
}

void OnReceiveDataBuffer(const char* label, const char* data, size_t size,
                        void* user_data) {
  if (!label || !data || size == 0) {
    return;
  }

  std::string channel_label(label);
  
  // 处理文件接收
  if (channel_label == g_file_label) {
    g_file_receiver.SetOnSendAck([](const FileTransferAck& ack) -> int {
      if (g_peer) {
        return SendReliableDataFrame(g_peer, 
                                     reinterpret_cast<const char*>(&ack),
                                     sizeof(FileTransferAck), 
                                     g_file_feedback_label.c_str());
      }
      return -1;
    });
    
    g_file_receiver.OnData(data, size);
    LOG_INFO("Processing file transfer data, size: {}", size);
    return;
  }
  
  // 处理剪贴板数据
  if (channel_label == g_clipboard_label) {
    // TODO: 实现剪贴板设置功能
    LOG_INFO("Received clipboard data, size: {}", size);
    return;
  }
  
  // 处理控制数据（鼠标、键盘等）
  if (channel_label == g_control_data_label) {
    // TODO: 解析并执行控制指令
    LOG_INFO("Received control data, size: {}", size);
    return;
  }
  
  // 处理远程CMD命令
  if (channel_label == g_cmd_label) {
    std::string command(data, size);
    LOG_INFO("Received remote CMD command: {}", command);
    
    if (!g_cmd_executor) {
      g_cmd_executor = std::make_unique<RemoteCmdExecutor>();
    }
    
    // 检查是否有管理员权限
    bool is_admin = RemoteCmdExecutor::IsRunningAsAdmin();
    LOG_INFO("Executing command with {} privileges", is_admin ? "admin" : "normal");
    
    // 异步执行命令并发送输出
    g_cmd_executor->ExecuteAsync(command, 
      [](const std::string& output, bool is_error) {
        if (g_peer) {
          // 构建输出消息：[ERROR] 或 [OUTPUT]
          std::string msg = (is_error ? "[ERROR] " : "[OUTPUT] ") + output;
          SendReliableDataFrame(g_peer, msg.c_str(), msg.length(), 
                               g_cmd_output_label.c_str());
          LOG_INFO("Sent CMD {}: {} bytes", is_error ? "error" : "output", output.length());
        }
      }, 
      true  // 要求管理员权限
    );
    return;
  }
  
  LOG_INFO("Received data on channel: {}, size: {}", label, size);
}

void OnReceiveVideoFrame(const char* label, const char* buffer, size_t size,
                        int width, int height, FrameFormat format,
                        void* user_data) {
  // 被控端不需要接收视频
}

void OnReceiveAudioBuffer(const char* label, const char* buffer, size_t size,
                         int sample_rate, int channels, void* user_data) {
  // 被控端不需要接收音频
}

void NetStatusReport(const XNetTrafficStats* stats, void* user_data) {
  // 可选：记录网络状态
}

// 初始化屏幕捕获
bool InitScreenCapturer(PeerPtr peer, int fps) {
  g_screen_capturer = CreateScreenCapturer();
  if (!g_screen_capturer) {
    LOG_ERROR("Failed to create screen capturer");
    return false;
  }
  
  auto callback = [peer](const char* label, const char* buffer, size_t size,
                        int width, int height, FrameFormat format) {
    if (peer && buffer && size > 0) {
      SendVideoFrame(peer, label, buffer, size, width, height, format);
    }
  };
  
  if (g_screen_capturer->Init(callback, fps) != 0) {
    LOG_ERROR("Failed to initialize screen capturer");
    return false;
  }
  
  g_screen_capturer->Start(0); // 捕获主显示器
  LOG_INFO("Screen capturer started");
  return true;
}

// 初始化音频捕获
bool InitSpeakerCapturer(PeerPtr peer) {
  g_speaker_capturer = CreateSpeakerCapturer();
  if (!g_speaker_capturer) {
    LOG_ERROR("Failed to create speaker capturer");
    return false;
  }
  
  auto callback = [peer](const char* label, const char* buffer, size_t size,
                        int sample_rate, int channels) {
    if (peer && buffer && size > 0) {
      SendAudioFrame(peer, label, buffer, size, sample_rate, channels);
    }
  };
  
  if (g_speaker_capturer->Init(callback) != 0) {
    LOG_ERROR("Failed to initialize speaker capturer");
    return false;
  }
  
  g_speaker_capturer->Start();
  LOG_INFO("Speaker capturer started");
  return true;
}

// 初始化设备控制器
bool InitDeviceControllers() {
  g_mouse_controller = CreateMouseController();
  if (!g_mouse_controller) {
    LOG_ERROR("Failed to create mouse controller");
    return false;
  }
  
  g_keyboard_controller = CreateKeyboardController();
  if (!g_keyboard_controller) {
    LOG_ERROR("Failed to create keyboard controller");
    return false;
  }
  
  LOG_INFO("Device controllers initialized");
  return true;
}

// 初始化RTC连接
bool InitPeer() {
  g_path_manager = std::make_unique<PathManager>("CrossDeskAgent");
  if (!g_path_manager) {
    LOG_ERROR("Failed to create path manager");
    return false;
  }
  
  std::string cert_path = (g_path_manager->GetCertPath() / "crossdesk.cn_root.crt").string();
  std::string cache_path = g_path_manager->GetCachePath().string();
  std::string log_path = g_path_manager->GetLogPath().string();
  
  g_config_center = std::make_unique<ConfigCenter>(cache_path + "/config.ini", cert_path);
  if (!g_config_center) {
    LOG_ERROR("Failed to create config center");
    return false;
  }
  
  // 配置参数
  Params params = {};
  
  // 使用默认服务器
  std::string server_host = "api.crossdesk.cn";
  int server_port = 9099;
  int coturn_port = 3478;
  
  strncpy((char*)params.signal_server_ip, server_host.c_str(), sizeof(params.signal_server_ip) - 1);
  params.signal_server_port = server_port;
  
  strncpy((char*)params.stun_server_ip, server_host.c_str(), sizeof(params.stun_server_ip) - 1);
  params.stun_server_port = coturn_port;
  
  strncpy((char*)params.turn_server_ip, server_host.c_str(), sizeof(params.turn_server_ip) - 1);
  params.turn_server_port = coturn_port;
  
  strncpy((char*)params.turn_server_username, "crossdesk", sizeof(params.turn_server_username) - 1);
  strncpy((char*)params.turn_server_password, "crossdeskpw", sizeof(params.turn_server_password) - 1);
  
  strncpy(params.log_path, log_path.c_str(), sizeof(params.log_path) - 1);
  
  // 配置编码参数
  params.hardware_acceleration = g_config_center->IsHardwareVideoCodec();
  params.av1_encoding = false; // 使用H264
  params.enable_turn = true;
  params.enable_srtp = g_config_center->IsEnableSrtp();
  params.video_quality = static_cast<VideoQuality>(g_config_center->GetVideoQuality());
  
  // 设置user_id为空，让服务器分配ID
  char user_id_with_password[32] = {0};
  snprintf(user_id_with_password, sizeof(user_id_with_password), "@%s", DEFAULT_PASSWORD);
  params.user_id = user_id_with_password;
  
  // 设置回调
  params.on_signal_status = OnSignalStatus;
  params.on_connection_status = OnConnectionStatus;
  params.on_receive_video_frame = OnReceiveVideoFrame;
  params.on_receive_audio_buffer = OnReceiveAudioBuffer;
  params.on_receive_data_buffer = OnReceiveDataBuffer;
  params.net_status_report = NetStatusReport;
  params.user_data = nullptr;
  
  // 创建Peer
  g_peer = CreatePeer(&params);
  if (!g_peer) {
    LOG_ERROR("Failed to create peer");
    return false;
  }
  
  // 初始化Peer
  if (Init(g_peer) != 0) {
    LOG_ERROR("Failed to initialize peer");
    return false;
  }
  
  // 获取设备ID
  g_device_id = GetLocalPeerId(g_peer);
  LOG_INFO("Device ID: {}", g_device_id);
  
  // 保存设备ID到文件
  SaveDeviceIDToFile(g_device_id);
  
  // 设置文件接收目录
  std::filesystem::path receive_path;
  std::string config_dir = g_config_center->GetFileReceiveDirectory();
  
  if (!config_dir.empty()) {
    // 优先使用配置文件中的目录
    receive_path = config_dir;
    LOG_INFO("Using configured file receive directory: {}", receive_path.string());
  } else {
    // 默认使用桌面
    receive_path = FileReceiver::GetDefaultDesktopPath();
    LOG_INFO("Using default desktop directory: {}", receive_path.string());
  }
  
  // 确保目录存在
  std::error_code ec;
  if (!receive_path.empty()) {
    std::filesystem::create_directories(receive_path, ec);
    if (!ec) {
      g_file_receiver.SetOutputDirectory(receive_path);
      LOG_INFO("File receiver output directory set to: {}", receive_path.string());
    } else {
      LOG_ERROR("Failed to create directory {}: {}", receive_path.string(), ec.message());
      LOG_WARN("Falling back to current directory");
    }
  } else {
    LOG_WARN("Failed to get file receive path, files will be saved to current directory");
  }
  
  // 初始化设备控制器
  if (!InitDeviceControllers()) {
    LOG_WARN("Failed to initialize device controllers");
  }
  
  // 添加视频流
  AddVideoStream(g_peer, "screen_0");
  
  // 添加音频流
  AddAudioStream(g_peer, "audio_0");
  
  // 添加数据通道
  AddDataStream(g_peer, "data", false);
  AddDataStream(g_peer, "control_data", true);
  AddDataStream(g_peer, "file", true);
  AddDataStream(g_peer, "file_feedback", true);
  AddDataStream(g_peer, "clipboard", true);
  AddDataStream(g_peer, g_cmd_label.c_str(), true);  // CMD命令通道
  AddDataStream(g_peer, g_cmd_output_label.c_str(), true);  // CMD输出通道
  
  LOG_INFO("Remote CMD channels initialized");
  
  // 初始化屏幕捕获
  int fps = (g_config_center->GetVideoFrameRate() == ConfigCenter::VIDEO_FRAME_RATE::FPS_60) ? 60 : 30;
  if (!InitScreenCapturer(g_peer, fps)) {
    LOG_ERROR("Failed to initialize screen capturer");
    return false;
  }
  
  // 初始化音频捕获
  if (!InitSpeakerCapturer(g_peer)) {
    LOG_WARN("Failed to initialize speaker capturer, continuing without audio");
  }
  
  LOG_INFO("Peer initialized successfully");
  return true;
}

// 清理资源
void Cleanup() {
  LOG_INFO("Cleaning up resources...");
  
  if (g_screen_capturer) {
    g_screen_capturer->Stop();
    g_screen_capturer.reset();
  }
  
  if (g_speaker_capturer) {
    g_speaker_capturer->Stop();
    g_speaker_capturer.reset();
  }
  
  g_mouse_controller.reset();
  g_keyboard_controller.reset();
  
  if (g_cmd_executor) {
    g_cmd_executor->Cancel();
    g_cmd_executor.reset();
  }
  
  if (g_peer) {
    Destroy(g_peer);
    g_peer = nullptr;
  }
  
  g_config_center.reset();
  g_path_manager.reset();
}

#ifdef _WIN32
// Windows控制台处理函数
BOOL WINAPI ConsoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
    LOG_INFO("Received shutdown signal");
    g_running = false;
    return TRUE;
  }
  return FALSE;
}
#endif

int main(int argc, char* argv[]) {
  // 获取可执行文件目录
  g_exe_dir = GetExecutableDirectory();
  
  // 初始化日志
  std::string log_file = g_exe_dir + "\\crossdesk_agent.log";
  rd_log::init(log_file.c_str());
  LOG_INFO("CrossDesk Agent Starting...");
  LOG_INFO("Executable directory: {}", g_exe_dir);
  
  // 检查管理员权限
  bool is_admin = RemoteCmdExecutor::IsRunningAsAdmin();
  LOG_INFO("Running with {} privileges", is_admin ? "ADMINISTRATOR" : "NORMAL USER");
  if (!is_admin) {
    LOG_WARN("Remote CMD commands will require administrator privileges");
    LOG_WARN("Consider running as administrator for full functionality");
  }
  
#ifdef _WIN32
  // 处理命令行参数
  bool install_service = false;
  std::string custom_receive_dir;
  
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--install-service") == 0) {
      install_service = true;
    } else if (strcmp(argv[i], "--receive-dir") == 0 && i + 1 < argc) {
      custom_receive_dir = argv[i + 1];
      i++; // 跳过下一个参数
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      LOG_INFO("Usage: crossdesk_agent.exe [options]");
      LOG_INFO("Options:");
      LOG_INFO("  --install-service     Install as Windows service");
      LOG_INFO("  --receive-dir <path>  Set file receive directory (default: Desktop)");
      LOG_INFO("  --help, -h            Show this help message");
      return 0;
    }
  }
  
  if (install_service) {
    LOG_INFO("Installing service...");
    if (InstallWindowsService()) {
      LOG_INFO("Service installation completed");
      return 0;
    } else {
      LOG_ERROR("Service installation failed, adding to startup registry instead");
      AddToStartup();
      return 1;
    }
  }
  
  // 检查是否在启动项中
  if (!IsInStartup()) {
    LOG_INFO("Not in startup, attempting to add...");
    if (AddToStartup()) {
      LOG_INFO("Successfully added to startup");
    } else {
      LOG_WARN("Failed to add to startup");
    }
  } else {
    LOG_INFO("Already in startup");
  }
  
  // 设置控制台处理函数
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);
  
  // 应用命令行指定的接收目录
  if (!custom_receive_dir.empty()) {
    LOG_INFO("Setting custom receive directory from command line: {}", custom_receive_dir);
    // 临时创建 PathManager 和 ConfigCenter 来保存配置
    auto temp_path_manager = std::make_unique<PathManager>("CrossDeskAgent");
    if (temp_path_manager) {
      std::string cache_path = temp_path_manager->GetCachePath().string();
      std::string cert_path = (temp_path_manager->GetCertPath() / "crossdesk.cn_root.crt").string();
      ConfigCenter temp_config(cache_path + "/config.ini", cert_path);
      temp_config.SetFileReceiveDirectory(custom_receive_dir);
      LOG_INFO("Custom receive directory saved to config");
    }
  }
#endif
  
  // 初始化RTC
  if (!InitPeer()) {
    LOG_ERROR("Failed to initialize peer, exiting");
    Cleanup();
    return 1;
  }
  
  LOG_INFO("Agent is running with Device ID: {}", g_device_id);
  LOG_INFO("Default password: {}", DEFAULT_PASSWORD);
  
  // 主循环
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  
  LOG_INFO("Agent shutting down...");
  Cleanup();
  LOG_INFO("Agent stopped");
  
  return 0;
}
