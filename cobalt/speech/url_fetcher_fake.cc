// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "cobalt/speech/url_fetcher_fake.h"

#if defined(ENABLE_FAKE_MICROPHONE)

#include "base/basictypes.h"
#include "base/sys_byteorder.h"
#include "cobalt/speech/google_streaming_api.pb.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "starboard/common/log.h"

namespace cobalt {
namespace speech {
namespace {
const int kDownloadTimerInterval = 100;

struct RecognitionAlternative {
  const char* transcript;
  float confidence;
};

const RecognitionAlternative kAlternatives_1[] = {
    {"tube", 0.0f},
};

const RecognitionAlternative kAlternatives_2[] = {
    {"to be", 0.0f},
};

const RecognitionAlternative kAlternatives_3[] = {
    {" or not", 0.0f},
};

const RecognitionAlternative kAlternatives_4[] = {
    {" or not to be", 0.0f}, {"to be", 0.0f},
};

const RecognitionAlternative kAlternatives_5[] = {
    {"to be or not to be", 0.728f}, {"2 B or not to be", 0.0f},
};

const RecognitionAlternative kAlternatives_6[] = {
    {"that", 0.0f}, {"is the", 0.0f},
};

const RecognitionAlternative kAlternatives_7[] = {
    {"that", 0.0f}, {"is the question", 0.0f},
};

const RecognitionAlternative kAlternatives_8[] = {
    {"that is", 0.0f}, {"the question", 0.0f},
};

const RecognitionAlternative kAlternatives_9[] = {
    {"that is the question", 0.8577f}, {"that is a question", 0.0f},
};

struct RecognitionResult {
  const RecognitionAlternative* alternatives;
  bool final;
  size_t number_of_alternatives;
};

const RecognitionResult kRecognitionResults[] = {
    {kAlternatives_1, false, SB_ARRAY_SIZE_INT(kAlternatives_1)},
    {kAlternatives_2, false, SB_ARRAY_SIZE_INT(kAlternatives_2)},
    {kAlternatives_3, false, SB_ARRAY_SIZE_INT(kAlternatives_3)},
    {kAlternatives_4, false, SB_ARRAY_SIZE_INT(kAlternatives_4)},
    {kAlternatives_5, true, SB_ARRAY_SIZE_INT(kAlternatives_5)},
    {kAlternatives_6, false, SB_ARRAY_SIZE_INT(kAlternatives_6)},
    {kAlternatives_7, false, SB_ARRAY_SIZE_INT(kAlternatives_7)},
    {kAlternatives_8, false, SB_ARRAY_SIZE_INT(kAlternatives_8)},
    {kAlternatives_9, true, SB_ARRAY_SIZE_INT(kAlternatives_9)},
};

std::string GetMockProtoResult(int index) {
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  proto_result->set_final(kRecognitionResults[index].final);
  const RecognitionAlternative* recognition_alternatives =
      kRecognitionResults[index].alternatives;

  for (size_t i = 0; i < kRecognitionResults[index].number_of_alternatives;
       ++i) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    proto_alternative->set_confidence(recognition_alternatives[i].confidence);
    proto_alternative->set_transcript(recognition_alternatives[i].transcript);
  }

  std::string response_string;
  proto_event.SerializeToString(&response_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  uint32_t prefix =
      base::HostToNet32(static_cast<uint32_t>(response_string.size()));
  response_string.insert(0, reinterpret_cast<char*>(&prefix), sizeof(prefix));
  return response_string;
}

}  // namespace

URLFetcherFake::URLFetcherFake(const GURL& url,
                               net::URLFetcher::RequestType request_type,
                               net::URLFetcherDelegate* delegate)
    : original_url_(url),
      delegate_(delegate),
      is_chunked_upload_(false),
      download_index_(0) {}

URLFetcherFake::~URLFetcherFake() {}

void URLFetcherFake::SetChunkedUpload(const std::string& upload_content_type) {
  is_chunked_upload_ = true;
}

void URLFetcherFake::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  SB_DCHECK(is_chunked_upload_);
  // no-op.
}

void URLFetcherFake::SetRequestContext(
    net::URLRequestContextGetter* request_context_getter) {
  // no-op.
}

void URLFetcherFake::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_chunked_upload_) {
    download_timer_.emplace();
    download_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kDownloadTimerInterval),
        this, &URLFetcherFake::OnURLFetchDownloadData);
  }
}

const net::URLRequestStatus& URLFetcherFake::GetStatus() const {
  return fake_status_;
}

int URLFetcherFake::GetResponseCode() const { return 200; }

void URLFetcherFake::OnURLFetchDownloadData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SB_DCHECK(!is_chunked_upload_);
  std::string actual_response_string = GetMockProtoResult(download_index_);
  int64_t response_size = actual_response_string.length();
  if (response_data_writer_) {
    response_data_writer_->Write(
        scoped_refptr<net::WrappedIOBuffer>(
            new net::WrappedIOBuffer(actual_response_string.data())),
        static_cast<int>(response_size), net::CompletionOnceCallback());
  } else {
    DLOG(WARNING) << "No response writer, mock proto result will be lost";
  }
  delegate_->OnURLFetchDownloadProgress(this, response_size, response_size,
                                        response_size);
  download_index_++;
  if (download_index_ ==
      static_cast<int>(SB_ARRAY_SIZE_INT(kRecognitionResults))) {
    download_index_ = 0;
    download_timer_->Stop();
  }
}

}  // namespace speech
}  // namespace cobalt

#endif  // defined(ENABLE_FAKE_MICROPHONE)
