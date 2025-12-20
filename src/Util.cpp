/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Util.h"

#include <windows.h>

const std::string CUtil::GetFileName(const std::string& strFileNameAndPath)
{
  const size_t slash = strFileNameAndPath.find_last_of("/\\");
  return strFileNameAndPath.substr(slash+1);
}

void CUtil::AddSlashAtEnd(std::string& strFolder)
{
  if (!HasSlashAtEnd(strFolder))
  {
    strFolder += '\\';
  }
}

void CUtil::RemoveSlashAtEnd(std::string& strFolder)
{
  if (HasSlashAtEnd(strFolder))
  {
    strFolder.erase(strFolder.size() - 1);
  }
}

bool CUtil::HasSlashAtEnd(const std::string& strPath)
{
  if (strPath.empty())
  {
    return false;
  }

  size_t iPos = strPath.size() - 1;
  return strPath.c_str()[iPos] == '\\';
}

std::string CUtil::GetParentPath(const std::string& strPath)
{
  std::string strParentPath(strPath);
  if (HasSlashAtEnd(strParentPath))
  {
    strParentPath.erase(strParentPath.size() - 1);
  }

  int iPos = strParentPath.rfind('\\');
  strParentPath.erase(iPos);
  AddSlashAtEnd(strParentPath);
  return strParentPath;
}
