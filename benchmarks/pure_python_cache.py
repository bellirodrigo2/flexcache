"""
Pure Python LRU Cache implementation for benchmark comparison.
Mirrors the C flexcache API for fair comparison.
"""

from __future__ import annotations

import time
from collections import OrderedDict
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Any, Callable


@dataclass
class CacheEntry:
    value: Any
    expires_at_ms: int  # 0 = no expiration
    byte_size: int


class PurePythonLRUCache:
    """
    Pure Python LRU cache with TTL support.
    API mirrors FlexCache for benchmark comparison.
    """

    def __init__(
        self,
        scan_interval: float = 0.0,
        max_items: int = 0,
        max_size: int = 0,
    ):
        self._data: OrderedDict[str, CacheEntry] = OrderedDict()
        self._max_items = max_items
        self._max_size = max_size
        self._total_size = 0
        self._scan_interval_ms = int(scan_interval * 1000)
        self._last_scan_ms = 0

    @staticmethod
    def _now_ms() -> int:
        return int(time.monotonic() * 1000)

    @staticmethod
    def _get_item_size(value: Any) -> int:
        if hasattr(value, "item_size"):
            return value.item_size()
        return 1

    def _parse_ttl(self, ttl: float | timedelta | datetime | None) -> int:
        """Returns absolute expiration timestamp in ms, or 0 for no expiration."""
        if ttl is None:
            return 0

        now_ms = self._now_ms()

        if isinstance(ttl, (int, float)):
            if ttl <= 0:
                return 0
            return now_ms + int(ttl * 1000)

        if isinstance(ttl, timedelta):
            total_sec = ttl.total_seconds()
            if total_sec <= 0:
                return 0
            return now_ms + int(total_sec * 1000)

        if isinstance(ttl, datetime):
            delta = (ttl - datetime.now()).total_seconds()
            if delta <= 0:
                return 1  # Already expired
            return now_ms + int(delta * 1000)

        raise TypeError("ttl must be None, float (seconds), timedelta, or datetime")

    def _is_expired(self, entry: CacheEntry) -> bool:
        if entry.expires_at_ms == 0:
            return False
        return self._now_ms() >= entry.expires_at_ms

    def _evict_lru(self) -> None:
        """Evict least recently used item."""
        if self._data:
            key, entry = self._data.popitem(last=False)
            self._total_size -= entry.byte_size
            self._call_close(entry.value)

    def _call_close(self, value: Any) -> None:
        if hasattr(value, "close"):
            try:
                value.close()
            except Exception:
                pass

    def _maybe_scan(self) -> None:
        if self._scan_interval_ms <= 0:
            return
        now = self._now_ms()
        if now - self._last_scan_ms >= self._scan_interval_ms:
            self.scan()
            self._last_scan_ms = now

    def set(
        self,
        key: str,
        value: Any,
        ttl: float | timedelta | datetime | None = None,
    ) -> None:
        if not key:
            raise ValueError("Key cannot be empty")

        if key in self._data:
            raise KeyError("Key already exists")

        expires_at = self._parse_ttl(ttl)
        byte_size = self._get_item_size(value)

        # Evict if needed
        while self._max_items > 0 and len(self._data) >= self._max_items:
            self._evict_lru()

        while self._max_size > 0 and self._total_size + byte_size > self._max_size:
            if not self._data:
                break
            self._evict_lru()

        entry = CacheEntry(value=value, expires_at_ms=expires_at, byte_size=byte_size)
        self._data[key] = entry
        self._total_size += byte_size

        self._maybe_scan()

    def get(self, key: str) -> Any | None:
        self._maybe_scan()

        entry = self._data.get(key)
        if entry is None:
            return None

        if self._is_expired(entry):
            self.delete(key)
            return None

        # Move to end (most recently used)
        self._data.move_to_end(key)
        return entry.value

    def delete(self, key: str) -> bool:
        entry = self._data.pop(key, None)
        if entry is None:
            return False

        self._total_size -= entry.byte_size
        self._call_close(entry.value)
        return True

    def scan(self) -> None:
        """Remove expired entries."""
        now = self._now_ms()
        expired = [k for k, v in self._data.items() if v.expires_at_ms and now >= v.expires_at_ms]
        for key in expired:
            self.delete(key)

    def clear(self) -> None:
        for entry in self._data.values():
            self._call_close(entry.value)
        self._data.clear()
        self._total_size = 0

    @property
    def items(self) -> int:
        return len(self._data)

    @property
    def size(self) -> int:
        return self._total_size
