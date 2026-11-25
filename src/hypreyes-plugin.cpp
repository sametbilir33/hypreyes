#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>

static int g_socket = -1;
static sockaddr_un g_addr{};
static CHyprSignalListener g_listener;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    if (std::string(__hyprland_api_get_hash()) != std::string(__hyprland_api_get_client_hash())) {
        throw std::runtime_error("hypreyes: Hyprland ABI mismatch");
    }

    g_socket = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (g_socket < 0) throw std::runtime_error("hypreyes: socket creation failed");

    g_addr.sun_family = AF_UNIX;
    snprintf(g_addr.sun_path, sizeof(g_addr.sun_path), "/tmp/hypreyes-%u.sock", (unsigned)getuid());

    g_listener = Event::bus()->m_events.input.mouse.move.listen([](Vector2D pos, Event::SCallbackInfo&) {
        if (g_socket < 0) return;
        char buffer[96];
        int len = snprintf(buffer, sizeof(buffer), "%.3f,%.3f", pos.x, pos.y);
        if (len > 0) sendto(g_socket, buffer, (size_t)len, MSG_DONTWAIT | MSG_NOSIGNAL, (sockaddr*)&g_addr, sizeof(g_addr));
    });

    return {"hypreyes", "Real-time cursor tracking for hypreyes", "hypreyes", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_listener = {};
    if (g_socket >= 0) close(g_socket);
    g_socket = -1;
}
