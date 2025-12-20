/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "CustomLaunch.h"

#include "utils/StringUtils.h"

#include <vector>
#include <hal/xbox.h>

void CCustomLaunch::Set(const std::string& key, const std::string& value)
{
  if (key == "version")
  {
    m_version = value;
  }
  else if (key == "revision")
  {
    m_revision = value;
  }
  else if (key == "channel")
  {
    m_updateChannel = value;
  }
}

bool CCustomLaunch::Read()
{
  unsigned long launchType;
  const unsigned char *launchData;
  if (XGetLaunchInfo(&launchType, &launchData) != ERROR_SUCCESS)
    return false;

  if (launchType == LDT_NONE)
    return false;

  CUSTOM_LAUNCH_DATA *data = (CUSTOM_LAUNCH_DATA*)launchData;
  std::vector<std::string> params = StringUtils::Split(data->reserved, "&");
  for (auto param : params)
  {
    size_t iPos = param.find("=");
    // invalid param
    if (iPos == std::string::npos)
      continue;

    // missing value
    if (param.size() - iPos == 1)
      continue;

    std::string key = param.substr(0, iPos);
    std::string value = param.substr(iPos + 1);
    Set(key, value);
  }

  return true;
}
