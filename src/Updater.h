/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>

enum class UpdaterStatus
{
  PREPARE,
  CHECK_FOR_UPDATE,
  DOWNLOAD_BUILD,
  EXTRACT_BUILD,
  COPY_USERDATA,
  FINISHED,
  ERROR
};

class CUpdater
{
public:
  CUpdater(std::string strRootPath);
  ~CUpdater() = default;

  int Process();

  void OnError();

  inline UpdaterStatus GetStatus() { return m_status; }

private:
  int Prepare();
  int CheckForUpdate();
  int Download();
  int Extract();
  int Install();
  std::string FindAsset(const std::string& strAsset) const;

  std::string m_strRootPath;
  std::string m_strUpdatePath;
  std::string m_strExtractPath;

  std::string m_currentRevision;
  std::string m_latestRevision;
  std::string m_updateChannel;

  std::string m_strError;

  UpdaterStatus m_status = UpdaterStatus::PREPARE;
};
