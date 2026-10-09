#include "orderflow/itch_parser.hpp"
#include "orderflow/order_book.hpp"
#include "orderflow/benchmark.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

using namespace std;
using namespace orderflow;

static uint16_t read_len(ifstream& f) {
    uint8_t buf[2];
    f.read(reinterpret_cast<char*>(buf), 2);
    if (f.gcount() != 2) return 0;
    return (uint16_t(buf[0]) << 8) | buf[1];
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: bench_replay <itch-file>\n";
        return 1;
    }

    ifstream f(argv[1], ios::binary);
    if (!f) { cerr << "Cannot open file\n"; return 1; }

    // Warm-up pass (parse only, no book updates)
    {
        vector<uint8_t> buf;
        uint64_t count = 0;
        while (count < 100000) {
            uint16_t len = read_len(f);
            if (!f || !len) break;
            buf.resize(len);
            f.read(reinterpret_cast<char*>(buf.data()), len);
            ParsedMsg msg;
            parse_message(buf.data(), len, msg);
            ++count;
        }
        f.seekg(0);
    }

    // Actual benchmark
    BookManager mgr;
    LatencyRecorder lat;
    Timer wall;
    vector<uint8_t> buf;

    wall.start();
    uint64_t parsed = 0;

    while (true) {
        uint16_t len = read_len(f);
        if (!f || !len) break;
        buf.resize(len);
        f.read(reinterpret_cast<char*>(buf.data()), len);
        if (static_cast<uint16_t>(f.gcount()) != len) break;

        ParsedMsg msg;
        if (!parse_message(buf.data(), len, msg)) continue;

        lat.tick();
        mgr.apply(msg);
        ++parsed;
    }

    uint64_t elapsed_ns = wall.elapsed_ns();
    uint64_t rss        = current_rss_kb();

    cout << fixed << setprecision(3);
    cout << "\n--- Benchmark Results ---\n";
    cout << "Messages     : " << parsed << "\n";
    cout << "Elapsed      : " << elapsed_ns / 1e6 << " ms\n";
    cout << "Throughput   : " << setprecision(2) << (parsed / (elapsed_ns / 1e9)) / 1e6
         << " M msg/s\n";
    cout << "Latency p50  : " << setprecision(1) << lat.percentile(50) << " ns\n";
    cout << "Latency p99  : " << lat.percentile(99) << " ns\n";
    cout << "Latency p99.9: " << lat.percentile(99.9) << " ns\n";
    cout << "RSS          : " << rss << " KB\n";
    cout << "Books        : " << mgr.all().size() << "\n";

    return 0;
}
