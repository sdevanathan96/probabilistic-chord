#include <iostream>
#include <cassert>
#include <cmath>
#include <set>
#include "filter/cuckoo_filter.h"

void test_basic_insert_lookup() {
    std::cout << "[1] Basic insert and lookup..." << std::endl;

    CuckooFilter cf(64, 12);

    assert(cf.insert(100));
    assert(cf.insert(200));
    assert(cf.insert(300));

    assert(cf.lookup(100));
    assert(cf.lookup(200));
    assert(cf.lookup(300));
    assert(!cf.lookup(400));

    assert(cf.count() == 3);

    std::cout << "  PASS" << std::endl;
}

void test_basic_delete() {
    std::cout << "[2] Basic delete..." << std::endl;

    CuckooFilter cf(64, 12);

    cf.insert(100);
    cf.insert(200);
    cf.insert(300);

    assert(cf.lookup(200));
    assert(cf.remove(200));
    assert(!cf.lookup(200));
    assert(cf.lookup(100));
    assert(cf.lookup(300));
    assert(cf.count() == 2);

    
    assert(!cf.remove(999));

    std::cout << "  PASS" << std::endl;
}

void test_insert_many() {
    std::cout << "[3] Insert 1000 elements..." << std::endl;

    
    CuckooFilter cf(512, 12);

    for (uint64_t i = 0; i < 1000; ++i) {
        bool ok = cf.insert(i);
        assert(ok);
    }

    assert(cf.count() == 1000);

    
    for (uint64_t i = 0; i < 1000; ++i) {
        assert(cf.lookup(i));
    }

    std::cout << "  count: " << cf.count()
              << ", load: " << cf.load_factor()
              << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_delete_half() {
    std::cout << "[4] Insert 1000, delete 500..." << std::endl;

    CuckooFilter cf(512, 12);

    for (uint64_t i = 0; i < 1000; ++i) {
        cf.insert(i);
    }

    
    for (uint64_t i = 0; i < 1000; i += 2) {
        bool ok = cf.remove(i);
        assert(ok);
    }

    assert(cf.count() == 500);

    
    int false_finds = 0;
    for (uint64_t i = 0; i < 1000; i += 2) {
        if (cf.lookup(i)) false_finds++;
    }
    
    std::cout << "  False finds after delete: " << false_finds << "/500" << std::endl;

    
    for (uint64_t i = 1; i < 1000; i += 2) {
        assert(cf.lookup(i));
    }

    std::cout << "  PASS" << std::endl;
}

void test_false_positive_rate_8bit() {
    std::cout << "[5] False positive rate (8-bit fingerprint)..." << std::endl;

    CuckooFilter cf(512, 8);

    
    for (uint64_t i = 0; i < 1000; ++i) {
        cf.insert(i);
    }

    
    int false_positives = 0;
    for (uint64_t i = 10000; i < 20000; ++i) {
        if (cf.lookup(i)) false_positives++;
    }

    double fp_rate = static_cast<double>(false_positives) / 10000.0;
    
    std::cout << "  FP rate: " << (fp_rate * 100.0) << "%" << std::endl;
    assert(fp_rate < 0.10);

    std::cout << "  PASS" << std::endl;
}

void test_false_positive_rate_12bit() {
    std::cout << "[6] False positive rate (12-bit fingerprint)..." << std::endl;

    CuckooFilter cf(512, 12);

    for (uint64_t i = 0; i < 1000; ++i) {
        cf.insert(i);
    }

    int false_positives = 0;
    for (uint64_t i = 10000; i < 20000; ++i) {
        if (cf.lookup(i)) false_positives++;
    }

    double fp_rate = static_cast<double>(false_positives) / 10000.0;
    
    std::cout << "  FP rate: " << (fp_rate * 100.0) << "%" << std::endl;
    assert(fp_rate < 0.02);

    std::cout << "  PASS" << std::endl;
}

void test_false_positive_rate_16bit() {
    std::cout << "[7] False positive rate (16-bit fingerprint)..." << std::endl;

    CuckooFilter cf(512, 16);

    for (uint64_t i = 0; i < 1000; ++i) {
        cf.insert(i);
    }

    int false_positives = 0;
    for (uint64_t i = 10000; i < 20000; ++i) {
        if (cf.lookup(i)) false_positives++;
    }

    double fp_rate = static_cast<double>(false_positives) / 10000.0;
    
    std::cout << "  FP rate: " << (fp_rate * 100.0) << "%" << std::endl;
    assert(fp_rate < 0.005);

    std::cout << "  PASS" << std::endl;
}

void test_high_load() {
    std::cout << "[8] High load factor (~95%)..." << std::endl;

    
    CuckooFilter cf(256, 12);

    int inserted = 0;
    int failed = 0;
    for (uint64_t i = 0; i < 950; ++i) {
        if (cf.insert(i)) {
            inserted++;
        } else {
            failed++;
        }
    }

    std::cout << "  Inserted: " << inserted
              << ", Failed: " << failed
              << ", Load: " << cf.load_factor() << std::endl;

    
    assert(inserted > 900);

    
    for (uint64_t i = 0; i < static_cast<uint64_t>(inserted); ++i) {
        assert(cf.lookup(i));
    }

    std::cout << "  PASS" << std::endl;
}

void test_memory_usage() {
    std::cout << "[9] Memory usage..." << std::endl;

    CuckooFilter cf8(512, 8);
    CuckooFilter cf12(512, 12);
    CuckooFilter cf16(512, 16);

    std::cout << "  512 buckets, 8-bit fp:  " << cf8.memory_usage() << " bytes" << std::endl;
    std::cout << "  512 buckets, 12-bit fp: " << cf12.memory_usage() << " bytes" << std::endl;
    std::cout << "  512 buckets, 16-bit fp: " << cf16.memory_usage() << " bytes" << std::endl;

    
    assert(cf16.memory_usage() > cf8.memory_usage());

    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "Running Cuckoo Filter tests..." << std::endl;

    test_basic_insert_lookup();
    test_basic_delete();
    test_insert_many();
    test_delete_half();
    test_false_positive_rate_8bit();
    test_false_positive_rate_12bit();
    test_false_positive_rate_16bit();
    test_high_load();
    test_memory_usage();

    std::cout << "All Cuckoo Filter tests passed." << std::endl;
    return 0;
}