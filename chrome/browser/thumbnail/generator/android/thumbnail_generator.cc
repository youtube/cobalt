// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/generator/android/thumbnail_generator.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser.h"
#include "chrome/browser/thumbnail/generator/jni_headers/ThumbnailGenerator_jni.h"
#include "chrome/browser/thumbnail/generator/thumbnail_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/android/java_bitmap.h"

class SkBitmap;

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace {

void ForwardJavaCallback(const ScopedJavaGlobalRef<jobject>& java_delegate,
                         const ScopedJavaGlobalRef<jstring>& content_id,
                         int icon_size,
                         const ScopedJavaGlobalRef<jobject>& callback,
                         SkBitmap thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ThumbnailGenerator_onThumbnailRetrieved(
      env, java_delegate, content_id, icon_size,
      thumbnail.drawsNothing() ? nullptr : gfx::ConvertToJavaBitmap(thumbnail),
      callback);
}

void OnThumbnailScaled(base::OnceCallback<void(SkBitmap)> java_callback,
                       SkBitmap scaled_thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(java_callback).Run(std::move(scaled_thumbnail));
}

}  // namespace

ThumbnailGenerator::ThumbnailGenerator(const JavaParamRef<jobject>& jobj)
    : java_delegate_(jobj) {
  DCHECK(!jobj.is_null());
}

ThumbnailGenerator::~ThumbnailGenerator() = default;

void ThumbnailGenerator::Destroy(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

void ThumbnailGenerator::OnImageThumbnailRetrieved(
    base::OnceCallback<void(SkBitmap)> java_callback,
    const SkBitmap& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Send the bitmap back to Java-land.
  std::move(java_callback).Run(std::move(thumbnail));
}

void ThumbnailGenerator::OnVideoThumbnailRetrieved(
    base::OnceCallback<void(SkBitmap)> java_callback,
    int icon_size,
    std::unique_ptr<ThumbnailMediaParser> parser,
    bool success,
    chrome::mojom::MediaMetadataPtr media_metadata,
    SkBitmap thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Scale the bitmap before sending back to Java.
  ScaleDownBitmap(icon_size, std::move(thumbnail),
                  base::BindOnce(&OnThumbnailScaled, std::move(java_callback)));

  // We want to delete |parser| but can't do it immediately because current
  // stack contains functions that belong to ThumbnailMediaParser and the
  // VideoDecoder that the parser owens. This would cause use-after-free.
  // That's why |parser|'s destruction is postponed till the current task
  // is completed and the call stack doesn't have frames referencing memory
  // owned by |parser|
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             std::move(parser));
}

void ThumbnailGenerator::RetrieveThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& jcontent_id,
    const JavaParamRef<jstring>& jfile_path,
    const JavaParamRef<jstring>& jmime_type,
    jint icon_size,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(
      base::android::ConvertJavaStringToUTF8(env, jfile_path));

  std::string mime_type =
      jmime_type.is_null()
          ? ""
          : base::android::ConvertJavaStringToUTF8(env, jmime_type);

  // Bind everything passed back to Java.
  auto java_callback =
      base::BindOnce(&ForwardJavaCallback, java_delegate_,
                     ScopedJavaGlobalRef<jstring>(jcontent_id), icon_size,
                     ScopedJavaGlobalRef<jobject>(callback));

  // Retrieve video thumbnail.
  if (base::StartsWith(mime_type, "video/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    auto parser = ThumbnailMediaParser::Create(mime_type, file_path);
    auto* const parser_ptr = parser.get();
    parser_ptr->Start(
        base::BindOnce(&ThumbnailGenerator::OnVideoThumbnailRetrieved,
                       weak_factory_.GetWeakPtr(), std::move(java_callback),
                       icon_size, std::move(parser)));
    return;
  }

  // Retrieve image thumbnail.
  auto request = std::make_unique<ImageThumbnailRequest>(
      icon_size,
      base::BindOnce(&ThumbnailGenerator::OnImageThumbnailRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(java_callback)));
  request->Start(file_path);

  // Dropping ownership of |request| here because it will clean itself up once
  // the started request finishes.
  request.release();
}

// static
static jlong JNI_ThumbnailGenerator_Init(JNIEnv* env,
                                         const JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new ThumbnailGenerator(jobj));
}
