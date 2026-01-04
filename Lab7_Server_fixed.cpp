// server_fixed.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <vector>

#define HOST "0.0.0.0"
#define PORT 12345
#define RECV_BUF 4096
#define MAX_CLIENTS 30

int main() {
    int server_fd, client_fd, max_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[RECV_BUF];
    fd_set read_fds, master_fds;
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;
    }
    
    // Allow reuse of address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }
    
    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }
    
    // Listen for connections
    if (listen(server_fd, 5) == -1) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }
    
    std::cout << "Listening on " << HOST << ":" << PORT << " ..." << std::endl;
    
    // Initialize fd_set
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    max_fd = server_fd;
    
    while (true) {
        // Copy master set to working set
        read_fds = master_fds;
        
        // Wait for activity on any socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity == -1) {
            perror("select failed");
            break;
        }
        
        // Check all file descriptors
        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                
                // New connection on server socket
                if (fd == server_fd) {
                    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                    
                    if (client_fd == -1) {
                        perror("accept failed");
                        continue;
                    }
                    
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                    std::cout << "Accepted connection from " << client_ip 
                              << ":" << ntohs(client_addr.sin_port) << std::endl;
                    
                    // Add new client to master set
                    FD_SET(client_fd, &master_fds);
                    
                    // Update max_fd if necessary
                    if (client_fd > max_fd) {
                        max_fd = client_fd;
                    }
                }
                // Data from existing client
                else {
                    ssize_t bytes_received = recv(fd, buffer, RECV_BUF, 0);
                    
                    if (bytes_received <= 0) {
                        // Client disconnected or error
                        if (bytes_received == 0) {
                            std::cout << "Client on socket " << fd << " disconnected." << std::endl;
                        } else {
                            perror("recv failed");
                        }
                        
                        // Close socket and remove from master set
                        close(fd);
                        FD_CLR(fd, &master_fds);
                    }
                    else {
                        // Echo data back to client
                        ssize_t bytes_sent = send(fd, buffer, bytes_received, 0);
                        
                        if (bytes_sent == -1) {
                            perror("send failed");
                        } else {
                            std::cout << "Echoed " << bytes_sent << " bytes back to client on socket " 
                                      << fd << std::endl;
                        }
                        
                        // Close connection after echoing (as per lab requirements)
                        close(fd);
                        FD_CLR(fd, &master_fds);
                    }
                }
            }
        }
    }
    
    close(server_fd);
    return 0;
}
