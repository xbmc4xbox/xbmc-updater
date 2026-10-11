/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Thread.h"

CThread::~CThread()
{
  StopThread();
}

void CThread::Start()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_running)
    return;

  // Reap a previous run that finished without an explicit stop.
  if (m_thread.joinable())
    m_thread.join();

  m_bStop = false;
  m_running = true;
  m_thread = std::thread(
      [this]()
      {
        Process();
        m_running = false;
      });
}

void CThread::StopThread(bool wait)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_bStop = true;
  if (wait && m_thread.joinable())
    m_thread.join();
}
