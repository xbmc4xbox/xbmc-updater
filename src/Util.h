/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CUtil
{
public:
  static const std::string GetFileName(const std::string& strFileNameAndPath);

  static void AddSlashAtEnd(std::string& strFolder);
  static void RemoveSlashAtEnd(std::string& strFolder);
  static bool HasSlashAtEnd(const std::string& strPath);
  static std::string GetParentPath(const std::string& strPath);
};
