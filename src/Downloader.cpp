/*
 *  Copyright (C) 2023-2026 Team Xodi
 *  This file is part of Xodi - https://xodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Downloader.h"

#include "utils/StringUtils.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include <windows.h>

const unsigned char cert[] = {
#embed "assets/cacert.pem" suffix(, ) // No suffix
    0 // always null-terminated
};

static void mbedtls_debug(void* ctx, int level, const char* file, int line, const char* str)
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

  if (mbedtls_x509_crt_parse(&cacert, (const unsigned char*)cert, sizeof(cert)) != 0)
    return;

  if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0)
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
                                               "Connection: close\r\n\r\n",
                                               strURL.c_str(), strHost.c_str());
  if (mbedtls_ssl_write(&ssl, (const unsigned char*)strRequest.c_str(), strRequest.size()) <= 0)
  {
    mbedtls_net_free(&server_fd);
    return false;
  }

  char buffer[4096];
  ret = mbedtls_ssl_read(&ssl, (unsigned char*)buffer, sizeof(buffer));
  if (ret <= 0)
    return false;

  std::string strHeader(buffer);
  if (StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 4") ||
      StringUtils::StartsWithNoCase(strHeader, "HTTP/1.1 5"))
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
  } while ((ret = mbedtls_ssl_read(&ssl, (unsigned char*)buffer, sizeof(buffer))) > 0);

  mbedtls_ssl_close_notify(&ssl);
  mbedtls_net_free(&server_fd);

  return true;
}

bool CDownloader::Download(const std::string& strDownloadLink,
                           const std::string& strDownloadPath,
                           const std::function<void(unsigned int)>& onProgress)
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
                                               "Connection: close\r\n\r\n",
                                               strDownloadLink.c_str(), strHost.c_str());
  if (mbedtls_ssl_write(&ssl, (const unsigned char*)strRequest.c_str(), strRequest.size()) <= 0)
  {
    mbedtls_net_free(&server_fd);
    return false;
  }

  // Headers can span several TLS reads; retain any body bytes in the last read.
  char buffer[4096];
  std::string response;
  size_t headerEnd;
  while ((headerEnd = response.find("\r\n\r\n")) == std::string::npos)
  {
    ret = mbedtls_ssl_read(&ssl, (unsigned char*)buffer, sizeof(buffer));
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
      continue;
    if (ret <= 0 || response.size() + ret > 65536)
    {
      mbedtls_ssl_close_notify(&ssl);
      mbedtls_net_free(&server_fd);
      return false;
    }
    response.append(buffer, ret);
  }

  const std::string headers = response.substr(0, headerEnd);
  const auto headerValue = [&](const std::string& name)
  {
    size_t pos = headers.find("\r\n");
    while (pos != std::string::npos)
    {
      pos += 2;
      const size_t end = headers.find("\r\n", pos);
      const std::string line = headers.substr(pos, end == std::string::npos ? end : end - pos);
      const size_t colon = line.find(':');
      if (colon != std::string::npos && StringUtils::EqualsNoCase(line.substr(0, colon), name))
      {
        const size_t first = line.find_first_not_of(" \t", colon + 1);
        if (first == std::string::npos)
          return std::string();
        return line.substr(first, line.find_last_not_of(" \t") - first + 1);
      }
      pos = end;
    }
    return std::string();
  };

  if (StringUtils::StartsWithNoCase(headers, "HTTP/1.1 302 "))
  {
    const std::string location = headerValue("Location");
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    return !location.empty() && Download(location, strDownloadPath, onProgress);
  }
  // This downloader accepts plain response bodies, not chunked transfer framing.
  if (!StringUtils::StartsWithNoCase(headers, "HTTP/1.1 200 ") ||
      !headerValue("Transfer-Encoding").empty())
  {
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    return false;
  }

  const std::string length = headerValue("Content-Length");
  uint64_t totalBytes = 0;
  const bool hasLength = !length.empty();
  if (hasLength)
  {
    char* end = nullptr;
    errno = 0;
    totalBytes = strtoull(length.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' || length[0] < '0' || length[0] > '9')
    {
      mbedtls_ssl_close_notify(&ssl);
      mbedtls_net_free(&server_fd);
      return false;
    }
  }

  HANDLE hFile = CreateFileA(strDownloadPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    return false;
  }

  uint64_t downloadedBytes = 0;
  unsigned int lastProgress = 0;
  const auto writeBody = [&](const char* data, size_t size)
  {
    if (hasLength && size > totalBytes - downloadedBytes)
      return false;
    DWORD written = 0;
    if (size && (!WriteFile(hFile, data, size, &written, NULL) || written != size))
      return false;
    downloadedBytes += size;
    if (onProgress && totalBytes > 0)
    {
      const auto progress =
          static_cast<unsigned int>(std::min(99.0, 100.0 * downloadedBytes / totalBytes));
      if (progress > lastProgress)
      {
        lastProgress = progress;
        onProgress(progress);
      }
    }
    return true;
  };

  const size_t bodyStart = headerEnd + 4;
  bool success = writeBody(response.data() + bodyStart, response.size() - bodyStart);
  while (success && (!hasLength || downloadedBytes < totalBytes))
  {
    ret = mbedtls_ssl_read(&ssl, (unsigned char*)buffer, sizeof(buffer));
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
      continue;
    if (ret <= 0)
    {
      success = (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) &&
                (!hasLength || downloadedBytes == totalBytes);
      break;
    }
    success = writeBody(buffer, ret);
  }

  if (!CloseHandle(hFile))
    success = false;

  mbedtls_ssl_close_notify(&ssl);
  mbedtls_net_free(&server_fd);

  if (success && onProgress)
    onProgress(100);

  return success;
}
