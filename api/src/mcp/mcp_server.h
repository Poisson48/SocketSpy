#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace socketspy::api::mcp {

using json = nlohmann::json;

// MCP tool handler signature: receives params object, returns result object
using ToolHandler = std::function<json(const json& params)>;

// Registered tool with schema
struct Tool {
    std::string name;
    std::string description;
    json        input_schema;   // JSON Schema for params
    ToolHandler handler;
};

class McpServer {
public:
    McpServer();
    ~McpServer();

    // Register a tool. Called at startup before serve().
    void register_tool(Tool tool);

    // Run stdio transport (blocking). Reads JSON-RPC from stdin, writes to stdout.
    void serve_stdio();

    // Run TCP transport on 127.0.0.1:port (blocking).
    // MUST bind ONLY to loopback. This is enforced in code, not config.
    void serve_tcp(uint16_t port);

    // Dispatch a single JSON-RPC request object and return the response.
    // Public so TCP connection handlers (free functions/threads) can call it.
    json handle_request(const json& request);

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

private:
    json handle_initialize(const json& params);
    json handle_tools_list(const json& params);
    json handle_tools_call(const json& params);
    json make_error(int code, std::string_view message) const;

    std::unordered_map<std::string, Tool> tools_;
};

// Factory: create and register all 11 tools on the server.
void register_all_tools(McpServer& server);

} // namespace socketspy::api::mcp
