#include <iostream>
#include <cassert>
#include <cmath>
#include "filter/quotient_filter.h"

void test_basic_insert_lookup() {
    std::cout << "[1] Basic insert and lookup..." << std::endl;

    QuotientFilter qf(8, 8);

    assert(qf.insert(100));
    assert(qf.insert(200));
    assert(qf.insert(300));

    assert(qf.lookup(100));
    assert(qf.lookup(200));
    assert(qf.lookup(300));
    assert(!qf.lookup(400));

    assert(qf.count() == 3);

    std::cout << "  PASS" << std::endl;
}

void test_basic_delete() {
    std::cout << "[2] Basic delete..." << std::endl;

    QuotientFilter qf(8, 8);

    qf.insert(100);
    qf.insert(200);
    qf.insert(300);

    assert(qf.lookup(200));
    assert(qf.remove(200));
    assert(!qf.lookup(200));
    assert(qf.lookup(100));
    assert(qf.lookup(300));
    assert(qf.count() == 2);

    assert(!qf.remove(999));

    std::cout << "  PASS" << std::endl;
}

void test_insert_many() {
    std::cout << "[3] Insert 500 elements..." << std::endl;

    QuotientFilter qf(10, 8);

    for (uint64_t i = 0; i < 500; ++i) {
        bool ok = qf.insert(i);
        assert(ok);
    }

    assert(qf.count() == 500);

    for (uint64_t i = 0; i < 500; ++i) {
        assert(qf.lookup(i));
    }

    std::cout << "  count: " << qf.count()
              << ", load: " << qf.load_factor()
              << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_delete_half() {
    std::cout << "[4] Insert 500, delete 250..." << std::endl;

    QuotientFilter qf(10, 8);

    for (uint64_t i = 0; i < 500; ++i) {
        qf.insert(i);
    }

    for (uint64_t i = 0; i < 500; ++i) {
        assert(qf.lookup(i));
    }

    for (uint64_t i = 0; i < 500; i += 2) {
        bool ok = qf.remove(i);
        assert(ok);
    }

    assert(qf.count() == 250);

    for (uint64_t i = 1; i < 500; i += 2) {
        assert(qf.lookup(i));
    }

    std::cout << "  PASS" << std::endl;
}

void test_false_positive_rate_8bit() {
    std::cout << "[5] False positive rate (8-bit remainder)..." << std::endl;

    QuotientFilter qf(10, 8);

    for (uint64_t i = 0; i < 500; ++i) {
        qf.insert(i);
    }

    int false_positives = 0;
    for (uint64_t i = 10000; i < 20000; ++i) {
        if (qf.lookup(i)) false_positives++;
    }

    double fp_rate = static_cast<double>(false_positives) / 10000.0;
    
    std::cout << "  FP rate: " << (fp_rate * 100.0) << "%" << std::endl;
    assert(fp_rate < 0.05);

    std::cout << "  PASS" << std::endl;
}

void test_false_positive_rate_12bit() {
    std::cout << "[6] False positive rate (12-bit remainder)..." << std::endl;

    QuotientFilter qf(10, 12);

    for (uint64_t i = 0; i < 500; ++i) {
        qf.insert(i);
    }

    int false_positives = 0;
    for (uint64_t i = 10000; i < 20000; ++i) {
        if (qf.lookup(i)) false_positives++;
    }

    double fp_rate = static_cast<double>(false_positives) / 10000.0;
    std::cout << "  FP rate: " << (fp_rate * 100.0) << "%" << std::endl;
    assert(fp_rate < 0.01);

    std::cout << "  PASS" << std::endl;
}

void test_colliding_quotients() {
    std::cout << "[7] Elements with same quotient (run handling)..." << std::endl;

    QuotientFilter qf(4, 8);

    for (uint64_t i = 0; i < 12; ++i) {
        bool ok = qf.insert(i * 1000);
        assert(ok);
    }

    for (uint64_t i = 0; i < 12; ++i) {
        assert(qf.lookup(i * 1000));
    }

    for (uint64_t i = 0; i < 12; i += 2) {
        assert(qf.remove(i * 1000));
    }

    for (uint64_t i = 1; i < 12; i += 2) {
        assert(qf.lookup(i * 1000));
    }

    std::cout << "  PASS" << std::endl;
}

void test_high_load() {
    std::cout << "[8] High load factor (~90%)..." << std::endl;

    QuotientFilter qf(8, 8);

    int inserted = 0;
    int failed = 0;
    for (uint64_t i = 0; i < 230; ++i) {
        if (qf.insert(i)) {
            inserted++;
        } else {
            failed++;
        }
    }

    std::cout << "  Inserted: " << inserted
              << ", Failed: " << failed
              << ", Load: " << qf.load_factor() << std::endl;

    assert(inserted > 200);

    for (uint64_t i = 0; i < static_cast<uint64_t>(inserted); ++i) {
        assert(qf.lookup(i));
    }

    std::cout << "  PASS" << std::endl;
}

void test_memory_usage() {
    std::cout << "[9] Memory usage..." << std::endl;

    QuotientFilter qf8(10, 8);
    QuotientFilter qf12(10, 12);

    std::cout << "  1024 slots, 8-bit remainder:  " << qf8.memory_usage() << " bytes" << std::endl;
    std::cout << "  1024 slots, 12-bit remainder: " << qf12.memory_usage() << " bytes" << std::endl;

    assert(qf12.memory_usage() > qf8.memory_usage());

    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "Running Quotient Filter tests..." << std::endl;

    test_basic_insert_lookup();
    test_basic_delete();
    test_insert_many();
    test_delete_half();
    test_false_positive_rate_8bit();
    test_false_positive_rate_12bit();
    test_colliding_quotients();
    test_high_load();
    test_memory_usage();

    std::cout << "All Quotient Filter tests passed." << std::endl;
    return 0;
}