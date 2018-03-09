// Copyright (c) 2015, myjfm(mwxjmmyjfm@gmail.com).  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//   * Neither the name of myjfm nor the names of other contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
#include <cnetpp/tcp/tcp_client.h>
#include <cnetpp/tcp/connection_factory.h>
#include <cnetpp/base/end_point.h>
#include <cnetpp/base/socket.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace cnetpp {
namespace tcp {

bool TcpClient::Launch(const std::string& name,
    const TcpClientOptions& options) {
  event_center_ = EventCenter::New(name, options.worker_count());
  assert(event_center_.get());
  return event_center_->Launch();
}

bool TcpClient::Shutdown() {
  event_center_->Shutdown();
  contexts_.clear();
  return true;
}

ConnectionId TcpClient::Connect(const base::EndPoint* remote,
                                const TcpClientOptions& options,
                                std::shared_ptr<void> cookie) {
  assert(remote);

  base::TcpSocket socket;
  if (!socket.Create() ||
      !socket.SetCloexec() ||
      !socket.SetBlocking(false) ||
      !socket.SetTcpNoDelay() ||
      !socket.SetKeepAlive() ||
      !socket.SetSendBufferSize(options.tcp_send_buffer_size()) ||
      !socket.SetReceiveBufferSize(options.tcp_receive_buffer_size()) ||
      !socket.Connect(*remote)) {
    return kInvalidConnectionId;
  }

  InternalConnectionContext cc;
  cc.status = Status::kConnecting;
  cc.options = options;
  cc.tcp_connection.reset();

  ConnectionFactory cf;
  auto connection = cf.CreateConnection(event_center_, socket.fd(), false);
  auto tcp_connection = std::static_pointer_cast<TcpConnection>(connection);
  tcp_connection->SetSendBufferSize(options.send_buffer_size());
  tcp_connection->SetRecvBufferSize(options.receive_buffer_size());
  tcp_connection->set_cookie(cookie);
  tcp_connection->set_remote_end_point(*remote);
  cc.tcp_connection = tcp_connection;
  std::unique_lock<std::mutex> guard(contexts_mutex_);
  contexts_[connection->id()] = cc;
  guard.unlock();

  tcp_connection->set_connected_callback(
      [this] (std::shared_ptr<TcpConnection> c) -> bool {
        return this->OnConnected(c);
      }
  );
  tcp_connection->set_closed_callback(
      [this] (std::shared_ptr<TcpConnection> c) -> bool {
        return this->OnClosed(c);
      }
  );
  tcp_connection->set_sent_callback(
      [this] (bool status, std::shared_ptr<TcpConnection> c) -> bool {
        return this->OnSent(status, c);
      }
  );
  tcp_connection->set_received_callback(
      [this] (std::shared_ptr<TcpConnection> c) -> bool {
        return this->OnReceived(c);
      }
  );

  socket.Detach();

  event_center_->AddCommand(
      Command(static_cast<int>(Command::Type::kAddConn), connection), true);
  return connection->id();
}

bool TcpClient::AsyncClosed(ConnectionId connection_id) {
  std::unique_lock<std::mutex> guard(contexts_mutex_);
  auto itr = contexts_.find(connection_id);
  if (itr == contexts_.end()) {
    return false;
  }
  auto connection = itr->second.tcp_connection;
  guard.unlock();
  connection->MarkAsClosed();
  return true;
}

bool TcpClient::OnConnected(
    std::shared_ptr<TcpConnection> tcp_connection) {
  assert(tcp_connection.get());
  std::unique_lock<std::mutex> guard(contexts_mutex_);
  auto itr = contexts_.find(tcp_connection->id());
  assert(itr != contexts_.end());
  itr->second.status = Status::kConnected;
  if (itr->second.options.connected_callback()) {
    auto& cb = itr->second.options.mutable_connected_callback();
    guard.unlock();
    return cb(tcp_connection);
  }
  return true;
}

bool TcpClient::OnClosed(
    std::shared_ptr<TcpConnection> tcp_connection) {
  assert(tcp_connection.get());
  std::unique_lock<std::mutex> guard(contexts_mutex_);
  auto itr = contexts_.find(tcp_connection->id());
  assert(itr != contexts_.end());
  itr->second.status = Status::kClosed;
  bool res = true;
  if (itr->second.options.closed_callback()) {
    auto& cb = itr->second.options.mutable_closed_callback();
    guard.unlock();
    res = cb(tcp_connection);
    guard.lock();
    contexts_.erase(itr);
    return res;
  }
  contexts_.erase(itr);
  return res;
}

bool TcpClient::OnSent(bool success,
                       std::shared_ptr<TcpConnection> tcp_connection) {
  assert(tcp_connection.get());
  std::unique_lock<std::mutex> guard(contexts_mutex_);
  auto itr = contexts_.find(tcp_connection->id());
  assert(itr != contexts_.end());
  assert(itr->second.status == Status::kConnected);
  if (itr->second.options.sent_callback()) {
    auto& cb = itr->second.options.mutable_sent_callback();
    guard.unlock();
    return cb(success, tcp_connection);
  }
  return true;
}

bool TcpClient::OnReceived(std::shared_ptr<TcpConnection> tcp_connection) {
  assert(tcp_connection.get());
  std::unique_lock<std::mutex> guard(contexts_mutex_);
  auto itr = contexts_.find(tcp_connection->id());
  assert(itr != contexts_.end());
  assert(itr->second.status == Status::kConnected);
  if (itr->second.options.received_callback()) {
    auto& cb = itr->second.options.mutable_received_callback();
    guard.unlock();
    return cb(tcp_connection);
  }
  return true;
}

}  // namespace tcp
}  // namespace cnetpp

