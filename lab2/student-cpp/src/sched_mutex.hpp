#pragma once

// Этап 2: лок, о котором знает планировщик. Поток, ждущий занятый лок, не попадает в enabled;
// обычный std::mutex здесь нельзя — поток уснёт с ходом в руках и всё зависнет.
// Лог: "lock <имя>", "unlock <имя>". Планировщик должен знать все локи, чтобы печатать held={a:A, b:B}.
#include <string>

#include "scheduler.hpp"

namespace sched {

class sched_mutex {
public:
    // Создаётся в setup() тестовой программы; зарегистрируйте лок в планировщике s.
    sched_mutex(Scheduler& scheduler, std::string name)
        : name(name), scheduler_(scheduler) {
        scheduler_.register_mutex(this);
    }
    sched_mutex(const sched_mutex&) = delete;

    // Пауза; если лок занят — ждать, не участвуя в выборе; затем стать владельцем.
    void lock() {
        scheduler_.before_operation([this] { return owner_ == -1; });
        owner_ = scheduler_.thread_id();
        scheduler_.after_operation("lock " + name);
    }

    // Пауза; освободить лок, ждавшие потоки снова могут быть выбраны.
    void unlock() {
        if (scheduler_.stopped()) {
            return;
        }
        try {
            scheduler_.before_operation();
        } catch (...) {
            if (scheduler_.stopped() && std::uncaught_exceptions() > 0) {
                return;
            }
            throw;
        }
        owner_ = -1;
        scheduler_.after_operation("unlock " + name);
    }

    int owner() const {
        return owner_;
    }

    const std::string name;

private:
    Scheduler& scheduler_;
    int owner_ = -1;
};

// RAII: lock сейчас, unlock на закрывающей скобке. Выдан готовым.
class sched_guard {
public:
    explicit sched_guard(sched_mutex& mutex) : mutex_(mutex) {
        mutex_.lock();
    }

    ~sched_guard() noexcept(false) {
        mutex_.unlock();
    }

    sched_guard(const sched_guard&) = delete;

private:
    sched_mutex& mutex_;
};

}  // namespace sched
