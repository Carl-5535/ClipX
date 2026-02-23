#include "ipc_client.h"
#include "common/logger.h"
#include "common/utils.h"
#include <windows.h>

namespace clipx {

IPCClient::IPCClient() = default;

IPCClient::~IPCClient() {
    Disconnect();
}

bool IPCClient::Connect(const std::string& pipeName, int timeoutMs) {
    if (m_connected) {
        return true;
    }

    std::wstring wpipeName = utils::Utf8ToWide(pipeName);

    // Try to connect with timeout
    DWORD startTime = GetTickCount();

    while (true) {
        m_pipe = CreateFileW(
            wpipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (m_pipe != INVALID_HANDLE_VALUE) {
            break;
        }

        DWORD error = GetLastError();

        // If pipe is busy, wait and retry
        if (error == ERROR_PIPE_BUSY) {
            if (GetTickCount() - startTime >= static_cast<DWORD>(timeoutMs)) {
                LOG_ERROR("Timeout waiting for pipe");
                return false;
            }

            if (!WaitNamedPipeW(wpipeName.c_str(), 100)) {
                Sleep(50);
            }
            continue;
        }

        LOG_ERROR("Failed to connect to pipe: " + std::to_string(error));
        return false;
    }

    // Set pipe to message mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(m_pipe, &mode, nullptr, nullptr)) {
        LOG_ERROR("Failed to set pipe mode");
        CloseHandle(m_pipe);
        m_pipe = nullptr;
        return false;
    }

    m_connected = true;
    LOG_INFO("Connected to IPC server: " + pipeName);
    return true;
}

void IPCClient::Disconnect() {
    if (m_pipe && m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipe);
        m_pipe = nullptr;
    }
    m_connected = false;
}

IPCResponse IPCClient::SendRequest(const IPCRequest& request) {
    if (!m_connected) {
        return IPCResponse::Error(request.requestId, "Not connected", IPCError::IPC_CONNECTION_FAILED);
    }

    // Serialize request
    std::string jsonStr = request.ToJson().dump();
    std::vector<uint8_t> requestData(jsonStr.begin(), jsonStr.end());

    // Send request
    if (!WriteMessage(requestData)) {
        LOG_ERROR("Failed to send request");
        return IPCResponse::Error(request.requestId, "Failed to send request", IPCError::IPC_CONNECTION_FAILED);
    }

    // Read response
    std::vector<uint8_t> responseData;
    if (!ReadMessage(responseData)) {
        LOG_ERROR("Failed to read response");
        return IPCResponse::Error(request.requestId, "Failed to read response", IPCError::IPC_CONNECTION_FAILED);
    }

    // Parse response
    try {
        std::string responseJson(responseData.begin(), responseData.end());
        nlohmann::json json = nlohmann::json::parse(responseJson);
        return IPCResponse::FromJson(json);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse response: " + std::string(e.what()));
        return IPCResponse::Error(request.requestId, "Invalid response", IPCError::IPC_INVALID_REQUEST);
    }
}

bool IPCClient::ReadMessage(std::vector<uint8_t>& buffer) {
    // Read header
    IPCHeader header;
    DWORD bytesRead = 0;
    DWORD totalRead = 0;

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Read header
    while (totalRead < sizeof(header)) {
        if (!ReadFile(m_pipe, reinterpret_cast<uint8_t*>(&header) + totalRead,
                      sizeof(header) - totalRead, &bytesRead, &overlapped)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
                if (waitResult != WAIT_OBJECT_0) {
                    CloseHandle(overlapped.hEvent);
                    return false;
                }
                GetOverlappedResult(m_pipe, &overlapped, &bytesRead, TRUE);
            } else if (GetLastError() == ERROR_MORE_DATA) {
                // Continue reading
            } else {
                CloseHandle(overlapped.hEvent);
                return false;
            }
        }
        totalRead += bytesRead;
    }

    CloseHandle(overlapped.hEvent);

    if (header.magic != IPC_MAGIC) {
        LOG_ERROR("Invalid IPC header magic");
        return false;
    }

    // Read payload
    buffer.resize(header.payloadSize);
    if (header.payloadSize > 0) {
        bytesRead = 0;
        totalRead = 0;
        overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        while (totalRead < header.payloadSize) {
            if (!ReadFile(m_pipe, buffer.data() + totalRead,
                          header.payloadSize - totalRead, &bytesRead, &overlapped)) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    DWORD waitResult = WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
                    if (waitResult != WAIT_OBJECT_0) {
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    GetOverlappedResult(m_pipe, &overlapped, &bytesRead, TRUE);
                } else if (GetLastError() == ERROR_MORE_DATA) {
                    // Continue reading
                } else {
                    CloseHandle(overlapped.hEvent);
                    return false;
                }
            }
            totalRead += bytesRead;
        }

        CloseHandle(overlapped.hEvent);
    }

    return true;
}

bool IPCClient::WriteMessage(const std::vector<uint8_t>& data) {
    IPCHeader header;
    header.magic = IPC_MAGIC;
    header.payloadSize = static_cast<uint32_t>(data.size());

    DWORD bytesWritten = 0;
    DWORD totalWritten = 0;

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Write header
    while (totalWritten < sizeof(header)) {
        if (!WriteFile(m_pipe, reinterpret_cast<uint8_t*>(&header) + totalWritten,
                       sizeof(header) - totalWritten, &bytesWritten, &overlapped)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
                if (waitResult != WAIT_OBJECT_0) {
                    CloseHandle(overlapped.hEvent);
                    return false;
                }
                GetOverlappedResult(m_pipe, &overlapped, &bytesWritten, TRUE);
            } else {
                CloseHandle(overlapped.hEvent);
                return false;
            }
        }
        totalWritten += bytesWritten;
    }

    CloseHandle(overlapped.hEvent);

    // Write payload
    if (!data.empty()) {
        bytesWritten = 0;
        totalWritten = 0;
        overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        while (totalWritten < data.size()) {
            if (!WriteFile(m_pipe, data.data() + totalWritten,
                           static_cast<DWORD>(data.size()) - totalWritten, &bytesWritten, &overlapped)) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    DWORD waitResult = WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
                    if (waitResult != WAIT_OBJECT_0) {
                        CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    GetOverlappedResult(m_pipe, &overlapped, &bytesWritten, TRUE);
                } else {
                    CloseHandle(overlapped.hEvent);
                    return false;
                }
            }
            totalWritten += bytesWritten;
        }

        CloseHandle(overlapped.hEvent);
    }

    return true;
}

} // namespace clipx
