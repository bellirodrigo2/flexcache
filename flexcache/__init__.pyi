# flexcache/__init__.pyi

from datetime import datetime, timedelta
from typing import Any, Literal

class FlexCache:
    def __init__(
        self,
        eviction_policy: Literal['lru', 'fifo', 'random'] = 'lru',
        scan_interval: float = 0.0,
        max_items: int = 0,
        max_bytes: int = 0,
    ) -> None: ...
    
    @property
    def items(self) -> int: ...
    
    @property
    def bytes(self) -> int: ...
    
    def set(
        self,
        key: str,
        value: Any,
        ttl: timedelta | datetime | None = None,
    ) -> None: ...
    
    def get(self, key: str) -> Any | None: ...
    
    def delete(self, key: str) -> bool: ...
    
    def scan(self) -> None: ...
    
    def clear(self) -> None: ...