// Copyright 2020 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tensorstore/internal/curl/curl_wrappers.h"

#include <stddef.h>

#include <string>
#include <string_view>

#include "absl/base/call_once.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include <curl/curl.h>
#include "tensorstore/internal/source_location.h"
#include "tensorstore/util/status.h"
#include "tensorstore/util/str_cat.h"

namespace tensorstore {
namespace internal_http {
namespace {

static absl::once_flag g_init;

// See curl:src/lib/curl_trc.c
static constexpr const char* kCurlTypeStrings[] = {
    ": * ",  // CURLINFO_TEXT,
    ": < ",  // CURLINFO_HEADER_IN,
    ": > ",  // CURLINFO_HEADER_OUT,
    ": ",    // CURLINFO_DATA_IN,
    ": ",    // CURLINFO_DATA_OUT,
    ": ",    // CURLINFO_SSL_DATA_IN,
    ": ",    // CURLINFO_SSL_DATA_OUT,
};

int CurlLogToAbseil(CURL* handle, curl_infotype type, char* data, size_t size,
                    void* userp) {
  switch (type) {
    case CURLINFO_TEXT:
    case CURLINFO_HEADER_OUT:
    case CURLINFO_HEADER_IN:
      break;
    default: /* nada */
      return 0;
  }
  ABSL_LOG(INFO) << handle << kCurlTypeStrings[type]
                 << std::string_view(data, size);
  return 0;
}

}  // namespace

void CurlPtrCleanup::operator()(CURL* c) { curl_easy_cleanup(c); }
void CurlMultiCleanup::operator()(CURLM* m) { curl_multi_cleanup(m); }
void CurlSlistCleanup::operator()(curl_slist* s) { curl_slist_free_all(s); }

/// Returns the default CurlUserAgent.
std::string GetCurlUserAgentSuffix() {
  static std::string agent =
      tensorstore::StrCat("tensorstore/0.1 ", curl_version());
  return agent;
}

/// Returns a absl::Status object for a corresponding CURLcode.
absl::Status CurlCodeToStatus(CURLcode code, std::string_view detail,
                              SourceLocation loc) {
  // Constant errors:
  auto error_code = absl::StatusCode::kUnknown;
  switch (code) {
    case CURLE_OK:
      return absl::OkStatus();

    case CURLE_COULDNT_RESOLVE_PROXY:
      error_code = absl::StatusCode::kUnavailable;
      if (detail.empty()) detail = "Failed to resolve proxy";
      break;

    case CURLE_OPERATION_TIMEDOUT:
      error_code = absl::StatusCode::kDeadlineExceeded;
      if (detail.empty()) detail = "Timed out";
      break;

    case CURLE_COULDNT_CONNECT:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_GOT_NOTHING:
    case CURLE_HTTP2:
    case CURLE_HTTP2_STREAM:
    case CURLE_PARTIAL_FILE:
    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
    case CURLE_SSL_CONNECT_ERROR:
      // Curl sometimes returns `CURLE_UNSUPPORTED_PROTOCOL` in the case of a
      // malformed response.
    case CURLE_UNSUPPORTED_PROTOCOL:
      error_code = absl::StatusCode::kUnavailable;
      break;

    case CURLE_URL_MALFORMAT:
      error_code = absl::StatusCode::kInvalidArgument;
      break;

    case CURLE_WRITE_ERROR:
      error_code = absl::StatusCode::kCancelled;
      break;

    case CURLE_ABORTED_BY_CALLBACK:
      error_code = absl::StatusCode::kAborted;
      break;

    case CURLE_REMOTE_ACCESS_DENIED:
      error_code = absl::StatusCode::kPermissionDenied;
      break;

    case CURLE_SEND_FAIL_REWIND:
    case CURLE_RANGE_ERROR:  // "the server does not *support* range requests".
      error_code = absl::StatusCode::kInternal;
      break;

    // These codes may be returned by curl_easy_setopt / curl_easy_getinfo
    // and almost always indicate a precondition or missing required feature.
    case CURLE_BAD_FUNCTION_ARGUMENT:
    case CURLE_OUT_OF_MEMORY:
    case CURLE_NOT_BUILT_IN:
    case CURLE_UNKNOWN_OPTION:
    case CURLE_BAD_DOWNLOAD_RESUME:
      error_code = absl::StatusCode::kInternal;
      break;

    default:
      break;
  }

  absl::Status status(
      error_code, tensorstore::StrCat("CURL error ", curl_easy_strerror(code),
                                      detail.empty() ? "" : ": ", detail));
  status.SetPayload("curl_code", absl::Cord(tensorstore::StrCat(code)));
  MaybeAddSourceLocation(status, loc);
  return status;
}

/// Returns a absl::Status object for a corresponding CURLcode.
absl::Status CurlMCodeToStatus(CURLMcode code, std::string_view detail,
                               SourceLocation loc) {
  if (code == CURLM_OK) {
    return absl::OkStatus();
  }
  absl::Status status(
      absl::StatusCode::kInternal,
      tensorstore::StrCat("CURLM error ", curl_multi_strerror(code),
                          detail.empty() ? "" : ": ", detail));
  status.SetPayload("curlm_code", absl::Cord(tensorstore::StrCat(code)));
  MaybeAddSourceLocation(status, loc);
  return status;
}

void SetLogToAbseil(CURL* handle) {
  ABSL_CHECK_EQ(CURLE_OK, curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION,
                                           CurlLogToAbseil));
}

void CurlInit() {
  // libcurl depends on many different SSL libraries, depending on the library
  // the library might not be thread safe. We defer such considerations for
  // now. https://curl.haxx.se/libcurl/c/threadsafe.html
  //
  // Automatically initialize the libcurl library.
  // Don't call `curl_global_cleanup()` since it is pointless anyway and
  // potentially leads to order of destruction race conditions.
  absl::call_once(g_init, [] { curl_global_init(CURL_GLOBAL_ALL); });
}

}  // namespace internal_http
}  // namespace tensorstore
