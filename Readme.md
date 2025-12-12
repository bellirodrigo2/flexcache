# FlexCache

A high-performance, single-threaded in-memory cache for Python, implemented in C.

FlexCache provides TTL-based expiration and pluggable eviction policies (LRU, FIFO, Random) with minimal overhead. It's designed for scenarios where you need fast, predictable caching without the complexity of distributed systems.

## Features

- **Fast**: Core operations implemented in C with O(1) lookup via hash table
- **TTL Support**: Set expiration using `timedelta` (relative) or `datetime` (absolute)
- **Pluggable Eviction**: Choose between LRU, FIFO, or Random eviction policies
- **Size Limits**: Constrain cache by item count, custom unit count, or both
- **Resource Management**: Automatic cleanup callbacks for cached objects
- **Zero Dependencies**: Pure C extension with no external runtime dependencies

## Installation

```bash
pip install flexcache
```

### From source

```bash
git clone https://github.com/rodbell/flexcache.git
cd flexcache
pip install -e .
```

## Quick Start

```python
from datetime import timedelta
from flexcache import FlexCache

# Create a cache with LRU eviction, max 1000 items
cache = FlexCache(
    eviction_policy='lru',
    max_items=1000,
    max_bytes=10_000,  # 10,000 units (you define what a unit means)
)

# Store a value with 5-minute TTL
cache.set("user:123", {"name": "Alice", "role": "admin"}, ttl=timedelta(minutes=5))

# Retrieve it
user = cache.get("user:123")

# Delete explicitly
cache.delete("user:123")
```

## API Reference

### Constructor

```python
FlexCache(
    eviction_policy: Literal['lru', 'fifo', 'random'] = 'lru',
    scan_interval: float = 0.0,
    max_items: int = 0,
    max_bytes: int = 0,
)
```

| Parameter | Description |
|-----------|-------------|
| `eviction_policy` | Eviction strategy when limits are exceeded |
| `scan_interval` | Minimum seconds between automatic expiration scans (0 = every operation) |
| `max_items` | Maximum number of items (0 = unlimited) |
| `max_bytes` | Maximum total units as reported by `item_size()` (0 = unlimited) |

### Methods

#### `set(key: str, value: Any, ttl: timedelta | datetime | None = None) -> None`

Store a value in the cache.

```python
# Relative TTL
cache.set("key", "value", ttl=timedelta(hours=1))

# Absolute expiration
cache.set("key", "value", ttl=datetime(2025, 12, 31, 23, 59, 59))

# No expiration
cache.set("key", "value")
```

Raises `KeyError` if the key already exists. Delete first to update.

#### `get(key: str) -> Any | None`

Retrieve a value. Returns `None` if not found or expired.

```python
value = cache.get("key")
if value is not None:
    print(f"Found: {value}")
```

#### `delete(key: str) -> bool`

Remove an item. Returns `True` if deleted, `False` if not found.

```python
if cache.delete("key"):
    print("Deleted")
```

#### `scan() -> None`

Manually trigger expiration scan and eviction enforcement.

```python
cache.scan()
```

#### `clear() -> None`

Remove all items from the cache.

```python
cache.clear()
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `items` | `int` | Current number of cached items |
| `bytes` | `int` | Total units of all cached items (sum of `item_size()`) |

## Eviction Policies

### LRU (Least Recently Used)

Evicts items that haven't been accessed recently. Best for workloads with temporal locality.

```python
cache = FlexCache(eviction_policy='lru', max_items=100)
```

### FIFO (First In, First Out)

Evicts oldest items first, regardless of access patterns. Predictable and simple.

```python
cache = FlexCache(eviction_policy='fifo', max_items=100)
```

### Random

Evicts items randomly. Useful when access patterns are unpredictable.

```python
cache = FlexCache(eviction_policy='random', max_items=100)
```

## Custom Size Tracking

By default, each item counts as 1 unit. Implement the `item_size()` method to track custom units — whether that's bytes, credits, weight, or any metric that makes sense for your use case:

```python
class CachedImage:
    def __init__(self, data: bytes):
        self.data = data
    
    def item_size(self) -> int:
        return len(self.data)  # track actual bytes

cache = FlexCache(eviction_policy='lru', max_bytes=100 * 1024 * 1024)  # 100 MB
cache.set("img:1", CachedImage(image_bytes))
```

```python
class ApiQuota:
    def __init__(self, response: dict):
        self.response = response
        self.cost = response.get("api_cost", 1)
    
    def item_size(self) -> int:
        return self.cost  # track API credits consumed

cache = FlexCache(eviction_policy='lru', max_bytes=1000)  # max 1000 API credits
```

## Resource Cleanup

Implement the `close()` method for automatic cleanup when items are evicted or expired:

```python
class DatabaseConnection:
    def __init__(self, conn):
        self.conn = conn
    
    def close(self):
        self.conn.close()
        print("Connection closed")

cache = FlexCache(eviction_policy='lru', max_items=10)
cache.set("db:1", DatabaseConnection(conn))

# When evicted or expired, close() is called automatically
```

## Protocol Reference

```python
from typing import Protocol

class CacheEntry(Protocol):
    def item_size(self) -> int:
        """Return size in units for cache accounting."""
        ...
    
    def close(self) -> None:
        """Called when item is removed from cache."""
        ...
```

Both methods are optional. If not implemented, defaults are used (size=1, no cleanup).

## Performance

FlexCache is designed for single-threaded use cases. Typical performance characteristics:

| Operation | Complexity |
|-----------|------------|
| `get` | O(1) |
| `set` | O(1) amortized |
| `delete` | O(1) |
| `scan` | O(n) |

## Thread Safety

FlexCache is **not thread-safe**. For multi-threaded applications, use external synchronization:

```python
import threading

cache = FlexCache(eviction_policy='lru', max_items=1000)
lock = threading.Lock()

def cached_get(key):
    with lock:
        return cache.get(key)

def cached_set(key, value, ttl=None):
    with lock:
        cache.set(key, value, ttl=ttl)
```

## Examples

### HTTP Response Cache

```python
from datetime import timedelta
from flexcache import FlexCache

response_cache = FlexCache(
    eviction_policy='lru',
    max_items=10000,
)

def get_api_response(url: str) -> dict:
    cached = response_cache.get(url)
    if cached is not None:
        return cached
    
    response = requests.get(url).json()
    response_cache.set(url, response, ttl=timedelta(minutes=5))
    return response
```

### Session Store

```python
from datetime import timedelta
from flexcache import FlexCache

sessions = FlexCache(
    eviction_policy='lru',
    max_items=100000,
)

def create_session(session_id: str, user_data: dict):
    sessions.set(session_id, user_data, ttl=timedelta(hours=24))

def get_session(session_id: str) -> dict | None:
    return sessions.get(session_id)

def logout(session_id: str):
    sessions.delete(session_id)
```

### Connection Pool with Cleanup

```python
from flexcache import FlexCache

class PooledConnection:
    def __init__(self, host: str):
        self.conn = create_connection(host)
    
    def item_size(self) -> int:
        return 1  # each connection counts as 1
    
    def close(self):
        self.conn.disconnect()

pool = FlexCache(
    eviction_policy='lru',
    max_items=50,
)

def get_connection(host: str):
    conn = pool.get(host)
    if conn is None:
        conn = PooledConnection(host)
        pool.set(host, conn, ttl=timedelta(minutes=30))
    return conn
```

## License

MIT License. See [LICENSE](LICENSE) for details.

## Roadmap

- [ ] Background expiration scan in separate thread
- [ ] `aclose()` for async resource cleanup
- [ ] LFU (Least Frequently Used) eviction policy
- [ ] Bulk operations (`set_many`, `get_many`, `delete_many`)
- [ ] Cache statistics (hits, misses, evictions)
- [ ] Optional thread-safe mode
- [ ] Custom eviction policy hooks from Python
- [ ] Serialization hooks for persistence