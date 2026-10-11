/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Updater.h"

#include "Downloader.h"
#include "Util.h"
#include "filesystem/HDDirectory.h"
#include "filesystem/HDFile.h"
#include "utils/CustomLaunch.h"
#include "utils/JSONVariantParser.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/Version.h"

#include <assert.h>
#include <stdio.h>
#include <fstream>
#include <map>
#include <algorithm>
#include <windows.h>

#include <nxdk/net.h>

#include <microtar/microtar.h>

static const std::map<UpdaterState, UpdaterState> stateTransitions =
{
  {UpdaterState::NONE, UpdaterState::INITIALIZE},
  {UpdaterState::INITIALIZE, UpdaterState::PREPARATION},
  {UpdaterState::PREPARATION, UpdaterState::CHECKING_FOR_UPDATE},
  {UpdaterState::CHECKING_FOR_UPDATE, UpdaterState::DOWNLOADING_UPDATE},
  {UpdaterState::DOWNLOADING_UPDATE, UpdaterState::EXTRACTING_UPDATE},
  {UpdaterState::EXTRACTING_UPDATE, UpdaterState::COPYING_USERDATA},
  {UpdaterState::COPYING_USERDATA, UpdaterState::FINISHED}
};

CUpdater::CUpdater(const std::string& strRootPath)
{
  m_strRootPath = strRootPath;
}

CUpdater::~CUpdater()
{
  StopThread();
}

void CUpdater::Process()
{
  while (!m_bStop)
  {
    auto it = stateTransitions.find(m_state);
    switch (it->second)
    {
      case UpdaterState::INITIALIZE:
        m_bStop = !Initialize();
        break;

      case UpdaterState::PREPARATION:
        m_bStop = !Prepare();
        break;

      case UpdaterState::CHECKING_FOR_UPDATE:
        m_bStop = !CheckForUpdate();
        break;

      case UpdaterState::DOWNLOADING_UPDATE:
        m_bStop = !Download();
        break;

      case UpdaterState::EXTRACTING_UPDATE:
        m_bStop = !Extract();
        break;

      case UpdaterState::COPYING_USERDATA:
        m_bStop = !Install();
        break;

      case UpdaterState::FINISHED:
        m_bStop = true;
        m_listener->OnProgress(0, UpdaterState::FINISHED);
        break;

      default:
        m_bStop = true;
    }
    m_state = it->second;
  }
}

void CUpdater::RegisterListener(IUpdater *listener)
{
  assert(listener != nullptr);
  m_listener = listener;
}

std::string CUpdater::FindAsset(const std::string& strAsset, const std::string& strTag) const
{
  std::string strBody;
  std::string strURL = StringUtils::Format("https://api.github.com/repos/antonic901/xbmc4xbox-redux/releases/tags/%s", strTag.c_str());
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

bool CUpdater::Initialize()
{
  m_listener->OnProgress(0, UpdaterState::INITIALIZE);
  CHDDirectory::WipeDir("Z:\\");

  if (nxNetInit(NULL) != 0)
  {
    m_listener->OnError(UpdaterError::INIT_ERROR);
    return false;
  }

  return true;
}

bool CUpdater::Prepare()
{
  m_listener->OnProgress(0, UpdaterState::PREPARATION);
  CCustomLaunch launch;
  if (!launch.Read())
  {
    m_listener->OnError(UpdaterError::FATAL);
    return false;
  }

  m_updateChannel = launch.GetUpdateChannel();
  m_version = launch.GetVersion();
  m_revision = launch.GetRevision();
  m_performMajorUpgrade = launch.GetPerformMajorUpgrade();

  if (m_revision.empty() || m_version.empty() || m_version == "0.0.0" || m_revision == "dev")
  {
    m_listener->OnError(UpdaterError::FATAL);
    return false;
  }

  return true;
}

bool CUpdater::CheckForUpdate()
{
  m_listener->OnProgress(0, UpdaterState::CHECKING_FOR_UPDATE);

  CDownloader downloader;
  if (m_updateChannel == "stable")
  {
    CVersion currentVersion(m_version);
    int currentMajor = atoi(StringUtils::Split(m_version, ".")[0].c_str());

    CVariant data;
    int page = 1;
    do
    {
      std::string response;
      if (!downloader.Get(StringUtils::Format("https://api.github.com/repos/antonic901/xbmc4xbox-redux/releases?page=%i", page), response))
      {
        m_listener->OnError(UpdaterError::FATAL);
        return false;
      }

      if (!CJSONVariantParser::Parse(response, data) || !data.isArray())
      {
        m_listener->OnError(UpdaterError::FATAL);
        return false;
      }

      for (CVariant::iterator_array it = data.begin_array(); it != data.end_array(); ++it)
      {
        CVariant release = *it;
        if (!release.isObject() || !release.isMember("tag_name"))
          continue;

        std::string strVersion = release["tag_name"].asString();

        std::vector<std::string> vecVersion = StringUtils::Split(strVersion, ".");
        if (vecVersion.size() != 3 ||
            !std::all_of(vecVersion.begin(), vecVersion.end(), [](const std::string& part) {
              return !part.empty() && std::all_of(part.begin(), part.end(), [](char c) {
                return c >= '0' && c <= '9';
              });
            }))
          continue;

        int major = atoi(vecVersion[0].c_str());

        // Ignore major upgrade if not allowed
        if (!m_performMajorUpgrade && major > currentMajor)
          continue;

        CVersion version(strVersion);
        if (version > currentVersion)
        {
          m_upstreamVersion = strVersion;
          return true;
        }
      }
      page++;
    } while (data.size() != 0);

    m_listener->OnError(UpdaterError::UP_TO_DATE);
    return false;
  }

  // non-stable channel (ex. nightly, nightly-py3, etc.)
  std::string strAssetLink = FindAsset("version.txt", m_updateChannel);
  if (strAssetLink.empty())
  {
    m_listener->OnError(UpdaterError::FATAL);
    return false;
  }

  if (!downloader.Download(strAssetLink, "Z:\\version.txt"))
  {
    m_listener->OnError(UpdaterError::FATAL);
    return false;
  }

  std::ifstream file("Z:\\version.txt", std::ios::in);
  if (file.is_open())
  {
    std::getline(file, m_upstreamRevision);
    file.close();
  }

  if (m_upstreamRevision.empty() || m_revision.empty())
  {
    m_listener->OnError(UpdaterError::FATAL);
    return false;
  }

  if (StringUtils::EqualsNoCase(m_upstreamRevision, m_revision))
  {
    m_listener->OnError(UpdaterError::UP_TO_DATE);
    return false;
  }

  return true;
}

bool CUpdater::Download()
{
  m_listener->OnProgress(0, UpdaterState::DOWNLOADING_UPDATE);

  std::string strAssetLink = FindAsset("Xodi.tar", m_updateChannel == "stable" ? m_upstreamVersion : m_updateChannel);
  if (strAssetLink.empty())
  {
    m_listener->OnError(UpdaterError::NETWORK_ERROR);
    return false;
  }

  CDownloader downloader;
  if (!downloader.Download(strAssetLink, "Z:\\Xodi.tar"))
  {
    m_listener->OnError(UpdaterError::DOWNLOAD_ERROR);
    return false;
  }

  return true;
}

bool CUpdater::Extract()
{
  m_listener->OnProgress(0, UpdaterState::EXTRACTING_UPDATE);
  mtar_t tar;
  int ret = mtar_open(&tar, "Z:\\Xodi.tar", "r");
  if (ret != MTAR_ESUCCESS)
  {
    m_listener->OnError(UpdaterError::EXTRACT_ERROR);
    return false;
  }

  FILE* destination_file = nullptr;
  const auto failExtraction = [&]() {
    if (destination_file)
      fclose(destination_file);
    mtar_close(&tar);
    m_listener->OnError(UpdaterError::EXTRACT_ERROR);
    return false;
  };

  /* Extract new build to E:\Xodi_NEW\ */
  m_strExtractPath = m_strRootPath;
  CUtil::RemoveSlashAtEnd(m_strExtractPath);
  m_strExtractPath += "_NEW";
  CUtil::AddSlashAtEnd(m_strExtractPath);

  std::string strLongPath;
  char buffer[4096];
  mtar_header_t header;
  while (true)
  {
    ret = mtar_read_header(&tar, &header);
    if (ret == MTAR_ENULLRECORD)
    {
      if (!strLongPath.empty())
        return failExtraction();
      break;
    }
    if (ret != MTAR_ESUCCESS)
      return failExtraction();
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
      if (header.size == 0 || header.size >= sizeof(longPath))
        return failExtraction();
      if (mtar_read_data(&tar, longPath, header.size) != MTAR_ESUCCESS)
        return failExtraction();
      longPath[header.size] = '\0';
      strLongPath = longPath;
      if (strLongPath.empty() || mtar_next(&tar) != MTAR_ESUCCESS)
        return failExtraction();
      continue;
    }

    if (CUtil::HasSlashAtEnd(strFile))
    {
      if (!CHDDirectory::Create(strFile))
      {
        return failExtraction();
      }
    }
    else
    {
      if (CFileHD::Exists(strFile))
      {
        CFileHD::Delete(strFile);
      }

      destination_file = fopen(strFile.c_str(), "wb");
      if (!destination_file)
      {
        return failExtraction();
      }

      size_t remaining = header.size;
      while (remaining > 0)
      {
        size_t chunk_size = (remaining < 4096) ? remaining : 4096;
        ret = mtar_read_data(&tar, buffer, chunk_size);
        if (ret != MTAR_ESUCCESS)
        {
          return failExtraction();
        }

        if (fwrite(buffer, 1, chunk_size, destination_file) != chunk_size)
          return failExtraction();
        remaining -= chunk_size;
      }

      const int closeResult = fclose(destination_file);
      destination_file = nullptr;
      if (closeResult != 0)
        return failExtraction();
    }
    if (mtar_next(&tar) != MTAR_ESUCCESS)
      return failExtraction();
  }

  if (mtar_close(&tar) != MTAR_ESUCCESS)
  {
    m_listener->OnError(UpdaterError::EXTRACT_ERROR);
    return false;
  }
  return true;
}

bool CUpdater::Install()
{
  m_listener->OnProgress(0, UpdaterState::INSTALLING_UPDATE);

  // Xodi 5.0+ keeps userdata inside UDATA, so no need to copy
  if (!m_upstreamVersion.empty() && StringUtils::StartsWithNoCase(m_upstreamVersion, "4."))
  {
    if (!CHDDirectory::Copy("D:\\home\\", m_strExtractPath))
    {
      m_listener->OnError(UpdaterError::INSTALL_ERROR);
      return false;
    }
  }

  /* E:\Xodi\ -> E:\Xodi_OLD\ */
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
    m_listener->OnError(UpdaterError::INSTALL_ERROR);
    return false;
  }

  /* E:\Xodi_NEW\ -> E:\Xodi\ */
  std::string strCurrentBuild = m_strExtractPath;
  CUtil::RemoveSlashAtEnd(strCurrentBuild);
  if (!CFileHD::Rename(strCurrentBuild, strPreviousBuild))
  {
    // Attempt rollback to previous version
    CFileHD::Rename(strPreviousBuild + "_OLD", strPreviousBuild);
    m_listener->OnError(UpdaterError::INSTALL_ERROR);
    return false;
  }

  return true;
}
