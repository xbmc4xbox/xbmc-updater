/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Updater.h"

#include "Downloader.h"
#include "Util.h"
#include "filesystem/HDDirectory.h"
#include "filesystem/HDFile.h"
#include "utils/CustomLaunch.h"
#include "utils/JSONVariantParser.h"
#include "utils/StringUtils.h"

#include <stdio.h>
#include <fstream>
#include <hal/debug.h>
#include <windows.h>

#include <microtar/microtar.h>


CUpdater::CUpdater(std::string strRootPath)
{
  m_strRootPath = strRootPath;
}

void CUpdater::OnError()
{
  if (!m_strError.empty())
  {
    debugPrint("FAILED: %s\n", m_strError.c_str());
  }
  m_status = UpdaterStatus::ERROR;
}

int CUpdater::Process()
{
  switch (m_status)
  {
  case UpdaterStatus::PREPARE:
    return Prepare();

  case UpdaterStatus::CHECK_FOR_UPDATE:
    return CheckForUpdate();

  case UpdaterStatus::DOWNLOAD_BUILD:
    return Download();

  case UpdaterStatus::EXTRACT_BUILD:
    return Extract();

  case UpdaterStatus::COPY_USERDATA:
    return Install();

  case UpdaterStatus::FINISHED:
  case UpdaterStatus::ERROR:
  default:
    return 0;
  }
}

std::string CUpdater::FindAsset(const std::string& strAsset) const
{
  std::string strBody;
  std::string strURL = StringUtils::Format("https://api.github.com/repos/antonic901/xbmc4xbox-redux/releases/tags/%s", m_updateChannel.c_str());
  CDownloader downloader;
  if (!downloader.Get(strURL, strBody) || strBody.empty())
    return "";

  CVariant data;
  if (!CJSONVariantParser::Parse(strBody, data))
    return "";

  if (data.isMember("assets") && data["assets"].isArray())
  {
    for (auto it = data["assets"].begin_array(); it != data["assets"].end_array(); ++it)
    {
      if (it->isObject() && it->isMember("url") && it->isMember("name"))
      {
        std::string url = (*it)["url"].asString();
        std::string name =(*it)["name"].asString();
        if (StringUtils::EqualsNoCase(name, strAsset))
          return url;
      }
    }
  }

  return "";
}

int CUpdater::Prepare()
{
  CCustomLaunch launch;
  if (!launch.Read())
  {
    m_strError = "failed to read launch data";
    return 1;
  }

  m_currentRevision = launch.GetRevision();
  if (m_currentRevision.empty() || StringUtils::EqualsNoCase(m_currentRevision, "dev"))
  {
    m_strError = StringUtils::Format("invalid version installed: %s", m_currentRevision.c_str());
    return 1;
  }

  m_updateChannel = launch.GetUpdateChannel();

  m_strExtractPath = m_strRootPath;
  CUtil::RemoveSlashAtEnd(m_strExtractPath);
  m_strExtractPath += "_NEW";
  CUtil::AddSlashAtEnd(m_strExtractPath);
  m_strUpdatePath = "Z:\\XBMC4Xbox.tar";

  m_status = UpdaterStatus::CHECK_FOR_UPDATE;
  return 0;
}

int CUpdater::CheckForUpdate()
{
  debugPrint("Checking for new version... ");
  std::string strAsset("version.txt");
  std::string strAssetLink = FindAsset(strAsset);
  if (strAssetLink.empty())
  {
    m_strError = StringUtils::Format("failed to find asset: %s", strAsset.c_str());
    return 1;
  }

  CDownloader downloader;
  if (!downloader.Download(strAssetLink, "Z:\\version.txt"))
  {
    m_strError = StringUtils::Format("failed to download asset: %s", strAsset.c_str());
    return 1;
  }

  std::ifstream file("Z:\\version.txt", std::ios::in);
  if (file.is_open())
  {
    std::getline(file, m_latestRevision);
    file.close();
  }

  if (m_latestRevision.empty() || m_currentRevision.empty())
  {
    m_strError = "failed to get latest version";
    return 1;
  }

  debugPrint("SUCCESS\n");
  if (StringUtils::EqualsNoCase(m_latestRevision, m_currentRevision))
  {
    debugPrint("You are already on latest version: %s\n", m_currentRevision.c_str());
    m_status = UpdaterStatus::FINISHED;
    return 0;
  }

  m_status = UpdaterStatus::DOWNLOAD_BUILD;
  debugPrint("Updating from %s to %s\n", m_currentRevision.c_str(), m_latestRevision.c_str());
  return 0;
}

int CUpdater::Download()
{
  debugPrint("Downloading update... ");
  std::string strAsset("XBMC4Xbox.tar");
  std::string strAssetLink = FindAsset(strAsset);
  if (strAssetLink.empty())
  {
    m_strError = StringUtils::Format("failed to find asset: %s", strAsset.c_str());
    return 1;
  }

  CDownloader downloader;
  if (!downloader.Download(strAssetLink, "Z:\\XBMC4Xbox.tar"))
  {
    m_strError = "failed to download update";
    return 1;
  }

  debugPrint("SUCCESS\n");
  m_status = UpdaterStatus::EXTRACT_BUILD;
  return 0;
}

int CUpdater::Extract()
{
  debugPrint("Extracting update...\n");
  mtar_t tar;
  int ret = mtar_open(&tar, m_strUpdatePath.c_str(), "r");
  if (ret != MTAR_ESUCCESS)
  {
    m_strError = StringUtils::Format("%s %s", mtar_strerror(ret), m_strUpdatePath.c_str());
    return 1;
  }

  std::string strLongPath;
  char buffer[4096];
  mtar_header_t header;
  while ((mtar_read_header(&tar, &header)) != MTAR_ENULLRECORD)
  {
    std::string strFile;
    if (!strLongPath.empty())
    {
      strFile = m_strExtractPath + strLongPath;
      strLongPath.clear();
    }
    else
    {
      strFile = m_strExtractPath + header.name;
    }
    StringUtils::Replace(strFile, "/", "\\");
    StringUtils::Replace(strFile, "BUILD\\", "");

    if (strcmp(header.name, "././@LongLink") == 0)
    {
      char longPath[MAX_PATH];
      mtar_read_data(&tar, longPath, header.size);
      longPath[header.size] = '\0';
      strLongPath = longPath;
      memset(longPath, 0, sizeof(longPath));
      mtar_next(&tar);
      continue;
    }

    if (CUtil::HasSlashAtEnd(strFile))
    {
      if (!CHDDirectory::Create(strFile))
      {
        m_strError = "failed to extract archive";
        mtar_close(&tar);
        return 1;
      }
    }
    else
    {
      if (CFileHD::Exists(strFile))
      {
        CFileHD::Delete(strFile);
      }

      FILE *destination_file = fopen(strFile.c_str(), "wb");
      if (!destination_file)
      {
        m_strError = StringUtils::Format("failed to extract file: %s", strFile.c_str());
        mtar_close(&tar);
        return 1;
      }

      size_t remaining = header.size;
      while (remaining > 0)
      {
        size_t chunk_size = (remaining < 4096) ? remaining : 4096;
        ret = mtar_read_data(&tar, buffer, chunk_size);
        if (ret != MTAR_ESUCCESS)
        {
          m_strError = StringUtils::Format("failed to extract file: %s", strFile.c_str());
          mtar_close(&tar);
          return 1;
        }

        fwrite(buffer, 1, chunk_size, destination_file);
        remaining -= chunk_size;
      }

      fclose(destination_file);
    }
    mtar_next(&tar);
  }

  m_status = UpdaterStatus::COPY_USERDATA;
  debugPrint("Extracting completed!\n");
  return 0;
}

int CUpdater::Install()
{
  debugPrint("Instaling update...\n");
  std::string strUserdata = "D:\\home\\";
  if (!CHDDirectory::Copy(strUserdata, m_strExtractPath))
  {
    m_strError = "failed to copy home folder";
    return 1;
  }

  std::string strPreviousBackup = m_strRootPath;
  CUtil::RemoveSlashAtEnd(strPreviousBackup);
  strPreviousBackup += "_OLD";
  CUtil::AddSlashAtEnd(strPreviousBackup);
  if (CHDDirectory::Exists(strPreviousBackup))
  {
    CHDDirectory::WipeDir(strPreviousBackup);
    CHDDirectory::Remove(strPreviousBackup);
  }

  std::string strPreviousBuild = m_strRootPath;
  CUtil::RemoveSlashAtEnd(strPreviousBuild);
  if (!CFileHD::Rename(strPreviousBuild, strPreviousBuild + "_OLD"))
  {
    m_strError = "failed to backup previous build";
    return 1;
  }

  std::string strCurrentBuild = m_strExtractPath;
  CUtil::RemoveSlashAtEnd(strCurrentBuild);
  if (!CFileHD::Rename(strCurrentBuild, strPreviousBuild))
  {
    m_strError = "failed to install new build";
    return 1;
  }

  m_status = UpdaterStatus::FINISHED;
  debugPrint("Install completed!\n");
  return 0;
}
