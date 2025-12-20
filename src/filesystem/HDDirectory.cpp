/*
 *  Copyright (C) 2014-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "HDDirectory.h"

#include "Util.h"
#include "filesystem/HDFile.h"
#include "utils/StringUtils.h"

#include <windows.h>

bool CHDDirectory::Copy(const std::string& strPath, const std::string& strDest)
{
  std::string path(strPath);
  CUtil::AddSlashAtEnd(path);

  WIN32_FIND_DATA fd;
  HANDLE hFind = FindFirstFileA((path + "*.*").c_str(), &fd);
  if (hFind == INVALID_HANDLE_VALUE)
    return GetLastError() == 2;

  do
  {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;

    std::string dest(path);
    CUtil::RemoveSlashAtEnd(dest);
    dest = CUtil::GetFileName(dest);
    CUtil::AddSlashAtEnd(dest);

    std::string strFile = path + fd.cFileName;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
      dest = strDest + dest;
      if (!Copy(strFile, dest))
      {
        FindClose(hFind);
        return false;
      }
    }
    else
    {
      dest = strDest + dest + fd.cFileName;
      if (!CFileHD::Copy(strFile, dest))
      {
        FindClose(hFind);
        return false;
      }
    }
  } while (FindNextFileA(hFind, &fd) != 0);

  FindClose(hFind);
  return true;
}

bool CHDDirectory::Create(const std::string& path)
{
  if (!CreateDirectoryA(path.c_str(), NULL))
  {
    if (GetLastError() == ERROR_ALREADY_EXISTS)
      return true;

    if (GetLastError() != ERROR_PATH_NOT_FOUND)
      return false;

    size_t sep = path.rfind('\\');
    if (sep == std::string::npos)
      return false;

    if (Create(path.substr(0, sep)))
      return Create(path);

    return false;
  }

  return true;
}

bool CHDDirectory::Exists(const std::string& strPath)
{
  const DWORD attrs = GetFileAttributesA(strPath.c_str());
  if(attrs == INVALID_FILE_ATTRIBUTES)
    return false;

  return FILE_ATTRIBUTE_DIRECTORY & attrs;
}

bool CHDDirectory::Remove(const std::string& strPath)
{
  std::string path(strPath);
  CUtil::AddSlashAtEnd(path);

  return RemoveDirectoryA(path.c_str());
}

void CHDDirectory::WipeDir(const std::string& strPath)
{
  std::string path(strPath);
  CUtil::AddSlashAtEnd(path);

  WIN32_FIND_DATA fd;
  HANDLE hFind = FindFirstFileA((path + "*.*").c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    do
    {
      if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
        continue;

      std::string strFile = path + fd.cFileName;
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        WipeDir(strFile);
        RemoveDirectoryA(strFile.c_str());
      }
      else
      {
        DeleteFileA(strFile.c_str());
      }
    } while (FindNextFileA(hFind, &fd) != 0);
  }

  FindClose(hFind);
}
