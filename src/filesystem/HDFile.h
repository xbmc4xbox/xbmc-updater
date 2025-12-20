/*
 *  Copyright (C) 2014-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CFileHD
{
public:
  static bool Copy(const std::string& strFile, const std::string& strDest);

  static bool Delete(const std::string& strFile);
  static bool Rename(const std::string& strFile, const std::string& strDest);
  static bool Exists(const std::string& strFile);
};
