#include "mcp_server.h"
#include "tools/can_monitor.h"
#include "tools/can_send.h"
#include "tools/can_stop.h"
#include "tools/can_decode.h"
#include "tools/can_replay.h"
#include "tools/can_script.h"
#include "tools/canopen_sdo_read.h"
#include "tools/canopen_sdo_write.h"
#include "tools/canopen_scan.h"
#include "tools/can_diff.h"
#include "tools/get_stats.h"

namespace socketspy::api::mcp {

void register_all_tools(McpServer& server) {
    server.register_tool(make_can_monitor_tool());
    server.register_tool(make_can_send_tool());
    server.register_tool(make_can_stop_tool());
    server.register_tool(make_can_decode_tool());
    server.register_tool(make_can_replay_tool());
    server.register_tool(make_can_script_tool());
    server.register_tool(make_canopen_sdo_read_tool());
    server.register_tool(make_canopen_sdo_write_tool());
    server.register_tool(make_canopen_scan_tool());
    server.register_tool(make_can_diff_tool());
    server.register_tool(make_get_stats_tool());
}

} // namespace socketspy::api::mcp
