#ifndef TALLY_IPC_UTIL_H
#define TALLY_IPC_UTIL_H

#define LOG_ERR_AND_EXIT(STR, ERR)          \
    std::cout << STR << ERR << std::endl;   \
    exit(1);

#define IOX_RECV_RETURN_STATUS(RET_TYPE)\
    while(!TallyClient::get_or_init_client()->iox_client->take()                                      \
        .and_then([&](const auto& responsePayload) {                                    \
            auto response = static_cast<const RET_TYPE*>(responsePayload);              \
            err = *response;                                                            \
            TallyClient::get_or_init_client()->iox_client->releaseResponse(responsePayload);          \
        }))                                                                             \
    {}

#if defined(RUN_LOCALLY)
    #define IOX_CLIENT_ACQUIRE_LOCK 
#else
    #define IOX_CLIENT_ACQUIRE_LOCK\
	auto* __tally_client = TallyClient::get_or_init_client();\
        if (!__tally_client->has_connected) __tally_client->connect_to_server();      \
        std::lock_guard<std::recursive_mutex> guard(__tally_client->iox_mtx);
#endif

#endif // TALLY_IPC_UTIL_H
