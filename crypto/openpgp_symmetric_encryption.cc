// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/openpgp_symmetric_encryption.h"

#include <stdlib.h>

#include <sechash.h>
#include <cryptohi.h>

#include <vector>

#include "base/logging.h"
#include "crypto/random.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/nss_util.h"

namespace crypto {

namespace {

// Reader wraps a StringPiece and provides methods to read several datatypes
// while advancing the StringPiece.
class Reader {
 public:
  Reader(base::StringPiece input)
      : data_(input) {
  }

  bool U8(uint8* out) {
    if (data_.size() < 1)
      return false;
    *out = static_cast<uint8>(data_[0]);
    data_.remove_prefix(1);
    return true;
  }

  bool U32(uint32* out) {
    if (data_.size() < 4)
      return false;
    *out = static_cast<uint32>(data_[0]) << 24 |
           static_cast<uint32>(data_[1]) << 16 |
           static_cast<uint32>(data_[2]) << 8 |
           static_cast<uint32>(data_[3]);
    data_.remove_prefix(4);
    return true;
  }

  // Prefix sets |*out| to the first |n| bytes of the StringPiece and advances
  // the StringPiece by |n|.
  bool Prefix(size_t n, base::StringPiece *out) {
    if (data_.size() < n)
      return false;
    *out = base::StringPiece(data_.data(), n);
    data_.remove_prefix(n);
    return true;
  }

  // Remainder returns the remainer of the StringPiece and advances it to the
  // end.
  base::StringPiece Remainder() {
    base::StringPiece ret = data_;
    data_ = base::StringPiece();
    return ret;
  }

  typedef base::StringPiece Position;

  Position tell() const {
    return data_;
  }

  void Seek(Position p) {
    data_ = p;
  }

  bool Skip(size_t n) {
    if (data_.size() < n)
      return false;
    data_.remove_prefix(n);
    return true;
  }

  bool empty() const {
    return data_.empty();
  }

  size_t size() const {
    return data_.size();
  }

 private:
  base::StringPiece data_;
};

// SaltedIteratedS2K implements the salted and iterated string-to-key
// convertion. See RFC 4880, section 3.7.1.3.
void SaltedIteratedS2K(unsigned cipher_key_length,
                       HASH_HashType hash_function,
                       base::StringPiece passphrase,
                       base::StringPiece salt,
                       unsigned count,
                       uint8 *out_key) {
  const std::string combined = salt.as_string() + passphrase.as_string();
  const size_t combined_len = combined.size();

  unsigned done = 0;
  uint8 zero[1] = {0};

  HASHContext* hash_context = HASH_Create(hash_function);

  for (unsigned i = 0; done < cipher_key_length; i++) {
    HASH_Begin(hash_context);

    for (unsigned j = 0; j < i; j++)
      HASH_Update(hash_context, zero, sizeof(zero));

    unsigned written = 0;
    while (written < count) {
      if (written + combined_len > count) {
        unsigned todo = count - written;
        HASH_Update(hash_context,
                     reinterpret_cast<const uint8*>(combined.data()),
                     todo);
        written = count;
      } else {
        HASH_Update(hash_context,
                     reinterpret_cast<const uint8*>(combined.data()),
                     combined_len);
        written += combined_len;
      }
    }

    unsigned num_hash_bytes;
    uint8 digest[HASH_LENGTH_MAX];
    HASH_End(hash_context, digest, &num_hash_bytes, sizeof(digest));

    unsigned todo = cipher_key_length - done;
    if (todo > num_hash_bytes)
      todo = num_hash_bytes;
    memcpy(out_key + done, digest, todo);
    done += todo;
  }

  HASH_Destroy(hash_context);
}

// CreateAESContext sets up |out_key| to be an AES context, with the given key,
// in ECB mode and with no IV.
bool CreateAESContext(const uint8* key, unsigned key_len,
                      ScopedPK11Context* out_decryption_context) {
  ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (!slot.get())
    return false;
  SECItem key_item;
  key_item.type = siBuffer;
  key_item.data = const_cast<uint8*>(key);
  key_item.len = key_len;
  ScopedPK11SymKey pk11_key(PK11_ImportSymKey(
      slot.get(), CKM_AES_ECB, PK11_OriginUnwrap, CKA_ENCRYPT, &key_item,
      NULL));
  if (!pk11_key.get())
    return false;
  ScopedSECItem iv_param(PK11_ParamFromIV(CKM_AES_ECB, NULL));
  out_decryption_context->reset(
      PK11_CreateContextBySymKey(CKM_AES_ECB, CKA_ENCRYPT, pk11_key.get(),
                                 iv_param.get()));
  return out_decryption_context->get() != NULL;
}


// These constants are the tag numbers for the various packet types that we
// use.
static const unsigned kSymmetricKeyEncryptedTag = 3;
static const unsigned kSymmetricallyEncryptedTag = 18;
static const unsigned kCompressedTag = 8;
static const unsigned kLiteralDataTag = 11;

class Decrypter {
 public:
  ~Decrypter() {
    for (std::vector<void*>::iterator
         i = arena_.begin(); i != arena_.end(); i++) {
      free(*i);
    }
    arena_.clear();
  }

  OpenPGPSymmetricEncrytion::Result Decrypt(base::StringPiece in,
                                            base::StringPiece passphrase,
                                            base::StringPiece *out_contents) {
    Reader reader(in);
    unsigned tag;
    base::StringPiece contents;
    ScopedPK11Context decryption_context;

    if (!ParsePacket(&reader, &tag, &contents))
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    if (tag != kSymmetricKeyEncryptedTag)
      return OpenPGPSymmetricEncrytion::NOT_SYMMETRICALLY_ENCRYPTED;
    Reader inner(contents);
    OpenPGPSymmetricEncrytion::Result result =
      ParseSymmetricKeyEncrypted(&inner, passphrase, &decryption_context);
    if (result != OpenPGPSymmetricEncrytion::OK)
      return result;

    if (!ParsePacket(&reader, &tag, &contents))
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    if (tag != kSymmetricallyEncryptedTag)
      return OpenPGPSymmetricEncrytion::NOT_SYMMETRICALLY_ENCRYPTED;
    if (!reader.empty())
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    inner = Reader(contents);
    if (!ParseSymmetricallyEncrypted(&inner, &decryption_context, &contents))
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;

    reader = Reader(contents);
    if (!ParsePacket(&reader, &tag, &contents))
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    if (tag == kCompressedTag)
      return OpenPGPSymmetricEncrytion::COMPRESSED;
    if (tag != kLiteralDataTag)
      return OpenPGPSymmetricEncrytion::NOT_SYMMETRICALLY_ENCRYPTED;
    inner = Reader(contents);
    if (!ParseLiteralData(&inner, out_contents))
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;

    return OpenPGPSymmetricEncrytion::OK;
  }

 private:
  // ParsePacket parses an OpenPGP packet from reader. See RFC 4880, section
  // 4.2.2.
  bool ParsePacket(Reader *reader,
                   unsigned *out_tag,
                   base::StringPiece *out_contents) {
    uint8 header;
    if (!reader->U8(&header))
      return false;
    if ((header & 0x80) == 0) {
      // Tag byte must have MSB set.
      return false;
    }

    if ((header & 0x40) == 0) {
      // Old format packet.
      *out_tag = (header & 0x3f) >> 2;

      uint8 length_type = header & 3;
      if (length_type == 3) {
        *out_contents = reader->Remainder();
        return true;
      }

      const unsigned length_bytes = 1 << length_type;
      size_t length = 0;
      for (unsigned i = 0; i < length_bytes; i++) {
        uint8 length_byte;
        if (!reader->U8(&length_byte))
          return false;
        length <<= 8;
        length |= length_byte;
      }

      return reader->Prefix(length, out_contents);
    }

    // New format packet.
    *out_tag = header & 0x3f;
    size_t length;
    bool is_partial;
    if (!ParseLength(reader, &length, &is_partial))
      return false;
    if (is_partial)
      return ParseStreamContents(reader, length, out_contents);
    return reader->Prefix(length, out_contents);
  }

  // ParseStreamContents parses all the chunks of a partial length stream from
  // reader. See http://tools.ietf.org/html/rfc4880#section-4.2.2.4
  bool ParseStreamContents(Reader *reader,
                           size_t length,
                           base::StringPiece *out_contents) {
    const Reader::Position beginning_of_stream = reader->tell();
    const size_t first_chunk_length = length;

    // First we parse the stream to find its length.
    if (!reader->Skip(length))
      return false;

    for (;;) {
      size_t chunk_length;
      bool is_partial;

      if (!ParseLength(reader, &chunk_length, &is_partial))
        return false;
      if (length + chunk_length < length)
        return false;
      length += chunk_length;
      if (!reader->Skip(chunk_length))
        return false;
      if (!is_partial)
        break;
    }

    // Now we have the length of the whole stream in |length|.
    char* buf = reinterpret_cast<char*>(malloc(length));
    arena_.push_back(buf);
    size_t j = 0;
    reader->Seek(beginning_of_stream);

    base::StringPiece first_chunk;
    if (!reader->Prefix(first_chunk_length, &first_chunk))
      return false;
    memcpy(buf + j, first_chunk.data(), first_chunk_length);
    j += first_chunk_length;

    // Now we parse the stream again, this time copying into |buf|
    for (;;) {
      size_t chunk_length;
      bool is_partial;

      if (!ParseLength(reader, &chunk_length, &is_partial))
        return false;
      base::StringPiece chunk;
      if (!reader->Prefix(chunk_length, &chunk))
        return false;
      memcpy(buf + j, chunk.data(), chunk_length);
      j += chunk_length;
      if (!is_partial)
        break;
    }

    *out_contents = base::StringPiece(buf, length);
    return true;
  }

  // ParseLength parses an OpenPGP length from reader. See RFC 4880, section
  // 4.2.2.
  bool ParseLength(Reader *reader, size_t *out_length, bool *out_is_prefix) {
    uint8 length_spec;
    if (!reader->U8(&length_spec))
      return false;

    *out_is_prefix = false;
    if (length_spec < 192) {
      *out_length = length_spec;
      return true;
    } else if (length_spec < 224) {
      uint8 next_byte;
      if (!reader->U8(&next_byte))
        return false;

      *out_length = (length_spec - 192) << 8;
      *out_length += next_byte;
      return true;
    } else if (length_spec < 255) {
      *out_length = 1u << (length_spec & 0x1f);
      *out_is_prefix = true;
      return true;
    } else {
      uint32 length32;
      if (!reader->U32(&length32))
        return false;
      *out_length = length32;
      return true;
    }
  }

  // ParseSymmetricKeyEncrypted parses a passphrase protected session key. See
  // RFC 4880, section 5.3.
  OpenPGPSymmetricEncrytion::Result ParseSymmetricKeyEncrypted(
      Reader *reader,
      base::StringPiece passphrase,
      ScopedPK11Context *decryption_context) {
    uint8 version, cipher, s2k_type, hash_func_id;
    if (!reader->U8(&version) || version != 4)
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;

    if (!reader->U8(&cipher) ||
        !reader->U8(&s2k_type) ||
        !reader->U8(&hash_func_id)) {
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    }

    uint8 cipher_key_length = OpenPGPCipherIdToKeyLength(cipher);
    if (cipher_key_length == 0)
      return OpenPGPSymmetricEncrytion::UNKNOWN_CIPHER;

    HASH_HashType hash_function;
    switch (hash_func_id) {
    case 2:  // SHA-1
      hash_function = HASH_AlgSHA1;
      break;
    case 8:  // SHA-256
      hash_function = HASH_AlgSHA256;
      break;
    default:
      return OpenPGPSymmetricEncrytion::UNKNOWN_HASH;
    }

    // This chunk of code parses the S2K specifier. See RFC 4880, section 3.7.1.
    base::StringPiece salt;
    uint8 key[32];
    uint8 count_spec;
    switch (s2k_type) {
    case 1:
      if (!reader->Prefix(8, &salt))
        return OpenPGPSymmetricEncrytion::PARSE_ERROR;
      // Fall through.
    case 0:
      SaltedIteratedS2K(cipher_key_length, hash_function, passphrase, salt,
                        passphrase.size() + salt.size(), key);
      break;
    case 3:
      if (!reader->Prefix(8, &salt) ||
          !reader->U8(&count_spec)) {
        return OpenPGPSymmetricEncrytion::PARSE_ERROR;
      }
      SaltedIteratedS2K(
          cipher_key_length, hash_function, passphrase, salt,
          static_cast<unsigned>(
            16 + (count_spec&15)) << ((count_spec >> 4) + 6), key);
      break;
    default:
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    }

    if (!CreateAESContext(key, cipher_key_length, decryption_context))
      return OpenPGPSymmetricEncrytion::INTERNAL_ERROR;

    if (reader->empty()) {
      // The resulting key is used directly.
      return OpenPGPSymmetricEncrytion::OK;
    }

    // The S2K derived key encrypts another key that follows:
    base::StringPiece encrypted_key = reader->Remainder();
    if (encrypted_key.size() < 1)
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;

    uint8* plaintext_key = reinterpret_cast<uint8*>(
        malloc(encrypted_key.size()));
    arena_.push_back(plaintext_key);

    CFBDecrypt(encrypted_key, decryption_context, plaintext_key);

    cipher_key_length = OpenPGPCipherIdToKeyLength(plaintext_key[0]);
    if (cipher_key_length == 0)
      return OpenPGPSymmetricEncrytion::UNKNOWN_CIPHER;
    if (encrypted_key.size() != 1u + cipher_key_length)
      return OpenPGPSymmetricEncrytion::PARSE_ERROR;
    if (!CreateAESContext(plaintext_key + 1, cipher_key_length,
                          decryption_context)) {
      return OpenPGPSymmetricEncrytion::INTERNAL_ERROR;
    }
    return OpenPGPSymmetricEncrytion::OK;
  }

  // CFBDecrypt decrypts the cipher-feedback encrypted data in |in| to |out|
  // using |decryption_context| and assumes an IV of all zeros.
  void CFBDecrypt(base::StringPiece in, ScopedPK11Context* decryption_context,
                  uint8* out) {
    // We need this for PK11_CipherOp to write to, but we never check it as we
    // work in ECB mode, one block at a time.
    int out_len;

    uint8 mask[AES_BLOCK_SIZE];
    memset(mask, 0, sizeof(mask));

    unsigned used = AES_BLOCK_SIZE;

    for (size_t i = 0; i < in.size(); i++) {
      if (used == AES_BLOCK_SIZE) {
        PK11_CipherOp(decryption_context->get(), mask, &out_len, sizeof(mask),
                      mask, AES_BLOCK_SIZE);
        used = 0;
      }

      uint8 t = in[i];
      out[i] = t ^ mask[used];
      mask[used] = t;
      used++;
    }
  }

  // OpenPGPCipherIdToKeyLength converts an OpenPGP cipher id (see RFC 4880,
  // section 9.2) to the key length of that cipher. It returns 0 on error.
  unsigned OpenPGPCipherIdToKeyLength(uint8 cipher) {
    switch (cipher) {
    case 7:  // AES-128
      return 16;
    case 8:  // AES-192
      return 24;
    case 9:  // AES-256
      return 32;
    default:
      return 0;
    }
  }

  // ParseSymmetricallyEncrypted parses a Symmetrically Encrypted packet. See
  // RFC 4880, sections 5.7 and 5.13.
  bool ParseSymmetricallyEncrypted(Reader *reader,
                                   ScopedPK11Context *decryption_context,
                                   base::StringPiece *out_plaintext) {
    // We need this for PK11_CipherOp to write to, but we never check it as we
    // work in ECB mode, one block at a time.
    int out_len;

    uint8 version;
    if (!reader->U8(&version) || version != 1)
      return false;

    base::StringPiece prefix_sp;
    if (!reader->Prefix(AES_BLOCK_SIZE + 2, &prefix_sp))
      return false;
    uint8 prefix[AES_BLOCK_SIZE + 2];
    memcpy(prefix, prefix_sp.data(), sizeof(prefix));

    uint8 prefix_copy[AES_BLOCK_SIZE + 2];
    uint8 fre[AES_BLOCK_SIZE];

    memset(prefix_copy, 0, AES_BLOCK_SIZE);
    PK11_CipherOp(decryption_context->get(), fre, &out_len, sizeof(fre),
                  prefix_copy, AES_BLOCK_SIZE);
    for (unsigned i = 0; i < AES_BLOCK_SIZE; i++)
      prefix_copy[i] = fre[i] ^ prefix[i];
    PK11_CipherOp(decryption_context->get(), fre, &out_len, sizeof(fre), prefix,
                  AES_BLOCK_SIZE);
    prefix_copy[AES_BLOCK_SIZE] = prefix[AES_BLOCK_SIZE] ^ fre[0];
    prefix_copy[AES_BLOCK_SIZE + 1] = prefix[AES_BLOCK_SIZE + 1] ^ fre[1];

    if (prefix_copy[AES_BLOCK_SIZE - 2] != prefix_copy[AES_BLOCK_SIZE] ||
        prefix_copy[AES_BLOCK_SIZE - 1] != prefix_copy[AES_BLOCK_SIZE + 1]) {
      return false;
    }

    fre[0] = prefix[AES_BLOCK_SIZE];
    fre[1] = prefix[AES_BLOCK_SIZE + 1];

    unsigned out_used = 2;

    const size_t plaintext_size = reader->size();
    if (plaintext_size < SHA1_LENGTH + 2) {
      // Too small to contain an MDC trailer.
      return false;
    }

    uint8* plaintext = reinterpret_cast<uint8*>(malloc(plaintext_size));
    arena_.push_back(plaintext);

    for (size_t i = 0; i < plaintext_size; i++) {
      uint8 b;
      if (!reader->U8(&b))
        return false;
      if (out_used == AES_BLOCK_SIZE) {
        PK11_CipherOp(decryption_context->get(), fre, &out_len, sizeof(fre),
                      fre, AES_BLOCK_SIZE);
        out_used = 0;
      }

      plaintext[i] = b ^ fre[out_used];
      fre[out_used++] = b;
    }

    // The plaintext should be followed by a Modification Detection Code
    // packet. This packet is specified such that the header is always
    // serialized as exactly these two bytes:
    if (plaintext[plaintext_size - SHA1_LENGTH - 2] != 0xd3 ||
        plaintext[plaintext_size - SHA1_LENGTH - 1] != 0x14) {
      return false;
    }

    HASHContext* hash_context = HASH_Create(HASH_AlgSHA1);
    HASH_Begin(hash_context);
    HASH_Update(hash_context, prefix_copy, sizeof(prefix_copy));
    HASH_Update(hash_context, plaintext, plaintext_size - SHA1_LENGTH);
    uint8 digest[SHA1_LENGTH];
    unsigned num_hash_bytes;
    HASH_End(hash_context, digest, &num_hash_bytes, sizeof(digest));
    HASH_Destroy(hash_context);

    if (memcmp(digest, &plaintext[plaintext_size - SHA1_LENGTH],
               SHA1_LENGTH) != 0) {
      return false;
    }

    *out_plaintext = base::StringPiece(reinterpret_cast<char*>(plaintext),
                                       plaintext_size - SHA1_LENGTH);
    return true;
  }

  // ParseLiteralData parses a Literal Data packet. See RFC 4880, section 5.9.
  bool ParseLiteralData(Reader *reader, base::StringPiece *out_data) {
    uint8 is_binary, filename_len;
    if (!reader->U8(&is_binary) ||
        !reader->U8(&filename_len) ||
        !reader->Skip(filename_len) ||
        !reader->Skip(sizeof(uint32) /* mtime */)) {
      return false;
    }

    *out_data = reader->Remainder();
    return true;
  }

  // arena_ contains malloced pointers that are used as temporary space during
  // the decryption.
  std::vector<void*> arena_;
};

class Encrypter {
 public:
  // ByteString is used throughout in order to avoid signedness issues with a
  // std::string.
  typedef std::basic_string<uint8> ByteString;

  static ByteString Encrypt(base::StringPiece plaintext,
                            base::StringPiece passphrase) {
    ByteString key;
    ByteString ske = SerializeSymmetricKeyEncrypted(passphrase, &key);

    ByteString literal_data = SerializeLiteralData(plaintext);
    ByteString se = SerializeSymmetricallyEncrypted(literal_data, key);
    return ske + se;
  }

 private:
  // MakePacket returns an OpenPGP packet tagged as type |tag|. It always uses
  // new-format headers. See RFC 4880, section 4.2.
  static ByteString MakePacket(unsigned tag, const ByteString& contents) {
    ByteString header;
    header.push_back(0x80 | 0x40 | tag);

    if (contents.size() < 192) {
      header.push_back(contents.size());
    } else if (contents.size() < 8384) {
      size_t length = contents.size();
      length -= 192;
      header.push_back(192 + (length >> 8));
      header.push_back(length & 0xff);
    } else {
      size_t length = contents.size();
      header.push_back(255);
      header.push_back(length >> 24);
      header.push_back(length >> 16);
      header.push_back(length >> 8);
      header.push_back(length);
    }

    return header + contents;
  }

  // SerializeLiteralData returns a Literal Data packet containing |contents|
  // as binary data with no filename nor mtime specified. See RFC 4880, section
  // 5.9.
  static ByteString SerializeLiteralData(base::StringPiece contents) {
    ByteString literal_data;
    literal_data.push_back(0x74);  // text mode
    literal_data.push_back(0x00);  // no filename
    literal_data.push_back(0x00);  // zero mtime
    literal_data.push_back(0x00);
    literal_data.push_back(0x00);
    literal_data.push_back(0x00);
    literal_data += ByteString(reinterpret_cast<const uint8*>(contents.data()),
                               contents.size());
    return MakePacket(kLiteralDataTag, literal_data);
  }

  // SerializeSymmetricKeyEncrypted generates a random AES-128 key from
  // |passphrase|, sets |out_key| to it and returns a Symmetric Key Encrypted
  // packet. See RFC 4880, section 5.3.
  static ByteString SerializeSymmetricKeyEncrypted(base::StringPiece passphrase,
                                                   ByteString *out_key) {
    ByteString ske;
    ske.push_back(4);  // version 4
    ske.push_back(7);  // AES-128
    ske.push_back(3);  // iterated and salted S2K
    ske.push_back(2);  // SHA-1

    uint64 salt64;
    crypto::RandBytes(&salt64, sizeof(salt64));
    ByteString salt(sizeof(salt64), 0);

    // It's a random value, so endianness doesn't matter.
    ske += ByteString(reinterpret_cast<uint8*>(&salt64), sizeof(salt64));
    ske.push_back(96);  // iteration count of 65536

    uint8 key[16];
    SaltedIteratedS2K(
        sizeof(key), HASH_AlgSHA1, passphrase,
        base::StringPiece(reinterpret_cast<char*>(&salt64), sizeof(salt64)),
        65536, key);
    *out_key = ByteString(key, sizeof(key));
    return MakePacket(kSymmetricKeyEncryptedTag, ske);
  }

  // SerializeSymmetricallyEncrypted encrypts |plaintext| with |key| and
  // returns a Symmetrically Encrypted packet containing the ciphertext. See
  // RFC 4880, section 5.7.
  static ByteString SerializeSymmetricallyEncrypted(ByteString plaintext,
                                                    const ByteString& key) {
    // We need this for PK11_CipherOp to write to, but we never check it as we
    // work in ECB mode, one block at a time.
    int out_len;

    ByteString packet;
    packet.push_back(1);  // version 1
    static const unsigned kBlockSize = 16;  // AES block size

    uint8 prefix[kBlockSize + 2], fre[kBlockSize], iv[kBlockSize];
    crypto::RandBytes(iv, kBlockSize);
    memset(fre, 0, sizeof(fre));

    ScopedPK11Context aes_context;
    CHECK(CreateAESContext(key.data(), key.size(), &aes_context));

    PK11_CipherOp(aes_context.get(), fre, &out_len, sizeof(fre), fre,
                  AES_BLOCK_SIZE);
    for (unsigned i = 0; i < 16; i++)
      prefix[i] = iv[i] ^ fre[i];
    PK11_CipherOp(aes_context.get(), fre, &out_len, sizeof(fre), prefix,
                  AES_BLOCK_SIZE);
    prefix[kBlockSize] = iv[kBlockSize - 2] ^ fre[0];
    prefix[kBlockSize + 1] = iv[kBlockSize - 1] ^ fre[1];

    packet += ByteString(prefix, sizeof(prefix));

    ByteString plaintext_copy = plaintext;
    plaintext_copy.push_back(0xd3);  // MDC packet
    plaintext_copy.push_back(20);  // packet length (20 bytes)

    HASHContext* hash_context = HASH_Create(HASH_AlgSHA1);
    HASH_Begin(hash_context);
    HASH_Update(hash_context, iv, sizeof(iv));
    HASH_Update(hash_context, iv + kBlockSize - 2, 2);
    HASH_Update(hash_context, plaintext_copy.data(), plaintext_copy.size());
    uint8 digest[SHA1_LENGTH];
    unsigned num_hash_bytes;
    HASH_End(hash_context, digest, &num_hash_bytes, sizeof(digest));
    HASH_Destroy(hash_context);

    plaintext_copy += ByteString(digest, sizeof(digest));

    fre[0] = prefix[kBlockSize];
    fre[1] = prefix[kBlockSize+1];
    unsigned out_used = 2;

    for (size_t i = 0; i < plaintext_copy.size(); i++) {
      if (out_used == kBlockSize) {
        PK11_CipherOp(aes_context.get(), fre, &out_len, sizeof(fre), fre,
                      AES_BLOCK_SIZE);
        out_used = 0;
      }

      uint8 c = plaintext_copy[i] ^ fre[out_used];
      fre[out_used++] = c;
      packet.push_back(c);
    }

    return MakePacket(kSymmetricallyEncryptedTag, packet);
  }
};

}  // anonymous namespace

// static
OpenPGPSymmetricEncrytion::Result OpenPGPSymmetricEncrytion::Decrypt(
    base::StringPiece encrypted,
    base::StringPiece passphrase,
    std::string *out) {
  EnsureNSSInit();

  Decrypter decrypter;
  base::StringPiece result;
  Result reader = decrypter.Decrypt(encrypted, passphrase, &result);
  if (reader == OK)
    *out = result.as_string();
  return reader;
}

// static
std::string OpenPGPSymmetricEncrytion::Encrypt(
    base::StringPiece plaintext,
    base::StringPiece passphrase) {
  EnsureNSSInit();

  Encrypter::ByteString b =
      Encrypter::Encrypt(plaintext, passphrase);
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

}  // namespace crypto
