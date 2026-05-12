#include "mcp_server.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

// socketspy-mcp — standalone MCP server binary.
// Usage:
//   socketspy-mcp --stdio            (Claude Desktop, default)
//   socketspy-mcp --tcp <port>       (TCP on 127.0.0.1 only)

int main(int argc, char* argv[]) {
    using namespace socketspy::api::mcp;

    McpServer server;
    register_all_tools(server);

    bool use_tcp = false;
    uint16_t port = 7891;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tcp") == 0 && i + 1 < argc) {
            use_tcp = true;
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--stdio") == 0) {
            use_tcp = false;
        }
    }

    if (use_tcp) {
        std::cerr << "socketspy-mcp: TCP on 127.0.0.1:" << port << "\n";
        server.serve_tcp(port);
    } else {
        server.serve_stdio();
    }
    return 0;
}
