/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include <string>
#include <vector>

#include <hal/video.h>
#include <hal/xbox.h>
#include <hal/debug.h>
#include <nxdk/mount.h>
#include <nxdk/path.h>
#include <nxdk/net.h>
#include <xboxkrnl/xboxkrnl.h>
#include <windows.h>

#include "Application.h"
#include "Updater.h"
#include "Util.h"
#include "filesystem/HDDirectory.h"
#include "utils/StringUtils.h"

int main(void)
{
  if (!XVideoSetMode(1280, 720, 32, REFRESH_DEFAULT))
  {
    return 0;
  }

  char launchPath[MAX_PATH];
  nxGetCurrentXbeNtPath(launchPath);
  *(strrchr(launchPath, '\\') + 1) = '\0';

  std::vector<std::string> tales = StringUtils::Split(launchPath, "\\");
  std::string strPartition, strLaunchPath = "F:";
  for (unsigned int i = 1; i < tales.size() - 1; ++i)
  {
    if (i < 4)
    {
      strPartition += "\\";
      strPartition += tales[i];
    }
    else
    {
      strLaunchPath += "\\";
      strLaunchPath += tales[i];
    }
  }
  CUtil::AddSlashAtEnd(strPartition);
  CUtil::AddSlashAtEnd(strLaunchPath);

  nxMountDrive('F', strPartition.c_str());
  nxMountDrive('Q', launchPath);
  nxMountDrive('Z', "\\Device\\Harddisk0\\Partition5\\");

  CApplication* m_application = new CApplication;
  if (!m_application->Initialize())
  {
    debugPrint("Updater initialization failed.\n");
    Sleep(5000);
    return 1;
  }

  CUpdater* m_updater = new CUpdater;
  m_updater->RegisterListener(m_application);
  m_updater->Start();

  while (!m_application->IsStopping())
  {
    m_application->Render();
  }

  m_updater->StopThread();
  delete m_updater;
  m_updater = nullptr;
  delete m_application;
  m_application = nullptr;

  Sleep(5000);
  return 1;
}
