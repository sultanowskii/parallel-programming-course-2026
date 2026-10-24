#pragma once

// Стратегии выбора следующего потока.
// FirstAlive — этап 1, Replay — этап 2, RandomPick — этап 3, Dfs — этап 4 (ограничение bound — этап 5).
// enabled — номера потоков, которые могут сделать шаг, по возрастанию; last — прошлый выбранный (-1 в начале).
#include "strategy.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace sched {

// Всегда первый поток из enabled.
struct FirstAlive : Strategy {
    int pick(const std::vector<int>& enabled, int) override {
        return enabled.front();
    }
};

// Проигрывает трассу: на i-м решении выбирает поток trace[i] ('A' -> 0, 'B' -> 1).
struct Replay : Strategy {
    explicit Replay(std::string trace) : trace_(trace) {
    }

    // Сбросить позицию в трассе перед прогоном.
    void begin_run() override {
        position_ = 0;
    }

    int pick(const std::vector<int>& enabled, int) override {
        if (position_ == trace_.size()) {
            throw std::invalid_argument("replay: trace ended before the run");
        }

        int next = trace_[position_++] - 'A';
        if (std::find(enabled.begin(), enabled.end(), next) == enabled.end()) {
            throw std::invalid_argument("replay: thread is not enabled");
        }
        return next;
    }

private:
    std::string trace_;
    size_t position_ = 0;
};

// Случайный поток: enabled[rnd() % enabled.size()], rnd — std::mt19937(seed).
struct RandomPick : Strategy {
    explicit RandomPick(uint32_t seed) : random_(seed) {
    }

    int pick(const std::vector<int>& enabled, int) override {
        return enabled[random_() % enabled.size()];
    }

private:
    std::mt19937 random_;
};

// Stateless DFS по дереву выборов: каждый прогон с нуля, повторяет префикс прошлого и берёт следующий вариант.
// bound — максимум вытеснений (этап 5); для dfs runner передаёт INT_MAX.
struct Dfs : Strategy {
    explicit Dfs(int bound) : bound_(bound) {
        if (bound < 0) {
            throw std::invalid_argument("dfs: bound must be nonnegative");
        }
    }

    // Подготовиться к новому прогону (позиция в пути, счётчик вытеснений).
    void begin_run() override {
        position_ = 0;
        preemptions_ = 0;
    }

    int pick(const std::vector<int>& enabled, int last) override {
        bool last_enabled = std::find(enabled.begin(), enabled.end(), last) != enabled.end();

        if (position_ == path_.size()) {
            Branch branch;

            // заполняем список разрешенных переходов на этом уровне DFS
            // учитываем счетчик совершенных вытеснений
            for (int next : enabled) {
                bool is_preemption = last_enabled && next != last;
                if (!is_preemption || preemptions_ < bound_) {
                    branch.choices.push_back(next);
                }
            }
            path_.push_back(branch);
        }

        const Branch& branch = path_[position_++];
        int next = branch.choices[branch.choice];
        if (last_enabled && next != last) {
            preemptions_++;
        }

        return next;
    }

    // Выбрать следующий путь; false — дерево обойдено.
    bool end_run() override {
        path_.resize(position_);
        while (!path_.empty()) {
            Branch& branch = path_.back();
            if (branch.choice + 1 < branch.choices.size()) {
                branch.choice++;
                return true;
            }
            path_.pop_back();
        }
        return false;
    }

private:
    struct Branch {
        std::vector<int> choices;
        size_t choice = 0;
    };

    int bound_;
    // счетчик вытеснений
    int preemptions_ = 0;
    size_t position_ = 0;
    std::vector<Branch> path_;
};

}  // namespace sched
