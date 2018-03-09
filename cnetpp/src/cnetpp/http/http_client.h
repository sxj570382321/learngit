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
#ifndef CNETPP_HTTP_HTTP_CLIENT_H_
#define CNETPP_HTTP_HTTP_CLIENT_H_

#include <cnetpp/http/http_base.h>
#include <cnetpp/http/http_connection.h>
#include <cnetpp/tcp/tcp_client.h>
#include <cnetpp/base/end_point.h>
#include <cnetpp/base/uri.h>

#include <functional>

namespace cnetpp {
namespace http {

class HttpClient final : public HttpBase {
 public:
  HttpClient() = default;
  ~HttpClient() = default;

  bool Launch(const HttpClientOptions& http_options = HttpClientOptions()) {
    tcp::TcpClientOptions options;
    options.set_worker_count(http_options.worker_count());
    return tcp_client_.Launch("hcli", options);
  }

  tcp::ConnectionId Connect(const base::EndPoint* remote,
                            const HttpClientOptions& options);
  tcp::ConnectionId Connect(base::StringPiece url,
                            const HttpClientOptions& options);

  bool AsyncClose(tcp::ConnectionId connection_id);

 private:
  tcp::TcpClient tcp_client_;

  tcp::ConnectionId DoConnect(const base::EndPoint* remote,
                              std::shared_ptr<HttpClientOptions> http_options);

  bool DoShutdown() override {
    return tcp_client_.Shutdown();
  }

  bool HandleConnected(
      std::shared_ptr<HttpConnection> http_connection) override;
};

}  // namespace http
}  // namespace cnetpp

#endif  // CNETPP_HTTP_HTTP_CLIENT_H_

