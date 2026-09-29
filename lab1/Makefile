.PHONY: benchmark
benchmark:
	g++ -O2 -std=c++20 -pthread src/*.cpp src/collectors/*.cpp -o bench

.PHONY: tsan
tsan:
	clang++ -fsanitize=thread -g -O1 -std=c++20 -pthread src/*.cpp src/collectors/*.cpp -o bench_tsan
