#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

void send_cmd(int s, std::string m, std::string k, std::string v = "") {
    std::string msg = "Method:" + m + "\r\nKey:" + k + "\r\n";
    if (!v.empty()) msg += "Value:" + v + "\r\n";
    msg += "\r\n";
    
    std::cout << "\n>> " << m << " " << k << std::endl;
    send(s, msg.c_str(), msg.size(), 0);
    
    char buf[8192] = {};
    read(s, buf, sizeof(buf));
    std::cout << "<< " << buf;
}

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "Can't connect! Is server running?" << std::endl;
        return 1;
    }
    
    std::cout << "=== Testing Server ===" << std::endl;
    
    send_cmd(s, "ADD", "user1", "HelloWorld");
    send_cmd(s, "GET", "user1");
    send_cmd(s, "UPDATE", "user1", "NewValue");
    send_cmd(s, "GET", "user1");
    send_cmd(s, "DELETE", "user1");
    send_cmd(s, "GET", "user1");
    
    close(s);
    return 0;
}
