/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

class CThread
{
public:
  CThread() = default;
  virtual ~CThread();

  CThread(const CThread&) = delete;
  CThread& operator=(const CThread&) = delete;

  void Start();
  void Create() { Start(); }
  void StopThread(bool wait = true);
  bool IsRunning() const { return m_running.load(); }

protected:
  virtual void Process() = 0;
  std::atomic<bool> m_bStop{false};

private:
  std::mutex m_mutex;
  std::thread m_thread;
  std::atomic<bool> m_running{false};
};
