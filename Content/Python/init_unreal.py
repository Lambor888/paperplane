# Runs automatically when the Unreal Editor starts (any init_unreal.py under Content/Python).
# Starts the ModelContextProtocol (MCP) server shortly after the editor has finished loading,
# so AI tools can connect without typing ModelContextProtocol.StartServer by hand.
import unreal

_WAIT_FRAMES = 60
_frames = 0
_handle = None


def _start_mcp_server(delta_seconds):
    global _frames
    _frames += 1
    if _frames < _WAIT_FRAMES:
        return
    unreal.unregister_slate_post_tick_callback(_handle)
    unreal.SystemLibrary.execute_console_command(None, "ModelContextProtocol.StartServer")
    unreal.log("init_unreal: requested ModelContextProtocol.StartServer")


_handle = unreal.register_slate_post_tick_callback(_start_mcp_server)
