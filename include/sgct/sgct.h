/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2020                                                               *
 * For conditions of distribution and use, see copyright notice in LICENSE.md            *
 ****************************************************************************************/

#ifndef __SGCT__SGCT__H__
#define __SGCT__SGCT__H__

#include <sgct/actions.h>
#include <sgct/clustermanager.h>
#include <sgct/commandline.h>
#include <sgct/engine.h>
#include <sgct/image.h>
#include <sgct/keys.h>
#include <sgct/log.h>
#include <sgct/networkmanager.h>
#include <sgct/node.h>
#include <sgct/ogl_headers.h>
#include <sgct/shadermanager.h>
#include <sgct/shareddata.h>
#include <sgct/shareddatatypes.h>
#include <sgct/texturemanager.h>

#ifdef SGCT_HAS_TEXT
#include <sgct/font.h>
#include <sgct/fontmanager.h>
#include <sgct/freetype.h>
#endif // SGCT_HAS_TEXT

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#endif // __SGCT__SGCT__H__