#include "util/util.hpp"

#include <locale>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <libpak/libpak.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace
{

  constexpr std::array<std::string_view, 2> ALICIA_PAK_PROMPT_TYPES = {
      "PAK file (*.pak)", "*.pak;*.pak.*;*.pak.bak"};

} // namespace

int main()
{
  asio::io_context ioCtx;

  const auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 8083);
  spdlog::info("Listening on 8083");

  asio::ip::tcp::acceptor acceptor(ioCtx, endpoint);

  // Listen for incoming connections.
  acceptor.listen(asio::socket_base::max_listen_connections);

  // Heartbeat loop.
  while (true)
  {
    auto client = acceptor.accept();

    spdlog::debug("New client connected, from port {}", client.remote_endpoint().port());

    // Accept the websocket.
    beast::websocket::stream<beast::tcp_stream> webSocket(std::move(client));
    webSocket.accept();

    beast::flat_buffer buffer;
    // Client read loop.
    while (true)
    {
      try
      {
        webSocket.read(buffer);
        if (!webSocket.got_text())
        {
          continue;
        }

        const std::string request_string(
            static_cast<const char*>(buffer.cdata().data()), buffer.size());
        const auto request = nlohmann::json::parse(request_string);

        if (request["action"] == "select_resource")
        {
          libpak::resource resource(
              util::win32_prompt_for_file("Select the PAK file.", ALICIA_PAK_PROMPT_TYPES));
          resource.read();

          nlohmann::json response;
          response["action"] = "asset_listing";
          response["path"] = resource.resource_path.c_str();
          response["data"] = nlohmann::json::array();

          for (const auto& [path, asset] : resource.assets)
          {
            const auto narrow_path = util::win32_narrow(path);
            nlohmann::json asset_data;
            asset_data["path"] = narrow_path;

            response["data"].emplace_back(asset_data);
          }

          const std::string response_string = response.dump();

          webSocket.text(true);
          webSocket.write(asio::buffer(response_string));
        }
      }
      catch (const std::exception& x)
      {
        spdlog::error("Error occurred in heartbeat loop: {}", x.what());
        break;
      }
    }
  }

  return 0;
}