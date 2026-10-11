/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

enum class UpdaterState : int
{
  NONE = 0,
  INITIALIZE,
  PREPARATION,
  CHECKING_FOR_UPDATE,
  DOWNLOADING_UPDATE,
  EXTRACTING_UPDATE,
  INSTALLING_UPDATE,
  COPYING_USERDATA,
  FINISHED,
};

enum class UpdaterError
{
  INIT_ERROR = 0,
  NETWORK_ERROR,
  DOWNLOAD_ERROR,
  EXTRACT_ERROR,
  INSTALL_ERROR,
  UP_TO_DATE,
  FATAL
};

class IUpdater
{
public:
  virtual ~IUpdater() = default;

  virtual void OnProgress(const unsigned int progress, const UpdaterState& state = UpdaterState::NONE) = 0;
  virtual void OnError(const UpdaterError& error) = 0;
};
