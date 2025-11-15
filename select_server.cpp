// select_server.cpp
#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
using namespace std;

#define HOST "0.0.0.0"
#define PORT 12345
#define RECV_BUF 4096
int set_nonblocking(int fd){ int flags = fcntl(fd, F_GETFL, 0); if(flags==-1) return -1; return fcntl(fd, F_SETFL, flags | O_NONBLOCK); }

int main(){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd<0){ perror("socket"); return 1; }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if(bind(server_fd, (sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); close(server_fd); return 1; }
    if(listen(server_fd, 128)<0){ perror("listen"); close(server_fd); return 1; }

    set_nonblocking(server_fd);
    cout << "select server listening on " << HOST << ":" << PORT << endl;

    fd_set readfds;
    int maxfd = server_fd;
    vector<int> clients; // track clients

    while(true){
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        for(int c: clients) FD_SET(c, &readfds), maxfd = max(maxfd, c);

        // timeout optional; here NULL => block until event
        int ready = select(maxfd+1, &readfds, NULL, NULL, NULL);
        if(ready < 0){ perror("select"); break; }

        if(FD_ISSET(server_fd, &readfds)){
            sockaddr_in cli_addr; socklen_t len = sizeof(cli_addr);
            int client_fd = accept(server_fd, (sockaddr*)&cli_addr, &len);
            if(client_fd >= 0){
                set_nonblocking(client_fd);
                clients.push_back(client_fd);
                char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
                cout << "Accepted " << ip << ":" << ntohs(cli_addr.sin_port) << " fd="<<client_fd<<endl;
            }
        }

        // check clients
        for(auto it = clients.begin(); it != clients.end(); ){
            int cfd = *it;
            if(FD_ISSET(cfd, &readfds)){
                char buf[RECV_BUF];
                ssize_t n = recv(cfd, buf, sizeof(buf), 0);
                if(n > 0){
                    ssize_t s = send(cfd, buf, n, 0);
                    if(s < 0) perror("send");
                } else if(n == 0){
                    // client closed
                    cout << "client fd="<<cfd<<" disconnected\n";
                    close(cfd);
                    it = clients.erase(it);
                    continue;
                } else {
                    if(errno != EWOULDBLOCK && errno != EAGAIN){
                        perror("recv");
                        close(cfd);
                        it = clients.erase(it);
                        continue;
                    }
                }
            }
            ++it;
        }
    }

    for(int c: clients) close(c);
    close(server_fd);
    return 0;
}
