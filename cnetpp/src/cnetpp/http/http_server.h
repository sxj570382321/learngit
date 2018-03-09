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
#ifndef CNETPP_HTTP_HTTP_SERVER_H_
#define CNETPP_HTTP_HTTP_SERVER_H_

#include <cnetpp/http/http_base.h>
#include <cnetpp/http/http_options.h>
#include <cnetpp/base/end_point.h>
#include <cnetpp/tcp/tcp_server.h>

namespace cnetpp {
namespace http {

class HttpServer final : public HttpBase {
 public:
  HttpServer() = default;
  virtual ~HttpServer() = default;

  // you must first call this method before you do any requests
  bool Launch(const base::EndPoint& local_address,
              const HttpServerOptions& options = HttpServerOptions());

 private:
  tcp::TcpServer tcp_server_;
  HttpServerOptions options_;

  bool DoShutdown() override {
    return tcp_server_.Shutdown();
  }

  bool HandleConnected(
      std::shared_ptr<HttpConnection> http_connection) override;
};

}  // namespace http
}  // namespace cnetpp

#endif  // CNETPP_HTTP_HTTP_SERVER_H_

