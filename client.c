#include "includes.h"
#include <termios.h>

// Print server response
void print_server_response(int sock_fd)
{
    Response res;
    ssize_t recv_result = recv(sock_fd, &res, sizeof(Response), 0);
    if (recv_result > 0)
    {
        printf("[Server]: %s\n", res.status);
        if (strlen(res.content) > 0)
        {
            printf("[Content]:\n%s\n", res.content);
        }
    }
    else
    {
        perror("Failed to receive server response");
    }
}
// Handle write command
void handle_write(struct User *user, int sock_fd, const char *command)
{
    ClientRequest request;
    memcpy(&request.user, user, sizeof(struct User));
    strncpy(request.command, command, BUFFER_SIZE);

    // Send the write command to the server
    if (send(sock_fd, &request, sizeof(ClientRequest), 0) < 0)
    {
        perror("Failed to send write command");
        return;
    }

    // Receive server response
    Response res;
    if (recv(sock_fd, &res, sizeof(Response), 0) > 0)
    {
        printf("[Server]: %s\n", res.status);
        if (strcmp(res.status, "Ready for writing the file") == 0)
        {
            // Enter content editing mode
            printf("Enter the content (Press 'Ctrl+q' & 'Enter' when finished):\n");

            struct termios oldt, newt;
            char content[CONTENT_SIZE] = {0};
            char c;
            int pos = 0;

            // Configure terminal for raw mode
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_iflag &= ~(IXON);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);

            // Read content from user
            while (read(STDIN_FILENO, &c, 1) > 0)
            {
                if (c == 17)
                { // Ctrl+q
                    break;
                }
                else if (c == 127 && pos > 0)
                { // Handle backspace
                    printf("\b \b");
                    content[--pos] = '\0';
                }
                else if (pos < CONTENT_SIZE - 1)
                {
                    content[pos++] = c;
                }
            }

            // Restore terminal settings
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

            // Send content to server
            if (send(sock_fd, content, pos, 0) < 0)
            {
                perror("Failed to send content");
            }
            print_server_response(sock_fd);
        }
        else
            return;
    }
    else
        perror("Failed to receive server response");
    return;
}
void client_handler(int sock_fd)
{
    struct User user;
    char command[BUFFER_SIZE];

    printf("Enter username: ");
    if (fgets(user.name, sizeof(user.name), stdin) == NULL)
    {
        perror("Failed to read username");
        return;
    }
    user.name[strcspn(user.name, "\n")] = 0;

    printf("Enter group (AOS-students/CSE-students): ");
    if (fgets(user.group, sizeof(user.group), stdin) == NULL)
    {
        perror("Failed to read group");
        return;
    }
    user.group[strcspn(user.group, "\n")] = 0;

    while (1)
    {
        printf("Enter command (create/read/write/mode/exit): ");
        fflush(stdout);

        memset(command, 0, sizeof(command));

        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            perror("Error reading input. Exiting.");
            break;
        }

        command[strcspn(command, "\n")] = '\0'; // Remove newline

        if (strcmp(command, "exit") == 0)
        {
            printf("Closing connection...\n");
            break;
        }
        else if (strncmp(command, "create", 6) == 0)
        {
            char filename[256], permissions[7];
            memset(filename, 0, sizeof(filename));
            memset(permissions, 0, sizeof(permissions));

            if (sscanf(command, "create %255s %6s", filename, permissions) != 2)
            {
                printf("Invalid format for create. Use: create <filename> <permissions>\n");
                continue;
            }
            // 驗證權限格式是否正確
            if (strlen(permissions) != 6 || strspn(permissions, "rw-") != 6) // 確保權限符合 rwr--- 格式
            {
                printf("Invalid permissions format. Use: rwr---\n");
                continue;
            }

            // Send create command to server
            ClientRequest request;
            memset(&request, 0, sizeof(ClientRequest)); // 清空結構體，避免未初始化的垃圾值
            memcpy(&request.user, &user, sizeof(struct User));
            snprintf(request.command, sizeof(request.command), "create %s %s", filename, permissions);
            printf("Sending create request to server ...\n");

            if (send(sock_fd, &request, sizeof(request), 0) < 0)
            {
                perror("Failed to send create command");
                close(sock_fd); // 確保關閉無效的 socket
                exit(EXIT_FAILURE);
            }

            // Receive and print server response
            print_server_response(sock_fd);
        }
        else if (strncmp(command, "read", 4) == 0)
        {
            char filename[256];
            if (sscanf(command, "read %s", filename) != 1)
            {
                printf("Invalid format for read. Use: read <filename>\n");
                continue;
            }
            ClientRequest request;
            memset(&request, 0, sizeof(ClientRequest));
            memcpy(&request.user, &user, sizeof(struct User));
            snprintf(request.command, sizeof(request.command), "read %s", filename);

            // Send read command to server
            if (send(sock_fd, &request, sizeof(request), 0) < 0)
            {
                perror("Failed to send read command");
                continue;
            }
            Response res;
            if (recv(sock_fd, &res, sizeof(Response), 0) > 0)
            {
                printf("[Server]: %s\n", res.status);
                if (strlen(res.content) > 0)
                {
                    printf("[Content]:\n%s\n", res.content);
                }
            }
            else
            {
                perror("Failed to receive server response");
            }
        }
        else if (strncmp(command, "write", 5) == 0)
        {
            char filename[256], write_mode[2];
            if (sscanf(command, "write %s %s", filename, write_mode) != 2)
            {
                printf("Invalid format for write. Use: write <filename> <o/a>\n");
                continue;
            }
            handle_write(&user, sock_fd, command);
            continue;
        }
        else if (strncmp(command, "mode", 4) == 0)
        {
            char filename[256], permissions[6];
            if (sscanf(command, "mode %s %s", filename, permissions) != 2)
            {
                printf("Invalid format for mode. Use: mode <filename> <permissions>\n");
                continue;
            }
            // 檢查權限格式是否正確
            if (strlen(permissions) != 6 || strspn(permissions, "rw-") != 6)
            {
                printf("Invalid permissions format. Use: rwrw--.\n");
                continue;
            }
            // 發送指令至伺服器
            ClientRequest request;
            memset(&request, 0, sizeof(request));
            memcpy(&request.user, &user, sizeof(struct User));
            snprintf(request.command, sizeof(request.command), "mode %s %s", filename, permissions);

            if (send(sock_fd, &request, sizeof(request), 0) < 0)
            {
                perror("Failed to send mode command");
                continue;
            }

            print_server_response(sock_fd);
        }
        else
        {
            printf("Unknown command. Supported commands are: create, read, write, mode, exit.\n");
            continue;
        }
    }
}
int main()
{
    int sock_fd;
    struct sockaddr_in server_addr;

    // Create client socket
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address or Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server successfully!\n");

    client_handler(sock_fd);

    // Close socket
    close(sock_fd);
    printf("Connection closed.\n");
    return 0;
}
