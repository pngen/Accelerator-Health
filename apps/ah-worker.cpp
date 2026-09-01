// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
// Worker: connects to a coordinator over framed TCP and publishes observations /
// validation results. Used by the multiprocess distributed-health proof.
#include "accelerator-health/Messages.hpp"
#include "accelerator-health/Net.hpp"
#include "accelerator-health/Persistence.hpp"
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Time.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using namespace ah;

static AcceleratorObservation makeObs(AcceleratorId id, DeviceUuid uuid, NodeId node, WorkerId w,
                                      WorkerBootId b, Timestamp ts, DeviceGeneration gen, ObservationGeneration og) {
  AcceleratorObservation o;
  (void)uuid;
  o.accelerator = id; o.node = node; o.worker = w; o.workerBootId = b;
  o.observationGeneration = og; o.deviceGeneration = gen; o.timestamp = ts;
  o.dimensions = {
    nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::RUNTIME_INIT, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::TRANSFER, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::EXECUTION, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::MEMORY_INTEGRITY, EvidenceClass::MEASURED, ts),
  };
  return o;
}

static bool sendAndExpect(net::TcpClient& c, proto::MessageType type, const wire::Writer& body,
                          proto::MessageType* respType, std::string* msg) {
  proto::Frame out;
  out.type = type;
  out.payload = body.data();
  std::string err;
  if (!net::sendFrame(c.socketFd(), out, &err)) { *msg = "send fail"; return false; }
  proto::Frame in;
  bool closed = false;
  if (!net::recvFrame(c.socketFd(), in, closed, &err)) { *msg = "recv fail " + err; return false; }
  if (respType) *respType = in.type;
  if (in.type == proto::MessageType::ACK) {
    wire::Reader r(in.payload);
    proto::MessageType t;
    bool ok;
    std::string m;
    if (!msg::readAckBody(r, t, ok, m)) return false;
    *msg = m;
    return ok;
  }
  if (in.type == proto::MessageType::SNAPSHOT_RESPONSE) { return true; }
  *msg = "unexpected " + std::to_string(static_cast<int>(in.type));
  return false;
}

int main(int argc, char** argv) {
  std::string host = "127.0.0.1";
  std::uint16_t port = 7000;
  std::uint64_t workerRaw = 1, bootRaw = 1, nodeRaw = 1, uuidRaw = 1, epoch = 1, obsgen = 1, devicegen = 1;
  std::string mode = "register";
  int holdSeconds = 8;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--host" && i + 1 < argc) host = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
    else if (a == "--worker-id" && i + 1 < argc) workerRaw = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--boot-id" && i + 1 < argc) bootRaw = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--node" && i + 1 < argc) nodeRaw = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--device" && i + 1 < argc) uuidRaw = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--epoch" && i + 1 < argc) epoch = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--obsgen" && i + 1 < argc) obsgen = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--devicegen" && i + 1 < argc) devicegen = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--mode" && i + 1 < argc) mode = argv[++i];
    else if (a == "--hold" && i + 1 < argc) holdSeconds = std::atoi(argv[++i]);
  }

  WorkerId worker{workerRaw};
  WorkerBootId boot{bootRaw};
  NodeId node{nodeRaw};
  DeviceUuid uuid{uuidRaw};

  net::TcpClient c;
  std::string err;
  if (!c.connect(host, port, &err)) { std::printf("ERROR connect: %s\n", err.c_str()); return 1; }

  // REGISTER
  msg::HelloBody hello;
  hello.node = node; hello.worker = worker; hello.boot = boot;
  hello.generation = WorkerGeneration{1};
  const AcceleratorId devId{uuidRaw};
  DeviceIdentity dev;
  dev.id = devId;
  dev.uuid = uuid;
  dev.name = "synthetic-accelerator";
  dev.computeCapabilityMajor = 12;
  dev.computeCapabilityMinor = 0;
  dev.totalMemory = 1ull << 30;
  dev.freeMemory = 1ull << 30;
  dev.supported = true;
  wire::Writer reg = msg::regBody(hello, {dev});
  proto::Frame regF; regF.type = proto::MessageType::REGISTER; regF.payload = reg.data();
  net::sendFrame(c.socketFd(), regF, &err);
  proto::Frame regAck; bool closed = false;
  net::recvFrame(c.socketFd(), regAck, closed, &err);

  std::string out;
  if (mode == "register") { std::printf("RESULT registered\n"); return 0; }

  if (mode == "query") {
    wire::Writer q;
    proto::Frame qf; qf.type = proto::MessageType::SNAPSHOT_REQUEST; qf.payload = q.data();
    net::sendFrame(c.socketFd(), qf, &err);
    proto::Frame resp; bool cl2 = false;
    if (!net::recvFrame(c.socketFd(), resp, cl2, &err)) { std::printf("ERROR query\n"); return 1; }
    HealthStore s(std::make_shared<FakeClock>(0), HealthPolicy{}, 1);
    std::string derr;
    if (!HealthStore::decode(resp.payload, s, &derr)) { std::printf("ERROR decode %s\n", derr.c_str()); return 1; }
    for (auto& snap : s.allSnapshots()) {
      if (snap.uuid == uuid) {
        std::printf("QUERY state=%s readiness=%s digest=%s tracked=%zu\n",
                    to_string(snap.state), to_string(snap.readiness), snap.latestAssessment.digest.c_str(),
                    s.deviceIds().size());
      }
    }
    return 0;
  }

  // Common: build observation & validation then act per mode.
  // Use a live monotonic timestamp so the coordinator treats evidence as CURRENT.
  const Timestamp now = SystemClock{}.now();
  AuthorityEnvelope env;
  env.coordinatorEpoch = epoch;
  env.workerId = worker;
  env.workerBootId = boot;
  env.observationGeneration = ObservationGeneration{obsgen};
  env.deviceGeneration = DeviceGeneration{devicegen};

  if (mode == "stale-epoch") env.coordinatorEpoch = epoch - 1;
  if (mode == "stale-boot") env.workerBootId = WorkerBootId{bootRaw + 1000};
  if (mode == "stale-devgen") env.deviceGeneration = DeviceGeneration{0};

  auto obs = makeObs(devId, uuid, node, worker, boot, now, DeviceGeneration{devicegen}, ObservationGeneration{obsgen});
  wire::Writer ob = msg::obsBody(env, obs);
  std::string m;
  proto::MessageType rt;
  bool accepted = false;
  if (mode == "stale-obsgen") {
    proto::MessageType rt2;
    std::string m2;
    bool a1 = sendAndExpect(c, proto::MessageType::OBSERVATION, ob, &rt, &m);
    bool a2 = sendAndExpect(c, proto::MessageType::OBSERVATION, ob, &rt2, &m2);
    std::printf("RESULT stale-obsgen first=%d second=%d msg=%s\n", a1 ? 1 : 0, a2 ? 1 : 0, m2.c_str());
    return 0;
  }
  if (mode == "validate" || mode == "revalidate" || mode == "combine" || mode == "live") {
    accepted = sendAndExpect(c, proto::MessageType::OBSERVATION, ob, &rt, &m);
    if (mode != "live") std::printf("RESULT OBSERVATION accept=%d msg=%s\n", accepted ? 1 : 0, m.c_str());
    ValidationRecord v;
    v.id = ValidationRunId{bootRaw}; v.accelerator = devId; v.timestamp = now;
    v.profile = ValidationProfile::FULL; v.depth = ValidationDepth::FULL; v.passed = true; v.maxError = 0.0f;
    v.worker = worker; v.workerBootId = boot; v.validationGeneration = ValidationGeneration{1};
    wire::Writer vb = msg::valBody(env, v);
    bool vac = sendAndExpect(c, proto::MessageType::VALIDATION_RESULT, vb, &rt, &m);
    if (mode != "live") std::printf("RESULT VALIDATION accept=%d msg=%s\n", vac ? 1 : 0, m.c_str());
  } else {
    accepted = sendAndExpect(c, proto::MessageType::OBSERVATION, ob, &rt, &m);
    (void)rt;
  }

  if (mode == "live") { std::printf("LIVE-READY\n"); std::fflush(stdout); std::this_thread::sleep_for(std::chrono::seconds(holdSeconds)); return 0; }
  std::printf("RESULT %s accept=%d msg=%s\n", mode.c_str(), accepted ? 1 : 0, m.c_str());
  return 0;
}
