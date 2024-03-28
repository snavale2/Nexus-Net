#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <asm-generic/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 25
#define MAX_CHANNELS 25
#define CHANNEL_NAME_LEN 50
#define MAX_NICK_LENGTH 20

char SERVER_IP[] = "128.226.114.201";
char nicknames[MAX_CLIENTS][MAX_NICK_LENGTH]; // Array to store nicknames

typedef struct
{
    char nickname[MAX_NICK_LENGTH];
    char realname[BUFFER_SIZE];
} UserInfo;

UserInfo user_info[MAX_CLIENTS]; // Array to store user information

typedef struct
{
    char channelName[CHANNEL_NAME_LEN];
    int clients[MAX_CLIENTS];
    int clientCount;
    char topic[BUFFER_SIZE];
} Channel;

Channel channels[MAX_CHANNELS];
int channelCount = 0;

void *handle_client(void *arg);
void handle_nick_command(int client_socket, const char *nickname);
void handle_user_command(int client_socket, const char *nickname, const char *realname);
void join_channel(int client_socket, const char *channelName);
void part_channel(int client_socket, const char *channelName);
void set_or_get_topic(int client_socket, const char *channelName, const char *topic);
void list_names(int client_socket, const char *channelName);

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)&new_socket) != 0)
        {
            perror("pthread_create");
            close(new_socket);
        }
    }

    return 0;
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    // char buffer[BUFFER_SIZE] = {0};
    char client_ip[INET_ADDRSTRLEN];

    // Get client's IP address
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client IP: %s\n", client_ip);
    }
    else
    {
        printf("Error getting client IP\n");
    }

    while (1)
    {
        char buffer[BUFFER_SIZE] = {0};
        // Read message from client
        int read_bytes = read(client_socket, buffer, BUFFER_SIZE);
        if (read_bytes > 0)
        {
            printf("Received message from client: %s\n", buffer);

            // Simple command parsing
            char *command = strtok(buffer, " ");
            if (strcmp(command, "JOIN") == 0)
            {
                char *channelName = strtok(NULL, " ");
                if (channelName)
                {
                    join_channel(client_socket, channelName);
                }
            }
            else if (strcmp(command, "PART") == 0)
            {
                char *channelName = strtok(NULL, " ");
                if (channelName)
                {
                    part_channel(client_socket, channelName);
                }
            }
            else if (strcmp(command, "TOPIC") == 0)
            {
                char *channelName = strtok(NULL, " ");
                char *topic = strtok(NULL, "");
                if (channelName)
                {
                    set_or_get_topic(client_socket, channelName, topic);
                }
            }
            else if (strcmp(command, "NAMES") == 0)
            {
                char *channelName = strtok(NULL, " ");
                list_names(client_socket, channelName);
            }
            else if (strcmp(command, "TIME") == 0)
            {
                time_t current_time;
                char time_str[BUFFER_SIZE];
                time(&current_time);
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

                // Send local time to client
                char reply_msg[BUFFER_SIZE];
                snprintf(reply_msg, BUFFER_SIZE, ":%s 391 %s %s :%s\n", SERVER_IP, user_info[client_socket].nickname, user_info[client_socket].nickname, time_str);
                write(client_socket, reply_msg, strlen(reply_msg));
            }
            else if (strcmp(command, "NICK") == 0)
            {
                char *nickname = strtok(NULL, " ");
                if (nickname)
                {
                    handle_nick_command(client_socket, nickname);
                }
                else
                {
                    // Send ERR_NONICKNAMEGIVEN
                    char err_msg[BUFFER_SIZE];
                    snprintf(err_msg, BUFFER_SIZE, ":%s 431 * :No nickname given\n", SERVER_IP);
                    write(client_socket, err_msg, strlen(err_msg));
                }
            }
            else if (strcmp(command, "USER") == 0)
            {
                char *nickname = strtok(NULL, " ");
                char *mode = strtok(NULL, " ");
                char *realname = strtok(NULL, "");
                if (nickname && mode && realname)
                {
                    handle_user_command(client_socket, nickname, realname);
                }
                else
                {
                    // Send ERR_NEEDMOREPARAMS
                    char err_msg[BUFFER_SIZE];
                    snprintf(err_msg, BUFFER_SIZE, ":%s 461 * :Not enough parameters\n", SERVER_IP);
                    write(client_socket, err_msg, strlen(err_msg));
                }
            }
            else if (strcmp(command, "QUIT") == 0)
            {
                char bye_message[BUFFER_SIZE];
                snprintf(bye_message, BUFFER_SIZE, "%s %s\n", "BYE", client_ip);
                write(client_socket, bye_message, strlen(bye_message)); // Send BYE message to client
                close(client_socket);
                printf("Client %s disconnected.\n", client_ip);
                pthread_exit(NULL); // Terminate the thread
            }
        }
    }
}

void handle_nick_command(int client_socket, const char *nickname)
{
    // Check if the nickname is already in use
    int nick_in_use = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(user_info[i].nickname, nickname) == 0)
        {
            nick_in_use = 1;
            break;
        }
    }

    if (!nick_in_use)
    {
        strncpy(user_info[client_socket].nickname, nickname, MAX_NICK_LENGTH - 1);
        user_info[client_socket].nickname[MAX_NICK_LENGTH - 1] = '\0';
        printf("Client %d set nickname to %s\n", client_socket, user_info[client_socket].nickname);
        // Send RPL_NICK
        char reply_msg[BUFFER_SIZE];
        snprintf(reply_msg, BUFFER_SIZE, ":%s 401 %s %s :Nickname is now %s\n", SERVER_IP, user_info[client_socket].nickname, user_info[client_socket].nickname, user_info[client_socket].nickname);
        write(client_socket, reply_msg, strlen(reply_msg));
    }
    else
    {
        // Send ERR_NICKNAMEINUSE
        char err_msg[BUFFER_SIZE];
        snprintf(err_msg, BUFFER_SIZE, ":%s 433 %s %s :Nickname is already in use\n", SERVER_IP, user_info[client_socket].nickname, user_info[client_socket].nickname);
        write(client_socket, err_msg, strlen(err_msg));
    }
}

void handle_user_command(int client_socket, const char *nickname, const char *realname)
{
    // Check if user is already registered
    if (strlen(user_info[client_socket].nickname) > 0)
    {
        // Send ERR_ALREADYREGISTRED
        char err_msg[BUFFER_SIZE];
        snprintf(err_msg, BUFFER_SIZE, ":%s 462 %s %s :You may not reregister\n", SERVER_IP, user_info[client_socket].nickname, user_info[client_socket].nickname);
        write(client_socket, err_msg, strlen(err_msg));
        return;
    }

    // Store user information
    strncpy(user_info[client_socket].nickname, nickname, MAX_NICK_LENGTH - 1);
    user_info[client_socket].nickname[MAX_NICK_LENGTH - 1] = '\0';
    strncpy(user_info[client_socket].realname, realname, BUFFER_SIZE - 1);
    user_info[client_socket].realname[BUFFER_SIZE - 1] = '\0';

    // Send RPL_WELCOME
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, BUFFER_SIZE, ":%s 001 %s :Welcome to the IRC network %s!%s@%s\n", SERVER_IP, user_info[client_socket].nickname, user_info[client_socket].nickname, user_info[client_socket].nickname, SERVER_IP);
    write(client_socket, welcome_msg, strlen(welcome_msg));
}

void join_channel(int client_socket, const char *channelName)
{
    printf("Client %d attempting to join channel %s\n", client_socket, channelName);

    // Check if channel already exists
    int found = 0;
    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channelName) == 0)
        {
            // Add client to existing channel
            if (channels[i].clientCount < MAX_CLIENTS)
            {
                channels[i].clients[channels[i].clientCount++] = client_socket;
                printf("Client %d joined channel %s\n", client_socket, channelName);
            }
            else
            {
                printf("Channel %s is full.\n", channelName);
            }
            found = 1;
            break;
        }
    }

    // If channel does not exist, create a new one
    if (!found)
    {
        if (channelCount < MAX_CHANNELS)
        {
            Channel newChannel;
            strncpy(newChannel.channelName, channelName, CHANNEL_NAME_LEN);
            newChannel.channelName[CHANNEL_NAME_LEN - 1] = '\0';
            newChannel.clients[0] = client_socket;
            newChannel.clientCount = 1;
            channels[channelCount++] = newChannel;
            printf("Channel %s created and client %d joined.\n", channelName, client_socket);
        }
        else
        {
            printf("Maximum number of channels reached.\n");
        }
    }
}

void part_channel(int client_socket, const char *channelName)
{
    printf("Client %d attempting to leave channel %s\n", client_socket, channelName);

    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channelName) == 0)
        {
            // Remove client from the channel
            int found = 0;
            for (int j = 0; j < channels[i].clientCount; j++)
            {
                if (channels[i].clients[j] == client_socket)
                {
                    for (int k = j; k < channels[i].clientCount - 1; k++)
                    {
                        channels[i].clients[k] = channels[i].clients[k + 1];
                    }
                    channels[i].clientCount--;
                    printf("Client %d left channel %s\n", client_socket, channelName);
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                printf("Client %d is not in channel %s\n", client_socket, channelName);
            }
            break;
        }
    }
}

void set_or_get_topic(int client_socket, const char *channelName, const char *topic)
{
    int found = 0;
    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channelName) == 0)
        {
            found = 1;
            if (topic == NULL)
            {
                // Get topic
                if (strlen(channels[i].topic) > 0)
                {
                    printf("Topic for %s is %s\n", channelName, channels[i].topic);
                    // send client_socket RPL_TOPIC with channels[i].topic
                }
                else
                {
                    printf("No topic is set for %s\n", channelName);
                    // send client_socket RPL_NOTOPIC
                }
            }
            else
            {
                // Set or remove topic
                if (strlen(topic) > 0)
                {
                    strncpy(channels[i].topic, topic, sizeof(channels[i].topic));
                    channels[i].topic[sizeof(channels[i].topic) - 1] = '\0';
                    printf("Topic for %s set to %s\n", channelName, topic);
                }
                else
                {
                    memset(channels[i].topic, 0, sizeof(channels[i].topic));
                    printf("Topic for %s has been removed\n", channelName);
                }
            }
            break;
        }
    }

    if (!found)
    {
        printf("Channel %s not found\n", channelName);
        // send client_socket ERR_NOSUCHCHANNEL
    }
}

void list_names(int client_socket, const char *channelName)
{
    int found = 0;
    if (channelName == NULL)
    {
        // List all channels and names
        for (int i = 0; i < channelCount; i++)
        {
            printf("Channel: %s\n", channels[i].channelName);
            for (int j = 0; j < channels[i].clientCount; j++)
            {
                printf("User: %d\n", channels[i].clients[j]);
                // send client_socket with names
            }
        }
    }
    else
    {
        // List names for the specific channel
        for (int i = 0; i < channelCount; i++)
        {
            if (strcmp(channels[i].channelName, channelName) == 0)
            {
                found = 1;
                printf("Channel: %s\n", channels[i].channelName);
                for (int j = 0; j < channels[i].clientCount; j++)
                {
                    printf("User: %d\n", channels[i].clients[j]);
                    // send client_socket with names
                }
                break;
            }
        }

        if (!found)
        {
            printf("Channel %s not found\n", channelName);
            // send client_socket ERR_NOSUCHCHANNEL
        }
    }
}
