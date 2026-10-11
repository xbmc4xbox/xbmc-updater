/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/IUpdater.h"
#include "threads/Thread.h"

class CUpdater : public CThread
{
public:
  CUpdater(const std::string& strRootPath);
  ~CUpdater() override;

  void RegisterListener(IUpdater* listener);

protected:
  void Process() override;

private:
  bool Initialize();
  bool Prepare();
  bool CheckForUpdate();
  bool Download();
  bool Extract();
  bool Install();

  std::string FindAsset(const std::string& strAsset, const std::string& strTag) const;

  std::string m_strRootPath; /* E:\Xodi\ */
  std::string m_strExtractPath; /* E:\Xodi_NEW\ */

  std::string m_updateChannel; /* stable / nightly */

  // Used by stable channel
  std::string m_version; // 4.0.0
  std::string m_upstreamVersion; // 5.0.0

  // Used by nightly channel
  std::string m_revision; // 8cfcde4
  std::string m_upstreamRevision; // 71162ed

  bool m_performMajorUpgrade =
      false; // if false, updater wont do major upgrades (ex. 4.x.x -> 5.x.x)

  IUpdater* m_listener;

  UpdaterState m_state = UpdaterState::NONE;
};
