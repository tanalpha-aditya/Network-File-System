#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H
// connecting from client to NM
#define NM_CLIENT_HANDLER_SERVER_IP "127.0.0.1"
#define NM_CLIENT_HANDLER_SERVER_PORT 7999
#define NM_CLIENT_HANDLER_TCP_WAIT_QUEUE_LENGTH 5
// connecting from SS to NM
#define NM_SS_HANDLER_SERVER_IP "127.0.0.1"
#define NM_SS_HANDLER_SERVER_PORT 8001
#define NM_SS_HANDLER_TCP_WAIT_QUEUE_LENGTH 5

#define MAX_SS_COUNT 1024
// connecting from NM to SS
#define SS_NM_HANDLER_IP "127.0.0.1"
#define SS_NM_HANDLER_BASE_PORT 8002
#define SS_NM_HANDLER_TCP_WAIT_QUEUE 5

// connecting from client to SS
#define SS_CLIENT_HANDLER_IP "127.0.0.1"
#define SS_CLIENT_HANDLER_BASE_PORT (8003 + MAX_SS_COUNT)
#define SS_CLIENT_HANDLER_TCP_WAIT_QUEUE 5

#endif // NETWORK_CONFIG_H