// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/qpack_header_table.h"

#include "base/logging.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_static_table.h"

namespace quic {

namespace {

const uint64_t kEntrySizeOverhead = 32;

uint64_t EntrySize(QuicStringPiece name, QuicStringPiece value) {
  return name.size() + value.size() + kEntrySizeOverhead;
}

}  // anonymous namespace

QpackHeaderTable::QpackHeaderTable()
    : static_entries_(ObtainQpackStaticTable().GetStaticEntries()),
      static_index_(ObtainQpackStaticTable().GetStaticIndex()),
      static_name_index_(ObtainQpackStaticTable().GetStaticNameIndex()),
      dynamic_table_size_(0),
      dynamic_table_capacity_(0),
      maximum_dynamic_table_capacity_(0),
      max_entries_(0),
      dropped_entry_count_(0) {}

QpackHeaderTable::~QpackHeaderTable() = default;

const QpackEntry* QpackHeaderTable::LookupEntry(bool is_static,
                                                uint64_t index) const {
  if (is_static) {
    if (index >= static_entries_.size()) {
      return nullptr;
    }

    return &static_entries_[index];
  }

  if (index < dropped_entry_count_) {
    return nullptr;
  }

  index -= dropped_entry_count_;

  if (index >= dynamic_entries_.size()) {
    return nullptr;
  }

  return &dynamic_entries_[index];
}

QpackHeaderTable::MatchType QpackHeaderTable::FindHeaderField(
    QuicStringPiece name,
    QuicStringPiece value,
    bool* is_static,
    uint64_t* index) const {
  QpackEntry query(name, value);

  // Look for exact match in static table.
  auto index_it = static_index_.find(&query);
  if (index_it != static_index_.end()) {
    DCHECK((*index_it)->IsStatic());
    *index = (*index_it)->InsertionIndex();
    *is_static = true;
    return MatchType::kNameAndValue;
  }

  // Look for exact match in dynamic table.
  index_it = dynamic_index_.find(&query);
  if (index_it != dynamic_index_.end()) {
    DCHECK(!(*index_it)->IsStatic());
    *index = (*index_it)->InsertionIndex();
    *is_static = false;
    return MatchType::kNameAndValue;
  }

  // Look for name match in static table.
  auto name_index_it = static_name_index_.find(name);
  if (name_index_it != static_name_index_.end()) {
    DCHECK(name_index_it->second->IsStatic());
    *index = name_index_it->second->InsertionIndex();
    *is_static = true;
    return MatchType::kName;
  }

  // Look for name match in dynamic table.
  name_index_it = dynamic_name_index_.find(name);
  if (name_index_it != dynamic_name_index_.end()) {
    DCHECK(!name_index_it->second->IsStatic());
    *index = name_index_it->second->InsertionIndex();
    *is_static = false;
    return MatchType::kName;
  }

  return MatchType::kNoMatch;
}

const QpackEntry* QpackHeaderTable::InsertEntry(QuicStringPiece name,
                                                QuicStringPiece value) {
  const uint64_t entry_size = EntrySize(name, value);
  if (entry_size > dynamic_table_capacity_) {
    return nullptr;
  }

  const uint64_t index = dropped_entry_count_ + dynamic_entries_.size();
  dynamic_entries_.push_back({name, value, /* is_static = */ false, index});
  QpackEntry* const new_entry = &dynamic_entries_.back();

  // Evict entries after inserting the new entry instead of before
  // in order to avoid invalidating |name| and |value|.
  dynamic_table_size_ += entry_size;
  EvictDownToCurrentCapacity();

  auto index_result = dynamic_index_.insert(new_entry);
  if (!index_result.second) {
    // An entry with the same name and value already exists.  It needs to be
    // replaced, because |dynamic_index_| tracks the most recent entry for a
    // given name and value.
    DCHECK_GT(new_entry->InsertionIndex(),
              (*index_result.first)->InsertionIndex());
    dynamic_index_.erase(index_result.first);
    auto result = dynamic_index_.insert(new_entry);
    CHECK(result.second);
  }

  auto name_result = dynamic_name_index_.insert({new_entry->name(), new_entry});
  if (!name_result.second) {
    // An entry with the same name already exists.  It needs to be replaced,
    // because |dynamic_name_index_| tracks the most recent entry for a given
    // name.
    DCHECK_GT(new_entry->InsertionIndex(),
              name_result.first->second->InsertionIndex());
    dynamic_name_index_.erase(name_result.first);
    auto result = dynamic_name_index_.insert({new_entry->name(), new_entry});
    CHECK(result.second);
  }

  return new_entry;
}

bool QpackHeaderTable::UpdateTableSize(uint64_t max_size) {
  if (max_size > maximum_dynamic_table_capacity_) {
    return false;
  }

  dynamic_table_capacity_ = max_size;
  EvictDownToCurrentCapacity();

  DCHECK_LE(dynamic_table_size_, dynamic_table_capacity_);

  return true;
}

void QpackHeaderTable::SetMaximumDynamicTableCapacity(
    uint64_t maximum_dynamic_table_capacity) {
  // This method can only be called once: in the decoding context, shortly after
  // construction; in the encoding context, upon receiving the SETTINGS frame.
  DCHECK_EQ(0u, dynamic_table_capacity_);
  DCHECK_EQ(0u, maximum_dynamic_table_capacity_);
  DCHECK_EQ(0u, max_entries_);

  dynamic_table_capacity_ = maximum_dynamic_table_capacity;
  maximum_dynamic_table_capacity_ = maximum_dynamic_table_capacity;
  max_entries_ = maximum_dynamic_table_capacity / 32;
}

void QpackHeaderTable::EvictDownToCurrentCapacity() {
  while (dynamic_table_size_ > dynamic_table_capacity_) {
    DCHECK(!dynamic_entries_.empty());

    QpackEntry* const entry = &dynamic_entries_.front();
    const uint64_t entry_size = EntrySize(entry->name(), entry->value());

    DCHECK_GE(dynamic_table_size_, entry_size);
    dynamic_table_size_ -= entry_size;

    auto index_it = dynamic_index_.find(entry);
    // Remove |dynamic_index_| entry only if it points to the same
    // QpackEntry in |dynamic_entries_|.  Note that |dynamic_index_| has a
    // comparison function that only considers name and value, not actual
    // QpackEntry* address, so find() can return a different entry if name and
    // value match.
    if (index_it != dynamic_index_.end() && *index_it == entry) {
      dynamic_index_.erase(index_it);
    }

    auto name_it = dynamic_name_index_.find(entry->name());
    // Remove |dynamic_name_index_| entry only if it points to the same
    // QpackEntry in |dynamic_entries_|.
    if (name_it != dynamic_name_index_.end() && name_it->second == entry) {
      dynamic_name_index_.erase(name_it);
    }

    dynamic_entries_.pop_front();
    ++dropped_entry_count_;
  }
}

}  // namespace quic
