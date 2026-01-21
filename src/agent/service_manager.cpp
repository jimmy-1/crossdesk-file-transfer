/*
 * Windows Service Manager Implementation
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#include "service_manager.h"

#ifdef _WIN32

#include <windows.h>
#include "rd_log.h"

namespace crossdesk {

ServiceManager::ServiceManager(const std::string& service_name,
                              const std::string& display_name,
                              const std::string& description)
    : service_name_(service_name),
      display_name_(display_name),
      description_(description) {}

ServiceManager::~ServiceManager() {}

bool ServiceManager::Install(const std::string& exe_path) {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
  if (!scm) {
    LOG_ERROR("Failed to open Service Control Manager: {}", GetLastError());
    return false;
  }

  SC_HANDLE service = CreateServiceA(
      scm,
      service_name_.c_str(),
      display_name_.c_str(),
      SERVICE_ALL_ACCESS,
      SERVICE_WIN32_OWN_PROCESS,
      SERVICE_AUTO_START,
      SERVICE_ERROR_NORMAL,
      exe_path.c_str(),
      NULL, NULL, NULL, NULL, NULL);

  if (!service) {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_EXISTS) {
      LOG_INFO("Service already exists");
      CloseServiceHandle(scm);
      return true;
    }
    LOG_ERROR("Failed to create service: {}", error);
    CloseServiceHandle(scm);
    return false;
  }

  // 设置服务描述
  SERVICE_DESCRIPTIONA desc;
  desc.lpDescription = (LPSTR)description_.c_str();
  ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &desc);

  LOG_INFO("Service installed successfully");
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

bool ServiceManager::Uninstall() {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
  if (!scm) {
    LOG_ERROR("Failed to open Service Control Manager: {}", GetLastError());
    return false;
  }

  SC_HANDLE service = OpenServiceA(scm, service_name_.c_str(), SERVICE_STOP | DELETE);
  if (!service) {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
      LOG_INFO("Service does not exist");
      CloseServiceHandle(scm);
      return true;
    }
    LOG_ERROR("Failed to open service: {}", error);
    CloseServiceHandle(scm);
    return false;
  }

  // 先停止服务
  SERVICE_STATUS status;
  ControlService(service, SERVICE_CONTROL_STOP, &status);

  // 删除服务
  if (!DeleteService(service)) {
    DWORD error = GetLastError();
    LOG_ERROR("Failed to delete service: {}", error);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
  }

  LOG_INFO("Service uninstalled successfully");
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

bool ServiceManager::Start() {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
  if (!scm) {
    LOG_ERROR("Failed to open Service Control Manager: {}", GetLastError());
    return false;
  }

  SC_HANDLE service = OpenServiceA(scm, service_name_.c_str(), SERVICE_START);
  if (!service) {
    LOG_ERROR("Failed to open service: {}", GetLastError());
    CloseServiceHandle(scm);
    return false;
  }

  if (!StartServiceA(service, 0, NULL)) {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_ALREADY_RUNNING) {
      LOG_INFO("Service is already running");
      CloseServiceHandle(service);
      CloseServiceHandle(scm);
      return true;
    }
    LOG_ERROR("Failed to start service: {}", error);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
  }

  LOG_INFO("Service started successfully");
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

bool ServiceManager::Stop() {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
  if (!scm) {
    LOG_ERROR("Failed to open Service Control Manager: {}", GetLastError());
    return false;
  }

  SC_HANDLE service = OpenServiceA(scm, service_name_.c_str(), SERVICE_STOP);
  if (!service) {
    LOG_ERROR("Failed to open service: {}", GetLastError());
    CloseServiceHandle(scm);
    return false;
  }

  SERVICE_STATUS status;
  if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_NOT_ACTIVE) {
      LOG_INFO("Service is not running");
      CloseServiceHandle(service);
      CloseServiceHandle(scm);
      return true;
    }
    LOG_ERROR("Failed to stop service: {}", error);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
  }

  LOG_INFO("Service stopped successfully");
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

bool ServiceManager::IsInstalled() {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
  if (!scm) {
    return false;
  }

  SC_HANDLE service = OpenServiceA(scm, service_name_.c_str(), SERVICE_QUERY_STATUS);
  if (!service) {
    CloseServiceHandle(scm);
    return false;
  }

  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

bool ServiceManager::IsRunning() {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
  if (!scm) {
    return false;
  }

  SC_HANDLE service = OpenServiceA(scm, service_name_.c_str(), SERVICE_QUERY_STATUS);
  if (!service) {
    CloseServiceHandle(scm);
    return false;
  }

  SERVICE_STATUS status;
  if (!QueryServiceStatus(service, &status)) {
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
  }

  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return status.dwCurrentState == SERVICE_RUNNING;
}

bool ServiceManager::SetAutoStart() {
  SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
  if (!scm) {
    LOG_ERROR("Failed to open Service Control Manager: {}", GetLastError());
    return false;
  }

  SC_HANDLE service = OpenServiceA(scm, service_name_.c_str(), SERVICE_CHANGE_CONFIG);
  if (!service) {
    LOG_ERROR("Failed to open service: {}", GetLastError());
    CloseServiceHandle(scm);
    return false;
  }

  if (!ChangeServiceConfigA(service, SERVICE_NO_CHANGE, SERVICE_AUTO_START,
                           SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL,
                           NULL, NULL)) {
    LOG_ERROR("Failed to set auto start: {}", GetLastError());
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
  }

  LOG_INFO("Service set to auto start");
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
}

}  // namespace crossdesk

#endif  // _WIN32
