#include <iostream>

#include "gtest/gtest.h"

#include "encode.h"

#define BUF_SIZE 64
td_endian_t endian_arr[2] = {TD_LITTLE_ENDIAN, TD_BIG_ENDIAN};

static int encode(SCoder *pCoder, int8_t val) { return tEncodeI8(pCoder, val); }
static int encode(SCoder *pCoder, uint8_t val) { return tEncodeU8(pCoder, val); }
static int encode(SCoder *pCoder, int16_t val) { return tEncodeI16(pCoder, val); }
static int encode(SCoder *pCoder, uint16_t val) { return tEncodeU16(pCoder, val); }
static int encode(SCoder *pCoder, int32_t val) { return tEncodeI32(pCoder, val); }
static int encode(SCoder *pCoder, uint32_t val) { return tEncodeU32(pCoder, val); }
static int encode(SCoder *pCoder, int64_t val) { return tEncodeI64(pCoder, val); }
static int encode(SCoder *pCoder, uint64_t val) { return tEncodeU64(pCoder, val); }

static int decode(SCoder *pCoder, int8_t *val) { return tDecodeI8(pCoder, val); }
static int decode(SCoder *pCoder, uint8_t *val) { return tDecodeU8(pCoder, val); }
static int decode(SCoder *pCoder, int16_t *val) { return tDecodeI16(pCoder, val); }
static int decode(SCoder *pCoder, uint16_t *val) { return tDecodeU16(pCoder, val); }
static int decode(SCoder *pCoder, int32_t *val) { return tDecodeI32(pCoder, val); }
static int decode(SCoder *pCoder, uint32_t *val) { return tDecodeU32(pCoder, val); }
static int decode(SCoder *pCoder, int64_t *val) { return tDecodeI64(pCoder, val); }
static int decode(SCoder *pCoder, uint64_t *val) { return tDecodeU64(pCoder, val); }

static int encodev(SCoder *pCoder, int8_t val) { return tEncodeI8(pCoder, val); }
static int encodev(SCoder *pCoder, uint8_t val) { return tEncodeU8(pCoder, val); }
static int encodev(SCoder *pCoder, int16_t val) { return tEncodeI16v(pCoder, val); }
static int encodev(SCoder *pCoder, uint16_t val) { return tEncodeU16v(pCoder, val); }
static int encodev(SCoder *pCoder, int32_t val) { return tEncodeI32v(pCoder, val); }
static int encodev(SCoder *pCoder, uint32_t val) { return tEncodeU32v(pCoder, val); }
static int encodev(SCoder *pCoder, int64_t val) { return tEncodeI64v(pCoder, val); }
static int encodev(SCoder *pCoder, uint64_t val) { return tEncodeU64v(pCoder, val); }

static int decodev(SCoder *pCoder, int8_t *val) { return tDecodeI8(pCoder, val); }
static int decodev(SCoder *pCoder, uint8_t *val) { return tDecodeU8(pCoder, val); }
static int decodev(SCoder *pCoder, int16_t *val) { return tDecodeI16v(pCoder, val); }
static int decodev(SCoder *pCoder, uint16_t *val) { return tDecodeU16v(pCoder, val); }
static int decodev(SCoder *pCoder, int32_t *val) { return tDecodeI32v(pCoder, val); }
static int decodev(SCoder *pCoder, uint32_t *val) { return tDecodeU32v(pCoder, val); }
static int decodev(SCoder *pCoder, int64_t *val) { return tDecodeI64v(pCoder, val); }
static int decodev(SCoder *pCoder, uint64_t *val) { return tDecodeU64v(pCoder, val); }

template <typename T>
static void simple_encode_decode_func(bool var_len) {
  uint8_t buf[BUF_SIZE];
  SCoder  coder;
  T       min_val, max_val;
  T       step = 1;

  if (typeid(T) == typeid(int8_t)) {
    min_val = INT8_MIN;
    max_val = INT8_MAX;
    step = 1;
  } else if (typeid(T) == typeid(uint8_t)) {
    min_val = 0;
    max_val = UINT8_MAX;
    step = 1;
  } else if (typeid(T) == typeid(int16_t)) {
    min_val = INT16_MIN;
    max_val = INT16_MAX;
    step = 1;
  } else if (typeid(T) == typeid(uint16_t)) {
    min_val = 0;
    max_val = UINT16_MAX;
    step = 1;
  } else if (typeid(T) == typeid(int32_t)) {
    min_val = INT32_MIN;
    max_val = INT32_MAX;
    step = ((T)1) << 16;
  } else if (typeid(T) == typeid(uint32_t)) {
    min_val = 0;
    max_val = UINT32_MAX;
    step = ((T)1) << 16;
  } else if (typeid(T) == typeid(int64_t)) {
    min_val = INT64_MIN;
    max_val = INT64_MAX;
    step = ((T)1) << 48;
  } else if (typeid(T) == typeid(uint64_t)) {
    min_val = 0;
    max_val = UINT64_MAX;
    step = ((T)1) << 48;
  }

  T i = min_val;
  for (;; /*T i = min_val; i <= max_val; i += step*/) {
    T dval;

    // Encode NULL
    for (td_endian_t endian : endian_arr) {
      tCoderInit(&coder, endian, NULL, 0, TD_ENCODER);

      if (var_len) {
        GTEST_ASSERT_EQ(encodev(&coder, i), 0);
      } else {
        GTEST_ASSERT_EQ(encode(&coder, i), 0);
        GTEST_ASSERT_EQ(coder.pos, sizeof(T));
      }

      tCoderClear(&coder);
    }

    // Encode and decode
    for (td_endian_t e_endian : endian_arr) {
      for (td_endian_t d_endian : endian_arr) {
        // Encode
        tCoderInit(&coder, e_endian, buf, BUF_SIZE, TD_ENCODER);

        if (var_len) {
          GTEST_ASSERT_EQ(encodev(&coder, i), 0);
        } else {
          GTEST_ASSERT_EQ(encode(&coder, i), 0);
          GTEST_ASSERT_EQ(coder.pos, sizeof(T));
        }

        int32_t epos = coder.pos;

        tCoderClear(&coder);
        // Decode
        tCoderInit(&coder, d_endian, buf, BUF_SIZE, TD_DECODER);

        if (var_len) {
          GTEST_ASSERT_EQ(decodev(&coder, &dval), 0);
        } else {
          GTEST_ASSERT_EQ(decode(&coder, &dval), 0);
          GTEST_ASSERT_EQ(coder.pos, sizeof(T));
        }

        GTEST_ASSERT_EQ(coder.pos, epos);

        if (typeid(T) == typeid(int8_t) || typeid(T) == typeid(uint8_t) || e_endian == d_endian) {
          GTEST_ASSERT_EQ(i, dval);
        }

        tCoderClear(&coder);
      }
    }

    if (i == max_val) break;

    if (max_val - i < step) {
      i = max_val;
    } else {
      i = i + step;
    }
  }
}

TEST(td_encode_test, encode_decode_fixed_len_integer) {
  simple_encode_decode_func<int8_t>(false);
  simple_encode_decode_func<uint8_t>(false);
  simple_encode_decode_func<int16_t>(false);
  simple_encode_decode_func<uint16_t>(false);
  simple_encode_decode_func<int32_t>(false);
  simple_encode_decode_func<uint32_t>(false);
  simple_encode_decode_func<int64_t>(false);
  simple_encode_decode_func<uint64_t>(false);
}

TEST(td_encode_test, encode_decode_variant_len_integer) {
  simple_encode_decode_func<int16_t>(true);
  simple_encode_decode_func<uint16_t>(true);
  simple_encode_decode_func<int32_t>(true);
  simple_encode_decode_func<uint32_t>(true);
  simple_encode_decode_func<int64_t>(true);
  simple_encode_decode_func<uint64_t>(true);
}

TEST(td_encode_test, encode_decode_cstr) {
  uint8_t *   buf = new uint8_t[1024 * 1024];
  char *      cstr = new char[1024 * 1024];
  const char *dcstr;
  SCoder      encoder;
  SCoder      decoder;

  for (size_t i = 0; i < 1024 * 2 - 1; i++) {
    memset(cstr, 'a', i);
    cstr[i] = '\0';
    for (td_endian_t endian : endian_arr) {
      // Encode
      tCoderInit(&encoder, endian, buf, 1024 * 1024, TD_ENCODER);

      GTEST_ASSERT_EQ(tEncodeCStr(&encoder, cstr), 0);

      tCoderClear(&encoder);

      // Decode
      tCoderInit(&decoder, endian, buf, 1024 * 1024, TD_DECODER);

      GTEST_ASSERT_EQ(tDecodeCStr(&decoder, &dcstr), 0);
      GTEST_ASSERT_EQ(memcmp(dcstr, cstr, i + 1), 0);

      tCoderClear(&decoder);
    }
  }

  delete buf;
  delete cstr;
}

typedef struct {
  int32_t A_a;
  int64_t A_b;
  char *  A_c;
} SStructA_v1;

static int tSStructA_v1_encode(SCoder *pCoder, const SStructA_v1 *pSAV1) {
  if (tStartEncode(pCoder) < 0) return -1;

  if (tEncodeI32(pCoder, pSAV1->A_a) < 0) return -1;
  if (tEncodeI64(pCoder, pSAV1->A_b) < 0) return -1;
  if (tEncodeCStr(pCoder, pSAV1->A_c) < 0) return -1;

  tEndEncode(pCoder);
  return 0;
}

static int tSStructA_v1_decode(SCoder *pCoder, SStructA_v1 *pSAV1) {
  if (tStartDecode(pCoder) < 0) return -1;

  if (tDecodeI32(pCoder, &pSAV1->A_a) < 0) return -1;
  if (tDecodeI64(pCoder, &pSAV1->A_b) < 0) return -1;
  const char *tstr;
  uint64_t    len;
  if (tDecodeCStrAndLen(pCoder, &tstr, &len) < 0) return -1;
  pSAV1->A_c = (char *)TCODER_MALLOC(len + 1, pCoder);
  memcpy(pSAV1->A_c, tstr, len + 1);

  tEndDecode(pCoder);
  return 0;
}

typedef struct {
  int32_t A_a;
  int64_t A_b;
  char *  A_c;
  // -------------------BELOW FEILDS ARE ADDED IN A NEW VERSION--------------
  int16_t A_d;
  int16_t A_e;
} SStructA_v2;

static int tSStructA_v2_encode(SCoder *pCoder, const SStructA_v2 *pSAV2) {
  if (tStartEncode(pCoder) < 0) return -1;

  if (tEncodeI32(pCoder, pSAV2->A_a) < 0) return -1;
  if (tEncodeI64(pCoder, pSAV2->A_b) < 0) return -1;
  if (tEncodeCStr(pCoder, pSAV2->A_c) < 0) return -1;

  // ------------------------NEW FIELDS ENCODE-------------------------------
  if (tEncodeI16(pCoder, pSAV2->A_d) < 0) return -1;
  if (tEncodeI16(pCoder, pSAV2->A_e) < 0) return -1;

  tEndEncode(pCoder);
  return 0;
}

static int tSStructA_v2_decode(SCoder *pCoder, SStructA_v2 *pSAV2) {
  if (tStartDecode(pCoder) < 0) return -1;

  if (tDecodeI32(pCoder, &pSAV2->A_a) < 0) return -1;
  if (tDecodeI64(pCoder, &pSAV2->A_b) < 0) return -1;
  const char *tstr;
  uint64_t    len;
  if (tDecodeCStrAndLen(pCoder, &tstr, &len) < 0) return -1;
  pSAV2->A_c = (char *)TCODER_MALLOC(len + 1, pCoder);
  memcpy(pSAV2->A_c, tstr, len + 1);

  // ------------------------NEW FIELDS DECODE-------------------------------
  if (!tDecodeIsEnd(pCoder)) {
    if (tDecodeI16(pCoder, &pSAV2->A_d) < 0) return -1;
    if (tDecodeI16(pCoder, &pSAV2->A_e) < 0) return -1;
  } else {
    pSAV2->A_d = 0;
    pSAV2->A_e = 0;
  }

  tEndDecode(pCoder);
  return 0;
}

typedef struct {
  SStructA_v1 *pA;
  int32_t      v_a;
  int8_t       v_b;
} SFinalReq_v1;

static int tSFinalReq_v1_encode(SCoder *pCoder, const SFinalReq_v1 *ps1) {
  if (tStartEncode(pCoder) < 0) return -1;

  if (tSStructA_v1_encode(pCoder, ps1->pA) < 0) return -1;
  if (tEncodeI32(pCoder, ps1->v_a) < 0) return -1;
  if (tEncodeI8(pCoder, ps1->v_b) < 0) return -1;

  tEndEncode(pCoder);
  return 0;
}

static int tSFinalReq_v1_decode(SCoder *pCoder, SFinalReq_v1 *ps1) {
  if (tStartDecode(pCoder) < 0) return -1;

  ps1->pA = (SStructA_v1 *)TCODER_MALLOC(sizeof(*(ps1->pA)), pCoder);
  if (tSStructA_v1_decode(pCoder, ps1->pA) < 0) return -1;
  if (tDecodeI32(pCoder, &ps1->v_a) < 0) return -1;
  if (tDecodeI8(pCoder, &ps1->v_b) < 0) return -1;

  tEndDecode(pCoder);
  return 0;
}

typedef struct {
  SStructA_v2 *pA;
  int32_t      v_a;
  int8_t       v_b;
  // ----------------------- Feilds added -----------------------
  int16_t v_c;
} SFinalReq_v2;

static int tSFinalReq_v2_encode(SCoder *pCoder, const SFinalReq_v2 *ps2) {
  if (tStartEncode(pCoder) < 0) return -1;

  if (tSStructA_v2_encode(pCoder, ps2->pA) < 0) return -1;
  if (tEncodeI32(pCoder, ps2->v_a) < 0) return -1;
  if (tEncodeI8(pCoder, ps2->v_b) < 0) return -1;

  // ----------------------- Feilds added encode -----------------------
  if (tEncodeI16(pCoder, ps2->v_c) < 0) return -1;

  tEndEncode(pCoder);
  return 0;
}

static int tSFinalReq_v2_decode(SCoder *pCoder, SFinalReq_v2 *ps2) {
  if (tStartDecode(pCoder) < 0) return -1;

  ps2->pA = (SStructA_v2 *)TCODER_MALLOC(sizeof(*(ps2->pA)), pCoder);
  if (tSStructA_v2_decode(pCoder, ps2->pA) < 0) return -1;
  if (tDecodeI32(pCoder, &ps2->v_a) < 0) return -1;
  if (tDecodeI8(pCoder, &ps2->v_b) < 0) return -1;

  // ----------------------- Feilds added decode -----------------------
  if (tDecodeIsEnd(pCoder)) {
    ps2->v_c = 0;
  } else {
    if (tDecodeI16(pCoder, &ps2->v_c) < 0) return -1;
  }

  tEndDecode(pCoder);
  return 0;
}

TEST(td_encode_test, compound_struct_encode_test) {
  SCoder       encoder, decoder;
  uint8_t *    buf1;
  int32_t      buf1size;
  uint8_t *    buf2;
  int32_t      buf2size;
  SStructA_v1  sa1 = {.A_a = 10, .A_b = 65478, .A_c = "Hello"};
  SStructA_v2  sa2 = {.A_a = 10, .A_b = 65478, .A_c = "Hello", .A_d = 67, .A_e = 13};
  SFinalReq_v1 req1 = {.pA = &sa1, .v_a = 15, .v_b = 35};
  SFinalReq_v2 req2 = {.pA = &sa2, .v_a = 15, .v_b = 32, .v_c = 37};
  SFinalReq_v1 dreq1;
  SFinalReq_v2 dreq21, dreq22;

  // Get size
  tCoderInit(&encoder, TD_LITTLE_ENDIAN, nullptr, 0, TD_ENCODER);
  GTEST_ASSERT_EQ(tSFinalReq_v1_encode(&encoder, &req1), 0);
  buf1size = encoder.pos;
  buf1 = new uint8_t[encoder.pos];
  tCoderClear(&encoder);

  tCoderInit(&encoder, TD_LITTLE_ENDIAN, nullptr, 0, TD_ENCODER);
  GTEST_ASSERT_EQ(tSFinalReq_v2_encode(&encoder, &req2), 0);
  buf2size = encoder.pos;
  buf2 = new uint8_t[encoder.pos];
  tCoderClear(&encoder);

  // Encode
  tCoderInit(&encoder, TD_LITTLE_ENDIAN, buf1, buf1size, TD_ENCODER);
  GTEST_ASSERT_EQ(tSFinalReq_v1_encode(&encoder, &req1), 0);
  tCoderClear(&encoder);

  tCoderInit(&encoder, TD_LITTLE_ENDIAN, buf2, buf2size, TD_ENCODER);
  GTEST_ASSERT_EQ(tSFinalReq_v2_encode(&encoder, &req2), 0);
  tCoderClear(&encoder);

  // Decode
  tCoderInit(&decoder, TD_LITTLE_ENDIAN, buf1, buf1size, TD_DECODER);
  GTEST_ASSERT_EQ(tSFinalReq_v1_decode(&decoder, &dreq1), 0);
  tCoderClear(&decoder);

  tCoderInit(&decoder, TD_LITTLE_ENDIAN, buf1, buf1size, TD_DECODER);
  GTEST_ASSERT_EQ(tSFinalReq_v2_decode(&decoder, &dreq21), 0);
  tCoderClear(&decoder);

  tCoderInit(&decoder, TD_LITTLE_ENDIAN, buf2, buf2size, TD_DECODER);
  GTEST_ASSERT_EQ(tSFinalReq_v2_decode(&decoder, &dreq22), 0);
  tCoderClear(&decoder);
}