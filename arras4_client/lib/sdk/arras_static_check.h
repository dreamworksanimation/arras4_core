// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ---------------------------------------------------------------------------

#ifndef ARRAS_STATIC_CHECK_INCLUDED
#define ARRAS_STATIC_CHECK_INCLUDED

// This is included in arras4_client to avoid a dependency on
// arras_common : it is the only file still needed from that
// code base. It is also available in the foundation folio
//
// TODO: decide where this really goes


#if defined(__ICC)

// use these defines to bracket a region of code that has safe static
// accesses. Keep the region as small as possible
#define ARRAS_START_THREADSAFE_STATIC_REFERENCE   __pragma(warning(disable:1710))
#define ARRAS_FINISH_THREADSAFE_STATIC_REFERENCE  __pragma(warning(default:1710))
#define ARRAS_START_THREADSAFE_STATIC_WRITE       __pragma(warning(disable:1711))
#define ARRAS_FINISH_THREADSAFE_STATIC_WRITE      __pragma(warning(default:1711))
#define ARRAS_START_THREADSAFE_STATIC_ADDRESS     __pragma(warning(disable:1712))
#define ARRAS_FINISH_THREADSAFE_STATIC_ADDRESS    __pragma(warning(default:1712))

// use these defines to bracket a region of code that has unsafe
// static accesses. Keep the region as small as possible
#define ARRAS_START_NON_THREADSAFE_STATIC_REFERENCE   __pragma(warning(disable:1710))
#define ARRAS_FINISH_NON_THREADSAFE_STATIC_REFERENCE  __pragma(warning(default:1710))
#define ARRAS_START_NON_THREADSAFE_STATIC_WRITE       __pragma(warning(disable:1711))
#define ARRAS_FINISH_NON_THREADSAFE_STATIC_WRITE      __pragma(warning(default:1711))
#define ARRAS_START_NON_THREADSAFE_STATIC_ADDRESS     __pragma(warning(disable:1712))
#define ARRAS_FINISH_NON_THREADSAFE_STATIC_ADDRESS    __pragma(warning(default:1712))

// simpler version for one-line cases
#define ARRAS_THREADSAFE_STATIC_REFERENCE(CODE) __pragma(warning(disable:1710)); CODE; __pragma(warning(default:1710))
#define ARRAS_THREADSAFE_STATIC_WRITE(CODE)     __pragma(warning(disable:1711)); CODE; __pragma(warning(default:1711))
#define ARRAS_THREADSAFE_STATIC_ADDRESS(CODE)   __pragma(warning(disable:1712)); CODE; __pragma(warning(default:1712))

#else // gcc does not support these compiler warnings

#define ARRAS_START_THREADSAFE_STATIC_REFERENCE
#define ARRAS_FINISH_THREADSAFE_STATIC_REFERENCE
#define ARRAS_START_THREADSAFE_STATIC_WRITE
#define ARRAS_FINISH_THREADSAFE_STATIC_WRITE
#define ARRAS_START_THREADSAFE_STATIC_ADDRESS
#define ARRAS_FINISH_THREADSAFE_STATIC_ADDRESS

#define ARRAS_START_NON_THREADSAFE_STATIC_REFERENCE
#define ARRAS_FINISH_NON_THREADSAFE_STATIC_REFERENCE
#define ARRAS_START_NON_THREADSAFE_STATIC_WRITE
#define ARRAS_FINISH_NON_THREADSAFE_STATIC_WRITE
#define ARRAS_START_NON_THREADSAFE_STATIC_ADDRESS
#define ARRAS_FINISH_NON_THREADSAFE_STATIC_ADDRESS


#define ARRAS_THREADSAFE_STATIC_REFERENCE(CODE) CODE
#define ARRAS_THREADSAFE_STATIC_WRITE(CODE) CODE
#define ARRAS_THREADSAFE_STATIC_ADDRESS(CODE) CODE

#endif

#endif // ARRAS_STATIC_CHECK_INCLUDED

// ---------------------------------------------------------------------------


