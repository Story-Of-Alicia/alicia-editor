#include "util/util.hpp"

#include <libpak/libpak.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace
{

constexpr std::array<std::string_view, 2> ALICIA_PAK_PROMPT_TYPES = {
  "PAK file (*.pak)",
  "*.pak;*.pak.*;*.pak.bak"};

struct Settings
{
  //! A port the websocket is listening on.
  uint16_t websocketPort = 8083;
  //! An address the websocket is listening on.
  asio::ip::address websocketAddress = asio::ip::address_v4::any();
};

} // namespace

int main()
{
  spdlog::set_level(spdlog::level::debug);

  Settings settings;

  asio::io_context ioCtx;

  const auto endpoint = asio::ip::tcp::endpoint(
    settings.websocketAddress,
    settings.websocketPort);
  spdlog::info(
    "Web socket is listening on {}:{}",
    endpoint.address().to_string(),
    endpoint.port());

  asio::ip::tcp::acceptor acceptor(ioCtx, endpoint);

  try
  {
    // Listen for incoming connections.
    acceptor.listen(asio::socket_base::max_listen_connections);
  }
  catch (const std::exception& x)
  {
    spdlog::error("Failed to host the web socket: {}", x.what());
    return 1;
  }

  using ControlEndpointHandler = std::function<nlohmann::json(const nlohmann::json& control)>;

  std::unordered_map<std::string, libpak::resource> resources;
  std::unordered_map<std::string, ControlEndpointHandler> endpoints;

  endpoints["pak"] = [&resources](const nlohmann::json& payload) -> nlohmann::json
  {
    const auto operation = payload.value("operation", "");
    nlohmann::json responseJson{};

    if (operation == "prompt")
    {
      const auto pakPath = util::win32_prompt_for_file(
        "Select the PAK file.",
        ALICIA_PAK_PROMPT_TYPES);

      responseJson["resource_path"] = pakPath.string();
      spdlog::debug("PAK resource file path: {}", pakPath.string());
    }
    else if (operation == "read")
    {
      const std::filesystem::path resourcePath = payload.value("resource_path", "");
      if (resourcePath.empty())
      {
        return nlohmann::json{
          {"error", "path is empty"}};
      }

      const auto [iterator, inserted] = resources.try_emplace(
        resourcePath.string(),
        resourcePath.string());

      auto& resource = iterator->second;
      resource.read(true);

      nlohmann::json& assetsJson = responseJson["assets"];

      for (const auto& [path, asset] : resource.assets)
      {
        const auto narrow_path = util::win32_narrow(path);

        nlohmann::json& assetJson = assetsJson.emplace_back();

        assetJson["timestamp"] = asset.header.timestamp;
        assetJson["path"] = narrow_path;
      }

      spdlog::debug("Read PAK resource '{}'", resourcePath.string());;
    }
    else if (operation == "write")
    {
      const std::filesystem::path resourcePath = payload.value("resource_path", "");
      const std::filesystem::path targetResourcePath = payload.value("target_resource_path", resourcePath);
      if (resourcePath.empty())
      {
        return nlohmann::json{
          {"error", "path is empty"}};
      }

      const auto resourceIterator = resources.find(resourcePath.string());
      if (resourceIterator == resources.cend())
      {
        return nlohmann::json{
          {"error", "resource does not exist"}};
      }

      auto& resource = resourceIterator->second;

      resource.resource_path = targetResourcePath.string();
      resource.write();

      spdlog::debug("Wrote PAK resource '{}'", resourcePath.string());
    }
    else if (operation == "invalidate")
    {
      const std::filesystem::path resourcePath = payload.value("resource_path", "");
      if (resourcePath.empty())
      {
        return nlohmann::json{
          {"error", "path is empty"}};
      }

      const auto resourceIterator = resources.find(resourcePath.string());
      if (resourceIterator == resources.cend())
      {
        return nlohmann::json{
          {"error", "resource does not exist"}};
      }

      resources.erase(resourceIterator);
      spdlog::debug("Invalidated PAK resource '{}'", resourcePath.string());
    }
    else
    {
      responseJson["error"] = "Unknown operation";
    }

    return responseJson;
  };

  endpoints["asset"] = [&resources](const nlohmann::json& payload) -> nlohmann::json
  {
    const auto operation = payload.value("operation", "");
    const std::filesystem::path resourcePath = payload.value("resource_path", "");
    const std::string assetPath = payload.value("asset_path", "");

    if (resourcePath.empty() || assetPath.empty())
    {
      return nlohmann::json{
        {"error", "either pak path or asset path are empty"}};
    }

    const auto resourceIterator = resources.find(resourcePath.string());
    if (resourceIterator == resources.cend())
    {
      return nlohmann::json{
        {"error", "resource does not exist"}};
    }

    auto& resource = resourceIterator->second;

    // todo: use unicode library
    std::u16string unicodeAssetPath;
    for (const char& c : assetPath)
    {
      unicodeAssetPath += static_cast<char16_t>(c);
    }
    const auto& [iterator, created] = resource.assets.try_emplace(unicodeAssetPath);

    auto& asset = iterator->second;
    if (created)
    {
      std::memcpy(
        asset.header.path,
        unicodeAssetPath.data(),
        unicodeAssetPath.length());
    }

    nlohmann::json responseJson{};

    if (operation == "read")
    {
      if (asset.data.buffer.empty())
        resource.read_asset_data(asset);

      responseJson["data"] = asset.data.buffer;

      spdlog::debug(
        "Read {} bytes of data from asset '{}' from a resource file '{}'",
        asset.data.buffer.size(),
        assetPath,
        resourcePath.string());
    }
    else if (operation == "write")
    {
      asset.data.buffer = payload.value("data", std::vector<std::byte>{});

      spdlog::debug(
        "Wrote {} bytes of data to asset '{}' to a resource file '{}'",
        asset.data.buffer.size(),
        assetPath,
        resourcePath.string());
    }

    return responseJson;
  };

  // Heartbeat loop.
  while (true)
  {
    auto client = acceptor.accept();

    spdlog::debug(
      "A client has connected from {}:{}",
      client.remote_endpoint().address().to_string(),
      client.remote_endpoint().port());

    // Accept the websocket.
    beast::websocket::stream<beast::tcp_stream> webSocket(
      std::move(client));
    webSocket.accept();

    beast::flat_buffer buffer;
    // Client read loop.
    while (true)
    {
      try
      {
        // Read websocket.
        const size_t messageBytes = webSocket.read(buffer);

        const auto timerBegin = std::chrono::steady_clock::now();

        if (webSocket.got_text())
        {
          const std::string request_string(
            static_cast<const char*>(buffer.cdata().data()),
            messageBytes);

          const auto request = nlohmann::json::parse(request_string);

          const auto endpointName = request.value("endpoint", "");
          const auto payload = request.value("payload", nlohmann::json{});

          const auto endpointHandlerIter = endpoints.find(endpointName);
          if (endpointHandlerIter == endpoints.cend())
            continue;

          const nlohmann::json& response = endpointHandlerIter->second(payload);

          const auto responseData = response.dump();
          webSocket.text(true);
          webSocket.write(asio::buffer(responseData));
        }
        else if (webSocket.got_binary())
        {
          // todo: binary
        }

        spdlog::debug(
          "Served request in {} milliseconds",
          std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - timerBegin).count());

        buffer.consume(messageBytes);
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
