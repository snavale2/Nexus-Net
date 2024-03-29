#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define QUIT_MESSAGE "QUIT"

int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection Failed");
        return -1;
    }

    while (1)
    {
        printf("Enter message (type 'QUIT' to exit):\n");
        fgets(message, BUFFER_SIZE, stdin);

        // Send message to server
        send(sock, message, strlen(message), 0);

        // Receive response from server
        valread = read(sock, buffer, BUFFER_SIZE);
        printf("Server response: %s\n", buffer);

        // Check if message is "QUIT"
        if (strncmp(message, QUIT_MESSAGE, strlen(QUIT_MESSAGE)) == 0)
        {
            break; // Exit loop and close connection
        }

        memset(buffer, 0, BUFFER_SIZE); // Clear buffer for next message
    }

    close(sock); // Close connection

    return 0;
}
