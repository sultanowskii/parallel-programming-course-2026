CPP_DIR           = student-cpp

SOURCES           = $(CPP_DIR)/src/*.cpp
STRESS_SOURCE     = $(CPP_DIR)/extra/stress.cpp
MINISTRESS_SOURCE = $(CPP_DIR)/extra/ministress.cpp

LAB               = $(CPP_DIR)/lab2
LAB_TSAN          = $(CPP_DIR)/lab2_tsan
STRESS            = $(CPP_DIR)/stress
MINISTRESS        = $(CPP_DIR)/ministress

.PHONY: build
build:
	g++ -O2 -std=c++20 -pthread $(SOURCES) -o $(LAB)

.PHONY: stress
stress:
	g++ -O2 -std=c++20 -pthread $(STRESS_SOURCE) -o $(STRESS)

.PHONY: ministress
ministress:
	g++ -O2 -std=c++20 -pthread $(MINISTRESS_SOURCE) -o $(MINISTRESS)

.PHONY: tsan
tsan:
	clang++ -fsanitize=thread -g -O1 -std=c++20 -pthread $(SOURCES) -o $(LAB_TSAN)

.PHONY: stage0
stage0: stress
	./$(STRESS) 10000 none v1
	./$(STRESS) 10000 yield v1
	./$(STRESS) 10000 none v2
	./$(STRESS) 10000 none v1
	./$(STRESS) 10000 yield v1
	./$(STRESS) 10000 none v2
	./$(STRESS) 10000 none v1
	./$(STRESS) 10000 yield v1
	./$(STRESS) 10000 none v2

.PHONY: stage1
stage1: build
	./$(LAB) withdraw-v0 first log

.PHONY: stage2
stage2: build
	./$(LAB) withdraw-v1 first log
	./$(LAB) transfer first
	./$(LAB) transfer replay AB log

.PHONY: stage3
stage3: build
	./$(LAB) withdraw-v1 randoms 1000

.PHONY: stage4
stage4: build
	./$(LAB) withdraw-v1 dfs
	./$(LAB) withdraw-v2 dfs
	./$(LAB) withdraw-v0 dfs
	./$(LAB) transfer dfs

.PHONY: stage5
stage5: build
	./$(LAB) withdraw-v1 bounded 0
	./$(LAB) withdraw-v1 bounded 1
	./$(LAB) withdraw-v1 bounded 2
	./$(LAB) transfer bounded 0
	./$(LAB) transfer bounded 1

.PHONY: stage6
stage6: build
	./$(LAB) collector-nostep3-1x1 dfs
	./$(LAB) collector-nostep3-1x2 dfs
	./$(LAB) collector-nostep3-1x3 dfs
	./$(LAB) collector-nostep3-1x4 dfs
	./$(LAB) collector-nostep3-2x1 bounded 0
	./$(LAB) collector-nostep3-2x1 bounded 1
	./$(LAB) collector-nostep3-2x1 bounded 2
	./$(LAB) collector-nostep3-2x1 bounded 3
	./$(LAB) collector-plain-1x1 dfs
	./$(LAB) collector-acqrel-1x1 dfs
	./$(LAB) collector-ok-1x1 dfs

.PHONY: stage7
stage7: ministress
	./$(MINISTRESS) relaxed 2000 10000
	./$(MINISTRESS) acqrel 2000 10000
	./$(MINISTRESS) seqcst 2000 10000
	./$(MINISTRESS) naive 200000 1
	./$(MINISTRESS) acqrel 200000 100
	./$(MINISTRESS) acqrel 200 100000

.PHONY: check
check: build
	./$(LAB) withdraw-v0 first
	./$(LAB) withdraw-v1 first
	./$(LAB) transfer first
	./$(LAB) transfer replay AB
	./$(LAB) collector-ok-2x1 bounded 2

.PHONY: check-tsan
check-tsan: tsan
	./$(LAB_TSAN) withdraw-v1 first log
	./$(LAB_TSAN) transfer first
	./$(LAB_TSAN) transfer replay AB log
	./$(LAB_TSAN) withdraw-v1 dfs
	./$(LAB_TSAN) withdraw-v2 dfs
	./$(LAB_TSAN) withdraw-v0 dfs
	./$(LAB_TSAN) transfer dfs

.PHONY: replays
replays: build
	./$(LAB) withdraw-v1 replay AABBBBAA log
	./$(LAB) withdraw-v1 replay AABBAABB log
	./$(LAB) withdraw-v0 replay AABABB log
	./$(LAB) transfer replay AB log
	./$(LAB) collector-nostep3-1x1 replay ABBBAABAAB log
