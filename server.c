#include "includes.h"
#include <sys/stat.h>
#include <pthread.h>
#include <limits.h>
#include <dirent.h>
#include <time.h>

#define MAX_CLIENTS 15
#define MAX_FILENAME 256
#define MAX_FILES_NUM 100
#define MAX_GROUPS 5
#define FILE_DIR "./file/"
#define PERMISSION_LEN 6

// Capability Structure
struct Capability
{
    char filename[MAX_FILENAME];
    char owner[50];
    char group[50];
    char username[50];
    char last_modified[20];
    char permissions[7]; // rwrwrw : (owner, group, others)
    size_t size;
    bool isModified;
};

// File Management Globals
struct Capability capabilities[100]; // 檔案清單
int file_count = 0;

// 格式化
void format_response(Response *res, const char *status, const char *content)
{
    strncpy(res->status, status, sizeof(res->status) - 1);
    res->status[sizeof(res->status) - 1] = '\0'; // 確保字串結尾

    if (content)
    {
        strncpy(res->content, content, sizeof(res->content) - 1);
        res->content[sizeof(res->content) - 1] = '\0'; // 確保字串結尾
    }
    else
    {
        res->content[0] = '\0'; // 若無內容則設為空
    }
}

void create_storage_dir()
{
    struct stat st = {0};
    if (stat(FILE_DIR, &st) == -1)
    {
        mkdir(FILE_DIR, 0755);
    }
}

// 確保權限為6個
void fix_permissions_format(char *permissions)
{
    if (strlen(permissions) < PERMISSION_LEN)
    {
        for (int i = strlen(permissions); i < PERMISSION_LEN; i++)
        {
            permissions[i] = '-'; // 不足分用 "-" 填充
        }
        permissions[PERMISSION_LEN] = '\0'; // Ensure null termination
    }
}

bool is_valid_permissions(const char *permissions)
{
    if (strlen(permissions) != PERMISSION_LEN)
        return false;

    // Check for valid permission format
    for (int i = 0; i < PERMISSION_LEN; i++)
    {
        if (i % 2 == 0) // Even index: 'r' or '-'
        {
            if (permissions[i] != 'r' && permissions[i] != '-')
            {
                return false;
            }
        }
        else // Odd index: 'w' or '-'
        {
            if (permissions[i] != 'w' && permissions[i] != '-')
            {
                return false;
            }
        }
    }
    return true;
}

bool file_exists(const char *filename)
{
    char filepath[MAX_FILENAME];
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_DIR, filename);
    return access(filepath, F_OK) == 0;
}

// Add log entry (placeholder)
void log_add(const char *username, const char *group, const char *action, const char *filename, size_t size, const char *status, const char *permissions, const char *last_modified)
{
    char formatted_permissions[PERMISSION_LEN + 1] = {0}; // 確保有空字元結尾
    strncpy(formatted_permissions, permissions, PERMISSION_LEN);
    formatted_permissions[PERMISSION_LEN] = '\0';

    fix_permissions_format(formatted_permissions);

    printf("[LOG] User: %s, Group: %s, Action: %s, File: %s, %lu, Status: %s, %s, %s\n", username, group, action, filename, size, status, formatted_permissions, last_modified);
}

// Create a file
void create_file(int client_socket, struct User client, const char *filename, const char *permissions)
{
    // Check if file already exists
    for (int i = 0; i < file_count; i++)
    {
        if (strcmp(capabilities[i].filename, filename) == 0)
        {
            Response res;
            format_response(&res, "File already exists", "");
            send(client_socket, &res, sizeof(res), 0);
            return;
        }
    }

    // Check if file limit is reached
    if (file_count >= MAX_FILES_NUM)
    {
        Response res;
        format_response(&res, "File limit reached", "");
        send(client_socket, &res, sizeof(res), 0);
        return;
    }

    // Ensure the storage directory exists
    struct stat st;
    if (stat(FILE_DIR, &st) == -1)
    {
        mkdir(FILE_DIR, 0700);
    }

    // Create file
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", FILE_DIR, filename);

    FILE *file = fopen(filepath, "w");
    if (!file)
    {
        perror("Failed to create file");

        Response res;
        format_response(&res, "Failed to create file", "");
        send(client_socket, &res, sizeof(res), 0);
        return;
    }
    fclose(file);

    // Update file list
    strncpy(capabilities[file_count].filename, filename, MAX_FILENAME - 1);
    strncpy(capabilities[file_count].owner, client.name, 49);
    strncpy(capabilities[file_count].group, client.group, 49);
    strncpy(capabilities[file_count].permissions, permissions, 5);

    capabilities[file_count].size = 0;
    capabilities[file_count].isModified = false;

    // Set last modified time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(capabilities[file_count].last_modified, sizeof(capabilities[file_count].last_modified), "%Y/%m/%d %H:%M", tm_info);

    log_add(client.name, client.group, "create", filename, capabilities[file_count].size, "success", permissions, capabilities[file_count].last_modified);

    file_count++;

    Response res;
    format_response(&res, "File created successfully", "");
    send(client_socket, &res, sizeof(res), 0);
}

// Read a file
void read_file(int client_socket, struct User client, const char *filename)
{
    for (int i = 0; i < file_count; i++)
    {
        if (strcmp(capabilities[i].filename, filename) == 0)
        {
            // Check permissions
            if ((capabilities[i].permissions[4] == 'r') ||
                (!strcmp(capabilities[i].group, client.group) && capabilities[i].permissions[2] == 'r') ||
                (!strcmp(capabilities[i].owner, client.name)))
            {

                if (capabilities[i].isModified)
                {
                    Response res;
                    format_response(&res, "File is modifying", "");
                    send(client_socket, &res, sizeof(res), 0);
                    return;
                }

                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", FILE_DIR, filename);

                // 打開檔案進行讀取
                FILE *file = fopen(filepath, "r");
                if (!file)
                {
                    perror("Failed to open file");
                    Response res;
                    format_response(&res, "Failed to read file", "");
                    send(client_socket, &res, sizeof(res), 0);
                    log_add(client.name, client.group, "read", filename, capabilities[i].size, "failed", capabilities[i].permissions, capabilities[i].last_modified);
                }
                else
                {
                    char file_content[CONTENT_SIZE];
                    size_t read_size = fread(file_content, 1, CONTENT_SIZE - 1, file);
                    file_content[read_size] = '\0';
                    fclose(file);

                    Response res;
                    format_response(&res, "File read successful", file_content);
                    send(client_socket, &res, sizeof(res), 0);
                    log_add(client.name, client.group, "read", filename, capabilities[i].size, "success", capabilities[i].permissions, capabilities[i].last_modified);
                }
            }
            else
            {
                Response res;
                format_response(&res, "Permission denied", "");
                send(client_socket, &res, sizeof(res), 0);
                log_add(client.name, client.group, "read", filename, capabilities[i].size, "permission denied", capabilities[i].permissions, capabilities[i].last_modified);
            }
            return;
        }
    }

    // File not found
    Response res;
    format_response(&res, "File not found", "");
    if (send(client_socket, &res, sizeof(Response), 0) < 0)
    {
        perror("Send failed");
    }
}

// Write to a file
void write_file(int client_socket, struct User client, const char *filename, const char *write_mode)
{
    for (int i = 0; i < file_count; i++)
    {
        if (strcmp(capabilities[i].filename, filename) == 0)
        {
            if (capabilities[i].isModified)
            {
                Response res;
                format_response(&res, "File is modifying", "");
                send(client_socket, &res, sizeof(res), 0);
                return;
            }

            capabilities[i].isModified = true;

            // Check if have the permissions
            if ((capabilities[i].permissions[5] == 'w') ||
                (!strcmp(capabilities[i].group, client.group) && capabilities[i].permissions[3] == 'w') ||
                (!strcmp(capabilities[i].owner, client.name)))
            {

                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", FILE_DIR, filename);

                FILE *file;
                Response res;

                file = fopen(filepath, "r");
                if (file == NULL)
                {
                    perror("Failed to open file");
                    format_response(&res, "Failed to get file content", "");
                    send(client_socket, &res, sizeof(res), 0);
                    log_add(client.name, client.group, "write", filename, capabilities[i].size, "failed", capabilities[i].permissions, capabilities[i].last_modified);
                }
                else
                {
                    char file_content[CONTENT_SIZE];
                    size_t read_size = fread(file_content, 1, CONTENT_SIZE - 1, file);
                    file_content[read_size] = '\0';
                    fclose(file);

                    format_response(&res, "Ready for writing the file", file_content);
                    send(client_socket, &res, sizeof(res), 0);
                }

                // 接收客戶端的內容
                char content[CONTENT_SIZE];
                int read_size = recv(client_socket, content, sizeof(content) - 1, 0);
                if (read_size <= 0)
                {
                    perror("Failed to receive content");
                    format_response(&res, "Failed to receive content", "");
                    send(client_socket, &res, sizeof(res), 0);
                    log_add(client.name, client.group, "write", filename, capabilities[i].size, "failed", capabilities[i].permissions, capabilities[i].last_modified);
                    capabilities[i].isModified = false;
                    return;
                }
                content[read_size] = '\0';

                if (!strcmp(write_mode, "o"))
                {
                    file = fopen(filepath, "w");
                    if (file == NULL)
                    {
                        perror("Failed to open file for overwriting");
                        format_response(&res, "Failed to overwrite file", "");
                        send(client_socket, &res, sizeof(res), 0);
                        log_add(client.name, client.group, "write", filename, capabilities[i].size, "failed", capabilities[i].permissions, capabilities[i].last_modified);
                        capabilities[i].isModified = false;
                        return;
                    }

                    fprintf(file, "%s", content);
                    fclose(file);
                    format_response(&res, "File overwritten", "");
                }
                else
                {
                    file = fopen(filepath, "a");
                    if (file == NULL)
                    {
                        perror("Failed to open file for appending");
                        format_response(&res, "Failed to append content", "");
                        send(client_socket, &res, sizeof(res), 0);
                        log_add(client.name, client.group, "write", filename, capabilities[i].size, "failed", capabilities[i].permissions, capabilities[i].last_modified);
                        capabilities[i].isModified = false;
                        return;
                    }

                    fprintf(file, "%s", content);
                    fclose(file);
                    format_response(&res, "Content appended", "");
                }

                struct stat st;
                if (stat(filepath, &st) == 0)
                    capabilities[i].size = st.st_size;
                else
                    perror("Failed to get file size");

                // update last modified time
                time_t now = time(NULL);
                struct tm *tm_info = localtime(&now);
                strftime(capabilities[i].last_modified, sizeof(capabilities[i].last_modified), "%Y/%m/%d %H:%M", tm_info);

                send(client_socket, &res, sizeof(res), 0);
                log_add(client.name, client.group, "write", filename, capabilities[i].size, "success", capabilities[i].permissions, capabilities[i].last_modified);
            }
            else
            {
                Response res;
                format_response(&res, "Permission denied.", "");
                send(client_socket, &res, sizeof(res), 0);
                log_add(client.name, client.group, "write", filename, capabilities[i].size, "permission denied", capabilities[i].permissions, capabilities[i].last_modified);
            }
            capabilities[i].isModified = false;
            return;
        }
    }

    Response res;
    format_response(&res, "File not found", "");
    send(client_socket, &res, sizeof(res), 0);
}

// Change file permissions
void change_mode(int client_socket, struct User client, const char *filename, const char *permissions)
{
    for (int i = 0; i < file_count; i++)
    {
        if (!strcmp(capabilities[i].filename, filename))
        {
            if (!strcmp(capabilities[i].owner, client.name))
            {
                strncpy(capabilities[i].permissions, permissions, 5);

                Response res;
                format_response(&res, "Permissions changed", "");
                send(client_socket, &res, sizeof(res), 0);
                log_add(client.name, client.group, "mode", filename, capabilities[i].size, "permissions changed", capabilities[i].permissions, capabilities[i].last_modified);
            }
            else
            {
                Response res;
                format_response(&res, "Permission denied", "");
                send(client_socket, &res, sizeof(res), 0);
                log_add(client.name, client.group, "mode", filename, capabilities[i].size, "permission denied", capabilities[i].permissions, capabilities[i].last_modified);
            }
            return;
        }
    }

    Response res;
    format_response(&res, "File not found", "");
    send(client_socket, &res, sizeof(res), 0);
}

// Client handler
void *handle_client(void *client_socket_ptr)
{
    int client_socket = *(int *)client_socket_ptr;
    ClientRequest request;

    Response res;

    // 處理客戶端的請求
    while (recv(client_socket, &request, sizeof(ClientRequest), 0) > 0)
    {
        struct User client = request.user;
        char *command = request.command;

        char filename[MAX_FILENAME], permissions[PERMISSION_LEN], write_mode[2];

        if (strlen(command) == 0)
        {
            format_response(&res, "INFO", "No command received.");
            send(client_socket, &res, sizeof(res), 0);
        }
        else if (strcmp(command, "ls") == 0)
        {
            char response[1024] = {0};
            format_response(&res, "SUCCESS", response);
            send(client_socket, &res, sizeof(res), 0);
        }
        else if (sscanf(command, "create %s %s", filename, permissions) == 2)
        {
            if (is_valid_permissions(permissions))
            {
                create_file(client_socket, client, filename, permissions);
            }
            else
            {
                format_response(&res, "Invalid permissions format.", "");
                send(client_socket, &res, sizeof(res), 0);
            }
        }
        else if (sscanf(command, "read %s", filename) == 1)
        {
            read_file(client_socket, client, filename);
        }
        else if (sscanf(command, "write %s %s", filename, write_mode) == 2)
        {
            write_file(client_socket, client, filename, write_mode);
        }
        else if (sscanf(command, "mode %s %s", filename, permissions) == 2)
        {
            if (is_valid_permissions(permissions))
            {
                change_mode(client_socket, client, filename, permissions);
            }
            else
            {
                format_response(&res, "Invalid permissions format.", "");
                send(client_socket, &res, sizeof(res), 0);
            }
        }
        else
        {
            format_response(&res, "Invalid permissions format.", "");
            send(client_socket, &res, sizeof(res), 0);
        }
    }

    close(client_socket);
    return NULL;
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_address;
    pthread_t thread_id;

    create_storage_dir();

    // 建 server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
        exit(1);

    // 設定 server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9003);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        exit(1);
    }
    printf("Server is running and waiting for connections...\n");

    while (1)
    {
        client_socket = accept(server_socket, NULL, NULL); // 接受客戶端連線
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        int *socket_ptr = malloc(sizeof(int));
        if (socket_ptr == NULL)
        {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }
        *socket_ptr = client_socket;

        // 建立新的執行緒處理客戶端請求
        if (pthread_create(&thread_id, NULL, handle_client, (void *)socket_ptr) != 0)
        {
            perror("Thread creation failed");
            close(client_socket);
            free(socket_ptr);
        }
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
