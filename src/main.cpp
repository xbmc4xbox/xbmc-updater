/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Application.h"
#include "Updater.h"
#include "Util.h"
#include "filesystem/HDDirectory.h"
#include "utils/StringUtils.h"

#include <string>
#include <vector>

#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <nxdk/net.h>
#include <nxdk/path.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

int main(void)
{
  if (!XVideoSetMode(1280, 720, 32, REFRESH_DEFAULT))
  {
    if (!XVideoSetMode(640, 480, 32, REFRESH_DEFAULT))
    {
      VIDEO_MODE xmode;
      void* p = NULL;
      while (XVideoListModes(&xmode, 0, 0, &p))
      {
        // ignore 1080i
        if (xmode.width == 1080)
          continue;
        // 720x480 doesnt work on pbkit for some reason
        if (xmode.width == 720)
          continue;

        XVideoSetMode(xmode.width, xmode.height, xmode.bpp, xmode.refresh);
        ;
        break;
      }
    }
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

  CUpdater* m_updater = new CUpdater(strLaunchPath);
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

  return 1;
}
