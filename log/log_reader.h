#ifndef LOG_LOG_READER_H
#define LOG_LOG_READER_H

#include <cstddef>
#include <cstdio>
#include <iterator>
#include <string>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "log.pb.h"

namespace witnesskvs::log {

class LogReader {
 public:
  LogReader() = delete;
  // Create a LogReader on the specified file.
  LogReader(std::string filename);

  // Disable copy (and move) semantics.
  LogReader(const LogReader&) = delete;
  LogReader& operator=(const LogReader&) = delete;
  ~LogReader();

  absl::StatusOr<Log::Header> header();

  std::string& filename() { return filename_; }

  struct iterator {
    using difference_type = std::ptrdiff_t;

   private:
    LogReader* log_reader;
    long pos;
    std::unique_ptr<Log::Message> cur;
    void next();
    void reset();

   public:
    iterator() { LOG(FATAL) << "not implemented."; };
    iterator(LogReader* lr);
    iterator(const iterator& it) {
      pos = it.pos;
      log_reader = it.log_reader;
    }
    iterator& operator=(iterator& other) {
      pos = other.pos;
      log_reader = other.log_reader;
      return *this;
    }
    iterator& operator=(iterator&& other) {
      pos = other.pos;
      log_reader = other.log_reader;
      cur = std::move(other.cur);
      return *this;
    }
    iterator(iterator&& it) {
      pos = it.pos;
      log_reader = it.log_reader;
      cur = std::move(it.cur);
    }
    iterator(LogReader* lr, std::unique_ptr<Log::Message> sentinel);
    Log::Message& operator*() { return *cur; }
    iterator& operator++() {
      next();
      return *this;
    }
    void operator++(int) { ++*this; }
    bool operator==(const iterator& it) const {
      return it.log_reader == log_reader && it.pos == pos && cur == nullptr &&
             it.cur == nullptr;
    }
  };
  static_assert(std::input_or_output_iterator<iterator>);

  iterator begin() { return iterator(this); }
  iterator end() { return iterator(this, nullptr); }

  // Returns the next message if any, or an error if not.
  absl::StatusOr<Log::Message> next() ABSL_LOCKS_EXCLUDED(lock_);

 private:
  // Returns the position of the header or
  absl::StatusOr<Log::Message> NextLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  absl::StatusOr<long> ReadHeader() ABSL_LOCKS_EXCLUDED(lock_);
  void MaybeSeekLocked(long pos) ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  absl::StatusOr<uint64_t> ReadUInt64Locked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  absl::StatusOr<uint32_t> ReadCRC32Locked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  absl::StatusOr<uint64_t> ReadIdxLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  absl::StatusOr<std::unique_ptr<char[]>> ReadBufferLocked(uint64_t size,
                                                           uint32_t crc32_val)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Reads the next message from the file position specified, incrementing the
  // position.
  absl::StatusOr<Log::Message> ReadNextMessage(long& pos)
      ABSL_LOCKS_EXCLUDED(lock_);
  std::string filename_;
  mutable absl::Mutex lock_;

  // Technically we could just mark this class as non-threadsafe, but with the
  // iterator paradigm you could still have two iterators trying to read through
  // the log at the same time. Guarding all the state by the lock is safer in
  // that respect just in case we want to respect the standard notion of being
  // able to safely read from an iterator without thinking about concurrency,
  // and more importantly be able to have two separate iterators open at the
  // same time, even if by accident (e.g. one was just not destructed yet).
  std::FILE* f_ ABSL_GUARDED_BY(lock_);
  // Position after reading the header.
  long pos_header_ ABSL_GUARDED_BY(lock_);
  // Current position in f_
  long pos_ ABSL_GUARDED_BY(lock_);
  long last_pos_ ABSL_GUARDED_BY(lock_);
  Log::Header header_ ABSL_GUARDED_BY(lock_);
};

}  // namespace witnesskvs::log

#endif
