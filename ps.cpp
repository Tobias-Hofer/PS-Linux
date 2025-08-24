#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <limits.h>
#include <errno.h>
#include <algorithm>

// Directory only numeric?
bool isNumeric(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

// Get the directory
std::string readLink(const std::string& path) {
    char buf[PATH_MAX];
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf)-1);
    if (len == -1) return "";
    buf[len] = '\0';
    return std::string(buf);
}

// Get the right status
std::string readStatusState(const std::string& pid) {
    std::ifstream status("/proc/" + pid + "/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("State:", 0) == 0) {
            std::istringstream iss(line);
            std::string key, state;
            iss >> key >> state;
            return state;
        }
    }
    return "";
}

// Get the right cmdline process
std::vector<std::string> readCmdline(const std::string& pid) {
    std::ifstream file("/proc/" + pid + "/cmdline", std::ios::binary);
    std::vector<std::string> result;
    std::string arg;
    char c;
    while (file.get(c)) {
        if (c == '\0') {
            if (!arg.empty()) result.push_back(arg);
            arg.clear();
        } else {
            arg += c;
        }
    }
    if (!arg.empty()) result.push_back(arg);
    return result;
}

// Get the right Baseadress 
uint64_t readBaseAddress(const std::string& pid, const std::string& exePath) {
    std::ifstream maps("/proc/" + pid + "/maps");
    std::string line;
    while (std::getline(maps, line)) {
        size_t pathPos = line.find(exePath);
        if (pathPos != std::string::npos) {
            std::istringstream iss(line);
            std::string addrRange;
            iss >> addrRange;
            size_t dashPos = addrRange.find('-');
            if (dashPos != std::string::npos) {
                std::string base = addrRange.substr(0, dashPos);
                return std::stoull(base, nullptr, 16);
            }
        }
    }
    return 0;
}

int main() {
    DIR* proc = opendir("/proc");
    if (!proc) {
        std::cerr << "Failed to open /proc\n";
        return 1;
    }

    std::vector<std::string> output;
    dirent* entry;
    while ((entry = readdir(proc)) != nullptr) {
        std::string pid(entry->d_name);
        if (!isNumeric(pid)) continue;

        std::string exe = readLink("/proc/" + pid + "/exe");
        std::string cwd = readLink("/proc/" + pid + "/cwd");
        if (exe.empty() || cwd.empty()) continue;

        std::string state = readStatusState(pid);
        if (state.empty()) continue;

        std::vector<std::string> cmdline = readCmdline(pid);
        if (cmdline.empty()) continue;

        uint64_t baseAddress = readBaseAddress(pid, exe);
        if (baseAddress == 0) continue;
	
	// create JSON File
        std::ostringstream json;
        json << "{";
        json << "\"pid\":" << pid << ",";
        json << "\"exe\":\"" << exe << "\",";
        json << "\"cwd\":\"" << cwd << "\",";
        json << "\"base_address\":" << baseAddress << ",";
        json << "\"state\":\"" << state << "\",";
        json << "\"cmdline\":[";
        for (size_t i = 0; i < cmdline.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << cmdline[i] << "\"";
        }
        json << "]}";
        output.push_back(json.str());
    }

    closedir(proc);

    // Output as JSON Array
    std::cout << "[";
    for (size_t i = 0; i < output.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << output[i];
    }
    std::cout << "]" << std::endl;

    return 0;
}

