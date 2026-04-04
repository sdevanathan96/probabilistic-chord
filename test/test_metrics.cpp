#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include "bench/metrics.h"

void test_basic_recording() {
    std::cout << "[1] Basic recording..." << std::endl;

    Metrics m;

    LookupRecord r1;
    r1.key = 100;
    r1.owner_id = 5;
    r1.hop_count = 3;
    r1.latency_us = 150.0;
    r1.used_fallback = false;
    r1.routing_mode = "finger_table";
    m.record_lookup(r1);

    LookupRecord r2;
    r2.key = 200;
    r2.owner_id = 10;
    r2.hop_count = 5;
    r2.latency_us = 250.0;
    r2.used_fallback = true;
    r2.routing_mode = "finger_table";
    m.record_lookup(r2);

    assert(m.total_lookups() == 2);
    assert(std::abs(m.avg_hop_count() - 4.0) < 0.01);
    assert(std::abs(m.avg_latency_us() - 200.0) < 0.01);
    assert(std::abs(m.fallback_rate() - 0.5) < 0.01);

    std::cout << "  PASS" << std::endl;
}

void test_message_counting() {
    std::cout << "[2] Message counting..." << std::endl;

    Metrics m;

    for (int i = 0; i < 10; ++i) m.count_message();
    for (int i = 0; i < 3; ++i) m.count_maintenance_message();

    assert(m.total_messages() == 10);
    assert(m.total_maintenance_messages() == 3);

    std::cout << "  PASS" << std::endl;
}

void test_percentiles() {
    std::cout << "[3] Percentile calculations..." << std::endl;

    Metrics m;

    // Add 100 lookups with latencies 1, 2, 3, ..., 100
    for (int i = 1; i <= 100; ++i) {
        LookupRecord r;
        r.key = i;
        r.owner_id = 1;
        r.hop_count = 1;
        r.latency_us = static_cast<double>(i);
        r.used_fallback = false;
        r.routing_mode = "finger_table";
        m.record_lookup(r);
    }

    assert(m.p50_latency_us() >= 49.0 && m.p50_latency_us() <= 51.0);
    // p99 should be around 99
    assert(m.p99_latency_us() >= 98.0 && m.p99_latency_us() <= 100.0);

    std::cout << "  PASS" << std::endl;
}

void test_scoped_timer() {
    std::cout << "[4] ScopedTimer..." << std::endl;

    double elapsed = 0;
    {
        ScopedTimer timer(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(elapsed > 5000.0);
    assert(elapsed < 50000.0);

    std::cout << "  PASS" << std::endl;
}

void test_csv_output() {
    std::cout << "[5] CSV output..." << std::endl;

    Metrics m;

    for (int i = 0; i < 5; ++i) {
        LookupRecord r;
        r.key = i * 100;
        r.owner_id = i + 1;
        r.hop_count = i + 1;
        r.latency_us = (i + 1) * 50.0;
        r.used_fallback = (i % 2 == 0);
        r.routing_mode = "finger_table";
        m.record_lookup(r);
    }

    MembershipRecord mr;
    mr.node_id = 42;
    mr.event_type = "join";
    mr.messages_sent = 8;
    mr.latency_us = 500.0;
    m.record_membership(mr);

    std::string lookup_path = "/tmp/test_lookups.csv";
    std::string membership_path = "/tmp/test_membership.csv";
    std::string summary_path = "/tmp/test_summary.csv";

    m.dump_lookups_csv(lookup_path);
    m.dump_membership_csv(membership_path);
    m.dump_summary_csv(summary_path, "finger_table", 16);

    std::ifstream lookup_file(lookup_path);
    assert(lookup_file.good());
    std::string header;
    std::getline(lookup_file, header);
    assert(header.find("key") != std::string::npos);

    int lines = 1;
    std::string line;
    while (std::getline(lookup_file, line)) lines++;
    assert(lines == 6);

    std::ifstream membership_file(membership_path);
    assert(membership_file.good());

    std::ifstream summary_file(summary_path);
    assert(summary_file.good());

    std::remove(lookup_path.c_str());
    std::remove(membership_path.c_str());
    std::remove(summary_path.c_str());

    std::cout << "  PASS" << std::endl;
}

void test_reset() {
    std::cout << "[6] Reset..." << std::endl;

    Metrics m;

    LookupRecord r;
    r.key = 1;
    r.owner_id = 1;
    r.hop_count = 1;
    r.latency_us = 100.0;
    r.used_fallback = false;
    r.routing_mode = "finger_table";
    m.record_lookup(r);
    m.count_message();

    assert(m.total_lookups() == 1);
    assert(m.total_messages() == 1);

    m.reset();

    assert(m.total_lookups() == 0);
    assert(m.total_messages() == 0);

    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "Running Metrics tests..." << std::endl;

    test_basic_recording();
    test_message_counting();
    test_percentiles();
    test_scoped_timer();
    test_csv_output();
    test_reset();

    std::cout << "All Metrics tests passed." << std::endl;
    return 0;
}