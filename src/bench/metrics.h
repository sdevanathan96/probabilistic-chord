#ifndef CHORD_METRICS_H
#define CHORD_METRICS_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>


struct LookupRecord {
    uint64_t key;
    uint64_t owner_id;
    int hop_count;
    double latency_us;
    bool used_fallback;
    std::string routing_mode;
};

struct MembershipRecord {
    uint64_t node_id;
    std::string event_type;
    int messages_sent;
    double latency_us;
};

struct TimedLookupRecord {
    double timestamp_ms;
    double latency_us;
    int hop_count;
    std::string routing_mode;
};

class Metrics {
public:
    Metrics();

    void record_lookup(const LookupRecord& record);

    void record_membership(const MembershipRecord& record);

    void count_message();

    void count_maintenance_message();

    int total_messages() const;
    int total_maintenance_messages() const;
    int total_lookups() const;

    double avg_hop_count() const;

    double avg_latency_us() const;

    double p50_latency_us() const;
    double p99_latency_us() const;

    double fallback_rate() const;

    void dump_lookups_csv(const std::string& filepath) const;

    void dump_membership_csv(const std::string& filepath) const;

    void dump_summary_csv(const std::string& filepath,
                          const std::string& routing_mode,
                          int num_nodes) const;

    void reset();

private:
    std::vector<double> sorted_latencies() const;

    std::vector<LookupRecord> lookups_;
    std::vector<MembershipRecord> memberships_;
    int message_count_;
    int maintenance_message_count_;
    mutable std::mutex mutex_;
};


class ScopedTimer {
public:
    explicit ScopedTimer(double& result_us);
    ~ScopedTimer();

private:
    double& result_us_;
    std::chrono::high_resolution_clock::time_point start_;
};

#endif // CHORD_METRICS_H