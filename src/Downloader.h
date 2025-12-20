/*
 *  Copyright (C) 2025-2025
 *  This file is part of XBMC - https://github.com/antonic901/xbmc4xbox-redux
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <string>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

class CDownloader
{
public:
  CDownloader();
  ~CDownloader();

  bool Get(const std::string& strURL, std::string& strBody);

  bool Download(const std::string& strDownloadLink, const std::string& strDownloadPath);

private:
  void Initialize();
  void Deinitialize();

  mbedtls_net_context server_fd;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;

  bool m_initialized = false;
};
