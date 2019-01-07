// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder_stream_sender.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"

using ::testing::Eq;
using ::testing::StrictMock;

namespace quic {
namespace test {
namespace {

class MockSenderDelegate : public QpackEncoderStreamSender::Delegate {
 public:
  ~MockSenderDelegate() override = default;

  MOCK_METHOD1(Write, void(QuicStringPiece data));
};

class QpackEncoderStreamSenderTest : public QuicTest {
 protected:
  QpackEncoderStreamSenderTest() : stream_(&delegate_) {}
  ~QpackEncoderStreamSenderTest() override = default;

  StrictMock<MockSenderDelegate> delegate_;
  QpackEncoderStreamSender stream_;
};

TEST_F(QpackEncoderStreamSenderTest, InsertWithNameReference) {
  // Static, index fits in prefix, empty value.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("c500"))));
  stream_.SendInsertWithNameReference(true, 5, "");

  // Static, index fits in prefix, Huffman encoded value.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("c28294e7"))));
  stream_.SendInsertWithNameReference(true, 2, "foo");

  // Not static, index does not fit in prefix, not Huffman encoded value.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("bf4a03626172"))));
  stream_.SendInsertWithNameReference(false, 137, "bar");

  // Value length does not fit in prefix.
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  EXPECT_CALL(
      delegate_,
      Write(Eq(QuicTextUtils::HexDecode(
          "aa7f005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"))));
  stream_.SendInsertWithNameReference(false, 42, QuicString(127, 'Z'));
}

TEST_F(QpackEncoderStreamSenderTest, InsertWithoutNameReference) {
  // Empty name and value.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("4000"))));
  stream_.SendInsertWithoutNameReference("", "");

  // Huffman encoded short strings.
  EXPECT_CALL(delegate_,
              Write(Eq(QuicTextUtils::HexDecode("4362617203626172"))));
  stream_.SendInsertWithoutNameReference("bar", "bar");

  // Not Huffman encoded short strings.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("6294e78294e7"))));
  stream_.SendInsertWithoutNameReference("foo", "foo");

  // Not Huffman encoded long strings; length does not fit on prefix.
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  EXPECT_CALL(
      delegate_,
      Write(Eq(QuicTextUtils::HexDecode(
          "5f005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a7f"
          "005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"))));
  stream_.SendInsertWithoutNameReference(QuicString(31, 'Z'),
                                         QuicString(127, 'Z'));
}

TEST_F(QpackEncoderStreamSenderTest, Duplicate) {
  // Small index fits in prefix.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("11"))));
  stream_.SendDuplicate(17);

  // Large index requires two extension bytes.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("1fd503"))));
  stream_.SendDuplicate(500);
}

TEST_F(QpackEncoderStreamSenderTest, DynamicTableSizeUpdate) {
  // Small max size fits in prefix.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("31"))));
  stream_.SendDynamicTableSizeUpdate(17);

  // Large max size requires two extension bytes.
  EXPECT_CALL(delegate_, Write(Eq(QuicTextUtils::HexDecode("3fd503"))));
  stream_.SendDynamicTableSizeUpdate(500);
}

}  // namespace
}  // namespace test
}  // namespace quic
