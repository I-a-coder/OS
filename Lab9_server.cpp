#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <map>
#include <cstring>
#include <sstream>

#define PORT 8080
#define PAGE_SIZE (40 * 1024)
#define TOTAL_PAGES (2ULL * 1024 * 1024 * 1024 / PAGE_SIZE)

struct ClientData {
    size_t start_page, num_pages;
    std::string value;
};

std::vector<bool> pages(TOTAL_PAGES, true);
std::map<std::string, ClientData> data;

size_t find_pages(size_t needed) {
    size_t count = 0, start = 0;
    for (size_t i = 0; i < TOTAL_PAGES; i++) {
        if (pages[i]) {
            if (count == 0) start = i;
            if (++count == needed) return start;
        } else count = 0;
    }
    return TOTAL_PAGES;
}

std::string handle(std::string msg) {
    std::istringstream ss(msg);
    std::string line, method, key, value;
    
    while (std::getline(ss, line)) {
        if (line.back() == '\r') line.pop_back();
        size_t pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string field = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (field == "Method") method = val;
        else if (field == "Key") key = val;
        else if (field == "Value") value = val;
    }
    
    if (method == "ADD") {
        if (data.count(key)) return "ERROR: Key already exists.";
        size_t needed = (value.size() + PAGE_SIZE - 1) / PAGE_SIZE;
        size_t start = find_pages(needed);
        if (start == TOTAL_PAGES) return "ERROR: Not enough contiguous space.";
        for (size_t i = start; i < start + needed; i++) pages[i] = false;
        data[key] = {start, needed, value};
        return "OK: Key added successfully.";
    }
    
    if (method == "GET") {
        if (!data.count(key)) return "ERROR: Key not found.";
        return "OK: Value=" + data[key].value;
    }
    
    if (method == "UPDATE") {
        if (!data.count(key)) return "ERROR: Key not found.";
        data[key].value = value;
        return "OK: Key updated successfully.";
    }
    
    if (method == "DELETE") {
        if (!data.count(key)) return "ERROR: Key not found.";
        auto &d = data[key];
        for (size_t i = d.start_page; i < d.start_page + d.num_pages; i++) 
            pages[i] = true;
        data.erase(key);
        return "OK: Key deleted successfully.";
    }
    
    return "ERROR: Invalid command.";
}

int main() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, 10);
    
    std::cout << "Server running on port " << PORT << std::endl;
    
    std::vector<pollfd> fds = {{srv, POLLIN, 0}};
    
    while (true) {
        poll(fds.data(), fds.size(), -1);
        
        for (size_t i = 0; i < fds.size(); i++) {
            if (!fds[i].revents) continue;
            
            if (fds[i].fd == srv) {
                int client = accept(srv, nullptr, nullptr);
                std::cout << "Client connected: " << client << std::endl;
                fds.push_back({client, POLLIN, 0});
            } else {
                char buf[8192] = {};
                int n = read(fds[i].fd, buf, sizeof(buf));
                
                if (n <= 0) {
                    std::cout << "Client disconnected" << std::endl;
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i--);
                } else {
                    std::string resp = handle(std::string(buf, n)) + "\r\n";
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    std::cout << "Response: " << resp;
                }
            }
        }
    }
}
