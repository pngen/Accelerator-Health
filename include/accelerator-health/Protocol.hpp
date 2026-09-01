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
#pragma once

#include "accelerator-health/Store.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ah::proto {

inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::size_t kMaxFramePayload = (std::size_t{1} << 20);  // 1 MiB
inline constexpr std::size_t kHeaderSize = 14;                            // magic(4)+ver(1)+type(1)+len(4)+reserved(4)
inline constexpr std::size_t kCrcSize = 4;

// Fixed magic bytes "AHF1".
inline constexpr std::array<std::uint8_t, 4> kMagic = {{0x41, 0x48, 0x46, 0x31}};

enum class MessageType : std::uint8_t {
  HELLO = 0,
  REGISTER = 1,
  OBSERVATION = 2,
  VALIDATION_RESULT = 3,
  FAULT_REPORT = 4,
  HEALTH_ASSESSMENT = 5,
  QUARANTINE = 6,
  CLEAR_QUARANTINE = 7,
  RECOVERY_BEGIN = 8,
  RECOVERY_RESULT = 9,
  POLICY_UPDATE = 10,
  SNAPSHOT_REQUEST = 11,
  SNAPSHOT_RESPONSE = 12,
  ACK = 13,
  ERROR = 14,
};
inline constexpr std::array<std::pair<MessageType, const char*>, 15> kMessageTypeNames = {{
  {MessageType::HELLO, "HELLO"}, {MessageType::REGISTER, "REGISTER"},
  {MessageType::OBSERVATION, "OBSERVATION"}, {MessageType::VALIDATION_RESULT, "VALIDATION_RESULT"},
  {MessageType::FAULT_REPORT, "FAULT_REPORT"}, {MessageType::HEALTH_ASSESSMENT, "HEALTH_ASSESSMENT"},
  {MessageType::QUARANTINE, "QUARANTINE"}, {MessageType::CLEAR_QUARANTINE, "CLEAR_QUARANTINE"},
  {MessageType::RECOVERY_BEGIN, "RECOVERY_BEGIN"}, {MessageType::RECOVERY_RESULT, "RECOVERY_RESULT"},
  {MessageType::POLICY_UPDATE, "POLICY_UPDATE"}, {MessageType::SNAPSHOT_REQUEST, "SNAPSHOT_REQUEST"},
  {MessageType::SNAPSHOT_RESPONSE, "SNAPSHOT_RESPONSE"}, {MessageType::ACK, "ACK"},
  {MessageType::ERROR, "ERROR"},
}};
constexpr const char* to_string(MessageType t) noexcept {
  for (auto& kv : kMessageTypeNames) if (kv.first == t) return kv.second;
  return "UNKNOWN";
}
constexpr std::optional<MessageType> message_type_from_byte(std::uint8_t b) noexcept {
  for (auto& kv : kMessageTypeNames) if (static_cast<std::uint8_t>(static_cast<int>(kv.first)) == b) return kv.first;
  return std::nullopt;
}

struct Frame {
  std::uint8_t version = kProtocolVersion;
  MessageType type = MessageType::HELLO;
  std::vector<std::uint8_t> payload;
};

enum class DecodeResult { OK, NEED_MORE, MALFORMED };

// Encode a frame. Throws on oversized payload.
std::vector<std::uint8_t> encodeFrame(MessageType type, std::span<const std::uint8_t> payload);

// Decode exactly one frame from a buffer. On OK, consumed is the number of bytes
// consumed (the frame). On NEED_MORE, no bytes are consumed (incomplete frame).
// On MALFORMED, err is set and nothing is consumed (peer must resync/reject).
DecodeResult decodeFrame(std::span<const std::uint8_t> buffer, Frame& out, std::size_t& consumed,
                         std::string* err);

// Convenience: encode a structured message body into a frame.
std::vector<std::uint8_t> encodeMessage(MessageType type, const wire::Writer& body);

}  // namespace ah::proto
