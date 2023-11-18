#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>

#include "client_handler.h"
#include "../Common/loggers.h"
#include "../Common/network_config.h"
#include "../Common/requests.h"
#include "../Common/responses.h"

typedef struct ClientHandlerArguments
{
    int socket;
    struct sockaddr_in client_address;
    socklen_t client_address_size;
} ClientHandlerArguments;


int read_file_and_send_data(const char *path, int client_socket)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        log_errno_error("Error while opening file:\n");
        return -1;
    }
    fseek(file, 0, SEEK_SET);

    char buffer[MAX_STREAMING_RESPONSE_PAYLOAD_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, MAX_STREAMING_RESPONSE_PAYLOAD_SIZE, file)) > 0)
    {
        if (send_streaming_response_payload(client_socket, buffer, bytes_read) == -1)
        {
            log_errno_error("Error while sending data to client:\n");
            return -1;
        }
    }
    end_streaming_response_payload(client_socket);
    fclose(file);
    return 0;
}
int recursive_path_finderr(char *path, char *prefix, char list_of_paths[][MAX_PATH_LENGTH], int *paths_count)
{
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    dir = opendir(path);
    if (dir == NULL)
    {
        log_errno_error("Unable to open directory: %s\n");
        // perror("Unable to open directory");
        return -1;
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        // Ignore "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path
        char *path = list_of_paths[(*paths_count)++];
        char follow_path[MAX_PATH_LENGTH];
        snprintf(follow_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);
        if (*prefix != '\0')
            snprintf(path, MAX_PATH_LENGTH, "%s/%s", prefix, entry->d_name);
        else
            snprintf(path, MAX_PATH_LENGTH, "%s", entry->d_name);

        if (entry->d_type == DT_DIR)
        {
            // Recursively call the function for subdirectories
            recursive_path_finderr(follow_path, path, list_of_paths, paths_count);
        }
    }
    return 0;
    // Close the directory
    closedir(dir);
}


int copy_file(const char *path, const char *destination)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        // Handle error appropriately, log, etc.
        perror("Error while opening source file");
        return -1;
    }

    FILE *destination_file = fopen(destination, "wb");
    if (destination_file == NULL)
    {
        // Attempt to create the destination file
        destination_file = fopen(destination, "wb");
        
        if (destination_file == NULL)
        {
            // Handle error appropriately, log, etc.
            perror("Error while opening/creating destination file");
            fclose(file);
            return -1;
        }
    }

    char buffer[MAX_FILE_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        fwrite(buffer, 1, bytes_read, destination_file);
    }

    fclose(file);
    fclose(destination_file);

    return 0;
}
int write_file(const char *path, char *data_buffer)
{
    FILE *file = fopen(path, "a");
    if (file == NULL)
    {
        log_errno_error("Error while opening file:\n");
        // fclose(file);
        return -1;
    }
    // write the data buffer to the file by concatinating it on the end of the file
    // Append the data buffer to the end of the file
    if (fprintf(file, "%s", data_buffer) < 0)
    {
        log_errno_error("Error while writing to file");
        fclose(file); // Close the file before returning in case of an error
        return -1;
    }

    fclose(file);
    return 0;
}
int check_file_exists(char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return -1;
    }
    fclose(file);
    return 0;
}
int get_info_send_info(const char *path, int client_socket)
{
    struct stat file_stat;
    if (stat(path, &file_stat) == -1)
    {
        log_errno_error("Error while getting file info:\n");
        return -1;
    }

    // send the file_stat

    if (send(client_socket, &file_stat, sizeof(file_stat), 0) == -1)
    {
        log_errno_error("Error while sending file info:\n");
        return -1;
    }

    return 0;
}
void *client_handler(void *arguments)
{
    ClientHandlerArguments *client_ss_handler_arguments = (ClientHandlerArguments *)arguments;
    Request request_buffer;
    if (receive_request(client_ss_handler_arguments->socket, &request_buffer) == -1)
    {
        log_errno_error("Error while receiving request from client:\n");
        return NULL;
    }
    char response;
    switch (request_buffer.request_type)
    {
    case READ_REQUEST:
        log_info("READ_REQUEST", &client_ss_handler_arguments->client_address);
        send_response(client_ss_handler_arguments->socket, OK_START_STREAM_RESPONSE);

        if (read_file_and_send_data(request_buffer.request_content.read_request_data.path, client_ss_handler_arguments->socket) == -1)
        {
            response = INTERNAL_ERROR_RESPONSE;
        }
        else
        {
            response = OK_RESPONSE;
        }
        send_response(client_ss_handler_arguments->socket, response);
        break;

    case WRITE_REQUEST:
        log_info("WRITE_REQUEST", &client_ss_handler_arguments->client_address);
        if (check_file_exists(request_buffer.request_content.write_request_data.path) == -1)
        {
            response = NOT_FOUND_RESPONSE;
        }
        else
        {
            response = OK_RESPONSE;
        }
        send_response(client_ss_handler_arguments->socket, response);

        char buffer[MAX_STREAMING_RESPONSE_PAYLOAD_SIZE + 1];

        if (receive_streaming_response_payload(client_ss_handler_arguments->socket, buffer) == -1)
        {
            log_errno_error("Error while receiving data from client:\n");
            response = INTERNAL_ERROR_RESPONSE;
        }
        else
        {
            if (write_file(request_buffer.request_content.write_request_data.path, buffer) == -1)
            {
                response = INTERNAL_ERROR_RESPONSE;
            }
            else
            {
                response = OK_RESPONSE;
            }
        }
        send_response(client_ss_handler_arguments->socket, response);
        break;

    case COPY_REQUEST:
        log_info("COPY_REQUEST", &client_ss_handler_arguments->client_address);
        if (check_file_exists(request_buffer.request_content.copy_request_data.source_path) == -1)
        {
            response = NOT_FOUND_RESPONSE;
        }
        else
        {
            if (copy_file(request_buffer.request_content.copy_request_data.source_path, request_buffer.request_content.copy_request_data.destination_path) == -1)
            {
                response = INTERNAL_ERROR_RESPONSE;
            }
            else
            {
                response = OK_RESPONSE;
            }
        }
        break;
    case FILE_INFO:
        log_info("FILE_INFO", &client_ss_handler_arguments->client_address);
        if (get_info_send_info(request_buffer.request_content.file_info_request_data.path, client_ss_handler_arguments->socket) == -1)
        {
            response = INTERNAL_ERROR_RESPONSE;
        }
        else
        {
            response = OK_RESPONSE;
        }

        send_response(client_ss_handler_arguments->socket, response);
        break;
    case GET_LIST:
        log_info("GET_LIST", &client_ss_handler_arguments->client_address);
        char list_of_paths[MAX_ACCESIBLE_PATHS][MAX_PATH_LENGTH];
        int paths_count = 0;
        if (recursive_path_finderr(request_buffer.request_content.get_list_request_data.path, "", list_of_paths, &paths_count) == -1)
        {
            response = INTERNAL_ERROR_RESPONSE;
        }
        else
        {
            response = OK_RESPONSE;
        }
        // send the data to the client list of paths , paths_count
        if (send(client_ss_handler_arguments->socket, &paths_count, sizeof(paths_count), 0) == -1)
        {
            log_errno_error("Error while sending paths_count:\n");
            return NULL;
        }
        send_response(client_ss_handler_arguments->socket, response);

    default:
        log_info("Invalid Request Type", &client_ss_handler_arguments->client_address);
        response = INVALID_REQUEST_RESPONSE;
        send_response(client_ss_handler_arguments->socket, response);
        break;
    }
    free(client_ss_handler_arguments);
    return NULL;
}

void *client_connection_acceptor(void *arguments)
{
    int ssid = ((ClientConnectionAcceptorArguments *)arguments)->ssid;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        log_errno_error("Error while creating client handler socket: %s\n");
        return NULL;
    }

    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(SS_CLIENT_HANDLER_BASE_PORT + ssid),
        .sin_addr = {
            .s_addr = inet_addr(SS_CLIENT_HANDLER_IP)}};

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        log_errno_error("Error while binding Client Handler Socket: %s\n");
        return NULL;
    }

    if (listen(server_socket, SS_CLIENT_HANDLER_TCP_WAIT_QUEUE) == -1)
    {
        log_errno_error("Error while listening using Client Handler Socket: %s\n");
        return NULL;
    }

    log_info("Listening for Client Connections", &server_address);
    while (1)
    {
        ClientHandlerArguments *client_handler_arguments = malloc(sizeof(ClientHandlerArguments));
        if (client_handler_arguments == NULL)
        {
            log_errno_error("Couldn't malloc: %s\n");
            return NULL;
        }
        client_handler_arguments->socket = accept(server_socket,
                                                  (struct sockaddr *)&client_handler_arguments->client_address,
                                                  &client_handler_arguments->client_address_size);
        if (client_handler_arguments->socket == -1)
        {
            log_errno_error("Error while accepting connection: %s\n");
            continue;
        }
        pthread_t client_handler_thread_id;
        pthread_create(&client_handler_thread_id, NULL, client_handler, client_handler_arguments);
    }
}