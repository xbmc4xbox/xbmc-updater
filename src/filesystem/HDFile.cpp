/*
 *  Copyright (C) 2014-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "HDFile.h"

#include "Util.h"
#include "filesystem/HDDirectory.h"

#include <windows.h>

bool CFileHD::Copy(const std::string& strFile, const std::string& strDest)
{
  std::string strParentPath = CUtil::GetParentPath(strDest);
  if (!CHDDirectory::Create(strParentPath))
  {
    return false;
  }

  if (Exists(strDest))
  {
    Delete(strDest);
  }

  return CopyFileA(strFile.c_str(), strDest.c_str(), FALSE);
}

bool CFileHD::Delete(const std::string& strFile)
{
  return DeleteFileA(strFile.c_str());
}

bool CFileHD::Rename(const std::string& strFile, const std::string& strDest)
{
  return MoveFileA(strFile.c_str(), strDest.c_str());
}

bool CFileHD::Exists(const std::string& strFile)
{
  const DWORD attrs = GetFileAttributesA(strFile.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}