"""
Benchmark tests for FlexCache (C) vs PurePythonLRUCache.

Run with:
    pytest benchmarks/test_benchmark.py -v --benchmark-only
    pytest benchmarks/test_benchmark.py -v -m "not benchmark"  # skip benchmarks
    pytest benchmarks/test_benchmark.py -v  # run all

Output options:
    --benchmark-json=output.json
    --benchmark-histogram=hist
    --benchmark-save=baseline
    --benchmark-compare
"""

from __future__ import annotations

import gc
from datetime import datetime, timedelta
from typing import TYPE_CHECKING

import pytest

from pure_python_cache import PurePythonLRUCache

if TYPE_CHECKING:
    from typing import Any, Callable

# Try to import C implementation
try:
    from flexcache import FlexCache

    HAS_C_IMPL = True
except ImportError:
    HAS_C_IMPL = False
    FlexCache = None


# =============================================================================
# Fixtures
# =============================================================================


@pytest.fixture
def c_cache():
    """Create C FlexCache instance."""
    if not HAS_C_IMPL:
        pytest.skip("C implementation not available")
    cache = FlexCache(eviction_policy="lru")
    yield cache
    cache.clear()


@pytest.fixture
def py_cache():
    """Create Python cache instance."""
    cache = PurePythonLRUCache()
    yield cache
    cache.clear()


@pytest.fixture(params=["c", "python"])
def cache_impl(request):
    """Parametrized fixture for both implementations."""
    if request.param == "c":
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")
        cache = FlexCache(eviction_policy="lru")
    else:
        cache = PurePythonLRUCache()

    yield request.param, cache
    cache.clear()


# =============================================================================
# Test parameters
# =============================================================================

N_ITEMS_SMALL = 100
N_ITEMS_MEDIUM = 1_000
N_ITEMS_LARGE = 10_000

ITEM_COUNTS = [N_ITEMS_SMALL, N_ITEMS_MEDIUM, N_ITEMS_LARGE]
ITEM_IDS = ["100", "1k", "10k"]


# =============================================================================
# Benchmark markers and utilities
# =============================================================================


def make_keys(n: int, prefix: str = "key") -> list[str]:
    """Generate list of keys."""
    return [f"{prefix}_{i}" for i in range(n)]


def prefill_cache(cache: Any, keys: list[str], value: Any = "value") -> None:
    """Fill cache with entries."""
    for key in keys:
        try:
            cache.set(key, value)
        except KeyError:
            pass  # Key exists


# =============================================================================
# Benchmark: SET operations
# =============================================================================


@pytest.mark.benchmark(group="set")
class TestBenchmarkSet:
    @pytest.mark.parametrize("n_items", ITEM_COUNTS, ids=ITEM_IDS)
    def test_set_c(self, benchmark, n_items):
        """Benchmark C implementation SET."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(n_items)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.set(key, "value")
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    @pytest.mark.parametrize("n_items", ITEM_COUNTS, ids=ITEM_IDS)
    def test_set_python(self, benchmark, n_items):
        """Benchmark Python implementation SET."""
        keys = make_keys(n_items)

        def setup():
            cache = PurePythonLRUCache()
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.set(key, "value")
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)


# =============================================================================
# Benchmark: GET operations
# =============================================================================


@pytest.mark.benchmark(group="get")
class TestBenchmarkGet:
    @pytest.mark.parametrize("n_items", ITEM_COUNTS, ids=ITEM_IDS)
    def test_get_c(self, benchmark, n_items):
        """Benchmark C implementation GET."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(n_items)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            prefill_cache(cache, keys)
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.get(key)

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    @pytest.mark.parametrize("n_items", ITEM_COUNTS, ids=ITEM_IDS)
    def test_get_python(self, benchmark, n_items):
        """Benchmark Python implementation GET."""
        keys = make_keys(n_items)

        def setup():
            cache = PurePythonLRUCache()
            prefill_cache(cache, keys)
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.get(key)

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)


# =============================================================================
# Benchmark: DELETE operations
# =============================================================================


@pytest.mark.benchmark(group="delete")
class TestBenchmarkDelete:
    @pytest.mark.parametrize("n_items", ITEM_COUNTS, ids=ITEM_IDS)
    def test_delete_c(self, benchmark, n_items):
        """Benchmark C implementation DELETE."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(n_items)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            prefill_cache(cache, keys)
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.delete(key)

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    @pytest.mark.parametrize("n_items", ITEM_COUNTS, ids=ITEM_IDS)
    def test_delete_python(self, benchmark, n_items):
        """Benchmark Python implementation DELETE."""
        keys = make_keys(n_items)

        def setup():
            cache = PurePythonLRUCache()
            prefill_cache(cache, keys)
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.delete(key)

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)


# =============================================================================
# Benchmark: TTL formats comparison
# =============================================================================


@pytest.mark.benchmark(group="ttl_format")
class TestBenchmarkTTLFormats:
    """Compare TTL parsing overhead for different formats."""

    N_ITEMS = 1000

    def test_ttl_float_c(self, benchmark):
        """C impl with float TTL (seconds)."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(self.N_ITEMS)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.set(key, "value", ttl=60.0)
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_float_python(self, benchmark):
        """Python impl with float TTL (seconds)."""
        keys = make_keys(self.N_ITEMS)

        def setup():
            cache = PurePythonLRUCache()
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.set(key, "value", ttl=60.0)
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_timedelta_c(self, benchmark):
        """C impl with timedelta TTL."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(self.N_ITEMS)
        ttl = timedelta(minutes=1)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            return (cache, keys, ttl), {}

        def run(cache, keys, ttl):
            for key in keys:
                cache.set(key, "value", ttl=ttl)
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_timedelta_python(self, benchmark):
        """Python impl with timedelta TTL."""
        keys = make_keys(self.N_ITEMS)
        ttl = timedelta(minutes=1)

        def setup():
            cache = PurePythonLRUCache()
            return (cache, keys, ttl), {}

        def run(cache, keys, ttl):
            for key in keys:
                cache.set(key, "value", ttl=ttl)
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_datetime_c(self, benchmark):
        """C impl with datetime TTL (absolute)."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(self.N_ITEMS)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            expires = datetime.now() + timedelta(hours=1)
            return (cache, keys, expires), {}

        def run(cache, keys, expires):
            for key in keys:
                cache.set(key, "value", ttl=expires)
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_datetime_python(self, benchmark):
        """Python impl with datetime TTL (absolute)."""
        keys = make_keys(self.N_ITEMS)

        def setup():
            cache = PurePythonLRUCache()
            expires = datetime.now() + timedelta(hours=1)
            return (cache, keys, expires), {}

        def run(cache, keys, expires):
            for key in keys:
                cache.set(key, "value", ttl=expires)
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_none_c(self, benchmark):
        """C impl without TTL (baseline)."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(self.N_ITEMS)

        def setup():
            cache = FlexCache(eviction_policy="lru")
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.set(key, "value")
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_ttl_none_python(self, benchmark):
        """Python impl without TTL (baseline)."""
        keys = make_keys(self.N_ITEMS)

        def setup():
            cache = PurePythonLRUCache()
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                cache.set(key, "value")
            cache.clear()

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)


# =============================================================================
# Benchmark: Mixed operations (realistic workload)
# =============================================================================


@pytest.mark.benchmark(group="mixed")
class TestBenchmarkMixed:
    """Simulate realistic cache workload: 70% get, 20% set, 10% delete."""

    @pytest.mark.parametrize("n_ops", [1000, 5000], ids=["1k_ops", "5k_ops"])
    def test_mixed_c(self, benchmark, n_ops):
        """C impl mixed operations."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        import random

        def setup():
            random.seed(42)
            cache = FlexCache(eviction_policy="lru", max_items=500)
            # Prefill
            for i in range(200):
                cache.set(f"init_{i}", "value")

            ops = []
            for i in range(n_ops):
                r = random.random()
                if r < 0.7:
                    ops.append(("get", f"init_{random.randint(0, 199)}"))
                elif r < 0.9:
                    ops.append(("set", f"new_{i}", "value"))
                else:
                    ops.append(("delete", f"init_{random.randint(0, 199)}"))

            return (cache, ops), {}

        def run(cache, ops):
            for op in ops:
                if op[0] == "get":
                    cache.get(op[1])
                elif op[0] == "set":
                    try:
                        cache.set(op[1], op[2])
                    except KeyError:
                        pass
                else:
                    cache.delete(op[1])

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    @pytest.mark.parametrize("n_ops", [1000, 5000], ids=["1k_ops", "5k_ops"])
    def test_mixed_python(self, benchmark, n_ops):
        """Python impl mixed operations."""
        import random

        def setup():
            random.seed(42)
            cache = PurePythonLRUCache(max_items=500)
            # Prefill
            for i in range(200):
                cache.set(f"init_{i}", "value")

            ops = []
            for i in range(n_ops):
                r = random.random()
                if r < 0.7:
                    ops.append(("get", f"init_{random.randint(0, 199)}"))
                elif r < 0.9:
                    ops.append(("set", f"new_{i}", "value"))
                else:
                    ops.append(("delete", f"init_{random.randint(0, 199)}"))

            return (cache, ops), {}

        def run(cache, ops):
            for op in ops:
                if op[0] == "get":
                    cache.get(op[1])
                elif op[0] == "set":
                    try:
                        cache.set(op[1], op[2])
                    except KeyError:
                        pass
                else:
                    cache.delete(op[1])

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)


# =============================================================================
# Benchmark: Eviction under pressure
# =============================================================================


@pytest.mark.benchmark(group="eviction")
class TestBenchmarkEviction:
    """Benchmark cache behavior under eviction pressure."""

    def test_eviction_c(self, benchmark):
        """C impl with constant eviction (max_items=100, insert 1000)."""
        if not HAS_C_IMPL:
            pytest.skip("C implementation not available")

        keys = make_keys(1000)

        def setup():
            cache = FlexCache(eviction_policy="lru", max_items=100)
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                try:
                    cache.set(key, "value")
                except KeyError:
                    pass

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)

    def test_eviction_python(self, benchmark):
        """Python impl with constant eviction (max_items=100, insert 1000)."""
        keys = make_keys(1000)

        def setup():
            cache = PurePythonLRUCache(max_items=100)
            return (cache, keys), {}

        def run(cache, keys):
            for key in keys:
                try:
                    cache.set(key, "value")
                except KeyError:
                    pass

        benchmark.pedantic(run, setup=setup, rounds=10, warmup_rounds=2)


# =============================================================================
# Non-benchmark functional tests (to verify correctness)
# =============================================================================


@pytest.mark.unit
class TestFunctional:
    """Basic functional tests to ensure implementations work correctly."""

    def test_set_get_delete(self, cache_impl):
        """Test basic operations."""
        name, cache = cache_impl

        cache.set("key1", "value1")
        assert cache.get("key1") == "value1"
        assert cache.items == 1

        assert cache.delete("key1") is True
        assert cache.get("key1") is None
        assert cache.items == 0

    def test_ttl_float(self, cache_impl):
        """Test float TTL."""
        name, cache = cache_impl
        cache.set("key", "value", ttl=3600.0)
        assert cache.get("key") == "value"

    def test_ttl_timedelta(self, cache_impl):
        """Test timedelta TTL."""
        name, cache = cache_impl
        cache.set("key", "value", ttl=timedelta(hours=1))
        assert cache.get("key") == "value"

    def test_ttl_datetime(self, cache_impl):
        """Test datetime TTL."""
        name, cache = cache_impl
        expires = datetime.now() + timedelta(hours=1)
        cache.set("key", "value", ttl=expires)
        assert cache.get("key") == "value"

    def test_duplicate_key_raises(self, cache_impl):
        """Test that duplicate key raises KeyError."""
        name, cache = cache_impl
        cache.set("key", "value")
        with pytest.raises(KeyError):
            cache.set("key", "value2")

    def test_max_items_eviction(self, cache_impl):
        """Test LRU eviction with max_items."""
        name, cache = cache_impl

        if name == "c":
            cache = FlexCache(eviction_policy="lru", max_items=3)
        else:
            cache = PurePythonLRUCache(max_items=3)

        cache.set("a", "1")
        cache.set("b", "2")
        cache.set("c", "3")
        assert cache.items == 3

        # Access 'a' to make it recently used
        cache.get("a")

        # Insert new item, should evict 'b' (LRU)
        cache.set("d", "4")
        assert cache.items == 3
        assert cache.get("b") is None  # evicted
        assert cache.get("a") == "1"  # still there
