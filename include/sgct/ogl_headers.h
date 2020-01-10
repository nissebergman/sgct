/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2020                                                               *
 * For conditions of distribution and use, see copyright notice in LICENSE.md            *
 ****************************************************************************************/

#ifndef __SGCT__OGLHEADERS__H__
#define __SGCT__OGLHEADERS__H__

#ifdef __APPLE__
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#undef __gl_h_
#endif

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <Windows.h>
#endif // WIN32

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#endif // __SGCT__OGLHEADERS__H__
