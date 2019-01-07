// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QPACK_QPACK_DECODER_TEST_UTILS_H_
#define QUICHE_QUIC_CORE_QPACK_QPACK_DECODER_TEST_UTILS_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_decoder.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_progressive_decoder.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"

namespace quic {
namespace test {

// QpackDecoder::EncoderStreamErrorDelegate implementation that does nothing.
class NoopEncoderStreamErrorDelegate
    : public QpackDecoder::EncoderStreamErrorDelegate {
 public:
  ~NoopEncoderStreamErrorDelegate() override = default;

  void OnError(QuicStringPiece error_message) override;
};

// Mock QpackDecoder::EncoderStreamErrorDelegate implementation.
class MockEncoderStreamErrorDelegate
    : public QpackDecoder::EncoderStreamErrorDelegate {
 public:
  ~MockEncoderStreamErrorDelegate() override = default;

  MOCK_METHOD1(OnError, void(QuicStringPiece error_message));
};

// QpackDecoderStreamSender::Delegate implementation that does nothing.
class NoopDecoderStreamSenderDelegate
    : public QpackDecoderStreamSender::Delegate {
 public:
  ~NoopDecoderStreamSenderDelegate() override = default;

  void Write(QuicStringPiece data) override;
};

// Mock QpackDecoderStreamSender::Delegate implementation.
class MockDecoderStreamSenderDelegate
    : public QpackDecoderStreamSender::Delegate {
 public:
  ~MockDecoderStreamSenderDelegate() override = default;

  MOCK_METHOD1(Write, void(QuicStringPiece data));
};

// HeadersHandlerInterface implementation that collects decoded headers
// into a SpdyHeaderBlock.
class TestHeadersHandler
    : public QpackProgressiveDecoder::HeadersHandlerInterface {
 public:
  TestHeadersHandler();
  ~TestHeadersHandler() override = default;

  // HeadersHandlerInterface implementation:
  void OnHeaderDecoded(QuicStringPiece name, QuicStringPiece value) override;
  void OnDecodingCompleted() override;
  void OnDecodingErrorDetected(QuicStringPiece error_message) override;

  // Release decoded header list.  Must only be called if decoding is complete
  // and no errors have been detected.
  spdy::SpdyHeaderBlock ReleaseHeaderList();

  bool decoding_completed() const;
  bool decoding_error_detected() const;

 private:
  spdy::SpdyHeaderBlock header_list_;
  bool decoding_completed_;
  bool decoding_error_detected_;
};

class MockHeadersHandler
    : public QpackProgressiveDecoder::HeadersHandlerInterface {
 public:
  MockHeadersHandler() = default;
  MockHeadersHandler(const MockHeadersHandler&) = delete;
  MockHeadersHandler& operator=(const MockHeadersHandler&) = delete;
  ~MockHeadersHandler() override = default;

  MOCK_METHOD2(OnHeaderDecoded,
               void(QuicStringPiece name, QuicStringPiece value));
  MOCK_METHOD0(OnDecodingCompleted, void());
  MOCK_METHOD1(OnDecodingErrorDetected, void(QuicStringPiece error_message));
};

class NoOpHeadersHandler
    : public QpackProgressiveDecoder::HeadersHandlerInterface {
 public:
  ~NoOpHeadersHandler() override = default;

  void OnHeaderDecoded(QuicStringPiece name, QuicStringPiece value) override{};
  void OnDecodingCompleted() override{};
  void OnDecodingErrorDetected(QuicStringPiece error_message) override{};
};

void QpackDecode(
    QpackDecoder::EncoderStreamErrorDelegate* encoder_stream_error_delegate,
    QpackDecoderStreamSender::Delegate* decoder_stream_sender_delegate,
    QpackProgressiveDecoder::HeadersHandlerInterface* handler,
    const FragmentSizeGenerator& fragment_size_generator,
    QuicStringPiece data);

}  // namespace test
}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QPACK_QPACK_DECODER_TEST_UTILS_H_
