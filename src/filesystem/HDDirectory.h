/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CHDDirectory
{
public:
  static bool Copy(const std::string& strPath, const std::string& strDest);
  static bool Create(const std::string& path);
  static bool Exists(const std::string& strPath);
  static bool Remove(const std::string& strPath);
  static void WipeDir(const std::string& strPath);
};
