// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
// Coordinator: authoritative health store over framed TCP with strict fencing.
#include "accelerator-health/Messages.hpp"
#include "accelerator-health/Net.hpp"
#include "accelerator-health/Persistence.hpp"
#include "accelerator-health/Store.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

using namespace ah;

namespace {
std::atomic<bool> g_stop{false};
std::mutex g_print;
void log(const std::string& s) {
  std::lock_guard<std::mutex> lk(g_print);
  std::fputs(s.c_str(), stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
}
}  // namespace

int main(int argc, char** argv) {
  std::uint16_t port = 7000;
  std::uint64_t epoch = 1;
  std::string stateFile;
  bool load = false;
  bool saveOnSnapshot = true;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
    else if (a == "--epoch" && i + 1 < argc) epoch = std::strtoull(argv[++i], nullptr, 10);
    else if (a == "--state" && i + 1 < argc) stateFile = argv[++i];
    else if (a == "--load") load = true;
    else if (a == "--no-save-on-snapshot") saveOnSnapshot = false;
  }

  std::shared_ptr<Clock> clock = std::make_shared<SystemClock>();
  HealthPolicy pol;
  pol.id = PolicyId{1};
  HealthStore store(clock, pol, epoch);
  if (load && !stateFile.empty()) {
    std::string err;
    if (!HealthStore::loadFromFile(stateFile, store, &err)) log("ERROR load: " + err);
  }
  net::TcpServer server;
  std::string err;
  if (!server.listen(port, &err)) { log("FATAL listen: " + err); return 1; }
  log("UP port=" + std::to_string(port) + " epoch=" + std::to_string(epoch));

  while (!g_stop) {
    int fd = server.acceptClient();
    if (fd < 0) break;
    std::thread([&store, &stateFile, saveOnSnapshot, fd] {
      WorkerId worker{};
      WorkerBootId boot{};
      bool haveWorker = false;
      bool peerClosed = false;
      try {
      while (!peerClosed) {
        proto::Frame frame;
        std::string err2;
        if (!net::recvFrame(fd, frame, peerClosed, &err2)) {
          if (err2 != "oversized frame" && err2 != "bad magic" && err2 != "invalid message type" &&
              err2 != "unsupported protocol version" && err2 != "crc mismatch") break;
          break;
        }
        proto::MessageType t = frame.type;
        wire::Reader r(frame.payload);
        switch (t) {
          case proto::MessageType::REGISTER: {
            msg::HelloBody hello;
            if (!msg::HelloBody::read(r, hello)) break;
            worker = hello.worker;
            boot = hello.boot;
            haveWorker = true;
            store.registerWorker(worker, boot);
            std::vector<DeviceIdentity> devs;
            const auto n = r.u32();
            if (r.fail() || n > util::kMaxCount) break;
            for (std::uint32_t i = 0; i < n; ++i) {
              DeviceIdentity d;
              d.id = AcceleratorId::read(r);
              d.uuid = DeviceUuid::read(r);
              d.name = r.string();
              d.computeCapabilityMajor = r.u8();
              d.computeCapabilityMinor = r.u8();
              d.totalMemory = r.u64();
              d.freeMemory = r.u64();
              d.supported = r.u8() != 0;
              devs.push_back(d);
              store.registerDevice(d.uuid, hello.node);
              log("REGISTER device=" + d.id.string() + " worker=" + worker.string() + " boot=" + boot.string());
            }
            store.reassessAll();
            wire::Writer ack = msg::ackBody(proto::MessageType::REGISTER, true, "registered");
            proto::Frame out; out.type = proto::MessageType::ACK; out.payload = ack.data();
            net::sendFrame(fd, out, &err2);
            break;
          }
          case proto::MessageType::OBSERVATION: {
            AuthorityEnvelope env;
            AcceleratorObservation obs;
            if (!msg::readObsBody(r, env, obs)) break;
            std::string res = store.acceptObservation(obs, env);
            auto snap = store.snapshot(obs.accelerator);
            log("EVENT OBSERVATION device=" + obs.accelerator.string() + " " + res + " state=" + to_string(snap.state) + " readiness=" + to_string(snap.readiness));
            wire::Writer ack = msg::ackBody(proto::MessageType::OBSERVATION, res == "accepted", res);
            proto::Frame out; out.type = proto::MessageType::ACK; out.payload = ack.data();
            net::sendFrame(fd, out, &err2);
            break;
          }
          case proto::MessageType::VALIDATION_RESULT: {
            AuthorityEnvelope env;
            ValidationRecord v;
            if (!msg::readValBody(r, env, v)) break;
            bool dup = false;
            for (auto& d : store.allSnapshots()) (void)d;  // touch
            store.recordValidation(v);
            auto snap = store.snapshot(v.accelerator);
            log("EVENT VALIDATION device=" + v.accelerator.string() + " passed=" + (v.passed ? "1" : "0") + " state=" + to_string(snap.state) + " readiness=" + to_string(snap.readiness));
            wire::Writer ack = msg::ackBody(proto::MessageType::VALIDATION_RESULT, true, "recorded");
            proto::Frame out; out.type = proto::MessageType::ACK; out.payload = ack.data();
            net::sendFrame(fd, out, &err2);
            (void)dup;
            break;
          }
          case proto::MessageType::FAULT_REPORT: {
            AuthorityEnvelope env;
            Fault f;
            if (!msg::readFaultBody(r, env, f)) break;
            store.recordFault(f);
            log("EVENT FAULT device=" + f.accelerator.string() + " " + to_string(f.type));
            wire::Writer ack = msg::ackBody(proto::MessageType::FAULT_REPORT, true, "recorded");
            proto::Frame out; out.type = proto::MessageType::ACK; out.payload = ack.data();
            net::sendFrame(fd, out, &err2);
            break;
          }
          case proto::MessageType::SNAPSHOT_REQUEST: {
            if (saveOnSnapshot && !stateFile.empty()) store.saveToFile(stateFile, &err2);
            const auto bytes = store.encodeState();
            proto::Frame out; out.type = proto::MessageType::SNAPSHOT_RESPONSE; out.payload = bytes;
            net::sendFrame(fd, out, &err2);
            log("EVENT SNAPSHOT saved=" + std::to_string(saveOnSnapshot ? 1 : 0));
            break;
          }
          default:
            break;
        }
      }
      } catch (const std::exception& ex) {
        log("THREAD-EXCEPTION " + std::string(ex.what()));
      } catch (...) {
        log("THREAD-EXCEPTION (unknown)");
      }
      if (haveWorker) {
        store.invalidateWorker(worker, boot, store.clock()->now());
        log("DISCONNECT worker=" + worker.string() + " boot=" + boot.string());
      }
      net::closeSocket(fd);
    }).detach();
  }
  server.close();
  return 0;
}