// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_PLATFORM_API_QUIC_INTERVAL_H_
#define QUICHE_QUIC_PLATFORM_API_QUIC_INTERVAL_H_

#include "net/quic/platform/impl/quic_interval_impl.h"

namespace quic {

template <class T>
using QuicInterval = QuicIntervalImpl<T>;

}  // namespace quic

#endif  // QUICHE_QUIC_PLATFORM_API_QUIC_INTERVAL_H_
