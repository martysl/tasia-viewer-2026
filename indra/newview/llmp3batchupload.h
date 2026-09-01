/**
 * @file llmp3batchupload.h
 * @brief Cost-safe MP3 to standard sound batch uploader.
 */
#ifndef LL_MP3_BATCH_UPLOAD_H
#define LL_MP3_BATCH_UPLOAD_H

#include <vector>
#include <string>

void start_mp3_batch_sound_upload();
void mp3_batch_sound_file_picked(const std::vector<std::string>& filenames);

#endif
