// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#pragma once
#include "accelerator-health/Backend.hpp"
#include "accelerator-health/Evidence.hpp"
#include "accelerator-health/Fault.hpp"
#include "accelerator-health/Protocol.hpp"
#include "accelerator-health/StrongIdentity.hpp"

namespace ah::msg {

struct HelloBody {
  std::uint8_t version = proto::kProtocolVersion;
  NodeId node{};
  WorkerId worker{};
  WorkerBootId boot{};
  WorkerGeneration generation{};
  void write(wire::Writer& w) const {
    w.u8(version); node.write(w); worker.write(w); boot.write(w); generation.write(w);
  }
  static bool read(wire::Reader& r, HelloBody& h) noexcept {
    h.version = r.u8(); h.node = NodeId::read(r); h.worker = WorkerId::read(r);
    h.boot = WorkerBootId::read(r); h.generation = WorkerGeneration::read(r);
    return r.ok();
  }
};

inline wire::Writer regBody(const HelloBody& hello, const std::vector<DeviceIdentity>& devices) {
  wire::Writer w;
  hello.write(w);
  w.u32(static_cast<std::uint32_t>(devices.size()));
  for (const auto& d : devices) {
    d.id.write(w);
    d.uuid.write(w);
    w.string(d.name);
    w.u8(static_cast<std::uint8_t>(d.computeCapabilityMajor));
    w.u8(static_cast<std::uint8_t>(d.computeCapabilityMinor));
    w.u64(d.totalMemory);
    w.u64(d.freeMemory);
    w.u8(d.supported ? 1 : 0);
  }
  return w;
}

inline wire::Writer obsBody(const AuthorityEnvelope& env, const AcceleratorObservation& obs) {
  wire::Writer w;
  env.write(w);
  obs.write(w);
  return w;
}
inline bool readObsBody(wire::Reader& r, AuthorityEnvelope& env, AcceleratorObservation& obs) {
  env = AuthorityEnvelope::read(r);
  return AcceleratorObservation::read(r, obs);
}

inline wire::Writer valBody(const AuthorityEnvelope& env, const ValidationRecord& v) {
  wire::Writer w;
  env.write(w);
  v.write(w);
  return w;
}
inline bool readValBody(wire::Reader& r, AuthorityEnvelope& env, ValidationRecord& v) {
  env = AuthorityEnvelope::read(r);
  return ValidationRecord::read(r, v);
}

inline wire::Writer faultBody(const AuthorityEnvelope& env, const Fault& f) {
  wire::Writer w;
  env.write(w);
  f.write(w);
  return w;
}
inline bool readFaultBody(wire::Reader& r, AuthorityEnvelope& env, Fault& f) {
  env = AuthorityEnvelope::read(r);
  return Fault::read(r, f);
}

inline wire::Writer ackBody(proto::MessageType type, bool accepted, const std::string& msg) {
  wire::Writer w;
  w.u8(static_cast<std::uint8_t>(static_cast<int>(type)));
  w.u8(accepted ? 1 : 0);
  w.string(msg);
  return w;
}
inline bool readAckBody(wire::Reader& r, proto::MessageType& type, bool& accepted, std::string& msg) {
  auto t = proto::message_type_from_byte(r.u8());
  if (!t) { r.set_fail(); return false; }
  type = *t;
  accepted = r.u8() != 0;
  msg = r.string();
  return r.ok();
}

}  // namespace ah::msg
