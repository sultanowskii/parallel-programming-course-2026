// Этап 1–2: реализация Scheduler, point() и await().
// Сейчас все методы бросают исключение «TODO»: заполните их по заданию этапов 1 и 2.
// Деструктор должен дождаться (join) всех запущенных потоков, в том числе после deadlock.
#include "scheduler.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "sched_mutex.hpp"

namespace sched {
namespace {
thread_local Scheduler* current_scheduler = nullptr;
thread_local int current_id = -1;
struct Cancelled {};
}

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::spawn(std::function<void()> body) {
    if (started_) {
        throw std::logic_error("spawn after run");
    }
    auto worker = std::make_unique<Worker>();
    worker->body = body;
    workers_.push_back(std::move(worker));
}

Scheduler& Scheduler::current() {
    if (!current_scheduler) {
        throw std::logic_error("operation outside scheduler");
    }
    return *current_scheduler;
}

int Scheduler::thread_id() const {
    return current_id;
}

// перед операцией при необходимости отдаем ход
// если !can_resume(), поток недоступен для выбора
void Scheduler::before_operation(std::function<bool()> can_resume) {
    if (stopped_) {
        throw Cancelled{};
    }

    Worker& worker = *workers_[current_id];

    // делаем паузу после предыдущей операции или если продолжать пока нельзя
    if (worker.needs_pause || (can_resume && !can_resume())) {
        worker.can_resume = can_resume;
        worker.needs_pause = false;

        // отдаем управление планировщику и ждем разрешения продолжить
        returned_.release();
        worker.turn.acquire();

        if (stopped_) {
            throw Cancelled{};
        }

        worker.can_resume = {};
    }
}

// отмечаем выполненную операцию: следующей нужна пауза
void Scheduler::after_operation(const std::string& operation) {
    workers_[current_id]->needs_pause = true;
    if (logging) {
        log.push_back(std::string(1, char('A' + current_id)) + "  " + operation);
    }
}

void Scheduler::register_mutex(sched_mutex* mutex) {
    mutexes_.push_back(mutex);
}

bool Scheduler::stopped() const {
    return stopped_;
}

void Scheduler::join() {
    for (auto& worker : workers_) {
        if (worker->thread.joinable()) {
            worker->thread.join();
        }
    }
}

void Scheduler::stop() {
    stopped_ = true;
    for (auto& worker : workers_) {
        if (worker->thread.joinable() && !worker->done) {
            worker->turn.release();
        }
    }
    join();
}

void Scheduler::start_workers() {
    for (size_t i = 0; i < workers_.size(); i++) {
        int idx = int(i);

        workers_[i]->thread = std::thread([this, idx] {
            run_worker(idx);
        });

        // заодно сразу дожидаемся первой паузы, чтобы планировщик смог спокойно выбирать
        wait_for_pause(idx);
    }
}

void Scheduler::run_worker(int id) {
    current_scheduler = this;
    current_id = id;
    Worker& worker = *workers_[id];

    try {
        worker.body();
    } catch (const Cancelled&) {
        return;
    } catch (...) {
        if (stopped_) {
            return;
        }
        worker.error = std::current_exception();
    }

    worker.done = true;
    returned_.release();
}

void Scheduler::wait_for_pause(int id) {
    returned_.acquire();

    if (workers_[id]->error) {
        std::rethrow_exception(workers_[id]->error);
    }
}

bool Scheduler::all_done() const {
    for (const auto& worker : workers_) {
        if (!worker->done) {
            return false;
        }
    }

    return true;
}

std::vector<int> Scheduler::enabled_threads() const {
    std::vector<int> enabled;

    for (size_t i = 0; i < workers_.size(); i++) {
        const Worker& worker = *workers_[i];
        if (!worker.done && (!worker.can_resume || worker.can_resume())) {
            enabled.push_back(int(i));
        }
    }

    return enabled;
}

void Scheduler::record_deadlock() {
    deadlock = "held={";

    bool first = true;
    for (auto* mutex : mutexes_) {
        if (mutex->owner() >= 0) {
            if (!first) deadlock += ", ";
            deadlock += mutex->name + ":" + char('A' + mutex->owner());
            first = false;
        }
    }

    deadlock += "}";
}

void Scheduler::run(Strategy& strategy) {
    if (started_) {
        throw std::logic_error("scheduler already used");
    }

    started_ = true;
    try {
        // запускаем потоки и доводим каждый до первой паузы или завершения
        start_workers();
        int last = -1;

        while (!all_done()) {
            // выбираем только потоки, которые сейчас могут продолжить работу
            std::vector<int> enabled = enabled_threads();

            // ну и нужно проверить, что хоть один поток готов, если нет - мы повисли
            if (enabled.empty()) {
                record_deadlock();
                stop();
                return;
            }

            // стратегия выбирает следующий поток из доступных
            int next = strategy.pick(enabled, last);
            if (std::find(enabled.begin(), enabled.end(), next) == enabled.end()) {
                throw std::logic_error("strategy chose a disabled thread");
            }

            // запись выбора в трассу и сохранение
            trace_ += char('A' + next);
            last = next;

            // разрешаем потоку продолжить, потом ждем его паузы/завершения
            workers_[next]->turn.release();
            wait_for_pause(next);
        }

        join();
    } catch (...) {
        stop();
        throw;
    }
}

std::string Scheduler::trace_string() const {
    return trace_;
}

void point() {
    Scheduler::current().after_operation("window");
}

void await(std::function<bool()> cond) {
    Scheduler& scheduler = Scheduler::current();
    scheduler.before_operation(cond);
    scheduler.after_operation("await done");
}

}  // namespace sched
