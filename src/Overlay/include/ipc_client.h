#pragma once

#include <string>
#include <windows.h>
#include "common/ipc_protocol.h"

namespace clipx {

class IPCClient {
public:
    IPCClient();
    ~IPCClient();

    bool Connect(const std::string& pipeName, int timeoutMs = 5000);
    void Disconnect();

    bool IsConnected() const { return m_connected; }

    IPCResponse SendRequest(const IPCRequest& request);

private:
    bool ReadMessage(std::vector<uint8_t>& buffer);
    bool WriteMessage(const std::vector<uint8_t>& data);

    HANDLE m_pipe = nullptr;
    bool m_connected = false;
};

} // namespace clipx
