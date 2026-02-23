#include "common/windows.h"
#include "ipc_server.h"
#include "common/logger.h"
#include "common/utils.h"
#include <vector>

namespace clipx {

IPCServer::IPCServer() {
    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

IPCServer::~IPCServer() {
    Stop();
    if (m_stopEvent) {
        CloseHandle(m_stopEvent);
    }
}

bool IPCServer::Start(const std::string& pipeName) {
    if (m_running) {
        return true;
    }

    m_pipeName = pipeName;
    m_running = true;
    ResetEvent(m_stopEvent);

    m_listenerThread = std::thread(&IPCServer::ListenLoop, this);

    LOG_INFO("IPC Server started: " + pipeName);
    return true;
}

void IPCServer::Stop() {
    if (!m_running) {
        return;
    }

    m_running = false;
    SetEvent(m_stopEvent);

    if (m_listenerThread.joinable()) {
        m_listenerThread.join();
    }

    LOG_INFO("IPC Server stopped");
}

void IPCServer::SetRequestHandler(RequestHandler handler) {
    m_handler = std::move(handler);
}

void IPCServer::ListenLoop() {
    while (m_running) {
        // Create named pipe
        HANDLE pipe = CreateNamedPipeW(
            utils::Utf8ToWide(m_pipeName).c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            IPC_BUFFER_SIZE,
            IPC_BUFFER_SIZE,
            0,
            nullptr
        );

        if (pipe == INVALID_HANDLE_VALUE) {
            LOG_ERROR("Failed to create named pipe: " + std::to_string(GetLastError()));
            if (!m_running) break;
            Sleep(1000);
            continue;
        }

        // Set up overlapped for async connect
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        // Wait for client connection
        BOOL connected = ConnectNamedPipe(pipe, &overlapped);
        if (!connected && GetLastError() == ERROR_IO_PENDING) {
            // Wait for connection or stop event
            HANDLE handles[] = { overlapped.hEvent, m_stopEvent };
            DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0 + 1) {
                // Stop event signaled
                CloseHandle(overlapped.hEvent);
                CloseHandle(pipe);
                break;
            }

            DWORD bytesTransferred;
            connected = GetOverlappedResult(pipe, &overlapped, &bytesTransferred, FALSE);
        }

        CloseHandle(overlapped.hEvent);

        if (connected || GetLastError() == ERROR_PIPE_CONNECTED) {
            // Handle client in a separate thread
            std::thread clientThread(&IPCServer::HandleClient, this, pipe);
            clientThread.detach();
        } else {
            CloseHandle(pipe);
        }
    }
}

void IPCServer::HandleClient(HANDLE pipe) {
    std::vector<uint8_t> buffer;

    while (m_running) {
        // Read message
        if (!ReadMessage(pipe, buffer)) {
            break;
        }

        // Parse request
        std::string jsonStr(buffer.begin(), buffer.end());
        IPCRequest request;
        IPCResponse response;

        try {
            nlohmann::json json = nlohmann::json::parse(jsonStr);
            request = IPCRequest::FromJson(json);

            // Handle request
            if (m_handler) {
                response = m_handler(request);
            } else {
                response = IPCResponse::Error(request.requestId, "No handler set", IPCError::IPC_INVALID_REQUEST);
            }

            // Send response
            std::string responseJson = response.ToJson().dump();
            std::vector<uint8_t> responseData(responseJson.begin(), responseJson.end());

            if (!WriteMessage(pipe, responseData)) {
                LOG_ERROR("Failed to send response");
                break;
            }

            LOG_DEBUG("Sent response for request: " + request.action);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse request: " + std::string(e.what()));
            IPCResponse errorResp = IPCResponse::Error(0, "Invalid request format", IPCError::IPC_INVALID_REQUEST);
            std::string responseJson = errorResp.ToJson().dump();
            std::vector<uint8_t> responseData(responseJson.begin(), responseJson.end());
            WriteMessage(pipe, responseData);
            break;
        }
    }

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

bool IPCServer::ReadMessage(HANDLE pipe, std::vector<uint8_t>& buffer) {
    // Read header first
    IPCHeader header;
    DWORD bytesRead = 0;

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    if (!ReadFile(pipe, &header, sizeof(header), &bytesRead, &overlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
            GetOverlappedResult(pipe, &overlapped, &bytesRead, TRUE);
        }
    }

    CloseHandle(overlapped.hEvent);

    if (bytesRead != sizeof(header) || header.magic != IPC_MAGIC) {
        LOG_ERROR("Invalid IPC header");
        return false;
    }

    // Read payload
    buffer.resize(header.payloadSize);
    if (header.payloadSize > 0) {
        bytesRead = 0;
        overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        if (!ReadFile(pipe, buffer.data(), header.payloadSize, &bytesRead, &overlapped)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
                GetOverlappedResult(pipe, &overlapped, &bytesRead, TRUE);
            }
        }

        CloseHandle(overlapped.hEvent);

        if (bytesRead != header.payloadSize) {
            LOG_ERROR("Failed to read full payload");
            return false;
        }
    }

    return true;
}

bool IPCServer::WriteMessage(HANDLE pipe, const std::vector<uint8_t>& data) {
    IPCHeader header;
    header.magic = IPC_MAGIC;
    header.payloadSize = static_cast<uint32_t>(data.size());

    DWORD bytesWritten = 0;

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Write header
    if (!WriteFile(pipe, &header, sizeof(header), &bytesWritten, &overlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
            GetOverlappedResult(pipe, &overlapped, &bytesWritten, TRUE);
        }
    }

    CloseHandle(overlapped.hEvent);

    if (bytesWritten != sizeof(header)) {
        return false;
    }

    // Write payload
    if (!data.empty()) {
        overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        if (!WriteFile(pipe, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, &overlapped)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(overlapped.hEvent, IPC_DEFAULT_TIMEOUT_MS);
                GetOverlappedResult(pipe, &overlapped, &bytesWritten, TRUE);
            }
        }

        CloseHandle(overlapped.hEvent);

        if (bytesWritten != data.size()) {
            return false;
        }
    }

    return true;
}

} // namespace clipx
