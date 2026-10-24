#pragma once

// Этап 1: планировщик, который ведёт настоящие потоки строго по одному (передача хода через семафоры).
// Этап 2: обнаружение deadlock в главном цикле и корректное завершение застрявших потоков.
// Внутреннее устройство (поля, семафоры, вспомогательные функции для shared и sched_mutex) придумываете сами.
#include <exception>
#include <functional>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include "strategy.hpp"

namespace sched {

class sched_mutex;

class Scheduler {
public:
    Scheduler();
    ~Scheduler();
    // Зарегистрировать поток. Потоки получают буквы A, B, C… в порядке вызова spawn.
    void spawn(std::function<void()> body);
    // Запустить потоки и вести их по одному: на каждой паузе спросить strategy.pick(enabled, last).
    void run(Strategy& strategy);
    // Трасса прогона: буквы потоков, выбранных во всех решениях подряд, например "AAAABB".
    std::string trace_string() const;

    // Если true, каждая операция с общими данными добавляет строку в log. Выставляет runner.
    bool logging = false;
    // Лог прогона: строки "<поток>  <операция>", например "A  lock mu", "A  balance -> 100".
    std::vector<std::string> log;
    // Пустая строка, если deadlock не было; иначе "held={a:A, b:B}" (этап 2).
    std::string deadlock;

    static Scheduler& current();
    int thread_id() const;
    void before_operation(std::function<bool()> can_resume = {});
    void after_operation(const std::string& operation);
    void register_mutex(sched_mutex* mutex);
    bool stopped() const;

private:
    struct Worker {
        std::function<void()> body;
        std::thread thread;
        // семафор: планировщик -> рабочий поток
        std::binary_semaphore turn{0};
        std::function<bool()> can_resume;
        std::exception_ptr error;
        bool done = false;
        bool needs_pause = true;
    };

    void start_workers();
    void run_worker(int id);
    void wait_for_pause(int id);
    bool all_done() const;
    std::vector<int> enabled_threads() const;
    void record_deadlock();
    void stop();
    void join();

    std::vector<std::unique_ptr<Worker>> workers_;
    std::vector<sched_mutex*> mutexes_;
    // семафор: рабочие потоки -> планировщик
    std::binary_semaphore returned_{0};
    std::string trace_;
    bool stopped_ = false;
    bool started_ = false;
};

// window() из тестовых программ: пауза перед следующей операцией с общими данными обязательна.
void point();
// Ждать, пока cond не станет true. До этого поток не попадает в enabled.
void await(std::function<bool()> cond);

}  // namespace sched
