#ifndef TALLY_CLIENT_H
#define TALLY_CLIENT_H

#include <signal.h>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <iostream>
#include <cassert>
#include <sstream>
#include <unistd.h>

#include "iceoryx_dust/posix_wrapper/signal_watcher.hpp"
#include "iceoryx_posh/popo/untyped_client.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iox/detail/unique_id.hpp"

#include "tally/msg_struct.h"

extern cudaError_t LAST_CUDA_ERR;
extern bool REPLACE_CUBLAS;

class TallyClient {

public:

    static TallyClient *client;
    int32_t client_id;
    bool has_connected = false;

    std::recursive_mutex iox_mtx;

    std::map<const void *, std::string> host_func_to_demangled_kernel_name_map;
    std::map<std::string, std::vector<uint32_t>> _kernel_name_to_args;

    std::unordered_map<const void *, std::vector<uint32_t>> _kernel_addr_to_args;
    std::unordered_map<CUfunction, std::vector<uint32_t>> _jit_kernel_addr_to_args;

    iox::popo::UntypedClient *iox_client = nullptr;

    TallyClient() :
        client_id(getpid())
    {}

    ~TallyClient(){}

    static TallyClient* get_or_init_client() 
    {
        if (!TallyClient::client) {
            TallyClient::client = new TallyClient();
        }
        return TallyClient::client;
    }

    void connect_to_server()
    {
        // Fork safety: children inherit this singleton and may keep the parent's
        // client_id/connection state, which can collide on the same IPC channel name.
        auto curr_pid = static_cast<int32_t>(getpid());
        if (client_id != curr_pid) {
            client_id = curr_pid;
            has_connected = false;

            if (iox_client) {
                delete iox_client;
                iox_client = nullptr;
            }
        }

        if (!has_connected) {
            int32_t priority = std::getenv("PRIORITY") ? std::stoi(std::getenv("PRIORITY")) : 1;

            auto app_name_str_base = std::string("tally-client-app");
            auto app_name_str = app_name_str_base + std::to_string(client_id);

            char APP_NAME[100];
            strcpy(APP_NAME, app_name_str.c_str()); 

            iox::runtime::PoshRuntime::initRuntime(APP_NAME);

            iox::popo::UntypedClient client_handshake({"Tally", "handshake", "event"});

            // Send handshake to server
            client_handshake.loan(sizeof(HandshakeMessgae), alignof(HandshakeMessgae))
                .and_then([&](auto& requestPayload) {

                    auto request = static_cast<HandshakeMessgae*>(requestPayload);
                    request->client_id = client_id;
                    request->priority = priority;

                    client_handshake.send(request).or_else(
                        [&](auto& error) { std::cout << "Could not send Request! Error: " << error << std::endl; });
                })
                .or_else([](auto& error) { std::cout << "Could not allocate Request! Error: " << error << std::endl; });

            while (!client_handshake.take().and_then([&](const auto& responsePayload) {

                auto response = static_cast<const HandshakeResponse*>(responsePayload);
                
                bool success = response->success;
                if (!success) {
                    std::cout << "Handshake with tally server failed. Exiting ..." << std::endl;
                    exit(1);
                }

                client_handshake.releaseResponse(responsePayload);

            })) {};

            auto channel_desc_str = std::string("Tally-Communication") + std::to_string(client_id);
            char channel_desc[100];
            strcpy(channel_desc, channel_desc_str.c_str()); 
            iox_client = new iox::popo::UntypedClient({channel_desc, "tally", "tally"});
        }

        has_connected = true;
    }
};

#endif // TALLY_CLIENT_H
