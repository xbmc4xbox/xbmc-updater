/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
//-----------------------------------------------------------------------
//
//  File:      StringUtils.cpp
//
//  Purpose:   ATL split string utility
//  Author:    Paul J. Weiss
//
//  Modified to use J O'Leary's std::string class by kraqh3d
//
//------------------------------------------------------------------------

#include "StringUtils.h"

using namespace std;

#define FORMAT_BLOCK_SIZE 512 // # of bytes for initial allocation for printf

string StringUtils::Format(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  string str = FormatV(fmt, args);
  va_end(args);

  return str;
}

string StringUtils::FormatV(const char *fmt, va_list args)
{
  if (!fmt || !fmt[0])
    return "";

  int size = FORMAT_BLOCK_SIZE;
  va_list argCopy;

  while (1)
  {
    char *cstr = reinterpret_cast<char*>(malloc(sizeof(char) * size));
    if (!cstr)
      return "";

    va_copy(argCopy, args);
    int nActual = vsnprintf(cstr, size, fmt, argCopy);
    va_end(argCopy);

    if (nActual > -1 && nActual < size) // We got a valid result
    {
      std::string str(cstr, nActual);
      free(cstr);
      return str;
    }
    free(cstr);
#ifndef TARGET_WINDOWS
    if (nActual > -1)                   // Exactly what we will need (glibc 2.1)
      size = nActual + 1;
    else                                // Let's try to double the size (glibc 2.0)
      size *= 2;
#else  // TARGET_WINDOWS
    va_copy(argCopy, args);
    size = _vscprintf(fmt, argCopy);
    va_end(argCopy);
    if (size < 0)
      return "";
    else
      size++; // increment for null-termination
#endif // TARGET_WINDOWS
  }

  return ""; // unreachable
}

bool StringUtils::EqualsNoCase(const std::string &str1, const std::string &str2)
{
  // before we do the char-by-char comparison, first compare sizes of both strings.
  // This led to a 33% improvement in benchmarking on average. (size() just returns a member of std::string)
  if (str1.size() != str2.size())
    return false;
  return EqualsNoCase(str1.c_str(), str2.c_str());
}

bool StringUtils::EqualsNoCase(const std::string &str1, const char *s2)
{
  return EqualsNoCase(str1.c_str(), s2);
}

bool StringUtils::EqualsNoCase(const char *s1, const char *s2)
{
  char c2; // we need only one char outside the loop
  do
  {
    const char c1 = *s1++; // const local variable should help compiler to optimize
    c2 = *s2++;
    if (c1 != c2 && ::tolower(c1) != ::tolower(c2)) // This includes the possibility that one of the characters is the null-terminator, which implies a string mismatch.
      return false;
  } while (c2 != '\0'); // At this point, we know c1 == c2, so there's no need to test them both.
  return true;
}

int StringUtils::Replace(string &str, char oldChar, char newChar)
{
  int replacedChars = 0;
  for (string::iterator it = str.begin(); it != str.end(); ++it)
  {
    if (*it == oldChar)
    {
      *it = newChar;
      replacedChars++;
    }
  }

  return replacedChars;
}

int StringUtils::Replace(std::string &str, const std::string &oldStr, const std::string &newStr)
{
  if (oldStr.empty())
    return 0;

  int replacedChars = 0;
  size_t index = 0;

  while (index < str.size() && (index = str.find(oldStr, index)) != string::npos)
  {
    str.replace(index, oldStr.size(), newStr);
    index += newStr.size();
    replacedChars++;
  }

  return replacedChars;
}

bool StringUtils::StartsWithNoCase(const std::string &str1, const std::string &str2)
{
  return StartsWithNoCase(str1.c_str(), str2.c_str());
}

bool StringUtils::StartsWithNoCase(const std::string &str1, const char *s2)
{
  return StartsWithNoCase(str1.c_str(), s2);
}

bool StringUtils::StartsWithNoCase(const char *s1, const char *s2)
{
  while (*s2 != '\0')
  {
    if (::tolower(*s1) != ::tolower(*s2))
      return false;
    s1++;
    s2++;
  }
  return true;
}

std::vector<std::string> StringUtils::Split(const std::string& input, const std::string& delimiter, unsigned int iMaxStrings /* = 0 */)
{
  std::vector<std::string> results;
  if (input.empty())
    return results;
  if (delimiter.empty())
  {
    results.push_back(input);
    return results;
  }

  const size_t delimLen = delimiter.length();
  size_t nextDelim;
  size_t textPos = 0;
  do
  {
    if (--iMaxStrings == 0)
    {
      results.push_back(input.substr(textPos));
      break;
    }
    nextDelim = input.find(delimiter, textPos);
    results.push_back(input.substr(textPos, nextDelim - textPos));
    textPos = nextDelim + delimLen;
  } while (nextDelim != std::string::npos);

  return results;
}

std::vector<std::string> StringUtils::Split(const std::string& input, const char delimiter, size_t iMaxStrings /*= 0*/)
{
  std::vector<std::string> results;
  if (input.empty())
    return results;

  size_t nextDelim;
  size_t textPos = 0;
  do
  {
    if (--iMaxStrings == 0)
    {
      results.push_back(input.substr(textPos));
      break;
    }
    nextDelim = input.find(delimiter, textPos);
    results.push_back(input.substr(textPos, nextDelim - textPos));
    textPos = nextDelim + 1;
  } while (nextDelim != std::string::npos);

  return results;
}
