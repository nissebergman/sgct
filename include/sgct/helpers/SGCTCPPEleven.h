/*************************************************************************
Copyright (c) 2012-2014 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#ifndef _SGCT_CPP_ELEVEN
#define _SGCT_CPP_ELEVEN

/*
The macros below set the propper c++ includes and namespaces
*/
#if (defined(__cplusplus) && __cplusplus > 201103L) || _MSC_VER >= 1600 //c++11 compiler or visual studio later than 2010
#define __USE_CPP11__ 1
#define __USE_CPP0X__ 0
#define __LOAD_CPP11_FUN__
#define SGCT_NULL_PTR nullptr 

#elif (defined(__cplusplus) && __cplusplus > 199711L) || _MSC_VER >= 1400 //c++0x compiler or visual studio 2008 - 2010
#define __USE_CPP11__ 0
#define __USE_CPP0X__ 1
#define __LOAD_CPP11_FUN__
#define SGCT_NULL_PTR NULL

#else //older compiler
#define __USE_CPP11__ 0
#define __USE_CPP0X__ 0
#define SGCT_NULL_PTR NULL
#endif

#if __USE_CPP11__
#include <functional>
	#if defined(__APPLE__) && defined(_GLIBCXX_FUNCTIONAL) //incorrect header loaded
	#include <tr1/functional>
	namespace sgct_cppxeleven = std::tr1;
	//#pragma message "SGCTNetwork will use std:tr1::functional"
	#else
	namespace sgct_cppxeleven = std;
	//#pragma message "SGCTNetwork will use std::functional"
	#endif

#elif __USE_CPP0X__ && defined(__APPLE__)
#include <tr1/functional>
namespace sgct_cppxeleven = std::tr1;
//#pragma message "SGCTNetwork will use std::tr1::functional"

#elif __USE_CPP0X__ && defined(__LINUX__)
#include <tr1/functional>
namespace sgct_cppxeleven = std::tr1;
//#pragma message "SGCTNetwork will use std:tr1::functional"

#elif __USE_CPP0X__ && !defined(__LINUX__)
#include <functional>
namespace sgct_cppxeleven = std::tr1;
//#pragma message "SGCTNetwork will use std::tr1::functional"

#endif

#endif
