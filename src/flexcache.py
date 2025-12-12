from datetime import datetime, timedelta
from typing import Any, Literal, Protocol

class FlexCache:
    
    def __init__(self, 
        eviction_policy: Literal['lru', 'fifo', 'random'],
        scan_interval: timedelta,
        max_items: int,
        max_bytes: int,
        ):
        pass
    
    @property
    def items(self) -> int:
        pass
    
    @property
    def bytes(self) -> int:
        pass

    def set(self, key:str, value:Any, ttl:timedelta | datetime):
        pass

    def get(self, key):
        pass
    
    def delete(self, key:str):
        pass

class CacheEntry(Protocol):
    
    def byteSize(self) -> int:
        "to be used in the c code on bytes_size callback"
        pass
        
    def close(self):
        "to be used in the c code on flexcache_ondelete_fn callback"
        pass