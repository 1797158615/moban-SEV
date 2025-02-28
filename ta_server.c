#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>

#define VSOCK_PORT 1234

atomic_int connection_active = 1; // 标志连接状态

// Function to handle sending data from host to guest
void *host_to_guest(void *arg) {
    int client_fd = *(int *)arg;
    char buffer[1024];

    // Set stdin to non-blocking mode
    int stdin_fd = fileno(stdin);
    int flags = fcntl(stdin_fd, F_GETFL, 0);
    fcntl(stdin_fd, F_SETFL, flags | O_NONBLOCK);

    while (atomic_load(&connection_active)) {
        // Non-blocking input from stdin
        if (fgets(buffer, sizeof(buffer), stdin)) {
            // Send input to guest
            if (send(client_fd, buffer, strlen(buffer), 0) < 0) {
                perror("send");
                break;
            }
        }
        usleep(100000); // Avoid busy-waiting
    }
    return NULL;
}

// Function to handle receiving data from guest
void *guest_to_host(void *arg) {
    int client_fd = *(int *)arg;
    char buffer[1024];
    ssize_t n;

    while (atomic_load(&connection_active)) {
        n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0'; // Null-terminate the string
            printf("%s", buffer); // Print received data
        } else if (n == 0) {
            // Connection closed by guest
            // printf("Guest disconnected\n");
            atomic_store(&connection_active, 0);
            break;
        } else {
            perror("recv");
            break;
        }
    }
    return NULL;
}


int main() {
    //开启服务器
    int server_fd, client_fd;
    struct sockaddr_vm addr;
    socklen_t addr_len = sizeof(addr);

    // Create VSOCK socket
    server_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configure address
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = VMADDR_CID_ANY; // Bind to any CID
    addr.svm_port = VSOCK_PORT;    // Listening port

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // printf("Waiting for connection...\n");
    //链接虚拟机
    system("ssh -p 10022 root@localhost './ta 40'");

    // Accept connection
    client_fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // printf("Client connected\n");

    // Create threads for bidirectional communication
    pthread_t send_thread, recv_thread;
    pthread_create(&send_thread, NULL, host_to_guest, &client_fd);
    pthread_create(&recv_thread, NULL, guest_to_host, &client_fd);

    // Wait for threads to finish
    pthread_join(recv_thread, NULL); // Wait for guest_to_host to detect disconnection
    atomic_store(&connection_active, 0); // Ensure send_thread also exits
    pthread_join(send_thread, NULL);

    // Cleanup
    close(client_fd);
    close(server_fd);
    // printf("Connection closed\n");
    system("ssh -p 10022 root@localhost 'shutdown -h now'");
    return 0;
}

