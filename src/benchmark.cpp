#include "orderflow/benchmark.hpp"
#include <fstream>
#include <string>
#include <sstream>

using namespace orderflow;

uint64_t orderflow::current_rss_kb() {
#if defined(__linux__)
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream ss(line);
            std::string key; uint64_t val;
            ss >> key >> val;
            return val;
        }
    }
#endif
    return 0;
}
