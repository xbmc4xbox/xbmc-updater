/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
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

#include "Updater.h"
#include "Util.h"
#include "filesystem/HDDirectory.h"
#include "utils/StringUtils.h"

int main(void)
{
  if (!XVideoSetMode(640, 480, 32, REFRESH_DEFAULT))
  {
    return 0;
  }

  debugPrint("Initializing network... ");
  if (nxNetInit(NULL) != 0)
    debugPrint("FAILED\n");
  else
    debugPrint("SUCCESS\n");

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

  CHDDirectory::WipeDir("Z:\\");

  CUpdater updater(strLaunchPath);
  while (true)
  {
    if (updater.GetStatus() == UpdaterStatus::FINISHED)
    {
      debugPrint("Rebooting...\n");
      LARGE_INTEGER delay;
      delay.QuadPart = -5LL * 1000 * 1000 * 10;
      KeDelayExecutionThread(KernelMode, FALSE, &delay);
      HalReturnToFirmware(HalRebootRoutine);
    }
    else if (UpdaterStatus() == UpdaterStatus::ERROR)
    {
      debugPrint("Press any button to exit\n");
    }
    else if(updater.Process())
    {
      updater.OnError();
    }
  }

  return 0;
}
