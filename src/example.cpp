// #include "global.hpp"
#include <iostream>
#include <algorithm>
#include <string>
#include <stdlib.h>
#include <cmath>
#include <random>
#include <unordered_map>
#include <functional>
#include <deque>
#include <numeric>
// #include <cilk/cilk.h>
// #include <cilk/cilk_api.h>
#include "ips4o.hpp"
#include "get_time.h"

#define NUM_ROUNDS 6

using namespace std;
using namespace ips4o;
constexpr const uint32_t THRESHOLDS = 10000;

template<typename K, typename V>
struct mypair {
    K first;
    V second;
};

inline uint32_t hash32_(uint32_t a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    if (a < 0) {
        a = -a;
    }
    return a;
}

inline uint64_t hash64_2(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

void scan_inplace__ (uint32_t* in, uint32_t n) {
    if (n <= THRESHOLDS) {
        // cout << "THRESHOLDS: " << THRESHOLDS << endl;
        for (size_t i = 1;i < n;i++) {
            in[i] += in[i-1];
        }
        return;
    }
    uint32_t root_n = (uint32_t)sqrt(n);// split the array into root n blocks
    uint32_t* offset = new uint32_t[root_n-1];   
    
    // parallel_for (0, root_n-1, [&] (size_t i) {
    for (size_t i = 0; i < root_n-1; i++) {
        offset[i] = 0;
        for (size_t j = i*root_n;j < (i+1)*root_n;j++) {
            offset[i] += in[j];
        }
    }

    for (size_t i = 1;i < root_n-1;i++) offset[i] += offset[i-1];

    // prefix sum for each subarray
    // parallel_for (0, root_n, [&] (size_t i) {
    for (size_t i = 0; i < root_n; i++) {
        if (i == root_n-1) {// the last one
            for (size_t j = i*root_n+1;j < n;j++) {
                in[j] += in[j-1];
            }
        } else {
            for (size_t j = i*root_n+1;j < (i+1)*root_n;j++) {
                in[j] += in[j-1];
            }
        }
    }

    // parallel_for (1, root_n, [&] (size_t i) {
    for (size_t i = 1; i < root_n; i++) {
        if (i == root_n-1)  {
            for (size_t j = i * root_n;j < n;j++){
                in[j] += offset[i-1];
            }
        } else {
            for (size_t j = i * root_n;j < (i+1)*root_n;j++) {
                in[j] +=  offset[i-1];
            }
        }
    }

    delete[] offset;
}

void uniform_generator_int64_ (std::vector<mypair<uint64_t, uint64_t>>& A, // mypair<uint64_t, uint64_t>* A, 
                               int n, uint64_t uniform_max_range) {
    for (uint32_t i = 0; i < n; i++) {
        // in order to put all keys in range [0, uniform_max_range]
        A[i].first = hash64_2(i) % uniform_max_range; 
        if (A[i].first > uniform_max_range) A[i].first -= uniform_max_range;
        if (A[i].first > uniform_max_range) cout << "wrong..." << endl;
        A[i].first = hash64_2(A[i].first);
        A[i].second = hash64_2(i);
    }
}

void zipfian_generator_int64_ (std::vector<mypair<uint64_t, uint64_t>>& A, // mypair<uint32_t, uint32_t> *A, 
                               int n, uint32_t zipf_s) {
    // pbbs::sequence<uint32_t> nums(zipf_s); // in total zipf_s kinds of keys
    uint32_t* nums = new uint32_t[zipf_s];

    /* 1. making nums[] array */
    uint32_t number = (uint32_t) (n / log(n)); // number= n/ln(n)
    for (uint32_t i = 0; i < zipf_s; i++) {
        nums[i] = (uint32_t) (number / (i+1));
    }

    // the last nums[zipf_s-1] should be (n - \sum{zipf_s-1}nums[])
    uint32_t offset = 0;
    for (uint32_t i = 0; i < zipf_s; i++) {
        offset += nums[i];
    }
    // uint32_t offset = pbbs::reduce(nums, pbbs::addm<uint32_t>()); // cout << "offset = " << offset << endl;
    nums[0] += (n - offset);

    uint32_t offset_ = 0;
    for (uint32_t i = 0; i < zipf_s; i++) {
        offset_ += nums[i];
    }
    // checking if the sum of nums[] equals to n
    if ((uint32_t)offset_ == (uint32_t)n) {
        cout << "sum of nums[] == n" << endl;
    }

    /* 2. scan to calculate position */ 
    uint32_t* addr = new uint32_t[zipf_s];
    // parallel_for (0, zipf_s, [&] (uint32_t i) {
    for (uint32_t i = 0; i < zipf_s; i++) {
        addr[i] = nums[i];
    }
    scan_inplace__(addr, zipf_s); // store all addresses into addr[]
    
    /* 3. distribute random numbers into A[i].first */
    for (size_t i = 0; i < zipf_s; i++) {
        size_t st = (i == 0) ? 0 : addr[i-1],
               ed = (i == zipf_s-1) ? n : addr[i];
        for (size_t j = st; j < ed; j++) {
            A[j].first = hash64_2(i);
        }
    }
    // parallel_for (0, n, [&] (uint32_t i){
    for (size_t i = 0; i < n; i++) {
        A[i].second = hash64_2(i);
    }

    /* 4. shuffle the keys */
    random_shuffle(A.begin(), A.end());

    delete[] nums;
    delete[] addr;
}


int main (int argc, char** argv) {
    if (argc < 4) {
        cout << "Usage: ..." << endl;
        cout << "./ips4o_example" <<  endl;
        cout << "[-n number of elements]" << endl;
        cout << "[-d distribution]" << endl;
        cout << "[uniform max key range / distribution parameter]" << endl;
        return 0;
    }

    int n = atoi(argv[1]);
    cout << "Number of keys: n = " << n << endl;
    cout << "Key type: int" << endl;
    cout << "Key range: (0, 2^64-1)" << endl;

    // mypair<uint64_t, uint64_t>* A1 = new mypair<uint64_t, uint64_t>[n];
    std::vector<mypair<uint64_t, uint64_t>> A(n);
    std::vector<mypair<uint64_t, uint64_t>> A1(n);

    // distributions
    if (strcmp(argv[2], "uniform") == 0) {
        uint64_t uniform_max_range = atoi(argv[3]);
        cout << "Uniform distribution (64bit key, 64bit value)..." << endl;
        cout << "Uniform parameter = " << uniform_max_range << endl;
        uniform_generator_int64_(A1, n, uniform_max_range);

        // for (size_t i = 0;i < n; i++) {
        //     A[i] = A1[i];
        // }
    // } else if (strcmp(argv[2], "exponential") == 0) {
    //     uint32_t exp_lambda = stod(argv[3]);
    //     cout << "Exponential distribution (32bit key, 32bit value)..." << endl;
    //     cout << "Exponential parameter = " << exp_lambda << endl;
    //     exponential_generator_int32_(A1, n, exp_lambda);

    //     for (size_t i = 0;i < n; i++) {
    //         A[i] = A1[i];
    //     }
    } else if (strcmp(argv[2], "zipfian") == 0) {
        uint32_t zipfian_s = stod(argv[3]);
        cout << "Zipfian distribution (64bit key, 64bit value)..." << endl;
        cout << "Zipfian parameter = " << zipfian_s << endl;
        zipfian_generator_int64_(A1, n, zipfian_s);

        // for (size_t i = 0;i < n; i++) {
        //     A[i] = A1[i];
        // }
    }

    // // just testing....
    // std::random_device r;
    // std::default_random_engine gen(r());
    // std::uniform_real_distribution<double> dist;
	//
    // std::vector<double> v(n);
    // for (auto& e : v) {
    //     e = dist(gen);
    // }
	//
    auto less_pair = [&] (mypair<uint64_t, uint64_t> a, mypair<uint64_t, uint64_t> b) {
        return a.first < b.first;
    };

    double t[NUM_ROUNDS];
    timer t_ips4o;
    for (size_t i = 0;i < NUM_ROUNDS;i++) {
		for (size_t i = 0;i < n; i++) {
            A[i] = A1[i];
        }
        t_ips4o.reset();
        t_ips4o.start();
        ips4o::parallel::sort(A.begin(), A.end(), less_pair);
        t_ips4o.stop();
        t[i] = t_ips4o.get_total();
        cout << "ips4o::sample sort costs: " << t[i] << endl;
    }
    
    std::sort(t+1, t+NUM_ROUNDS);
    uint32_t mid = NUM_ROUNDS/2 - 1;
    cout << "(~Median) ips4o::sample_sort() running time: " << t[mid] << endl;

    // delete[] A;
    // delete[] A1;
    return 0;
}


