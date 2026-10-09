#include "orderflow/itch_parser.hpp"
#include "orderflow/order_book.hpp"
#include "orderflow/features.hpp"
#include "orderflow/benchmark.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <cstdint>
#include <cmath>
#include <iomanip>

using namespace std;
using namespace orderflow;

// Read big-endian uint16 from file
static uint16_t read_len(ifstream& f) {
    uint8_t buf[2];
    f.read(reinterpret_cast<char*>(buf), 2);
    if (f.gcount() != 2) return 0;
    return (uint16_t(buf[0]) << 8) | buf[1];
}

static string json_escape(const string& s) {
    string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: orderflow <itch-file> [max-messages]\n";
        return 1;
    }

    string path = argv[1];
    uint64_t max_msgs = (argc >= 3) ? stoull(argv[2]) : UINT64_MAX;

    ifstream f(path, ios::binary);
    if (!f) {
        cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    BookManager mgr;
    LatencyRecorder lat;
    Timer wall;
    vector<uint8_t> buf;

    cout << "Parsing " << path << " ...\n";
    wall.start();

    uint64_t parsed = 0, skipped = 0;
    while (parsed < max_msgs) {
        uint16_t len = read_len(f);
        if (!f || len == 0) break;

        buf.resize(len);
        f.read(reinterpret_cast<char*>(buf.data()), len);
        if (static_cast<uint16_t>(f.gcount()) != len) break;

        ParsedMsg msg;
        if (!parse_message(buf.data(), len, msg)) { ++skipped; continue; }

        lat.tick();
        mgr.apply(msg);
        ++parsed;
    }

    uint64_t elapsed = wall.elapsed_ns();
    uint64_t rss     = current_rss_kb();

    double throughput = parsed / (elapsed / 1e9);

    cout << fixed << setprecision(2);
    cout << "\n=== Benchmark ===\n";
    cout << "Messages parsed   : " << parsed << "\n";
    cout << "Skipped           : " << skipped << "\n";
    cout << "Elapsed           : " << elapsed / 1e6 << " ms\n";
    cout << "Throughput        : " << throughput / 1e6 << " M msg/sec\n";
    cout << "Latency p50       : " << lat.percentile(50) << " ns\n";
    cout << "Latency p99       : " << lat.percentile(99) << " ns\n";
    cout << "Latency p99.9     : " << lat.percentile(99.9) << " ns\n";
    cout << "Peak RSS          : " << rss << " KB\n";
    cout << "Books tracked     : " << mgr.all().size() << "\n";

    // Top symbols by OFI
    struct SymOFI { string sym; double ofi; double spread_bps; uint64_t msgs; };
    vector<SymOFI> ranking;

    for (auto& [sym, book] : mgr.all()) {
        auto snap = book.snapshot(0, 5);
        auto feat = compute_features(snap);
        ranking.push_back({sym, feat.ofi_l1, feat.spread_bps,
                           book.add_count() + book.cancel_count() + book.execute_count()});
    }
    sort(ranking.begin(), ranking.end(),
         [](const SymOFI& a, const SymOFI& b){ return fabs(a.ofi) > fabs(b.ofi); });

    cout << "\n=== Top 10 symbols by |OFI| ===\n";
    cout << left << setw(10) << "Symbol"
         << setw(10) << "OFI"
         << setw(14) << "Spread(bps)"
         << "Activity\n";
    cout << string(45, '-') << "\n";
    int top = min(10, (int)ranking.size());
    for (int i = 0; i < top; i++) {
        cout << setw(10) << ranking[i].sym
             << setw(10) << setprecision(4) << ranking[i].ofi
             << setw(14) << setprecision(2) << ranking[i].spread_bps
             << ranking[i].msgs << "\n";
    }

    // Emit results.json
    ofstream jf("results.json");
    jf << "{\n";
    jf << "  \"dataset\": \"" << json_escape(path) << "\",\n";
    jf << "  \"messages_parsed\": " << parsed << ",\n";
    jf << "  \"elapsed_ms\": " << setprecision(2) << elapsed / 1e6 << ",\n";
    jf << "  \"throughput_mps\": " << setprecision(3) << throughput / 1e6 << ",\n";
    jf << "  \"latency_p50_ns\": " << setprecision(1) << lat.percentile(50) << ",\n";
    jf << "  \"latency_p99_ns\": " << setprecision(1) << lat.percentile(99) << ",\n";
    jf << "  \"latency_p999_ns\": " << setprecision(1) << lat.percentile(99.9) << ",\n";
    jf << "  \"peak_rss_kb\": " << rss << ",\n";
    jf << "  \"books_tracked\": " << mgr.all().size() << ",\n";
    jf << "  \"top_ofi\": [\n";
    for (int i = 0; i < top; i++) {
        jf << "    {\"symbol\": \"" << json_escape(ranking[i].sym) << "\","
           << " \"ofi\": " << setprecision(6) << ranking[i].ofi << ","
           << " \"spread_bps\": " << setprecision(4) << ranking[i].spread_bps << ","
           << " \"activity\": " << ranking[i].msgs << "}";
        if (i < top - 1) jf << ",";
        jf << "\n";
    }
    jf << "  ]\n}\n";
    jf.close();

    cout << "\nResults written to results.json\n";
    return 0;
}
