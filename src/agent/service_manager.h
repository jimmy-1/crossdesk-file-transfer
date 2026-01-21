/*
 * Windows Service Manager
 * 用于管理Windows服务的安装、卸载、启动、停止
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SERVICE_MANAGER_H_
#define _SERVICE_MANAGER_H_

#include <string>

#ifdef _WIN32

namespace crossdesk {

class ServiceManager {
 public:
  ServiceManager(const std::string& service_name,
                const std::string& display_name,
                const std::string& description);
  ~ServiceManager();

  // 安装服务
  bool Install(const std::string& exe_path);
  
  // 卸载服务
  bool Uninstall();
  
  // 启动服务
  bool Start();
  
  // 停止服务
  bool Stop();
  
  // 检查服务是否已安装
  bool IsInstalled();
  
  // 检查服务是否正在运行
  bool IsRunning();
  
  // 设置服务为自动启动
  bool SetAutoStart();

 private:
  std::string service_name_;
  std::string display_name_;
  std::string description_;
};

}  // namespace crossdesk

#endif  // _WIN32

#endif  // _SERVICE_MANAGER_H_
