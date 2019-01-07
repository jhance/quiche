// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some helpers for quic crypto

#ifndef QUICHE_QUIC_CORE_CRYPTO_CRYPTO_UTILS_H_
#define QUICHE_QUIC_CORE_CRYPTO_CRYPTO_UTILS_H_

#include <cstddef>
#include <cstdint>

#include "base/macros.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake_message.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypter.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

class QuicRandom;

class QUIC_EXPORT_PRIVATE CryptoUtils {
 public:
  CryptoUtils() = delete;

  // Diversification is a utility class that's used to act like a union type.
  // Values can be created by calling the functions like |NoDiversification|,
  // below.
  class Diversification {
   public:
    enum Mode {
      NEVER,  // Key diversification will never be used. Forward secure
              // crypters will always use this mode.

      PENDING,  // Key diversification will happen when a nonce is later
                // received. This should only be used by clients initial
                // decrypters which are waiting on the divesification nonce
                // from the server.

      NOW,  // Key diversification will happen immediate based on the nonce.
            // This should only be used by servers initial encrypters.
    };

    Diversification(const Diversification& diversification) = default;

    static Diversification Never() { return Diversification(NEVER, nullptr); }
    static Diversification Pending() {
      return Diversification(PENDING, nullptr);
    }
    static Diversification Now(DiversificationNonce* nonce) {
      return Diversification(NOW, nonce);
    }

    Mode mode() const { return mode_; }
    DiversificationNonce* nonce() const {
      DCHECK_EQ(mode_, NOW);
      return nonce_;
    }

   private:
    Diversification(Mode mode, DiversificationNonce* nonce)
        : mode_(mode), nonce_(nonce) {}

    Mode mode_;
    DiversificationNonce* nonce_;
  };

  // SetKeyAndIV derives the key and IV from the given packet protection secret
  // |pp_secret| and sets those fields on the given QuicCrypter |*crypter|.
  // This follows the derivation described in section 7.3 of RFC 8446, except
  // with the label prefix in HKDF-Expand-Label changed from "tls13 " to "quic "
  // as described in draft-ietf-quic-tls-14, section 5.1.
  static void SetKeyAndIV(const EVP_MD* prf,
                          const std::vector<uint8_t>& pp_secret,
                          QuicCrypter* crypter);

  // QUIC encrypts TLS handshake messages with a version-specific key (to
  // prevent network observers that are not aware of that QUIC version from
  // making decisions based on the TLS handshake). This packet protection secret
  // is derived from the connection ID in the client's Initial packet.
  //
  // This function takes that |connection_id| and creates the encrypter and
  // decrypter (put in |*crypters|) to use for this packet protection, as well
  // as setting the key and IV on those crypters.
  static void CreateTlsInitialCrypters(Perspective perspective,
                                       QuicConnectionId connection_id,
                                       CrypterPair* crypters);

  // Generates the connection nonce. The nonce is formed as:
  //   <4 bytes> current time
  //   <8 bytes> |orbit| (or random if |orbit| is empty)
  //   <20 bytes> random
  static void GenerateNonce(QuicWallTime now,
                            QuicRandom* random_generator,
                            QuicStringPiece orbit,
                            QuicString* nonce);

  // DeriveKeys populates |crypters->encrypter|, |crypters->decrypter|, and
  // |subkey_secret| (optional -- may be null) given the contents of
  // |premaster_secret|, |client_nonce|, |server_nonce| and |hkdf_input|. |aead|
  // determines which cipher will be used. |perspective| controls whether the
  // server's keys are assigned to |encrypter| or |decrypter|. |server_nonce| is
  // optional and, if non-empty, is mixed into the key derivation.
  // |subkey_secret| will have the same length as |premaster_secret|.
  //
  // If |pre_shared_key| is non-empty, it is incorporated into the key
  // derivation parameters.  If it is empty, the key derivation is unaltered.
  //
  // If the mode of |diversification| is NEVER, the the crypters will be
  // configured to never perform key diversification. If the mode is
  // NOW (which is only for servers, then the encrypter will be keyed via a
  // two-step process that uses the nonce from |diversification|.
  // If the mode is PENDING (which is only for servres), then the
  // decrypter will only be keyed to a preliminary state: a call to
  // |SetDiversificationNonce| with a diversification nonce will be needed to
  // complete keying.
  static bool DeriveKeys(QuicStringPiece premaster_secret,
                         QuicTag aead,
                         QuicStringPiece client_nonce,
                         QuicStringPiece server_nonce,
                         QuicStringPiece pre_shared_key,
                         const QuicString& hkdf_input,
                         Perspective perspective,
                         Diversification diversification,
                         CrypterPair* crypters,
                         QuicString* subkey_secret);

  // Performs key extraction to derive a new secret of |result_len| bytes
  // dependent on |subkey_secret|, |label|, and |context|. Returns false if the
  // parameters are invalid (e.g. |label| contains null bytes); returns true on
  // success.
  static bool ExportKeyingMaterial(QuicStringPiece subkey_secret,
                                   QuicStringPiece label,
                                   QuicStringPiece context,
                                   size_t result_len,
                                   QuicString* result);

  // Computes the FNV-1a hash of the provided DER-encoded cert for use in the
  // XLCT tag.
  static uint64_t ComputeLeafCertHash(QuicStringPiece cert);

  // Validates that |server_hello| is actually an SHLO message and that it is
  // not part of a downgrade attack.
  //
  // Returns QUIC_NO_ERROR if this is the case or returns the appropriate error
  // code and sets |error_details|.
  static QuicErrorCode ValidateServerHello(
      const CryptoHandshakeMessage& server_hello,
      const ParsedQuicVersionVector& negotiated_versions,
      QuicString* error_details);

  // Validates that the |server_versions| received do not indicate that the
  // ServerHello is part of a downgrade attack. |negotiated_versions| must
  // contain the list of versions received in the server's version negotiation
  // packet (or be empty if no such packet was received).
  //
  // Returns QUIC_NO_ERROR if this is the case or returns the appropriate error
  // code and sets |error_details|.
  static QuicErrorCode ValidateServerHelloVersions(
      const QuicVersionLabelVector& server_versions,
      const ParsedQuicVersionVector& negotiated_versions,
      QuicString* error_details);

  // Validates that |client_hello| is actually a CHLO and that this is not part
  // of a downgrade attack.
  // This includes verifiying versions and detecting downgrade attacks.
  //
  // Returns QUIC_NO_ERROR if this is the case or returns the appropriate error
  // code and sets |error_details|.
  static QuicErrorCode ValidateClientHello(
      const CryptoHandshakeMessage& client_hello,
      ParsedQuicVersion version,
      const ParsedQuicVersionVector& supported_versions,
      QuicString* error_details);

  // Validates that the |client_version| received does not indicate that a
  // downgrade attack has occurred. |connection_version| is the version of the
  // QuicConnection, and |supported_versions| is all versions that that
  // QuicConnection supports.
  //
  // Returns QUIC_NO_ERROR if this is the case or returns the appropriate error
  // code and sets |error_details|.
  static QuicErrorCode ValidateClientHelloVersion(
      QuicVersionLabel client_version,
      ParsedQuicVersion connection_version,
      const ParsedQuicVersionVector& supported_versions,
      QuicString* error_details);

  // Returns the name of the HandshakeFailureReason as a char*
  static const char* HandshakeFailureReasonToString(
      HandshakeFailureReason reason);

  // Writes a hash of the serialized |message| into |output|.
  static void HashHandshakeMessage(const CryptoHandshakeMessage& message,
                                   QuicString* output,
                                   Perspective perspective);

 private:
  // Implements the HKDF-Expand-Label function as defined in section 7.1 of RFC
  // 8446, except that it uses "quic " as the prefix instead of "tls13 ", as
  // specified by draft-ietf-quic-tls-14. The HKDF-Expand-Label function takes 4
  // explicit arguments (Secret, Label, Context, and Length), as well as
  // implicit PRF which is the hash function negotiated by TLS. Its use in QUIC
  // (as needed by the QUIC stack, instead of as used internally by the TLS
  // stack) is only for deriving initial secrets for obfuscation and for
  // calculating packet protection keys and IVs from the corresponding packet
  // protection secret. Neither of these uses need a Context (a zero-length
  // context is provided), so this argument is omitted here.
  //
  // The implicit PRF is explicitly passed into HkdfExpandLabel as |prf|; the
  // Secret, Label, and Length are passed in as |secret|, |label|, and
  // |out_len|, respectively. The resulting expanded secret is returned.
  static std::vector<uint8_t> HkdfExpandLabel(
      const EVP_MD* prf,
      const std::vector<uint8_t>& secret,
      const QuicString& label,
      size_t out_len);
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_CRYPTO_CRYPTO_UTILS_H_
