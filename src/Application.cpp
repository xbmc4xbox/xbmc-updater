/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Application.h"

#include "utils/StringUtils.h"

#include <cmath>
#include <hal/xbox.h>

#define BODY_TEMPLATE "Step %i of %d: %s"

namespace
{
  const unsigned char fontData[] = {
  #embed "fonts/DejaVuSans.ttf"
  };

  TTF_Font* OpenFont(int size)
  {
    return TTF_OpenFontRW(SDL_RWFromConstMem(fontData, sizeof(fontData)), 1, size);
  }

  void RoundedLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, float radius)
  {
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
    SDL_BlendMode blendMode;
    SDL_GetRenderDrawBlendMode(renderer, &blendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const float dx = static_cast<float>(x2 - x1);
    const float dy = static_cast<float>(y2 - y1);
    const float lengthSquared = dx * dx + dy * dy;
    const int padding = static_cast<int>(radius) + 1;
    for (int y = std::min(y1, y2) - padding; y <= std::max(y1, y2) + padding; ++y)
    {
      for (int x = std::min(x1, x2) - padding; x <= std::max(x1, x2) + padding; ++x)
      {
        const float t = lengthSquared > 0.0f
          ? std::max(0.0f, std::min(1.0f, ((x - x1) * dx + (y - y1) * dy) / lengthSquared))
          : 0.0f;
        const float offsetX = x - (x1 + t * dx);
        const float offsetY = y - (y1 + t * dy);
        const float coverage = std::max(0.0f, std::min(1.0f, radius + 0.5f - std::sqrt(offsetX * offsetX + offsetY * offsetY)));
        if (coverage > 0.0f)
        {
          SDL_SetRenderDrawColor(renderer, red, green, blue, static_cast<Uint8>(alpha * coverage));
          SDL_RenderDrawPoint(renderer, x, y);
        }
      }
    }
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    SDL_SetRenderDrawBlendMode(renderer, blendMode);
  }
}

CApplication::CApplication()
{
  m_strTitle = "Updating...";
  m_strSubtitle = "Sit tight while we update Xodi.";
  m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::PREPARATION, UpdaterState::FINISHED, "Initializing...");
  m_strSmall = "Your console will restart after the update.  %i%";
}

CApplication::~CApplication()
{
  for (SDL_Joystick* joystick : m_joysticks)
    SDL_JoystickClose(joystick);

  TTF_CloseFont(m_smallFont);
  TTF_CloseFont(m_bodyFont);
  TTF_CloseFont(m_subtitleFont);
  TTF_CloseFont(m_titleFont);
  SDL_DestroyRenderer(m_renderer);
  SDL_DestroyWindow(m_window);
}

bool CApplication::Initialize()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER) != 0 || TTF_Init() != 0)
    return false;

  SDL_DisplayMode displayMode;
  if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0 || displayMode.w <= 0 || displayMode.h <= 0)
    return false;

  m_window = SDL_CreateWindow("Xodi Auto Updater", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, displayMode.w, displayMode.h, SDL_WINDOW_SHOWN);
  if (!m_window)
    return false;

  m_renderer = SDL_CreateRenderer(m_window, -1, 0);
  if (!m_renderer)
    return false;

  int outputHeight = 0;
  if (SDL_GetRendererOutputSize(m_renderer, &m_outputWidth, &outputHeight) != 0 || outputHeight <= 0)
    return false;
  m_uiScale = outputHeight / 480.0f;

  const auto fontSize = [this](int size) { return std::max(1, static_cast<int>(size * m_uiScale + 0.5f)); };

  m_titleFont = OpenFont(fontSize(30));
  if (!m_titleFont)
    return false;
  m_subtitleFont = OpenFont(fontSize(14));
  if (!m_subtitleFont)
    return false;
  m_bodyFont = OpenFont(fontSize(14));
  if (!m_bodyFont)
    return false;
  m_smallFont = OpenFont(fontSize(11));
  if (!m_smallFont)
    return false;

  SDL_JoystickEventState(SDL_ENABLE);
  return true;
}

void CApplication::Render()
{
  bool isDirty;
  bool canReboot;
  std::string title, subtitle, body, small;
  int totalPercentage = 0;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    isDirty = m_isDirty;
    canReboot = m_waitingOnUserAction && !m_isDirty;
    if (isDirty)
    {
      title = m_strTitle;
      subtitle = m_strSubtitle;
      body = m_strBody;
      small = m_strSmall;
      totalPercentage = m_iTotalPercentage;
      m_isDirty = false;
    }
  }

  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_JOYDEVICEADDED)
    {
      SDL_Joystick* joystick = SDL_JoystickOpen(event.jdevice.which);
      if (joystick)
        m_joysticks.push_back(joystick);
    }
    else if (event.type == SDL_JOYDEVICEREMOVED)
    {
      for (auto it = m_joysticks.begin(); it != m_joysticks.end(); ++it)
      {
        if (SDL_JoystickInstanceID(*it) == event.jdevice.which)
        {
          SDL_JoystickClose(*it);
          m_joysticks.erase(it);
          break;
        }
      }
    }
    else if (event.type == SDL_JOYBUTTONDOWN && canReboot)
    {
      XReboot();
    }
  }

  if (isDirty)
  {
    SDL_SetRenderDrawColor(m_renderer, 80, 80, 80, 255);
    SDL_RenderClear(m_renderer);

    const auto px = [this](int value) { return static_cast<int>(value * m_uiScale + 0.5f); };
    SDL_Rect track = {px(32), px(414), m_outputWidth - 2 * px(32), px(6)};
    const int iconCenterX = m_outputWidth - px(122);

    // Render icon
    SDL_SetRenderDrawColor(m_renderer, 240, 240, 240, 255);
    RoundedLine(m_renderer, iconCenterX, px(149), iconCenterX, px(193), 2.00f * m_uiScale);
    RoundedLine(m_renderer, iconCenterX - px(21), px(175), iconCenterX, px(193), 2.00f * m_uiScale);
    RoundedLine(m_renderer, iconCenterX, px(193), iconCenterX + px(21), px(175), 2.00f * m_uiScale);
    RoundedLine(m_renderer, iconCenterX - px(28), px(205), iconCenterX + px(28), px(205), 2.00f * m_uiScale);

    // Render text
    Text(m_titleFont, title, track.x, px(125), px(242));
    Text(m_subtitleFont, subtitle, track.x + px(2), px(190), px(242));
    Text(m_bodyFont, body, track.x, px(383), px(190));
    Text(m_smallFont, StringUtils::Format(small.c_str(), totalPercentage), track.x + track.w, px(386), px(322), true);

    // Render track
    SDL_SetRenderDrawColor(m_renderer, 113, 113, 113, 255);
    SDL_RenderFillRect(m_renderer, &track);
    SDL_SetRenderDrawColor(m_renderer, 16, 124, 16, 255);
    SDL_Rect progress = {track.x, track.y, track.w * totalPercentage / 100, track.h};
    SDL_RenderFillRect(m_renderer, &progress);

    SDL_RenderPresent(m_renderer);
  }
}

void CApplication::OnProgress(const unsigned int progress, const UpdaterState& state /* = UpdaterState::NONE */)
{
  std::unique_lock<std::mutex> lckdl(m_mutex);
  switch(state)
  {
    case UpdaterState::INITIALIZE:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::INITIALIZE, UpdaterState::FINISHED, "Initializing...");
      break;

    case UpdaterState::PREPARATION:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::PREPARATION, UpdaterState::FINISHED, "Preparing...");
      break;

    case UpdaterState::CHECKING_FOR_UPDATE:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::CHECKING_FOR_UPDATE, UpdaterState::FINISHED, "Checking for update...");
      break;

    case UpdaterState::DOWNLOADING_UPDATE:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::DOWNLOADING_UPDATE, UpdaterState::FINISHED, "Downloading...");
      break;

    case UpdaterState::EXTRACTING_UPDATE:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::EXTRACTING_UPDATE, UpdaterState::FINISHED, "Extracting...");
      break;

    case UpdaterState::INSTALLING_UPDATE:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::INSTALLING_UPDATE, UpdaterState::FINISHED, "Installing...");
      break;

    case UpdaterState::COPYING_USERDATA:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::COPYING_USERDATA, UpdaterState::FINISHED, "Finalizing...");
      break;

    case UpdaterState::FINISHED:
      m_strBody = StringUtils::Format(BODY_TEMPLATE, UpdaterState::FINISHED, UpdaterState::FINISHED, "Xodi has been updated! Rebooting in 5s...");
      m_bStop = true;
      break;
  }

  m_iTotalPercentage = static_cast<int>(state) * 100 / static_cast<int>(UpdaterState::FINISHED);
  m_isDirty = true;
}

void CApplication::OnError(const UpdaterError& error)
{
  std::unique_lock<std::mutex> lckdl(m_mutex);
  switch (error)
  {
    case UpdaterError::INIT_ERROR:
      m_strSmall = "ERROR: INIT ERROR";
      break;

    case UpdaterError::NETWORK_ERROR:
      m_strSmall = "ERROR: NETWORK ERROR";
      break;

    case UpdaterError::DOWNLOAD_ERROR:
      m_strSmall = "ERROR: DOWNLOAD FAILED";
      break;

    case UpdaterError::EXTRACT_ERROR:
      m_strSmall = "ERROR: EXTRACT FAILED";
      break;

    case UpdaterError::INSTALL_ERROR:
      m_strSmall = "ERROR: INSTALL FAILED";
      break;

    case UpdaterError::UP_TO_DATE:
      m_iTotalPercentage = 100;
      m_strSmall = "Xodi is already up to date!";
      break;

    case UpdaterError::FATAL:
      m_strSmall = "ERROR: UPDATE FAILED";
      break;

    default:
      break;
  }
  m_strBody = "Press any button to reboot";
  m_waitingOnUserAction = true;
  m_isDirty = true;
}

void CApplication::Text(TTF_Font* font, const std::string& text, int x, int y, int maxWidth, bool alignRight)
{
  SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), {240, 240, 240, 255});
  if (surface)
  {
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    const float scale = std::min(1.0f, static_cast<float>(maxWidth) / surface->w);
    SDL_Rect rect = {x, y, static_cast<int>(surface->w * scale), static_cast<int>(surface->h * scale)};
    if (alignRight)
      rect.x -= rect.w;
    SDL_FreeSurface(surface);
    if (texture)
    {
      SDL_RenderCopy(m_renderer, texture, nullptr, &rect);
      SDL_DestroyTexture(texture);
    }
  }
}
