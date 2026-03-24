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

  std::unique_ptr<libpak::resource> resource;

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
        // Read websocket.
        webSocket.read(buffer);
        if (!webSocket.got_text())
        {
          continue;
        }


        const std::string request_string(
          static_cast<const char*>(buffer.cdata().data()), buffer.size());
        buffer.consume(buffer.size());

        const auto request = nlohmann::json::parse(request_string);

        spdlog::info("Received request: {}", request_string);

        if (request["action"] == "read")
        {
          resource = std::make_unique<libpak::resource>(
            util::win32_prompt_for_file("Select the PAK file.", ALICIA_PAK_PROMPT_TYPES));
          resource->read();

          nlohmann::json response;
          response["action"] = "asset_listing";
          response["path"] = resource->resource_path.c_str();
          response["data"] = nlohmann::json::array();

          for (const auto& [path, asset] : resource->assets)
          {
            const auto narrow_path = util::win32_narrow(path);
            nlohmann::json asset_data;
            asset_data["path"] = narrow_path;
            asset_data["size"] = asset.header.data_decompressed_length;
            asset_data["compressed_size"] = asset.header.embedded_data_length;
            asset_data["is_embedded"] = static_cast<bool>(asset.header.is_asset_embedded);
            asset_data["is_compressed"] = static_cast<bool>(asset.header.is_data_compressed);
            response["data"].emplace_back(asset_data);
          }

          const std::string response_string = response.dump();

          webSocket.text(true);
          webSocket.write(asio::buffer(response_string));
        }
        if (request["action"] == "fetch")
        {
          if (!resource)
          {
            nlohmann::json err;
            err["action"] = "error";
            err["message"] = "No PAK file is open. Send 'read' first.";
            webSocket.text(true);
            webSocket.write(asio::buffer(err.dump()));
            continue;
          }

          const std::string asset_path_utf8 = request["path"];

          // Convert UTF-8 path to u16string for asset map lookup.
          const auto wide_path = util::win32_widen(asset_path_utf8);
          std::u16string key(
            reinterpret_cast<const char16_t*>(wide_path.data()), wide_path.size());

          auto it = resource->assets.find(key);
          if (it == resource->assets.end())
          {
            nlohmann::json err;
            err["action"] = "error";
            err["message"] = "Asset not found: " + asset_path_utf8;
            webSocket.text(true);
            webSocket.write(asio::buffer(err.dump()));
            continue;
          }

          auto& asset = it->second;

          if (!asset.header.is_asset_embedded)
          {
            nlohmann::json err;
            err["action"] = "error";
            err["message"] = "Asset is not embedded: " + asset_path_utf8;
            webSocket.text(true);
            webSocket.write(asio::buffer(err.dump()));
            continue;
          }

          // Read asset data on demand if not already loaded.
          if (!asset.data.buffer)
          {
            resource->read_asset_data(asset);
          }

          if (!asset.data.buffer)
          {
            nlohmann::json err;
            err["action"] = "error";
            err["message"] = "Failed to read asset data: " + asset_path_utf8;
            webSocket.text(true);
            webSocket.write(asio::buffer(err.dump()));
            continue;
          }

          // Determine the actual data size.
          uint32_t data_size = asset.header.is_data_compressed
            ? asset.header.data_decompressed_length
            : asset.header.embedded_data_length;

          // Send JSON header so the client knows what's coming.
          nlohmann::json header;
          header["action"] = "asset_data";
          header["path"] = asset_path_utf8;
          header["size"] = data_size;
          webSocket.text(true);
          webSocket.write(asio::buffer(header.dump()));

          // Send the raw binary data.
          webSocket.binary(true);
          webSocket.write(asio::buffer(asset.data.buffer.get(), data_size));
        }
        if (request["action"] == "write")
        {
          resource->write();
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