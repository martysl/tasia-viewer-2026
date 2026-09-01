/**
 * @file tasiafeedconnect.cpp
 * @brief TasiaFeed snapshot upload implementation
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

#include "llviewerprecompiledheaders.h"

#include "tasiafeedconnect.h"

#include "llagent.h"
#include "llagentui.h"
#include "llbase64.h"
#include "llcorehttputil.h"
#include "llfilesystem.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llsd.h"
#include "llslurl.h"
#include "lltrans.h"
#include "lluri.h"
#include "llversioninfo.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewerparcelmgr.h"

#include "fscorehttputil.h"
#include "lfsimfeaturehandler.h"

#include <boost/bind.hpp>

const std::string TASIAFEED_UPLOAD_URL = "https://api.tasiaviewer.work/feed/api/v1/snapshots/upload.php";

void TasiaFeedUploadResponse(LLSD const& aData, TasiaFeedUpload::response_callback_t aCallback)
{
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);

    LL_INFOS("TasiaFeed") << "Upload response status: " << status.getType() << LL_ENDL;

    // postJsonAndSuspend + HttpCoroJSONHandler returns parsed JSON keys directly in aData.
    // Keys: success (bool), post_url (string), message (string), plus http_result.
    if (aData.has("success") && aData["success"].asBoolean())
    {
        LL_INFOS("TasiaFeed") << "Upload successful: " << aData["post_url"].asString() << LL_ENDL;
        if (aCallback)
            aCallback(true, aData);
    }
    else
    {
        std::string err_msg = aData.has("message") ? aData["message"].asString() : "No response from server";
        LL_WARNS("TasiaFeed") << "Upload failed: " << err_msg << LL_ENDL;
        if (aCallback)
        {
            LLSD error;
            error["message"] = err_msg;
            aCallback(false, error);
        }
    }
}

void TasiaFeedUploadCoro(std::string url, LLSD body, TasiaFeedUpload::response_callback_t callback)
{
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("TasiaFeedUpload", LLCore::HttpRequest::DEFAULT_POLICY_ID));
    LLCore::HttpHeaders::ptr_t httpHeaders(new LLCore::HttpHeaders);
    LLCore::HttpOptions::ptr_t httpOptions(new LLCore::HttpOptions);

    httpHeaders->append("Content-Type", "application/json");

    LLSD result = httpAdapter->postJsonAndSuspend(httpRequest, url, body, httpOptions, httpHeaders);

    TasiaFeedUploadResponse(result, callback);
}

// static
void TasiaFeedUpload::uploadSnapshot(const LLSD& metadata, LLImageFormatted* image, response_callback_t callback)
{
    if (!image)
    {
        LL_WARNS("TasiaFeed") << "No image data" << LL_ENDL;
        if (callback)
        {
            LLSD error;
            error["message"] = "No image data";
            callback(false, error);
        }
        return;
    }

    // Decode formatted image to raw
    LLPointer<LLImageRaw> raw_image = new LLImageRaw;
    if (!image->decode(raw_image, 0.0f))
    {
        LL_WARNS("TasiaFeed") << "Failed to decode image" << LL_ENDL;
        if (callback)
        {
            LLSD error;
            error["message"] = "Failed to decode image";
            callback(false, error);
        }
        return;
    }

    // Encode raw image to JPEG
    LLPointer<LLImageJPEG> jpeg = new LLImageJPEG;
    if (!jpeg->encode(raw_image, 85.0f))
    {
        LL_WARNS("TasiaFeed") << "Failed to encode image as JPEG" << LL_ENDL;
        if (callback)
        {
            LLSD error;
            error["message"] = "Failed to encode image";
            callback(false, error);
        }
        return;
    }

    // Base64-encode the JPEG data
    std::string image_base64 = LLBase64::encode(reinterpret_cast<const U8*>(jpeg->getData()), jpeg->getDataSize());

    // Build the JSON body
    LLSD body;
    body["image"] = image_base64;
    body["title"] = metadata["title"];
    body["description"] = metadata["description"];
    body["visibility"] = metadata["visibility"];
    body["maturity"] = metadata["maturity"];
    body["avatar_name"] = metadata["avatar_name"];
    body["grid_name"] = metadata["grid_name"];
    body["region_name"] = metadata["region_name"];
    body["position"] = metadata["position"];
    body["viewer_ver"] = metadata["viewer_ver"];
    body["commit_sha"] = metadata["commit_sha"];
    body["build_num"] = metadata["build_num"];
    body["user_uuid"] = metadata["user_uuid"];

    // Launch upload coroutine
    LLCoros::instance().launch("TasiaFeedUploadCoro",
        boost::bind(&TasiaFeedUploadCoro, TASIAFEED_UPLOAD_URL, body, callback));
}
