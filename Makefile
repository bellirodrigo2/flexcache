.PHONY: help build install all clean ctest test test-unit benchmark benchmark-console \
        benchmark-json benchmark-html benchmark-save benchmark-compare

# =============================================================================
# Help
# =============================================================================

help:
	@echo "FlexCache - Makefile targets"
	@echo ""
	@echo "Build & Install:"
	@echo "  all              Build, install and test everything"
	@echo "  install          Build extension and install in editable mode"
	@echo "  clean            Remove build artifacts"
	@echo ""
	@echo "Tests:"
	@echo "  ctest            Run C tests (src/)"
	@echo "  test             Run Python unit tests (tests/)"
	@echo "  test-unit        Run unit tests, skip benchmarks"
	@echo "  test-all         Run all tests including benchmarks"
	@echo ""
	@echo "Benchmarks:"
	@echo "  benchmark        Run benchmarks (console output)"
	@echo "  benchmark-json   Save JSON report to benchmark_results/"
	@echo "  benchmark-html   Generate histogram SVG"
	@echo "  benchmark-save   Save results as baseline"
	@echo "  benchmark-compare Compare against baseline (fails if >10% regression)"
	@echo "  benchmark-quick  Quick C vs Python comparison (1k items)"

# =============================================================================
# Core targets
# =============================================================================

all: ctest build install test

install:
	python setup.py build_ext --inplace --force
	pip install -e .

ctest:
	$(MAKE) -C src test

test:
	pytest tests/ -s

clean:
	rm -rf build *.so *.pyd *.c *.cpp
	rm -rf benchmark_results
	rm -rf .pytest_cache
	find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true

# =============================================================================
# Test variants
# =============================================================================

# Run only unit tests (skip benchmarks)
test-unit:
	pytest tests/ benchmarks/ -s -m "not benchmark" --benchmark-disable

# Run all tests including benchmarks
test-all:
	pytest tests/ benchmarks/ -s

# =============================================================================
# Benchmarks
# =============================================================================

BENCH_RESULTS := benchmark_results

$(BENCH_RESULTS):
	mkdir -p $(BENCH_RESULTS)

# Run benchmarks with console output
benchmark:
	pytest benchmarks/ -v -m "benchmark" --benchmark-only \
		--benchmark-columns=min,max,mean,stddev,median,rounds \
		--benchmark-sort=mean \
		--benchmark-group-by=group

benchmark-console: benchmark

# Save JSON report
benchmark-json: $(BENCH_RESULTS)
	pytest benchmarks/ -v -m "benchmark" --benchmark-only \
		--benchmark-autosave \
		--benchmark-storage=$(BENCH_RESULTS) \
		--benchmark-sort=mean \
		--benchmark-group-by=group

# Generate histogram
benchmark-html: $(BENCH_RESULTS)
	pytest benchmarks/ -v -m "benchmark" --benchmark-only \
		--benchmark-histogram=$(BENCH_RESULTS)/histogram \
		--benchmark-sort=mean \
		--benchmark-group-by=group

# Save baseline for comparison
benchmark-save: $(BENCH_RESULTS)
	pytest benchmarks/ -v -m "benchmark" --benchmark-only \
		--benchmark-save=baseline \
		--benchmark-storage=$(BENCH_RESULTS) \
		--benchmark-sort=mean

# Compare against baseline
benchmark-compare: $(BENCH_RESULTS)
	pytest benchmarks/ -v -m "benchmark" --benchmark-only \
		--benchmark-compare=baseline \
		--benchmark-storage=$(BENCH_RESULTS) \
		--benchmark-sort=mean \
		--benchmark-compare-fail=mean:10%

# Quick C vs Python comparison (1k items only)
benchmark-quick:
	pytest benchmarks/ -v -m "benchmark" --benchmark-only \
		-k "1k" \
		--benchmark-columns=min,mean,max \
		--benchmark-sort=name