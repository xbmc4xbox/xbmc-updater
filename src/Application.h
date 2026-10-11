/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <atomic>
#include <vector>
#include <mutex>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

#include "interfaces/IUpdater.h"

class CApplication : public IUpdater
{
public:
  CApplication();
  virtual ~CApplication();

  bool Initialize();
  void Render();
  bool IsStopping() { return m_bStop; };

  void OnProgress(const unsigned int progress, const UpdaterState& state = UpdaterState::NONE) override;
  void OnError(const UpdaterError& error) override;

private:
  void Text(TTF_Font* font, const std::string& text, int x, int y, int maxWidth, bool alignRight = false);

  bool m_isDirty = true;
  bool m_waitingOnUserAction = false;

  std::vector<SDL_Joystick*> m_joysticks;

  SDL_Window* m_window = nullptr;
  SDL_Renderer* m_renderer = nullptr;
  int m_outputWidth = 640;
  float m_uiScale = 1.0f;

  TTF_Font* m_titleFont = nullptr;
  TTF_Font* m_subtitleFont = nullptr;
  TTF_Font* m_bodyFont = nullptr;
  TTF_Font* m_smallFont = nullptr;

  std::string m_strTitle;
  std::string m_strSubtitle;
  std::string m_strBody;
  std::string m_strSmall;

  int m_iTotalPercentage = 0;

  std::mutex m_mutex;

  std::atomic<bool> m_bStop = false;
};
