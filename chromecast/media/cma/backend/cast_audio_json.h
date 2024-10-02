// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromecast {
namespace media {

class CastAudioJson {
 public:
  // Returns GetFilePathForTuning() if a file exists at that path, otherwise
  // returns GetReadOnlyFilePath().
  static base::FilePath GetFilePath();
  static base::FilePath GetReadOnlyFilePath();
  static base::FilePath GetFilePathForTuning();
};

// Provides an interface for reading CastAudioJson and registering for file
// updates.
class CastAudioJsonProvider {
 public:
  using TuningChangedCallback =
      base::RepeatingCallback<void(absl::optional<base::Value::Dict> contents)>;

  virtual ~CastAudioJsonProvider() = default;

  // Returns the contents of cast_audio.json.
  // If a file exists at CastAudioJson::GetFilePathForTuning() and is valid
  // JSON, its contents will be returned. Otherwise, the contents of the file
  // at CastAudioJson::GetReadOnlyFilePath() will be returned.
  // This function will run on the thread on which it is called, and may
  // perform blocking I/O.
  virtual absl::optional<base::Value::Dict> GetCastAudioConfig() = 0;

  // |callback| will be called when a new cast_audio config is available.
  // |callback| will always be called from the same thread, but not necessarily
  // the same thread on which |SetTuningChangedCallback| is called.
  virtual void SetTuningChangedCallback(TuningChangedCallback callback) = 0;
};

class CastAudioJsonProviderImpl : public CastAudioJsonProvider {
 public:
  CastAudioJsonProviderImpl();

  CastAudioJsonProviderImpl(const CastAudioJsonProviderImpl&) = delete;
  CastAudioJsonProviderImpl& operator=(const CastAudioJsonProviderImpl&) =
      delete;

  ~CastAudioJsonProviderImpl() override;

 private:
  class FileWatcher {
   public:
    FileWatcher();
    ~FileWatcher();

    void SetTuningChangedCallback(TuningChangedCallback callback);

   private:
    base::FilePathWatcher watcher_;
  };

  // CastAudioJsonProvider implementation:
  absl::optional<base::Value::Dict> GetCastAudioConfig() override;
  void SetTuningChangedCallback(TuningChangedCallback callback) override;

  base::SequenceBound<FileWatcher> cast_audio_watcher_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
