/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <windows.h>

typedef struct _CUSTOM_LAUNCH_DATA
{
  DWORD magic;
  char szFilename[300];
  char szLaunchXBEOnExit[100];
  char szRemap_D_As[350];
  BYTE country;
  BYTE launchInsertedMedia;
  BYTE executionType;
  char reserved[2315];
} CUSTOM_LAUNCH_DATA;

class CCustomLaunch
{
public:
  CCustomLaunch() = default;
  ~CCustomLaunch() = default;

  bool Read();

  std::string GetVersion() const { return m_version; }
  std::string GetRevision() const { return m_revision; }
  std::string GetUpdateChannel() const { return m_updateChannel; }

private:
  void Set(const std::string& key, const std::string& value);

  std::string m_version;
  std::string m_revision;
  std::string m_updateChannel;
};
