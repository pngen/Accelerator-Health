// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#pragma once
#include "accelerator-health/Protocol.hpp"
#include <cstdint>
#include <span>
#include <string>

namespace ah::net {

enum class SocketStatus { OK, CLOSED, FAILED, TIMEOUT };

// Small blocking TCP client over framed bytes.
class TcpClient {
 public:
  TcpClient();
  ~TcpClient();
  TcpClient(const TcpClient&) = delete;
  TcpClient& operator=(const TcpClient&) = delete;
  bool connect(const std::string& host, std::uint16_t port, std::string* err);
  void close();
  bool sendBytes(std::span<const std::uint8_t> data);
  // Reads exactly one protocol frame (returns false on close/error and sets ok).
  bool recvFrame(proto::Frame& out, bool& peerClosed, std::string* err);
  bool recvBytes(std::uint8_t* out, std::size_t n) noexcept;
  int socketFd() const { return sock_; }

 private:
  int sock_ = -1;
};

// Blocking TCP server that accepts one connection at a time.
class TcpServer {
 public:
  TcpServer();
  ~TcpServer();
  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;
  bool listen(std::uint16_t port, std::string* err);
  int acceptClient();  // returns fd or -1
  void close();

 private:
  int listener_ = -1;
};

// Raw-fd framed helpers (for server-accepted sockets).
bool sendFrame(int fd, const proto::Frame& frame, std::string* err);
bool recvFrame(int fd, proto::Frame& frame, bool& peerClosed, std::string* err);
bool closeSocket(int fd);

}  // namespace ah::net
