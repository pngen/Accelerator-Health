// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "accelerator-health/Protocol.hpp"
#include "accelerator-health/Util.hpp"

#include <cstring>
#include <stdexcept>

namespace ah::proto {

std::vector<std::uint8_t> encodeFrame(MessageType type, std::span<const std::uint8_t> payload) {
  if (payload.size() > kMaxFramePayload) throw std::runtime_error("protocol frame payload too large");
  std::vector<std::uint8_t> frame;
  frame.reserve(kHeaderSize + payload.size() + kCrcSize);
  frame.insert(frame.end(), kMagic.begin(), kMagic.end());
  frame.push_back(kProtocolVersion);
  frame.push_back(static_cast<std::uint8_t>(static_cast<int>(type)));
  std::uint8_t lenBytes[4];
  util::store_le(static_cast<std::uint32_t>(payload.size()), lenBytes);
  frame.insert(frame.end(), lenBytes, lenBytes + 4);
  frame.insert(frame.end(), {0, 0, 0, 0});  // reserved
  frame.insert(frame.end(), payload.begin(), payload.end());
  const std::uint32_t crc = util::crc32(frame.data(), frame.size());
  std::uint8_t crcBytes[4];
  util::store_le(crc, crcBytes);
  frame.insert(frame.end(), crcBytes, crcBytes + 4);
  return frame;
}

DecodeResult decodeFrame(std::span<const std::uint8_t> buffer, Frame& out, std::size_t& consumed,
                         std::string* err) {
  consumed = 0;
  if (buffer.size() < kHeaderSize) return DecodeResult::NEED_MORE;  // incomplete header
  if (std::memcmp(buffer.data(), kMagic.data(), 4) != 0) { if (err) *err = "bad magic"; return DecodeResult::MALFORMED; }
  const std::uint8_t version = buffer[4];
  if (version != kProtocolVersion) { if (err) *err = "unsupported protocol version"; return DecodeResult::MALFORMED; }
  auto type = message_type_from_byte(buffer[5]);
  if (!type) { if (err) *err = "invalid message type"; return DecodeResult::MALFORMED; }
  const std::uint32_t len = util::load_le32(buffer.data() + 6);
  if (len > kMaxFramePayload) { if (err) *err = "oversized frame"; return DecodeResult::MALFORMED; }
  const std::size_t total = kHeaderSize + static_cast<std::size_t>(len) + kCrcSize;
  if (buffer.size() < total) return DecodeResult::NEED_MORE;  // truncated frame
  const std::uint32_t crcExpected = util::load_le32(buffer.data() + total - kCrcSize);
  const std::uint32_t crcActual = util::crc32(buffer.data(), total - kCrcSize);
  if (crcExpected != crcActual) { if (err) *err = "crc mismatch"; return DecodeResult::MALFORMED; }
  out.version = version;
  out.type = *type;
  out.payload.assign(buffer.data() + kHeaderSize, buffer.data() + kHeaderSize + len);
  consumed = total;
  return DecodeResult::OK;
}

std::vector<std::uint8_t> encodeMessage(MessageType type, const wire::Writer& body) {
  return encodeFrame(type, body.data());
}

}  // namespace ah::proto
