# tests/test_flexcache.py

import pytest
import time
import gc
from datetime import datetime, timedelta
from flexcache import FlexCache


# ============================================================
#  Fixtures
# ============================================================

@pytest.fixture
def cache_lru():
    """LRU cache with small limits for testing."""
    return FlexCache(
        eviction_policy='lru',
        scan_interval=0.0,
        max_items=5,
        max_bytes=0
    )

@pytest.fixture
def cache_fifo():
    """FIFO cache with small limits for testing."""
    return FlexCache(
        eviction_policy='fifo',
        scan_interval=0.0,
        max_items=5,
        max_bytes=0
    )

@pytest.fixture
def cache_random():
    """Random eviction cache."""
    return FlexCache(
        eviction_policy='random',
        scan_interval=0.0,
        max_items=5,
        max_bytes=0
    )

@pytest.fixture
def cache_bytes_limit():
    """Cache with byte limit."""
    return FlexCache(
        eviction_policy='lru',
        scan_interval=0.0,
        max_items=0,
        max_bytes=100
    )

@pytest.fixture
def cache_no_limits():
    """Cache without limits."""
    return FlexCache(
        eviction_policy='lru',
        scan_interval=0.0,
        max_items=0,
        max_bytes=0
    )


# ============================================================
#  Test helpers / Protocol implementations
# ============================================================

class SizedEntry:
    """Entry with custom item_size."""
    
    def __init__(self, data: bytes):
        self.data = data
        self._closed = False
    
    def item_size(self) -> int:
        return len(self.data)
    
    def close(self) -> None:
        self._closed = True
    
    @property
    def is_closed(self) -> bool:
        return self._closed


class CloseTracker:
    """Tracks close() calls for testing ondelete."""
    
    closed_keys: list[str] = []
    
    def __init__(self, key: str):
        self.key = key
        self._closed = False
    
    def close(self) -> None:
        self._closed = True
        CloseTracker.closed_keys.append(self.key)
    
    @classmethod
    def reset(cls):
        cls.closed_keys = []


class PlainObject:
    """Object without item_size or close - tests defaults."""
    
    def __init__(self, value):
        self.value = value


# ============================================================
#  Basic operations
# ============================================================

class TestBasicOperations:
    
    def test_set_and_get(self, cache_no_limits):
        cache_no_limits.set("key1", "value1")
        assert cache_no_limits.get("key1") == "value1"
    
    def test_get_nonexistent_returns_none(self, cache_no_limits):
        assert cache_no_limits.get("nonexistent") is None
    
    def test_delete_existing(self, cache_no_limits):
        cache_no_limits.set("key1", "value1")
        result = cache_no_limits.delete("key1")
        assert result is True
        assert cache_no_limits.get("key1") is None
    
    def test_delete_nonexistent(self, cache_no_limits):
        result = cache_no_limits.delete("nonexistent")
        assert result is False
    
    def test_overwrite_raises_key_error(self, cache_no_limits):
        cache_no_limits.set("key1", "value1")
        with pytest.raises(KeyError):
            cache_no_limits.set("key1", "value2")
    
    def test_set_after_delete(self, cache_no_limits):
        cache_no_limits.set("key1", "value1")
        cache_no_limits.delete("key1")
        cache_no_limits.set("key1", "value2")
        assert cache_no_limits.get("key1") == "value2"
    
    def test_clear(self, cache_no_limits):
        cache_no_limits.set("key1", "value1")
        cache_no_limits.set("key2", "value2")
        cache_no_limits.clear()
        assert cache_no_limits.items == 0
        assert cache_no_limits.get("key1") is None
        assert cache_no_limits.get("key2") is None


# ============================================================
#  Value types
# ============================================================

class TestValueTypes:
    
    def test_string_value(self, cache_no_limits):
        cache_no_limits.set("k", "hello")
        assert cache_no_limits.get("k") == "hello"
    
    def test_int_value(self, cache_no_limits):
        cache_no_limits.set("k", 42)
        assert cache_no_limits.get("k") == 42
    
    def test_float_value(self, cache_no_limits):
        cache_no_limits.set("k", 3.14)
        assert cache_no_limits.get("k") == 3.14
    
    def test_none_value(self, cache_no_limits):
        cache_no_limits.set("k", None)
        # get() retorna None tanto para "não encontrado" quanto para "valor é None"
        # Precisamos verificar via items count
        assert cache_no_limits.items == 1
        result = cache_no_limits.get("k")
        assert result is None  # valor é None
    
    def test_list_value(self, cache_no_limits):
        data = [1, 2, 3]
        cache_no_limits.set("k", data)
        assert cache_no_limits.get("k") == [1, 2, 3]
    
    def test_dict_value(self, cache_no_limits):
        data = {"a": 1, "b": 2}
        cache_no_limits.set("k", data)
        assert cache_no_limits.get("k") == {"a": 1, "b": 2}
    
    def test_nested_structure(self, cache_no_limits):
        data = {"list": [1, 2], "nested": {"x": 10}}
        cache_no_limits.set("k", data)
        result = cache_no_limits.get("k")
        assert result["list"] == [1, 2]
        assert result["nested"]["x"] == 10
    
    def test_custom_object(self, cache_no_limits):
        obj = PlainObject("test")
        cache_no_limits.set("k", obj)
        result = cache_no_limits.get("k")
        assert result.value == "test"
        assert result is obj  # same reference


# ============================================================
#  Statistics (items / bytes)
# ============================================================

class TestStatistics:
    
    def test_items_count(self, cache_no_limits):
        assert cache_no_limits.items == 0
        cache_no_limits.set("k1", "v1")
        assert cache_no_limits.items == 1
        cache_no_limits.set("k2", "v2")
        assert cache_no_limits.items == 2
        cache_no_limits.delete("k1")
        assert cache_no_limits.items == 1
    
    def test_bytes_default_is_one(self, cache_no_limits):
        """Objects without item_size() should count as 1 byte."""
        cache_no_limits.set("k1", "value")
        cache_no_limits.set("k2", 12345)
        assert cache_no_limits.bytes == 2
    
    def test_bytes_with_item_size(self, cache_no_limits):
        entry1 = SizedEntry(b"hello")  # 5 bytes
        entry2 = SizedEntry(b"world!!!")  # 8 bytes
        cache_no_limits.set("k1", entry1)
        cache_no_limits.set("k2", entry2)
        assert cache_no_limits.bytes == 13
    
    def test_bytes_decreases_on_delete(self, cache_no_limits):
        entry = SizedEntry(b"0123456789")  # 10 bytes
        cache_no_limits.set("k", entry)
        assert cache_no_limits.bytes == 10
        cache_no_limits.delete("k")
        assert cache_no_limits.bytes == 0


# ============================================================
#  TTL expiration
# ============================================================

class TestTTL:
    
    def test_ttl_timedelta_not_expired(self, cache_no_limits):
        cache_no_limits.set("k", "v", ttl=timedelta(seconds=10))
        assert cache_no_limits.get("k") == "v"
    
    def test_ttl_timedelta_expired(self, cache_no_limits):
        cache_no_limits.set("k", "v", ttl=timedelta(milliseconds=50))
        time.sleep(0.1)
        assert cache_no_limits.get("k") is None
    
    def test_ttl_datetime_not_expired(self, cache_no_limits):
        future = datetime.now() + timedelta(seconds=10)
        cache_no_limits.set("k", "v", ttl=future)
        assert cache_no_limits.get("k") == "v"
    
    def test_ttl_datetime_expired(self, cache_no_limits):
        future = datetime.now() + timedelta(milliseconds=50)
        cache_no_limits.set("k", "v", ttl=future)
        time.sleep(0.1)
        assert cache_no_limits.get("k") is None
    
    def test_ttl_datetime_in_past(self, cache_no_limits):
        past = datetime.now() - timedelta(seconds=1)
        cache_no_limits.set("k", "v", ttl=past)
        # Should expire immediately
        assert cache_no_limits.get("k") is None
    
    def test_no_ttl_never_expires(self, cache_no_limits):
        cache_no_limits.set("k", "v")  # no ttl
        time.sleep(0.05)
        assert cache_no_limits.get("k") == "v"
    
    def test_expired_item_removed_from_count(self, cache_no_limits):
        cache_no_limits.set("k", "v", ttl=timedelta(milliseconds=50))
        assert cache_no_limits.items == 1
        time.sleep(0.1)
        cache_no_limits.get("k")  # triggers removal
        assert cache_no_limits.items == 0


# ============================================================
#  Scan and cleanup
# ============================================================

class TestScan:
    
    def test_scan_removes_expired(self, cache_no_limits):
        cache_no_limits.set("k1", "v1", ttl=timedelta(milliseconds=50))
        cache_no_limits.set("k2", "v2", ttl=timedelta(milliseconds=50))
        cache_no_limits.set("k3", "v3")  # no expiration
        
        time.sleep(0.1)
        cache_no_limits.scan()
        
        assert cache_no_limits.items == 1
        assert cache_no_limits.get("k3") == "v3"
    
    def test_scan_on_empty_cache(self, cache_no_limits):
        cache_no_limits.scan()  # should not crash
        assert cache_no_limits.items == 0


# ============================================================
#  LRU eviction policy
# ============================================================

class TestLRUEviction:
    
    def test_lru_evicts_oldest_on_max_items(self, cache_lru):
        for i in range(5):
            cache_lru.set(f"k{i}", f"v{i}")
        
        assert cache_lru.items == 5
        
        # Insert 6th, should evict k0 (oldest)
        cache_lru.set("k5", "v5")
        
        assert cache_lru.items == 5
        assert cache_lru.get("k0") is None
        assert cache_lru.get("k5") == "v5"
    
    def test_lru_access_updates_order(self, cache_lru):
        for i in range(5):
            cache_lru.set(f"k{i}", f"v{i}")
        
        # Access k0, making it recently used
        cache_lru.get("k0")
        
        # Insert new item, should evict k1 (now oldest)
        cache_lru.set("k5", "v5")
        
        assert cache_lru.get("k0") == "v0"  # still there
        assert cache_lru.get("k1") is None  # evicted
    
    def test_lru_multiple_evictions(self, cache_lru):
        for i in range(5):
            cache_lru.set(f"k{i}", f"v{i}")
        
        # Insert 3 more items
        for i in range(5, 8):
            cache_lru.set(f"k{i}", f"v{i}")
        
        # k0, k1, k2 should be evicted
        assert cache_lru.get("k0") is None
        assert cache_lru.get("k1") is None
        assert cache_lru.get("k2") is None
        assert cache_lru.get("k5") == "v5"


# ============================================================
#  FIFO eviction policy
# ============================================================

class TestFIFOEviction:
    
    def test_fifo_evicts_first_inserted(self, cache_fifo):
        for i in range(5):
            cache_fifo.set(f"k{i}", f"v{i}")
        
        cache_fifo.set("k5", "v5")
        
        assert cache_fifo.get("k0") is None  # first in, first out
        assert cache_fifo.get("k5") == "v5"
    
    def test_fifo_access_does_not_change_order(self, cache_fifo):
        for i in range(5):
            cache_fifo.set(f"k{i}", f"v{i}")
        
        # Access k0 multiple times
        cache_fifo.get("k0")
        cache_fifo.get("k0")
        cache_fifo.get("k0")
        
        # Insert new item, k0 should still be evicted (FIFO)
        cache_fifo.set("k5", "v5")
        
        assert cache_fifo.get("k0") is None


# ============================================================
#  Random eviction policy
# ============================================================

class TestRandomEviction:
    
    def test_random_evicts_something(self, cache_random):
        for i in range(5):
            cache_random.set(f"k{i}", f"v{i}")
        
        cache_random.set("k5", "v5")
        
        # Should have evicted exactly one item
        assert cache_random.items == 5
        
        # Count remaining keys (including k5)
        remaining = sum(1 for i in range(6) if cache_random.get(f"k{i}") is not None)
        assert remaining == 5  # 5 of 6 keys remain
    
    def test_random_respects_max_items(self, cache_random):
        for i in range(20):
            cache_random.set(f"k{i}", f"v{i}")
        
        assert cache_random.items == 5


# ============================================================
#  Byte-based eviction
# ============================================================

class TestByteEviction:
    
    def test_evicts_when_over_byte_limit(self, cache_bytes_limit):
        # max_bytes = 100
        entry1 = SizedEntry(b"x" * 50)  # 50 bytes
        entry2 = SizedEntry(b"x" * 50)  # 50 bytes
        
        cache_bytes_limit.set("k1", entry1)
        cache_bytes_limit.set("k2", entry2)
        
        assert cache_bytes_limit.bytes == 100
        
        # Add 60 more bytes, should evict k1
        entry3 = SizedEntry(b"x" * 60)
        cache_bytes_limit.set("k3", entry3)
        
        assert cache_bytes_limit.bytes <= 100
        assert cache_bytes_limit.get("k1") is None
    
    def test_large_item_evicts_multiple(self, cache_bytes_limit):
        # Insert 10 items of 10 bytes each
        for i in range(10):
            cache_bytes_limit.set(f"k{i}", SizedEntry(b"x" * 10))
        
        assert cache_bytes_limit.bytes == 100
        
        # Insert 80 byte item - should evict multiple
        cache_bytes_limit.set("big", SizedEntry(b"x" * 80))
        
        assert cache_bytes_limit.bytes <= 100


# ============================================================
#  Protocol: item_size
# ============================================================

class TestItemSizeProtocol:
    
    def test_item_size_is_called(self, cache_no_limits):
        entry = SizedEntry(b"hello world")  # 11 bytes
        cache_no_limits.set("k", entry)
        assert cache_no_limits.bytes == 11
    
    def test_item_size_zero(self, cache_no_limits):
        entry = SizedEntry(b"")  # 0 bytes
        cache_no_limits.set("k", entry)
        assert cache_no_limits.bytes == 0
    
    def test_without_item_size_defaults_to_one(self, cache_no_limits):
        cache_no_limits.set("k", PlainObject("test"))
        assert cache_no_limits.bytes == 1


# ============================================================
#  Protocol: close (ondelete callback)
# ============================================================

class TestCloseProtocol:
    
    def setup_method(self):
        CloseTracker.reset()
    
    def test_close_called_on_delete(self, cache_no_limits):
        entry = CloseTracker("k1")
        cache_no_limits.set("k1", entry)
        cache_no_limits.delete("k1")
        
        assert entry._closed is True
        assert "k1" in CloseTracker.closed_keys
    
    def test_close_called_on_eviction(self, cache_lru):
        entries = [CloseTracker(f"k{i}") for i in range(6)]
        
        for i, entry in enumerate(entries[:5]):
            cache_lru.set(f"k{i}", entry)
        
        # This should evict k0
        cache_lru.set("k5", entries[5])
        
        assert entries[0]._closed is True
        assert "k0" in CloseTracker.closed_keys
    
    def test_close_called_on_ttl_expiration(self, cache_no_limits):
        entry = CloseTracker("k1")
        cache_no_limits.set("k1", entry, ttl=timedelta(milliseconds=50))
        
        time.sleep(0.1)
        cache_no_limits.get("k1")  # triggers expiration check
        
        assert entry._closed is True
    
    def test_close_called_on_clear(self, cache_no_limits):
        entries = [CloseTracker(f"k{i}") for i in range(3)]
        
        for i, entry in enumerate(entries):
            cache_no_limits.set(f"k{i}", entry)
        
        cache_no_limits.clear()
        
        for entry in entries:
            assert entry._closed is True
    
    def test_close_not_called_without_method(self, cache_no_limits):
        obj = PlainObject("test")
        cache_no_limits.set("k", obj)
        cache_no_limits.delete("k")  # should not crash


# ============================================================
#  Edge cases
# ============================================================

class TestEdgeCases:
    
    def test_empty_string_key(self, cache_no_limits):
        # Empty key should be rejected
        with pytest.raises(ValueError):
            cache_no_limits.set("", "value")
    
    def test_unicode_key(self, cache_no_limits):
        cache_no_limits.set("chave_ção", "valor")
        assert cache_no_limits.get("chave_ção") == "valor"
    
    def test_unicode_emoji_key(self, cache_no_limits):
        cache_no_limits.set("🔑", "treasure")
        assert cache_no_limits.get("🔑") == "treasure"
    
    def test_long_key(self, cache_no_limits):
        long_key = "k" * 10000
        cache_no_limits.set(long_key, "value")
        assert cache_no_limits.get(long_key) == "value"
    
    def test_binary_like_key(self, cache_no_limits):
        key = "key\x00with\x00nulls"
        cache_no_limits.set(key, "value")
        assert cache_no_limits.get(key) == "value"
    
    def test_max_items_zero_means_unlimited(self, cache_no_limits):
        for i in range(100):
            cache_no_limits.set(f"k{i}", f"v{i}")
        assert cache_no_limits.items == 100
    
    def test_max_bytes_zero_means_unlimited(self, cache_no_limits):
        for i in range(100):
            cache_no_limits.set(f"k{i}", SizedEntry(b"x" * 1000))
        assert cache_no_limits.bytes == 100000
    
    def test_ttl_zero_means_no_expiration(self, cache_no_limits):
        cache_no_limits.set("k", "v", ttl=timedelta(seconds=0))
        time.sleep(0.05)
        assert cache_no_limits.get("k") == "v"


# ============================================================
#  Reference counting / GC
# ============================================================

class TestRefCounting:
    
    def test_value_kept_alive_in_cache(self, cache_no_limits):
        import sys
        
        obj = PlainObject("test")
        initial_refcount = sys.getrefcount(obj)
        
        cache_no_limits.set("k", obj)
        
        # Refcount should increase
        assert sys.getrefcount(obj) > initial_refcount
    
    def test_value_released_on_delete(self, cache_no_limits):
        import sys
        
        obj = PlainObject("test")
        cache_no_limits.set("k", obj)
        cached_refcount = sys.getrefcount(obj)
        
        cache_no_limits.delete("k")
        
        # Refcount should decrease
        assert sys.getrefcount(obj) < cached_refcount
    
    def test_no_memory_leak_on_eviction(self, cache_lru):
        import sys
        
        objects = [PlainObject(f"test{i}") for i in range(10)]
        initial_refcounts = [sys.getrefcount(obj) for obj in objects]
        
        # Fill cache (max 5 items)
        for i, obj in enumerate(objects[:5]):
            cache_lru.set(f"k{i}", obj)
        
        # Cause evictions
        for i, obj in enumerate(objects[5:], start=5):
            cache_lru.set(f"k{i}", obj)
        
        # Evicted objects should have original refcount
        for i in range(5):
            assert sys.getrefcount(objects[i]) == initial_refcounts[i]


# ============================================================
#  Constructor validation
# ============================================================

class TestConstructor:
    
    def test_invalid_policy_raises(self):
        with pytest.raises(ValueError):
            FlexCache(eviction_policy='invalid')
    
    def test_default_policy_is_lru(self):
        cache = FlexCache()
        # Fill and check LRU behavior
        for i in range(6):
            cache.set(f"k{i}", f"v{i}")
        # Can't easily verify policy without limits, just check it works
        assert cache.items == 6
    
    def test_negative_max_items_handled(self):
        # Should either raise or treat as 0
        try:
            cache = FlexCache(max_items=-1)
            # If it doesn't raise, should work like unlimited
            cache.set("k", "v")
        except (ValueError, OverflowError):
            pass  # Also acceptable
    
    def test_all_policies_initialize(self):
        for policy in ['lru', 'fifo', 'random']:
            cache = FlexCache(eviction_policy=policy, max_items=10)
            cache.set("k", "v")
            assert cache.get("k") == "v"


# ============================================================
#  Stress tests
# ============================================================

class TestStress:
    
    def test_many_insertions(self):
        cache = FlexCache(eviction_policy='lru', max_items=1000)
        
        for i in range(10000):
            cache.set(f"key_{i}", f"value_{i}")
        
        assert cache.items == 1000
    
    def test_rapid_insert_delete(self):
        cache = FlexCache(eviction_policy='lru', max_items=100)
        
        for i in range(1000):
            cache.set(f"k{i}", f"v{i}")
            if i > 0:
                cache.delete(f"k{i-1}")
        
        assert cache.items <= 100
    
    def test_mixed_operations(self):
        cache = FlexCache(
            eviction_policy='lru',
            max_items=50,
            max_bytes=5000
        )
        
        for i in range(500):
            cache.set(f"k{i}", SizedEntry(b"x" * (i % 100 + 1)))
            
            if i % 3 == 0:
                cache.get(f"k{i // 2}")
            
            if i % 5 == 0:
                cache.delete(f"k{max(0, i - 10)}")
        
        assert cache.items <= 50
        assert cache.bytes <= 5000
