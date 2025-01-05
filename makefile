CC = gcc

# -Wall: 顯示所有警告訊息
# -Wextra: 顯示額外的警告訊息
# -pthread: 支援多執行緒編程的選項
CFLAGS = -Wall -Wextra -pthread

LDFLAGS = -pthread

all: server client

# 編譯 server端和 client端
server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

clean:
	rm -f server client