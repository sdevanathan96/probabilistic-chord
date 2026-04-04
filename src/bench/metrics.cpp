#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <fstream>
#include <algorithm>
#include "metrics.h"

Metrics::Metrics() : message_count_(0), maintenance_message_count_(0) {}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    lookups_.clear();
    memberships_.clear();
    message_count_ = 0;
    maintenance_message_count_ = 0;
}

void Metrics::record_lookup(const LookupRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    lookups_.push_back(record);
}

void Metrics::record_membership(const MembershipRecord& record){
    std::lock_guard<std::mutex> lock(mutex_);
    memberships_.push_back(record);
}

void Metrics::count_message(){
    std::lock_guard<std::mutex> lock(mutex_);
    message_count_++;
}

void Metrics::count_maintenance_message(){
    std::lock_guard<std::mutex> lock(mutex_);
    maintenance_message_count_++;
}

int Metrics::total_messages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return message_count_;
}

int Metrics::total_maintenance_messages() const{
    std::lock_guard<std::mutex> lock(mutex_);
    return maintenance_message_count_;
}

int Metrics::total_lookups() const{
    std::lock_guard<std::mutex> lock(mutex_);
    return lookups_.size();
}

double Metrics::avg_hop_count() const{
    std::lock_guard<std::mutex> lock(mutex_);
    if (lookups_.empty()) return 0.0;
    int total_hops = 0;
    for (const auto& r : lookups_) total_hops += r.hop_count;
    return static_cast<double>(total_hops) / lookups_.size();
}

double Metrics::avg_latency_us() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lookups_.empty()) return 0.0;
    double total_lat = 0.0;
    for (const auto& r : lookups_) total_lat += r.latency_us;
    return total_lat / lookups_.size();
}

double Metrics::fallback_rate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lookups_.empty()) return 0.0;
    int fallback_count = 0;
    for (const auto& r : lookups_) {
        if (r.used_fallback) fallback_count++;
    }
    return static_cast<double>(fallback_count) / lookups_.size();
}

std::vector<double> Metrics::sorted_latencies() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<double> latencies;
    for (const auto& r : lookups_) latencies.push_back(r.latency_us);
    std::sort(latencies.begin(), latencies.end());
    return latencies;
}

double Metrics::p50_latency_us() const {
    std::vector<double> latencies = sorted_latencies();
    if (latencies.empty()) return 0.0;
    size_t idx = latencies.size() / 2;
    return latencies[idx];
}

double Metrics::p99_latency_us() const {
    std::vector<double> latencies = sorted_latencies();
    if (latencies.empty()) return 0.0;
    size_t idx = static_cast<size_t>(latencies.size() * 0.99);
    if (idx >= latencies.size()) idx = latencies.size() - 1;
    return latencies[idx];
}

void Metrics::dump_lookups_csv(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(filepath);
    file << "key,owner_id,hop_count,latency_us,used_fallback,routing_mode\n";
    for (const auto& r : lookups_) {
        file << r.key << "," << r.owner_id << "," << r.hop_count << ","
             << r.latency_us << "," << r.used_fallback << "," << r.routing_mode << "\n";
    }
}

void Metrics::dump_membership_csv(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(filepath);
    file << "node_id,event_type,messages_sent,latency_us\n";
    for (const auto& r : memberships_) {
        file << r.node_id << "," << r.event_type << "," << r.messages_sent << "," << r.latency_us << "\n";
    }
}

void Metrics::dump_summary_csv(
    const std::string& filepath,
    const std::string& routing_mode,
    int num_nodes
) const {
    int n_lookups = total_lookups();
    double hops = avg_hop_count();
    double lat = avg_latency_us();
    double p50 = p50_latency_us();
    double p99 = p99_latency_us();
    double fb = fallback_rate();
    int msgs = total_messages();
    int maint = total_maintenance_messages();

    std::ifstream check(filepath);
    bool empty = (check.peek() == std::ifstream::traits_type::eof());
    check.close();

    std::ofstream file(filepath, std::ios::app);
    if (empty) {
        file << "routing_mode,num_nodes,num_lookups,avg_hops,avg_latency_us,p50_latency_us,p99_latency_us,fallback_rate,total_messages,maintenance_messages\n";
    }
    file << routing_mode << "," << num_nodes << "," << n_lookups << ","
         << hops << "," << lat << "," << p50 << "," << p99 << ","
         << fb << "," << msgs << "," << maint << "\n";
}

ScopedTimer::ScopedTimer(double& result_us)
    : result_us_(result_us),
      start_(std::chrono::high_resolution_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    result_us_ = std::chrono::duration<double, std::micro>(end - start_).count();
}
