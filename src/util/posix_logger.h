//  Copyright (c) 2014, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Logger implementation that can be shared by all environments
// where enough posix functionality is available.

#pragma once
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef OS_LINUX
#include <linux/falloc.h>
#endif
#include <algorithm>
#include <atomic>
#include <cassert>
#include <string>
#include "include/Env.h"

namespace rocketspeed {

const int kDebugLogChunkSize = 128 * 1024;

class PosixLogger : public Logger {
 private:
  FILE* file_;
  uint64_t (*gettid_)();  // Return the thread id for the current thread
  std::string (*gettname_)();  // Return the thread name for the current thread
  std::atomic_size_t log_size_;
  int fd_;
  const static uint64_t flush_every_seconds_ = 5;
  std::atomic_uint_fast64_t last_flush_micros_;
  Env* env_;
  bool flush_pending_;
 public:
  PosixLogger(FILE* f, uint64_t (*gettid)(), std::string (*gettname)(),
              Env* env,
              const InfoLogLevel log_level = InfoLogLevel::ERROR_LEVEL)
      : Logger(log_level),
        file_(f),
        gettid_(gettid),
        gettname_(gettname),
        log_size_(0),
        fd_(fileno(f)),
        last_flush_micros_(0),
        env_(env),
        flush_pending_(false) {}
  virtual ~PosixLogger() {
    fclose(file_);
  }
  virtual void Flush() {
    if (flush_pending_) {
      flush_pending_ = false;
      fflush(file_);
    }
    last_flush_micros_ = env_->NowMicros();
  }
  virtual void Logv(const char* format, va_list ap) {
    const uint64_t thread_id = (*gettid_)();
    const char* name = (*gettname_)().c_str();

    // We try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 30000;
        base = new char[bufsize];
      }
      char* p = base;
      char* limit = base + bufsize;

      struct timeval now_tv;
      gettimeofday(&now_tv, nullptr);
      const time_t seconds = now_tv.tv_sec;
      struct tm t;
      localtime_r(&seconds, &t);
      p += snprintf(p, limit - p,
                    "%02d/%02d-%02d:%02d:%02d.%06d %llx %-11s ",
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    static_cast<int>(now_tv.tv_usec),
                    static_cast<long long unsigned int>(thread_id),
                    name);

      // Print the message
      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // Truncate to available space if necessary
      if (p >= limit) {
        if (iter == 0) {
          continue;       // Try again with larger buffer
        } else {
          p = limit - 1;
        }
      }

      // Add newline if necessary
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      const size_t write_size = p - base;

#ifdef ROCKETSPEED_FALLOCATE_PRESENT
      // If this write would cross a boundary of kDebugLogChunkSize
      // space, pre-allocate more space to avoid overly large
      // allocations from filesystem allocsize options.
      const size_t log_size = log_size_;
      const int last_allocation_chunk =
        ((kDebugLogChunkSize - 1 + log_size) / kDebugLogChunkSize);
      const int desired_allocation_chunk =
        ((kDebugLogChunkSize - 1 + log_size + write_size) /
           kDebugLogChunkSize);
      if (last_allocation_chunk != desired_allocation_chunk) {
        fallocate(fd_, FALLOC_FL_KEEP_SIZE, 0,
                  desired_allocation_chunk * kDebugLogChunkSize);
      }
#endif

      size_t sz = fwrite(base, 1, write_size, file_);
      flush_pending_ = true;
      assert(sz == write_size);
      if (sz > 0) {
        log_size_ += write_size;
      }
      uint64_t now_micros = static_cast<uint64_t>(now_tv.tv_sec) * 1000000 +
        now_tv.tv_usec;
      if (now_micros - last_flush_micros_ >= flush_every_seconds_ * 1000000) {
        flush_pending_ = false;
        fflush(file_);
        last_flush_micros_ = now_micros;
      }
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }
  size_t GetLogFileSize() const {
    return log_size_;
  }
};

}  // namespace rocketspeed
