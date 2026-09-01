// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Protocol.hpp"
#include "test_fw.hpp"
#include <string>
#include <vector>
#include <span>
using namespace ah::proto;

AH_TEST(roundtrip) {
  std::vector<std::uint8_t> payload = {1, 2, 3, 4, 5};
  auto frame = encodeFrame(MessageType::OBSERVATION, payload);
  Frame out;
  std::size_t consumed = 0;
  std::string err;
  auto r = decodeFrame(frame, out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::OK);
  CHECK_EQ((int)out.type, (int)MessageType::OBSERVATION);
  CHECK_EQ(consumed, frame.size());
  CHECK(out.payload == payload);
}

AH_TEST(truncated_needs_more) {
  auto frame = encodeFrame(MessageType::HELLO, std::span<const std::uint8_t>());
  Frame out; std::size_t consumed = 0; std::string err;
  // Truncate to half: header may be incomplete or frame incomplete
  auto r = decodeFrame(std::span<const std::uint8_t>(frame.data(), 5), out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::NEED_MORE);
}

AH_TEST(malformed_magic) {
  auto frame = encodeFrame(MessageType::HELLO, std::span<const std::uint8_t>());
  Frame out; std::size_t consumed = 0; std::string err;
  frame[0] = 'X';
  auto r = decodeFrame(frame, out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::MALFORMED);
}

AH_TEST(invalid_version) {
  auto frame = encodeFrame(MessageType::HELLO, std::span<const std::uint8_t>());
  Frame out; std::size_t consumed = 0; std::string err;
  frame[4] = 99;
  auto r = decodeFrame(frame, out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::MALFORMED);
}

AH_TEST(invalid_enum_type) {
  auto frame = encodeFrame(MessageType::HELLO, std::span<const std::uint8_t>());
  Frame out; std::size_t consumed = 0; std::string err;
  frame[5] = 99;
  auto r = decodeFrame(frame, out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::MALFORMED);
}

AH_TEST(crc_mismatch) {
  auto frame = encodeFrame(MessageType::HELLO, std::span<const std::uint8_t>());
  Frame out; std::size_t consumed = 0; std::string err;
  frame[frame.size() - 1] ^= 0xFF;
  auto r = decodeFrame(frame, out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::MALFORMED);
}

AH_TEST(trailing_garbage_consumed_exactly) {
  auto frame = encodeFrame(MessageType::ACK, std::vector<std::uint8_t>{9});
  Frame out; std::size_t consumed = 0; std::string err;
  auto r = decodeFrame(frame, out, consumed, &err);
  CHECK_EQ((int)r, (int)DecodeResult::OK);
  CHECK_EQ(consumed, frame.size());
  // The exact single frame leaves no trailing bytes in the remainder.
  CHECK_EQ(consumed, frame.size());
  auto remainder = std::span<const std::uint8_t>(frame.data() + consumed, frame.size() - consumed);
  CHECK_EQ(remainder.size(), 0u);
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_protocol"); }
