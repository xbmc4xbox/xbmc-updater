/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Downloader.h"

#include "utils/StringUtils.h"

#include <windows.h>

const unsigned char cert[] = {
#embed "assets/cacert.pem" suffix(, ) // No suffix
    0                                 // always null-terminated
};

static void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str)
{
  (void)ctx;
  (void)level;
  printf("%s:%d: %s\n", file, line, str);
}

CDownloader::CDownloader()
{
  Initialize();
}

CDownloader::~CDownloader()
{
  Deinitialize();
}

void CDownloader::Initialize()
{
  mbedtls_debug_set_threshold(0);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_x509_crt_init(&cacert);
  mbedtls_ssl_init(&ssl);
  mbedtls_ssl_config_init(&conf);
  mbedtls_entropy_init(&entropy);
  mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
  mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
  mbedtls_ssl_conf_dbg(&conf, mbedtls_debug, stdout);
  mbedtls_net_init(&server_fd);
  mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

  if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0) != 0)
    return;

  if (mbedtls_x509_crt_parse(&cacert, (const unsigned char *)cert, sizeof(cert)) != 0)
    return;

  if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
    return;

  if (mbedtls_ssl_setup(&ssl, &conf) != 0)
    return;

  m_initialized = true;
}

void CDownloader::Deinitialize()
{
  mbedtls_ssl_close_notify(&ssl);
  mbedtls_ssl_free(&ssl);
  mbedtls_ssl_config_free(&conf);
  mbedtls_x509_crt_free(&cacert);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  mbedtls_net_free(&server_fd);

  m_initialized = false;
}

bool CDownloader::Get(const std::string& strURL, std::string& strBody)
{
  mbedtls_ssl_session_reset(&ssl);

  size_t iPos = strURL.find("://");
  if (iPos == std::string::npos)
    return false;

  std::string strHost = strURL.substr(iPos + 3);
  iPos = strHost.find("/");
  if (iPos == std::string::npos)
    return false;
  strHost = strHost.substr(0, iPos);

  if (mbedtls_ssl_set_hostname(&ssl, strHost.c_str()) != 0)
    return false;

  if (mbedtls_net_connect(&server_fd, strHost.c_str(), "443", MBEDTLS_NET_PROTO_TCP) != 0)
    return false;

  int ret;
  while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
  {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
    {
      mbedtls_net_free(&server_fd);
      return false;
    }
  }

  std::string strRequest = StringUtils::Format("GET %s HTTP/1.1\r\n"
                                              "Host: %s\r\n"
                                              "User-Agent: xbmc-updater\r\n"
                                              "Accept: application/vnd.github+json\r\n"
                                              "X-GitHub-Api-Version: 2022-11-28\r\n"
                                              "Connection: close\r\n\r\n", strURL.c_str(), strHost.c_str());
  if (mbedtls_ssl_write(&ssl, (const unsigned char *)strRequest.c_str(), strRequest.size()) <= 0)
  {
    mbedtls_net_free(&server_fd);
    return false;
  }

  char buffer[4096];
  ret = mbedtls_ssl_read(&ssl, (unsigned char *)buffer, sizeof(buffer));
  if (ret <= 0)
    return false;

  std::string strHeader(buffer);
  if (StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 4") || StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 5"))
  {
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    return false;
  }

  if (StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 302"))
  {
    size_t iPos = strHeader.find("Location: ");
    if (iPos == std::string::npos)
      return false;

    std::string strRedirectLink = strHeader.substr(iPos + 10);
    iPos = strRedirectLink.find("\r\n");
    if (iPos == std::string::npos)
      return false;

    strRedirectLink = strRedirectLink.substr(0, iPos);

    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    return Get(strRedirectLink, strBody);
  }

  bool foundBody = false;
  std::string strHeaderCache;
  do
  {
    if (!foundBody)
    {
      strHeaderCache.append(buffer, ret);
      size_t iPos = strHeaderCache.find("\r\n\r\n");
      if (iPos != std::string::npos)
      {
        foundBody = true;
        size_t iBodyPos = iPos + 4;
        strBody.append(strHeaderCache.data() + iBodyPos, strHeaderCache.size() - iBodyPos);
      }
    }
    else
      strBody.append(buffer, ret);
  } while ((ret = mbedtls_ssl_read(&ssl, (unsigned char *)buffer, sizeof(buffer))) > 0);

  mbedtls_ssl_close_notify(&ssl);
  mbedtls_net_free(&server_fd);

  return true;
}

bool CDownloader::Download(const std::string& strDownloadLink, const std::string& strDownloadPath)
{
  mbedtls_ssl_session_reset(&ssl);

  size_t iPos = strDownloadLink.find("://");
  if (iPos == std::string::npos)
    return false;

  std::string strHost = strDownloadLink.substr(iPos + 3);
  iPos = strHost.find("/");
  if (iPos == std::string::npos)
    return false;
  strHost = strHost.substr(0, iPos);

  if (mbedtls_ssl_set_hostname(&ssl, strHost.c_str()) != 0)
    return false;

  if (mbedtls_net_connect(&server_fd, strHost.c_str(), "443", MBEDTLS_NET_PROTO_TCP) != 0)
    return false;

  int ret;
  while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
  {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
    {
      mbedtls_net_free(&server_fd);
      return false;
    }
  }

  std::string strRequest = StringUtils::Format("GET %s HTTP/1.1\r\n"
                                              "Host: %s\r\n"
                                              "User-Agent: xbmc-updater\r\n"
                                              "Accept: application/octet-stream\r\n"
                                              "X-GitHub-Api-Version: 2022-11-28\r\n"
                                              "Connection: close\r\n\r\n", strDownloadLink.c_str(), strHost.c_str());
  if (mbedtls_ssl_write(&ssl, (const unsigned char *)strRequest.c_str(), strRequest.size()) <= 0)
  {
    mbedtls_net_free(&server_fd);
    return false;
  }

  char buffer[4096];
  ret = mbedtls_ssl_read(&ssl, (unsigned char *)buffer, sizeof(buffer));
  if (ret <= 0)
    return false;

  std::string strHeader(buffer);
  if (StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 4") || StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 5"))
    return false;

  if (StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 302"))
  {
    size_t iPos = strHeader.find("Location: ");
    if (iPos == std::string::npos)
      return false;

    std::string strRedirectLink = strHeader.substr(iPos + 10);
    iPos = strRedirectLink.find("\r\n");
    if (iPos == std::string::npos)
      return false;

    strRedirectLink = strRedirectLink.substr(0, iPos);

    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    return Download(strRedirectLink, strDownloadPath);
  }

  DWORD written = 0;
  HANDLE hFile = CreateFileA(strDownloadPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE)
  {
    bool foundBody = false;
    std::string strHeaderCache;
    do
    {
      if (!foundBody)
      {
        strHeaderCache.append(buffer, ret);
        size_t iPos = strHeaderCache.find("\r\n\r\n");
        if (iPos != std::string::npos)
        {
          foundBody = true;
          size_t iBodyPos = iPos + 4;
          WriteFile(hFile, strHeaderCache.data() + iBodyPos, strHeaderCache.size() - iBodyPos, &written, NULL);
        }
      }
      else
        WriteFile(hFile, buffer, ret, &written, NULL);
    } while ((ret = mbedtls_ssl_read(&ssl, (unsigned char *)buffer, sizeof(buffer))) > 0);
    CloseHandle(hFile);
  }

  mbedtls_ssl_close_notify(&ssl);
  mbedtls_net_free(&server_fd);

  return written != 0;
}
