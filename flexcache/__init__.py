# flexcache/__init__.py

from typing import Protocol
from flexcache._flexcache import FlexCache

class CacheEntry(Protocol):
    def item_size(self) -> int: ...
    def close(self) -> None: ...

__all__ = ["FlexCache", "CacheEntry"]
__version__ = "0.1.0"