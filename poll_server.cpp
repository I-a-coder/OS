// poll_server.cpp
#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
using namespace std;

#define HOST "0.0.0.0"
#define PORT 12345
#define RECV_BUF 4096

int set_nonblocking(int fd){ 
    int flags = fcntl(fd, F_GETFL, 0); 
    if(flags==-1) return -1; 
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK); 
}

int main(){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd<0){ perror("socket"); return 1; }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if(bind(server_fd, (sockaddr*)&addr, sizeof(addr))<0){ 
        perror("bind"); 
        close(server_fd); 
        return 1; 
    }
    
    if(listen(server_fd, 128)<0){ 
        perror("listen"); 
        close(server_fd); 
        return 1; 
    }
    
    set_nonblocking(server_fd);
    
    cout << "poll server listening on " << HOST << ":" << PORT << endl;
    
    // Initialize poll array with server socket
    vector<pollfd> fds;
    fds.push_back({server_fd, POLLIN, 0});
    
    while(true){
        // Wait for events on any socket
        int ready = poll(fds.data(), fds.size(), -1);  // -1 = block indefinitely
        if(ready < 0){ 
            perror("poll"); 
            break; 
        }
        
        // Check all file descriptors
        for(size_t i = 0; i < fds.size(); i++){
            // Skip if no events
            if(fds[i].revents == 0) continue;
            
            // Check for errors
            if(fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)){
                if(fds[i].fd != server_fd){
                    cout << "client fd=" << fds[i].fd << " error/hangup\n";
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                }
                continue;
            }
            
            // Server socket - new connection
            if(fds[i].fd == server_fd && (fds[i].revents & POLLIN)){
                sockaddr_in cli_addr; 
                socklen_t len = sizeof(cli_addr);
                int client_fd = accept(server_fd, (sockaddr*)&cli_addr, &len);
                
                if(client_fd >= 0){
                    set_nonblocking(client_fd);
                    fds.push_back({client_fd, POLLIN, 0});
                    
                    char ip[INET_ADDRSTRLEN]; 
                    inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
                    cout << "Accepted " << ip << ":" << ntohs(cli_addr.sin_port) 
                         << " fd=" << client_fd << endl;
                } else {
                    if(errno != EWOULDBLOCK && errno != EAGAIN)
                        perror("accept");
                }
            }
            // Client socket - data ready
            else if(fds[i].revents & POLLIN){
                int cfd = fds[i].fd;
                char buf[RECV_BUF];
                
                ssize_t n = recv(cfd, buf, sizeof(buf), 0);
                if(n > 0){
                    // Echo back
                    ssize_t s = send(cfd, buf, n, 0);
                    if(s < 0) perror("send");
                } else if(n == 0){
                    // Client closed connection
                    cout << "client fd=" << cfd << " disconnected\n";
                    close(cfd);
                    fds.erase(fds.begin() + i);
                    i--;
                } else {
                    // Error
                    if(errno != EWOULDBLOCK && errno != EAGAIN){
                        perror("recv");
                        close(cfd);
                        fds.erase(fds.begin() + i);
                        i--;
                    }
                }
            }
        }
    }
    
    // Cleanup
    for(auto& pfd : fds){
        if(pfd.fd != server_fd) close(pfd.fd);
    }
    close(server_fd);
    return 0;
}
