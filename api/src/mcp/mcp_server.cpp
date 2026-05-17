#include "mcp_server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace socketspy::api::mcp {

static constexpr const char* kProtocolVersion = "2024-11-05";
static constexpr const char* kServerName      = "socketspy";
static constexpr const char* kServerVersion   = "0.1.0";

McpServer::McpServer()  = default;
McpServer::~McpServer() = default;

void McpServer::register_tool(Tool tool) {
    tools_[tool.name] = std::move(tool);
}

// ---------------------------------------------------------------------------
// JSON-RPC dispatch
// ---------------------------------------------------------------------------

json McpServer::make_error(int code, std::string_view message) const {
    return {{"code", code}, {"message", std::string(message)}};
}

json McpServer::handle_initialize(const json& /*params*/) {
    return {
        {"protocolVersion", kProtocolVersion},
        {"serverInfo",
         {{"name", kServerName}, {"version", kServerVersion}}},
        {"capabilities", {{"tools", {{"listChanged", false}}}}}
    };
}

json McpServer::handle_tools_list(const json& /*params*/) {
    json arr = json::array();
    for (const auto& [name, tool] : tools_) {
        arr.push_back({
            {"name",        tool.name},
            {"description", tool.description},
            {"inputSchema", tool.input_schema}
        });
    }
    return {{"tools", arr}};
}

json McpServer::handle_tools_call(const json& params) {
    if (!params.contains("name") || !params["name"].is_string()) {
        return {{"isError", true},
                {"content", json::array({{{"type","text"},
                  {"text","missing or invalid 'name' field"}}})}};
    }
    const std::string name = params["name"].get<std::string>();
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return {{"isError", true},
                {"content", json::array({{{"type","text"},
                  {"text","unknown tool: " + name}}})}};
    }
    const json& args = params.contains("arguments") ? params["arguments"] : json::object();
    try {
        json result = it->second.handler(args);
        return {
            {"isError", false},
            {"content", json::array({{{"type","text"},
              {"text", result.dump()}}})}
        };
    } catch (const std::exception& e) {
        return {{"isError", true},
                {"content", json::array({{{"type","text"},
                  {"text", std::string("tool error: ") + e.what()}}})}};
    }
}

json McpServer::handle_request(const json& req) {
    json response;
    response["jsonrpc"] = "2.0";
    response["id"]      = req.contains("id") ? req["id"] : json(nullptr);

    if (!req.contains("method") || !req["method"].is_string()) {
        response["error"] = make_error(-32600, "invalid request: missing method");
        return response;
    }

    const std::string method = req["method"].get<std::string>();
    const json params = req.contains("params") ? req["params"] : json::object();

    try {
        if (method == "initialize") {
            response["result"] = handle_initialize(params);
        } else if (method == "tools/list") {
            response["result"] = handle_tools_list(params);
        } else if (method == "tools/call") {
            response["result"] = handle_tools_call(params);
        } else {
            response["error"] = make_error(-32601, "method not found: " + method);
        }
    } catch (const std::exception& e) {
        response["error"] = make_error(-32603, std::string("internal error: ") + e.what());
    }
    return response;
}

// ---------------------------------------------------------------------------
// stdio transport
// ---------------------------------------------------------------------------

void McpServer::serve_stdio() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            const json req  = json::parse(line);
            const json resp = handle_request(req);
            std::cout << resp.dump() << "\n";
            std::cout.flush();
        } catch (const json::parse_error&) {
            json err;
            err["jsonrpc"] = "2.0";
            err["id"]      = nullptr;
            err["error"]   = make_error(-32700, "parse error");
            std::cout << err.dump() << "\n";
            std::cout.flush();
        }
    }
}

// ---------------------------------------------------------------------------
// TCP transport (loopback only)
// ---------------------------------------------------------------------------

static void handle_tcp_connection(int client_fd, McpServer* server) {
    auto read_line = [&](std::string& out) -> bool {
        out.clear();
        char c;
        while (true) {
            ssize_t n = ::read(client_fd, &c, 1);
            if (n <= 0) return !out.empty();
            if (c == '\n') return true;
            out += c;
        }
    };

    std::string line;
    while (read_line(line)) {
        if (line.empty()) continue;
        try {
            const json req  = json::parse(line);
            const json resp = server->handle_request(req); // delegated
            const std::string out = resp.dump() + "\n";
            if (::write(client_fd, out.c_str(), out.size()) < 0) break;
        } catch (...) {
            const std::string err =
                R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"parse error"}})"
                "\n";
            if (::write(client_fd, err.c_str(), err.size()) < 0) break;
        }
    }
    ::close(client_fd);
}

void McpServer::serve_tcp(uint16_t port) {
    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    // Security invariant: ONLY bind to loopback.
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        ::close(server_fd);
        throw std::runtime_error("inet_pton failed");
    }

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(server_fd);
        throw std::runtime_error("bind() failed on 127.0.0.1:" + std::to_string(port));
    }
    if (::listen(server_fd, 8) < 0) {
        ::close(server_fd);
        throw std::runtime_error("listen() failed");
    }

    while (true) {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);
        int client_fd = ::accept(server_fd,
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &client_len);
        if (client_fd < 0) continue;
        std::thread([client_fd, this]() {
            handle_tcp_connection(client_fd, this);
        }).detach();
    }
    ::close(server_fd);
}

} // namespace socketspy::api::mcp
