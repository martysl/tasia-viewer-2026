/**
 * @file tasiafeedconnect.h
 * @brief TasiaFeed snapshot upload API client
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Tasia Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */

#ifndef TASIA_FEED_CONNECT_H
#define TASIA_FEED_CONNECT_H

#include "llimage.h"
#include "llsd.h"
#include "llsingleton.h"
#include <boost/function.hpp>
#include <string>

class TasiaFeedUpload
{
public:
    typedef boost::function<void(bool success, const LLSD& response)> response_callback_t;

    static void uploadSnapshot(const LLSD& metadata, LLImageFormatted* image, response_callback_t callback);
};

#endif
