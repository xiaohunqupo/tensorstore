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

#ifndef TENSORSTORE_KVSTORE_TRANSACTION_H_
#define TENSORSTORE_KVSTORE_TRANSACTION_H_

/// \file
///
/// Facilities for implementing the transactional operations:
///
/// - `kvstore::Driver::ReadModifyWrite`
///
/// - `kvstore::Driver::TransactionalDeleteRange`
///
/// There are three types of key-value store drivers:
///
/// - Terminal: The `kvstore::Driver` directly accesses the underlying storage
///   without going through any lower-level `kvstore::Driver`.  There are two
///   sub-types of terminal `kvstore::Driver`:
///
///   - Non-atomic: implements `Read`, `Write` (and possibly) `DeleteRange` but
///     not any transactional operations.  The default `Driver::ReadModifyWrite`
///     and `Driver::TransactionalDeleteRange` implementations using
///     `NonAtomicTransactionNode` are used, which are based on the driver's
///     non-transactional `Read`, `Write`, and `DeleteRange` methods.  When used
///     with an atomic-mode transaction, the transaction will fail if more than
///     a single key is affected.  The "file" and "gcs" drivers are in this
///     category.
///
///   - Atomic: The `Driver` implements its own `TransactionNode` class
///     that inherits from `AtomicTransactionNode` and defines `ReadModifyWrite`
///     based on `AddReadModifyWrite` and `TransactionalDeleteRange` based on
///     `AddDeleteRange`.  The driver implements non-transactional `Read` and
///     may either directly implement the non-transactional `Write`, or may use
///     `WriteViaTransaction` to implement it in terms of the transactional
///     `ReadModifyWrite`.  The "memory" driver is in this category.
///
/// - Non-terminal: The `Driver` driver is merely an adapter over an underlying
///   `Driver`; the connection to the underlying `Driver` is provided by
///   `KvsBackedCache`.  The uint64_sharded_key_value_store driver is in this
///   category.
///
/// For all three types of key-value store driver, the `MultiPhaseMutation`
/// class tracks the uncommitted transactional operations in order to provide a
/// virtual view of the result after the transaction is committed:
///
/// In the simple case, the only transactional operations are `ReadModifyWrite`
/// operations to independent keys, all in a single phase (i.e. without any
/// interleaved `Transaction::Barrier` calls), and there is not much to track.
///
/// In general, though, there may be multiple `ReadModifyWrite` operations for
/// the same key, possibly with intervening `Transaction::Barrier` calls, and
/// there may be `TransctionalDeleteRange` operations before or after
/// `ReadModifyWrite` operations that affect some of the same keys.  (Note that
/// `Transaction::Barrier` calls are only relevant to terminal key-value store
/// drivers, which use a multi-phase TransactionNode.  A TransactionNode for a
/// non-terminal driver only needs to track operations for a single phase; if
/// transactional operations are performed on a non-terminal key-value store
/// over multiple phases, a separate TransactionNode object is used for each
/// phase.)
///
/// Transactional operations are tracked as follows:
///
/// `MultiPhaseMutation` contains a circular linked list of
/// `SinglePhaseMutation` objects corresponding to each phase in which
/// transactional operations are performed, in order of phase number.
///
/// `SinglePhaseMutation` contains an interval tree (implemented as a red-black
/// tree) containing `ReadModifyWriteEntry` (corresponding to a single key) or
/// `DeleteRangeEntry` (corresponding to a range of keys) objects.  Before
/// commit starts, the entries in the interval tree of the last
/// `SinglePhaseMutation` represent the combined effect of all transactional
/// operations.  This makes it possible for subsequent `ReadModifyWrite`
/// operations to efficiently take into account the current modified state.

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "tensorstore/internal/container/intrusive_red_black_tree.h"
#include "tensorstore/internal/mutex.h"
#include "tensorstore/internal/source_location.h"
#include "tensorstore/internal/tagged_ptr.h"
#include "tensorstore/kvstore/driver.h"
#include "tensorstore/kvstore/generation.h"
#include "tensorstore/kvstore/key_range.h"
#include "tensorstore/kvstore/operations.h"
#include "tensorstore/kvstore/read_modify_write.h"
#include "tensorstore/kvstore/read_result.h"
#include "tensorstore/kvstore/spec.h"
#include "tensorstore/transaction.h"
#include "tensorstore/util/future.h"
#include "tensorstore/util/quote_string.h"  // IWYU pragma: keep
#include "tensorstore/util/result.h"

namespace tensorstore {
namespace internal_kvstore {

using kvstore::Driver;
using kvstore::Key;
using kvstore::ReadModifyWriteSource;
using kvstore::ReadModifyWriteTarget;
using kvstore::ReadOptions;
using kvstore::ReadResult;
using kvstore::Value;
using kvstore::WriteOptions;

class ReadModifyWriteEntry;
class DeleteRangeEntry;
class MutationEntry;
class MultiPhaseMutation;
class SinglePhaseMutation;

/// The type of entry within the interval tree contained in a
/// `SinglePhaseMutation` object.
enum MutationEntryType {
  /// ReadModifyWriteEntry object corresponding to a ReadModifyWrite operation.
  kReadModifyWrite = 0,
  /// DeleteRangeEntry object corresponding to a TransactionalDeleteRange
  /// operation.
  kDeleteRange = 1,
  /// Same as `kDeleteRange`, except this indicates a placeholder entry created
  /// when a `DeleteRangeEntry` from a prior phase is split by a subsequent
  /// ReadModifyWrite or DeleteRange operation in a later phase.  The phase
  /// indicated by the entry's `single_phase_mutation()` must already contain a
  /// `kDeleteRange` entry that contains this entry's range.  Entries of this
  /// type serve solely to provide a view of the modified state; they are
  /// discarded during commit.
  kDeleteRangePlaceholder = 2,
};

using MutationEntryTree =
    internal::intrusive_red_black_tree::Tree<MutationEntry>;
using ReadModifyWriteEntryTree =
    internal::intrusive_red_black_tree::Tree<ReadModifyWriteEntry>;

/// Base class for interval tree entry types `ReadModifyWriteEntry` and
/// `DeleteRangeEntry`.
class MutationEntry : public MutationEntryTree::NodeBase {
 public:
  /// The single affected key for `ReadModifyWriteEntry`, or the `inclusive_min`
  /// bound of the range to be deleted for `DeleteRangeEntry`.
  std::string key_;

  /// Pointer to the `SinglePhaseMutation` object indicating the phase with
  /// which this entry is associated.  Prior to commit starting, all entries are
  /// added to the interval tree of the last phase, and remain there until they
  /// are superseded by another entry (in order to ensure the last phase
  /// provides a complete view of the modified state).  Therefore, the last
  /// phase interval tree may contain some entries for prior phases.  When
  /// commit starts, entries are moved to the interval tree of their appropriate
  /// phase using a single pass through the last phase's interval tree.
  ///
  /// The tag bits specify the `MutationEntryType`.
  internal::TaggedPtr<SinglePhaseMutation, 2> single_phase_mutation_;

  SinglePhaseMutation& single_phase_mutation() const {
    return *single_phase_mutation_;
  }

  MutationEntryType entry_type() const {
    return static_cast<MutationEntryType>(single_phase_mutation_.tag());
  }

  /// Returns the `MultiPhaseMutation` to which this entry is associated.
  MultiPhaseMutation& multi_phase() const;

  /// Returns the mutex of the `MultiPhaseMutation` object.
  absl::Mutex& mutex() const;

  using DeleteRangeEntry = internal_kvstore::DeleteRangeEntry;
  using ReadModifyWriteEntry = internal_kvstore::ReadModifyWriteEntry;

  constexpr static MutationEntryType kReadModifyWrite =
      MutationEntryType::kReadModifyWrite;
  constexpr static MutationEntryType kDeleteRange =
      MutationEntryType::kDeleteRange;

 protected:
  ~MutationEntry() = default;
};

/// Atomic counter with error indicator, used to track number of outstanding
/// entries.
class EntryCounter {
 public:
  /// Sets the error indicator (cannot be unset).
  void SetError() { value_.fetch_or(1, std::memory_order_relaxed); }

  /// Returns `true` if `SetError` has been called.
  bool HasError() const { return value_.load(std::memory_order_relaxed) & 1; }

  /// Increments the count by `amount`.
  void IncrementCount(size_t amount = 1) {
    value_.fetch_add(2 * amount, std::memory_order_relaxed);
  }

  /// Decrements the count by `amount`, returns `true` if the count becomes
  /// zero.  Wrapping is allowed.
  bool DecrementCount(size_t amount = 1) {
    return value_.fetch_sub(2 * amount, std::memory_order_acq_rel) -
               2 * amount <=
           1;
  }

  /// Returns `true` if the count is 0.
  bool IsDone() const { return value_ <= 1; }

 private:
  std::atomic<size_t> value_{0};
};

/// MutationEntry representing a `TransactionalDeleteRange` operation.
class DeleteRangeEntry final : public MutationEntry {
 public:
  /// The `exclusive_max` bound for the range to be deleted.
  std::string exclusive_max_;

  /// Tree of `ReadModifyWriteEntry` objects representing prior
  /// `ReadModifyWrite` operations in the same phase that were
  /// superseded/overwritten by this `TransacitonalDeleteRange` operation.
  ///
  /// These entries don't contribute to the writeback result, but they are still
  /// able to perform validation on the prior state, and may abort the
  /// transaction if the validation fails.
  ReadModifyWriteEntryTree superseded_;

  /// Counter used during writeback to track the number of entries in
  /// `superseded_` not yet completed.
  EntryCounter remaining_entries_;
};

/// MutationEntry representing a `ReadModifyWrite` operation.
class ReadModifyWriteEntry : public MutationEntry,
                             public ReadModifyWriteTarget {
 public:
  /// The `ReadModifyWriteSource` (normally associated with a
  /// `KvsBackedCache::TransactionNode`) provided to the `ReadModifyWrite`
  /// operation.
  ReadModifyWriteSource* source_;

  /// Pointer to prior `ReadModifyWrite` operation (possibly in a prior phase)
  /// that was superseded by this operation.
  ReadModifyWriteEntry* prev_ = nullptr;

  /// If `superseded_` is `false`, pointer to next `ReadModifyWrite` operation
  /// (possibly in a subsequent phase) that supersedes this operation.  If this
  /// entry is directly contained in a `SinglePhaseMutation::entries_` tree,
  /// `next_` is `nullptr`.  Otherwise, `next_` must not be `nullptr`.
  ///
  /// If `superseded_` is `true`, will be `nullptr` prior to writeback/commit
  /// starting; after writeback/commit starts, will be set to a pointer to the
  /// `DeleteRangeEntry` that directly contains this entry in its `superseded_`
  /// tree.  While transactional operations may still be added, we don't set
  /// `next_` to point to the `DeleteRangeEntry` that contains this entry,
  /// because (1) we don't need it; and (2) the `next_` pointers would have to
  /// be updated when a split happens, and that can't be done in amortized O(log
  /// N) time.
  MutationEntry* next_ = nullptr;

  /// Returns a pointer to the next `ReadModifyWriteEntry` that supersedes this
  /// entry, or `nullptr` if there is none.  This returns `nullptr` if, and only
  /// if, this entry is directly contained in either a
  /// `SinglePhaseMutation::entries_` tree or a `DeleteRangeEntry::supereseded_`
  /// tree.
  ReadModifyWriteEntry* next_read_modify_write() const {
    if (!next_ || (flags_.load(std::memory_order_relaxed) & kDeleted)) {
      return nullptr;
    }
    return static_cast<ReadModifyWriteEntry*>(next_);
  }

  // Bit vector of flags, see flag definitions below.
  using Flags = uint16_t;

  // Writing to `flags_` requires that `this->multi_phase().mutex()` is locked,
  // and are done using relaxed operations.
  //
  // Certain flags indicated in the
  // descriptions below are not modified after the entry is first initialized
  // and may be safely read using relaxed atomic reads without locking the
  // mutex.
  std::atomic<Flags> flags_ = 0;

  /// Indicates whether `ReadModifyWriteSource::Writeback` was called (and has
  /// completed) on `source_`.
  constexpr static Flags kWritebackProvided = 1;

  /// Indicates whether a prior call to `ReadModifyWriteSource::Writeback` on
  /// this entry's `_source_` or that of a direct or indirect predecessor (via
  /// `prev_`) has returned an unconditional generation, which indicates that
  /// the writeback result is not conditional on the existing read state.  In
  /// this case, if the actual writeback result is not needed (because a
  /// successor entry is also unconditional), `ReadModifyWriteSource::Writeback`
  /// won't be called again merely to validate the existing read state.
  constexpr static Flags kTransitivelyUnconditional = 2;

  /// Indicates that this entry supersedes a prior `TransactionalDeleteRange`
  /// operation that affected the same key, and therefore the input is always a
  /// `ReadResult` with a `state` of `kMissing`.  If this flag is specified,
  /// note that `prev_`, if non-null, is not a special entry corresponding to
  /// this deletion; rather, it is a regular `ReadModifyWriteEntry` for the same
  /// key in the same phase that was superseded by the
  /// `TransactionalDeleteRange` operation.
  constexpr static Flags kPrevDeleted = 8;

  /// WritebackError has already been called on this entry sequence.
  constexpr static Flags kError = 16;

  /// If set, this is a member of the supereseded_ list of a `DeleteRangeEntry`.
  /// After commit starts, `next_` will be updated to point to that
  /// DeleteRangeEntry, which also serves to signal that any necessary
  /// "writeback" can begin.
  ///
  /// We don't set `next_` prior to commit because we may have to modify it
  /// repeatedly due to splits and merges.
  constexpr static Flags kDeleted = 32;

  /// Only meaningful if `kWritebackProvided` is also set.  Indicates that the
  /// writeback completed with a state not equal to `ReadResult::kUnspecified`.
  constexpr static Flags kTransitivelyDirty = 64;

  /// Indicates that `source_->KvsRevoke()` has already been called.
  constexpr static Flags kRevoked = 128;

  /// Indicates that if this node successfully returns a desired writeback
  /// state, but then the writeback fails due to a generation mismatch, that
  /// writeback should not be retried because the desired writeback state will
  /// not change.
  ///
  /// This flag must NOT be modified after the entry is initialized, and may
  /// safely be read without locking `this->multi_phase().mutex()`.
  ///
  /// TODO(jbms): Currently this flag is only respected for non-atomic commits.
  constexpr static Flags kNonRetryable = 256;

  /// Indicates that `source_->KvsWriteback` supports the `byte_range` option.
  ///
  /// This flag must NOT be modified after the entry is initialized, and may
  /// safely be read without locking `this->multi_phase().mutex()`.
  constexpr static Flags kSupportsByteRangeReads = 512;

  // Implementation of `ReadModifyWriteTarget` interface:

  /// Satisfies a read request by requesting a writeback of `prev_`, or by
  /// calling `MultiPhaseMutation::Read` if there is no previous operation.
  void KvsRead(ReadModifyWriteTarget::ReadModifyWriteReadOptions options,
               ReadReceiver receiver) override;

  /// Returns `false` if `prev_` is not null, or if
  /// `MultiPhaseMutation::MultiPhaseReadsCommitted` returns `false`.
  /// Otherwise, returns `true`.
  bool KvsReadsCommitted() override;

  /// Calls `source_->KvsRevoke()` if not already called.
  void EnsureRevoked();

  virtual ~ReadModifyWriteEntry() = default;
};

/// Represents the modifications made during a single phase.
class SinglePhaseMutation {
 public:
  SinglePhaseMutation() = default;
  SinglePhaseMutation(const SinglePhaseMutation&) = delete;

  /// The `MultiPhaseMutation` object that contains this `SinglePhaseMutation`
  /// object.
  MultiPhaseMutation* multi_phase_;

  /// The phase number to which this object is associated.  The special value of
  /// `std::numeric_limits<size_t>::max()` is used for the initial
  /// `SinglePhaseMutation` of a `MultiPhaseMutation` when it is first
  /// initialized before any operations are added.
  size_t phase_number_;

  /// The interval tree representing the operations.
  MutationEntryTree entries_;

  /// Pointer to next phase in circular linked list contained in `multi_phase_`.
  SinglePhaseMutation* next_;

  /// Pointer to previous phase in circular linked list contained in
  /// `multi_phase_`.
  SinglePhaseMutation* prev_;

  /// Counter used during writeback to track the number of entries in `entries_`
  /// not yet completed.
  EntryCounter remaining_entries_;
};

/// Destroys all entries backward-reachable from the interval tree contained in
/// `single_phase_mutation`, and removes any linked list references to the
/// destroyed nodes.
///
/// Entries in a later phase that supersede a destroyed node will end up with a
/// `prev_` pointer of `nullptr`.
///
/// The reachable entries include all entries directly contained in the interval
/// tree, as well as:
///
///   - in the case of a `ReadModifyWriteEntry`, any entry reachable by
///     following `prev_` pointers.
///
///   - in the case of a `DeleteRangeEntry` contained in the interval tree, all
///     `ReadModifyWriteEntry` object directly contained in its `superseded_`
///     tree, as well as any prior entries reachable by following `prev_`
///     pointers from any of those entries.
///
/// This should be called to abort, or after commit completes.
void DestroyPhaseEntries(SinglePhaseMutation& single_phase_mutation);

/// Notifies all `ReadModifyWriteEntry` objects backward-reachable from `entry`
/// of a writeback error.
///
/// Has no effect on entries that have already been notified.
void WritebackError(MutationEntry& entry);
void WritebackError(ReadModifyWriteEntry& entry);
void WritebackError(DeleteRangeEntry& entry);

/// Calls `WritebackError` on all entries in the interval tree contained in
/// `single_phase_mutation`.
void WritebackError(SinglePhaseMutation& single_phase_mutation);

void WritebackSuccess(DeleteRangeEntry& entry);
void WritebackSuccess(ReadModifyWriteEntry& entry,
                      TimestampedStorageGeneration new_stamp,
                      const StorageGeneration& orig_generation);
void InvalidateReadState(SinglePhaseMutation& single_phase_mutation);

class MultiPhaseMutation {
 public:
  MultiPhaseMutation();

  SinglePhaseMutation phases_;

  virtual internal::TransactionState::Node& GetTransactionNode() = 0;
  virtual std::string DescribeKey(std::string_view key) = 0;

  SinglePhaseMutation& GetCommittingPhase();

  /// Allocates a new `ReadModifyWriteEntry`.
  ///
  /// By default returns `new ReadModifyWriteEntry`, but if a derived
  /// `MultiPhaseMutation` type uses a derived `ReadModifyWriteEntry` type, it
  /// must override this method to return a new derived entry object.
  virtual ReadModifyWriteEntry* AllocateReadModifyWriteEntry();

  /// Destroys and frees an entry returned by `AllocateReadModifyWriteEntry`.
  ///
  /// By default calls `delete entry`, but derived classes must override this if
  /// `AllocateReadModifyWriteEntry` is overridden.
  virtual void FreeReadModifyWriteEntry(ReadModifyWriteEntry* entry);

  /// Reads from the underlying storage. This is called when a read that cannot
  /// be satisfied by a prior (superseded) entry.
  virtual void Read(std::string_view key,
                    ReadModifyWriteTarget::ReadModifyWriteReadOptions&& options,
                    ReadModifyWriteTarget::ReadReceiver&& receiver) = 0;

  /// Lists the underlying storage. This is called when transactional list
  /// request cannot be satisfied from the mutation tree alone.
  virtual void ListUnderlying(kvstore::ListOptions options,
                              kvstore::ListReceiver receiver) = 0;

  /// Called when the writeback result for a chain of entries for a given key is
  /// ready.
  ///
  /// If some suffix of the chain produces a `ReadResult::kUnspecified` state
  /// (meaning unmodified), then `read_result` may have come from an earlier
  /// entry in the chain, indicated by `source_entry`, and should be interpreted
  /// according to `source_entry.source_->IsSpecialSource()` rather than
  /// `entry.source_->IsSpecialSource()`.
  virtual void Writeback(ReadModifyWriteEntry& entry,
                         ReadModifyWriteEntry& source_entry,
                         ReadResult&& read_result) = 0;

  virtual void WritebackDelete(DeleteRangeEntry& entry) = 0;
  virtual bool MultiPhaseReadsCommitted() { return true; }
  virtual void PhaseCommitDone(size_t next_phase) = 0;

  virtual void AllEntriesDone(SinglePhaseMutation& single_phase_mutation);
  virtual void RecordEntryWritebackError(ReadModifyWriteEntry& entry,
                                         absl::Status error);

  void AbortRemainingPhases();
  void CommitNextPhase();

  enum class ReadModifyWriteStatus {
    // No change to the number of keys affected.
    kExisting,
    // Added first affected key.
    kAddedFirst,
    // Added subsequent affected key.
    kAddedSubsequent,
  };

  /// Registers a `ReadModifyWrite` operation for the specified `key`.
  ///
  /// This is normally called by implementations of `Driver::ReadModifyWrite`.
  ///
  /// \pre Must be called with `mutex()` held.
  /// \param phase[out] On return, set to the transaction phase to which the
  ///     operation was added.  Note that the transaction phase number may
  ///     change due to a concurrent call to `Transaction::Barrier`; therefore,
  ///     the current transaction phase may not equal `phase` after this call
  ///     completes.
  /// \param key The key affected by the operation.
  /// \param source The write source.
  /// \returns A status value that may be used to validate constraints in the
  ///     case that multi-key transactions are not supported.
  ReadModifyWriteStatus ReadModifyWrite(size_t& phase, Key key,
                                        ReadModifyWriteSource& source);

  virtual absl::Status ValidateReadModifyWriteStatus(
      ReadModifyWriteStatus rmw_status) = 0;

  /// Registers a `DeleteRange` operation for the specified `range`.
  ///
  /// This is normally called by implementations of `Driver::DeleteRange`.
  ///
  /// \pre Must be called with `mutex()` held.
  /// \param range The range to delete.
  void DeleteRange(KeyRange range);

  Future<ReadResult> ReadImpl(internal::OpenTransactionNodePtr<> node,
                              kvstore::Driver* driver, Key&& key,
                              kvstore::ReadOptions&& options,
                              absl::FunctionRef<void()> unlock);

  void ListImpl(internal::OpenTransactionNodePtr<> node,
                kvstore::ListOptions&& options,
                kvstore::ListReceiver&& receiver);

  /// Returns a description of the first entry, used for error messages in the
  /// case that an atomic transaction is requested but is not supported.
  std::string DescribeFirstEntry();

  /// Returns the mutex used to protect access to the data structures.  Derived
  /// classes must implement this.  In the simple case, the derived class will
  /// simply declare an `absl::Mutex` data member, but the indirection through
  /// this virtual method allows a `MultiPhaseMutation` that is embedded within
  /// an `AsyncCache::TransactionNode` to share the mutex used by
  /// `AsyncCache::TransactionNode`.
  virtual absl::Mutex& mutex() = 0;

 protected:
  ~MultiPhaseMutation() = default;
};

inline MultiPhaseMutation& MutationEntry::multi_phase() const {
  return *single_phase_mutation().multi_phase_;
}
inline absl::Mutex& MutationEntry::mutex() const {
  return multi_phase().mutex();
}

class AtomicMultiPhaseMutationBase : public MultiPhaseMutation {
 public:
  static void AtomicWritebackReady(ReadModifyWriteEntry& entry);

  struct ReadModifyWriteEntryWithStamp
      : public internal_kvstore::ReadModifyWriteEntry {
    bool IsOutOfDate(absl::Time staleness_bound) {
      return stamp_.time == absl::InfinitePast() ||
             stamp_.time < staleness_bound;
    }
    TimestampedStorageGeneration& stamp() { return stamp_; }

    // Before writeback completes `stamp_` indicates the writeback request (i.e.
    // base generation + mutation tags) and staleness and `orig_generation_` is
    // ignored.
    //
    // After writeback completes successfully, the writeback request generation
    // is moved to `orig_generation_` and `stamp_` stores the new committed
    // generation and timestamp.
    TimestampedStorageGeneration stamp_;
    StorageGeneration orig_generation_;
  };
  void RetryAtomicWriteback(absl::Time staleness_bound);
  void WritebackDelete(DeleteRangeEntry& entry) override;
  void AtomicCommitWritebackSuccess();
  void RevokeAllEntries();
  absl::Status ValidateReadModifyWriteStatus(
      ReadModifyWriteStatus rmw_status) final;

 protected:
  ~AtomicMultiPhaseMutationBase() = default;
};

class AtomicMultiPhaseMutation : public AtomicMultiPhaseMutationBase {
 public:
  class BufferedReadModifyWriteEntry
      : public AtomicMultiPhaseMutationBase::ReadModifyWriteEntryWithStamp {
   public:
    ReadResult::State value_state_;
    absl::Cord value_;
  };

  ReadModifyWriteEntry* AllocateReadModifyWriteEntry() override;
  void FreeReadModifyWriteEntry(ReadModifyWriteEntry* entry) override;
  void Writeback(ReadModifyWriteEntry& entry,
                 ReadModifyWriteEntry& source_entry,
                 ReadResult&& read_result) override;

 protected:
  ~AtomicMultiPhaseMutation() = default;
};

void ReadDirectly(Driver* driver, std::string_view key,
                  ReadModifyWriteTarget::ReadModifyWriteReadOptions&& options,
                  ReadModifyWriteTarget::ReadReceiver&& receiver);

void WritebackDirectly(Driver* driver, ReadModifyWriteEntry& entry,
                       ReadResult&& read_result);

void WritebackDirectly(Driver* driver, DeleteRangeEntry& entry);

template <typename DerivedMultiPhaseMutation = MultiPhaseMutation>
class TransactionNodeBase : public internal::TransactionState::Node,
                            public DerivedMultiPhaseMutation {
 public:
  TransactionNodeBase(Driver* driver)
      : internal::TransactionState::Node(driver) {
    intrusive_ptr_increment(driver);
  }

  ~TransactionNodeBase() { intrusive_ptr_decrement(this->driver()); }

  Driver* driver() { return static_cast<Driver*>(this->associated_data()); }

  internal::TransactionState::Node& GetTransactionNode() override {
    return *this;
  }

  std::string DescribeKey(std::string_view key) override {
    return this->driver()->DescribeKey(key);
  }

  void Read(std::string_view key,
            ReadModifyWriteTarget::ReadModifyWriteReadOptions&& options,
            ReadModifyWriteTarget::ReadReceiver&& receiver) override {
    internal_kvstore::ReadDirectly(driver(), key, std::move(options),
                                   std::move(receiver));
  }

  void ListUnderlying(kvstore::ListOptions options,
                      kvstore::ListReceiver receiver) override {
    driver()->ListImpl(std::move(options), std::move(receiver));
  }

  void PhaseCommitDone(size_t next_phase) override {
    this->CommitDone(next_phase);
  }

  void PrepareForCommit() override {
    this->PrepareDone();
    this->ReadyForCommit();
  }

  void Commit() override { this->CommitNextPhase(); }

  absl::Mutex& mutex() override { return mutex_; }

  void Abort() override {
    this->AbortRemainingPhases();
    this->AbortDone();
  }

  std::string Describe() override {
    absl::MutexLock lock(&mutex_);
    return this->DescribeFirstEntry();
  }

  absl::Mutex mutex_;
};

class NonAtomicTransactionNode
    : public TransactionNodeBase<MultiPhaseMutation> {
 public:
  using TransactionNodeBase<MultiPhaseMutation>::TransactionNodeBase;

  void Writeback(ReadModifyWriteEntry& entry,
                 ReadModifyWriteEntry& source_entry,
                 ReadResult&& read_result) override {
    internal_kvstore::WritebackDirectly(this->driver(), entry,
                                        std::move(read_result));
  }

  void WritebackDelete(DeleteRangeEntry& entry) override {
    internal_kvstore::WritebackDirectly(driver(), entry);
  }

  absl::Status ValidateReadModifyWriteStatus(
      ReadModifyWriteStatus rmw_status) final;
};

using AtomicTransactionNode = TransactionNodeBase<AtomicMultiPhaseMutation>;

template <typename TransactionNode, typename... Arg>
Result<internal::OpenTransactionNodePtr<TransactionNode>> GetTransactionNode(
    Driver* driver, internal::OpenTransactionPtr& transaction, Arg&&... arg) {
  TENSORSTORE_ASSIGN_OR_RETURN(auto node,
                               internal::GetOrCreateOpenTransaction(transaction)
                                   .GetOrCreateMultiPhaseNode(driver, [&] {
                                     return new TransactionNode(
                                         driver, std::forward<Arg>(arg)...);
                                   }));
  return internal::static_pointer_cast<TransactionNode>(std::move(node));
}

template <typename TransactionNode>
internal::OpenTransactionNodePtr<TransactionNode> GetExistingTransactionNode(
    Driver* driver, const internal::OpenTransactionPtr& transaction) {
  return internal::static_pointer_cast<TransactionNode>(
      transaction->GetExistingMultiPhaseNode(driver));
}

template <typename TransactionNode, typename... Arg>
absl::Status AddReadModifyWrite(Driver* driver,
                                internal::OpenTransactionPtr& transaction,
                                size_t& phase, Key key,
                                ReadModifyWriteSource& source, Arg&&... arg) {
  TENSORSTORE_ASSIGN_OR_RETURN(
      auto node, internal_kvstore::GetTransactionNode<TransactionNode>(
                     driver, transaction, std::forward<Arg>(arg)...));
  absl::MutexLock lock(&node->mutex_);
  node->ReadModifyWrite(phase, std::move(key), source);
  return absl::OkStatus();
}

template <typename TransactionNode, typename... Arg>
absl::Status AddDeleteRange(Driver* driver,
                            const internal::OpenTransactionPtr& transaction,
                            KeyRange&& range, Arg&&... arg) {
  auto transaction_copy = transaction;
  TENSORSTORE_ASSIGN_OR_RETURN(
      auto node, internal_kvstore::GetTransactionNode<TransactionNode>(
                     driver, transaction_copy, std::forward<Arg>(arg)...));
  absl::MutexLock lock(&node->mutex_);
  node->DeleteRange(std::move(range));
  return absl::OkStatus();
}

template <typename TransactionNode>
void TransactionalListImpl(Driver* driver,
                           const internal::OpenTransactionPtr& transaction,
                           kvstore::ListOptions&& options,
                           kvstore::ListReceiver&& receiver) {
  if (transaction->mode() & repeatable_read) {
    execution::submit(ErrorSender{absl::UnimplementedError(
                          "repeatable_read mode not supported for "
                          "transactional list operations")},
                      std::move(receiver));
  }
  auto node = internal_kvstore::GetExistingTransactionNode<TransactionNode>(
      driver, transaction);
  if (!node) {
    driver->ListImpl(std::move(options), std::move(receiver));
    return;
  }
  MultiPhaseMutation* multi_phase_mutation = node.get();
  multi_phase_mutation->ListImpl(std::move(node), std::move(options),
                                 std::move(receiver));
}

template <typename TransactionNode, typename... Arg>
Future<ReadResult> TransactionalReadImpl(
    Driver* driver, const internal::OpenTransactionPtr& transaction, Key&& key,
    kvstore::ReadOptions&& options, Arg&&... arg) {
  assert(transaction);
  auto node = internal_kvstore::GetExistingTransactionNode<TransactionNode>(
      driver, transaction);
  if (!node && !(transaction->mode() & repeatable_read)) {
    // Just perform a regular read.
    return driver->Read(std::move(key), std::move(options));
  }
  if (!node) {
    TENSORSTORE_ASSIGN_OR_RETURN(
        node, internal_kvstore::GetTransactionNode<TransactionNode>(
                  driver,
                  // const_cast is safe because `transaction` is guaranteed to
                  // be non-null, and `GetTransactionNode` only modifies it if
                  // it is null.
                  const_cast<internal::OpenTransactionPtr&>(transaction),
                  std::forward<Arg>(arg)...));
  }
  MultiPhaseMutation* multi_phase_mutation = node.get();
  auto& mutex = multi_phase_mutation->mutex();
  UniqueWriterLock<absl::Mutex> lock(mutex);
  return multi_phase_mutation->ReadImpl(std::move(node), driver, std::move(key),
                                        std::move(options),
                                        [&lock] { lock.unlock(); });
}

/// Performs a read via a `ReadModifyWrite` operation in an existing
/// transaction.
///
/// This may be used to perform a simple read within the context of a
/// transaction.
///
/// This guarantees consistent reads: the transaction will fail to commit if the
/// generation for `key` changes after it is read.
///
/// Warning: Currently each call to this function consumes a small amount of
/// additional memory that is not released until the transaction is committed.
Future<ReadResult> ReadViaExistingTransaction(
    Driver* driver, internal::OpenTransactionPtr& transaction, size_t& phase,
    Key key, kvstore::TransactionalReadOptions options);

/// Performs a write via a `ReadModifyWrite` operation in an existing
/// transaction.
///
/// This may be used to perform a simple write within the context of a
/// transaction.
Future<TimestampedStorageGeneration> WriteViaExistingTransaction(
    Driver* driver, internal::OpenTransactionPtr& transaction, size_t& phase,
    Key key, std::optional<Value> value, WriteOptions options,
    bool fail_transaction_on_mismatch, StorageGeneration* out_generation);

/// Performs a write via a `ReadModifyWrite` operation in a new transaction.
///
/// This may be used by drivers that rely on transactions for all operations in
/// order to implement the non-transactional `Driver::Write` operation.
Future<TimestampedStorageGeneration> WriteViaTransaction(
    Driver* driver, Key key, std::optional<Value> value, WriteOptions options);

#ifdef TENSORSTORE_INTERNAL_KVSTORE_TRANSACTION_DEBUG

/// Logs a message associated with a `MutationEntry` when debugging is enabled.
///
/// The first parameter must be a `MutationEntry` reference.  The remaining
/// parameters must be compatible with `tensorstore::StrCat`.
///
/// Usage:
///
///     MutationEntry &entry = ...;
///     TENSORSTORE_KVSTORE_DEBUG_LOG(entry, "Information", to, "include");
#define TENSORSTORE_KVSTORE_DEBUG_LOG(...)                    \
  do {                                                        \
    tensorstore::internal_kvstore::KvstoreDebugLog(           \
        tensorstore::SourceLocation::current(), __VA_ARGS__); \
  } while (false)

template <typename... T>
void KvstoreDebugLog(tensorstore::SourceLocation loc, MutationEntry& entry,
                     const T&... arg) {
  std::string message;
  tensorstore::StrAppend(
      &message, "[", typeid(entry.multi_phase()).name(),
      ": multi_phase=", &entry.multi_phase(), ", entry=", &entry,
      ", phase=", entry.single_phase_mutation().phase_number_,
      ", key=", tensorstore::QuoteString(entry.key_));
  if (entry.entry_type() == kDeleteRange) {
    tensorstore::StrAppend(
        &message, ", exclusive_max=",
        tensorstore::QuoteString(
            static_cast<DeleteRangeEntry&>(entry).exclusive_max_));
  } else {
    size_t seq = 0;
    for (auto* e = static_cast<ReadModifyWriteEntry*>(&entry)->prev_; e;
         e = e->prev_) {
      ++seq;
    }
    tensorstore::StrAppend(&message, ", seq=", seq);
  }
  tensorstore::StrAppend(&message, "] ", arg...);
  ABSL_LOG(INFO).AtLocation(loc.file_name(), loc.line()) << message;
}
#else
#define TENSORSTORE_KVSTORE_DEBUG_LOG(...) while (false)
#endif

}  // namespace internal_kvstore
}  // namespace tensorstore

#endif  // TENSORSTORE_KVSTORE_TRANSACTION_H_
