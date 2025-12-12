from flexcache import FlexCache
from datetime import timedelta

cache = FlexCache(
    eviction_policy='lru',
    scan_interval=1.0,
    max_items=100,
    max_bytes=1024*1024
)

cache.set("foo", {"bar": 123}, ttl=timedelta(seconds=30))
value = cache.get("foo")
print(value)