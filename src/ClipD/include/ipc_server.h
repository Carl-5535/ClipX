#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <windows.h>
#include "common/ipc_protocol.h"

namespace clipx {

class IPCServer {
public:
    using RequestHandler = std::function<IPCResponse(const IPCRequest&)>;

    IPCServer();
    ~IPCServer();

    bool Start(const std::string& pipeName);
    void Stop();

    void SetRequestHandler(RequestHandler handler);

    bool IsRunning() const { return m_running; }

private:
    void ListenLoop();
    void HandleClient(HANDLE pipe);
    bool ReadMessage(HANDLE pipe, std::vector<uint8_t>& buffer);
    bool WriteMessage(HANDLE pipe, const std::vector<uint8_t>& data);

    std::string m_pipeName;
    std::thread m_listenerThread;
    std::atomic<bool> m_running{false};
    RequestHandler m_handler;
    HANDLE m_stopEvent = nullptr;
};

} // namespace clipx
