// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Net.hpp"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

#include <cstring>
#include <vector>

namespace ah::net {

namespace {
bool initWinsock() {
#ifdef _WIN32
  static bool done = [] { WSADATA d; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }();
  return done;
#else
  return true;
#endif
}
int lastSocketError() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}
// Windows and POSIX both use int fd; close() maps to closesocket.
bool closeFd(int fd) {
#ifdef _WIN32
  return closesocket(fd) == 0;
#else
  return close(fd) == 0;
#endif
}
int fdType() {
#ifdef _WIN32
  return SOCK_STREAM;
#else
  return SOCK_STREAM;
#endif
}
}  // namespace

TcpClient::TcpClient() {}
TcpClient::~TcpClient() { close(); }

bool TcpClient::connect(const std::string& host, std::uint16_t port, std::string* err) {
  if (!initWinsock()) { if (err) *err = "winsock init failed"; return false; }
  sock_ = static_cast<int>(socket(AF_INET, fdType(), 0));
  if (sock_ < 0) { if (err) *err = "socket failed"; return false; }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
  if (::connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    if (err) *err = "connect failed";
    close();
    return false;
  }
  return true;
}

void TcpClient::close() {
  if (sock_ >= 0) { closeFd(sock_); sock_ = -1; }
}

bool TcpClient::sendBytes(std::span<const std::uint8_t> data) {
  if (sock_ < 0) return false;
  std::size_t sent = 0;
  while (sent < data.size()) {
    const int n = static_cast<int>(::send(sock_, reinterpret_cast<const char*>(data.data() + sent),
                                   static_cast<int>(data.size() - sent), 0));
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool TcpClient::recvBytes(std::uint8_t* out, std::size_t n) noexcept {
  std::size_t got = 0;
  while (got < n) {
    const int r = static_cast<int>(::recv(sock_, reinterpret_cast<char*>(out + got),
                                   static_cast<int>(n - got), 0));
    if (r == 0) return false;
    if (r < 0) return false;
    got += static_cast<std::size_t>(r);
  }
  return true;
}

bool TcpClient::recvFrame(proto::Frame& out, bool& peerClosed, std::string* err) {
  peerClosed = false;
  if (sock_ < 0) { if (err) *err = "not connected"; return false; }
  std::vector<std::uint8_t> buf(proto::kHeaderSize);
  if (!recvBytes(buf.data(), proto::kHeaderSize)) { peerClosed = true; return false; }
  const std::uint32_t len = util::load_le32(buf.data() + 6);
  if (len > proto::kMaxFramePayload) { if (err) *err = "oversized frame"; return false; }
  const std::size_t total = proto::kHeaderSize + static_cast<std::size_t>(len) + proto::kCrcSize;
  buf.resize(total);
  if (!recvBytes(buf.data() + proto::kHeaderSize, total - proto::kHeaderSize)) { peerClosed = true; return false; }
  std::size_t consumed = 0;
  auto r = proto::decodeFrame(buf, out, consumed, err);
  return r == proto::DecodeResult::OK;
}

TcpServer::TcpServer() {}
TcpServer::~TcpServer() { close(); }

bool TcpServer::listen(std::uint16_t port, std::string* err) {
  if (!initWinsock()) { if (err) *err = "winsock init failed"; return false; }
  listener_ = static_cast<int>(socket(AF_INET, fdType(), 0));
  if (listener_ < 0) { if (err) *err = "socket failed"; return false; }
  int reuse = 1;
  setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (::bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    if (err) *err = "bind failed";
    close();
    return false;
  }
  if (::listen(listener_, 8) != 0) {
    if (err) *err = "listen failed";
    close();
    return false;
  }
  return true;
}

int TcpServer::acceptClient() {
  if (listener_ < 0) return -1;
  return static_cast<int>(::accept(listener_, nullptr, nullptr));
}

void TcpServer::close() {
  if (listener_ >= 0) { closeFd(listener_); listener_ = -1; }
}


// Raw-fd helpers.
bool sendFrame(int fd, const proto::Frame& frame, std::string* err) {
  const std::vector<std::uint8_t> bytes = proto::encodeFrame(frame.type, frame.payload);
  std::size_t sent = 0;
  while (sent < bytes.size()) {
    const int n = static_cast<int>(::send(fd, reinterpret_cast<const char*>(bytes.data() + sent), static_cast<int>(bytes.size() - sent), 0));
    if (n <= 0) { if (err) *err = "send failed"; return false; }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}
bool recvFrame(int fd, proto::Frame& frame, bool& peerClosed, std::string* err) {
  peerClosed = false;
  std::vector<std::uint8_t> buf(proto::kHeaderSize);
  std::size_t got = 0;
  while (got < buf.size()) {
    const int r = static_cast<int>(::recv(fd, reinterpret_cast<char*>(buf.data() + got), static_cast<int>(buf.size() - got), 0));
    if (r == 0) { peerClosed = true; return false; }
    if (r < 0) { if (err) *err = "recv failed"; peerClosed = true; return false; }
    got += static_cast<std::size_t>(r);
  }
  const std::uint32_t len = util::load_le32(buf.data() + 6);
  if (len > proto::kMaxFramePayload) { if (err) *err = "oversized frame"; return false; }
  buf.resize(proto::kHeaderSize + len + proto::kCrcSize);
  std::size_t rest = proto::kHeaderSize + len + proto::kCrcSize;
  got = proto::kHeaderSize;
  while (got < rest) {
    const int r = static_cast<int>(::recv(fd, reinterpret_cast<char*>(buf.data() + got), static_cast<int>(rest - got), 0));
    if (r == 0) { peerClosed = true; return false; }
    if (r < 0) { if (err) *err = "recv failed"; peerClosed = true; return false; }
    got += static_cast<std::size_t>(r);
  }
  std::size_t consumed = 0;
  auto res = proto::decodeFrame(buf, frame, consumed, err);
  return res == proto::DecodeResult::OK;
}
bool closeSocket(int fd) { return closeFd(fd); }
}  // namespace ah::net

