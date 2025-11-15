// epoll_server.cpp
#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
using namespace std;
#define PORT 12345
#define RECV_BUF 4096
int set_nonblocking(int fd){ int flags = fcntl(fd, F_GETFL, 0); if(flags==-1) return -1; return fcntl(fd, F_SETFL, flags | O_NONBLOCK); }

int main(){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd<0){ perror("socket"); return 1; }
    int opt=1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(PORT);
    if(bind(server_fd, (sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); return 1; }
    if(listen(server_fd, 128)<0){ perror("listen"); return 1; }
    set_nonblocking(server_fd);
    cout<<"epoll server listening on port "<<PORT<<endl;

    int epfd = epoll_create1(0);
    if(epfd < 0){ perror("epoll_create1"); return 1; }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    const int MAX_EVENTS = 1000;
    vector<epoll_event> events(MAX_EVENTS);

    while(true){
        int n = epoll_wait(epfd, events.data(), MAX_EVENTS, -1);
        if(n < 0){ perror("epoll_wait"); break; }
        for(int i=0;i<n;i++){
            int fd = events[i].data.fd;
            if(fd == server_fd){
                // accept loop (non-blocking)
                while(true){
                    sockaddr_in cli; socklen_t len = sizeof(cli);
                    int c = accept(server_fd, (sockaddr*)&cli, &len);
                    if(c < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                        else { perror("accept"); break; }
                    }
                    set_nonblocking(c);
                    epoll_event cev{}; cev.events = EPOLLIN | EPOLLET; cev.data.fd = c; // edge-triggered
                    epoll_ctl(epfd, EPOLL_CTL_ADD, c, &cev);
                    char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
                    cout<<"Accepted "<<ip<<":"<<ntohs(cli.sin_port)<<" fd="<<c<<"\n";
                }
            } else {
                // handle data
                int cfd = fd;
                char buf[RECV_BUF];
                while(true){
                    ssize_t r = recv(cfd, buf, sizeof(buf), 0);
                    if(r > 0){
                        send(cfd, buf, r, 0);
                    } else if(r == 0){
                        cout<<"client fd="<<cfd<<" closed\n";
                        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
                        close(cfd);
                        break;
                    } else {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                        else { perror("recv"); epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL); close(cfd); break; }
                    }
                }
            }
        }
    }

    close(epfd);
    close(server_fd);
    return 0;
}
