#include "test_data.h"

#include <array>
#include <cmath>
#include <random>

const size_t VALUE_COUNT = 1 << 20;
const size_t MAX_VALUE = 1023;

size_t find_weight_index(const std::array<double, MAX_VALUE>& cumulative_weights, double target) {
    size_t left = 0;
    size_t right = cumulative_weights.size() - 1;

    while (left < right) {
        size_t middle = (left + right) / 2;
        if (target < cumulative_weights[middle]) {
            right = middle;
        } else {
            left = middle + 1;
        }
    }

    return left;
}

std::vector<uint64_t> generate_test_data(uint64_t seed) {
    const double exponent = 1.15;

    std::array<double, MAX_VALUE> cumulative_weights{};
    double total_weight = 0;
    for (size_t k = 1; k <= MAX_VALUE; k++) {
        total_weight += 1.0 / std::pow(k, exponent);
        cumulative_weights[k - 1] = total_weight;
    }

    std::mt19937_64 generator(seed);

    std::vector<uint64_t> values(VALUE_COUNT);
    for (size_t i = 0; i < VALUE_COUNT; i++) {
        // сжимаем до [0; 1]
        double random_fraction = static_cast<double>(generator()) / generator.max();
        // расжимаем до [0; total_weight]
        double target = random_fraction * total_weight;

        // +1, так как хотим значение (мс), а не индекс
        values[i] = find_weight_index(cumulative_weights, target) + 1;
    }

    return values;
}
