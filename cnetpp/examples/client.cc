#include <cnetpp/http/http_callbacks.h>
#include <cnetpp/http/http_client.h>
#include <cnetpp/http/http_request.h>
#include <cnetpp/http/http_response.h>
#include <cnetpp/base/end_point.h>
#include <cnetpp/base/ip_address.h>
#include <cnetpp/base/log.h>

#include <unistd.h>

#include <iostream>

int main(int argc, const char **argv) {
  if(argc != 2) {
    std::cout << "Usage: " << argv[0] << " <url>" << std::endl;
    return 1;
  }

  std::unordered_map<cnetpp::tcp::ConnectionId, int> counts;
  using HttpConnectionPtr = std::shared_ptr<cnetpp::http::HttpConnection>;

  cnetpp::http::HttpClientOptions http_options;
  http_options.set_send_buffer_size(1024 * 1024);
  http_options.set_receive_buffer_size(1024 * 1024);

  auto send_func = [] (HttpConnectionPtr c) -> bool {
    std::shared_ptr<cnetpp::http::HttpRequest> http_request(
        new cnetpp::http::HttpRequest);
    http_request->set_method(cnetpp::http::HttpRequest::MethodType::kGet);
    http_request->set_uri("/");
    // http_request->SetHttpHeader("Content-Length", "0");
    if (!c->remote_hostname().empty()) {
      http_request->SetHttpHeader("Host", c->remote_hostname());
    } else {
      auto tc = c->tcp_connection();
      http_request->SetHttpHeader(
          "Host",
          tc->remote_end_point().ToStringWithoutPort());
    }
    http_request->SetHttpHeader("User-Agent", "cnetpp/1.0");
    return c->SendPacket(http_request->ToString());
  };

  http_options.set_connected_callback([&send_func] (HttpConnectionPtr c) -> bool {
      Info("Connected to the server");
      return send_func(c);
  });

  http_options.set_closed_callback([] (HttpConnectionPtr c) -> bool {
      Info("Connection closed");
      (void) c;
      return true;
  });

  http_options.set_received_callback([&send_func, &counts] (
        HttpConnectionPtr c) -> bool {
      auto http_response =
        std::static_pointer_cast<cnetpp::http::HttpResponse>(c->http_packet());
      std::string headers;
      http_response->HttpHeadersToString(&headers);
      Info("headers: %s", headers.c_str());
      if (http_response->http_body().length() > 0) {
        Info("body: %s", http_response->http_body().c_str());
      }
      if (counts[c->id()]++ == 10) {
        c->MarkAsClosed();
        Info("MarkAsClosed");
        return true;
      } else {
        return send_func(c);
      }
  });

  http_options.set_sent_callback([] (bool success, HttpConnectionPtr c) -> bool {
    (void) c;
    Info("Send packet successfully");
    return success;
  });

  cnetpp::http::HttpClient http_client;
  cnetpp::http::HttpClientOptions options;
  options.set_worker_count(1);
  if (!http_client.Launch(options)) {
    Fatal("Failed to launch http client, exiting...");
    return 1;
  }

  for (auto i = 0; i < 8; ++i) {
    auto connection_id = http_client.Connect(argv[1], http_options);
    if (connection_id == cnetpp::tcp::kInvalidConnectionId) {
      Info("Failed to connect to server");
      return 1;
    }
  }

  sleep(2);

  http_client.Shutdown();

  return 0;
}

