"""Tests for the async token bucket rate limiter."""

import asyncio
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.common.rate_limiter import TokenBucketRateLimiter


class TestInitialTokens:
    """Fresh limiter has max_tokens available."""

    @pytest.mark.asyncio
    async def test_initial_tokens(self):
        limiter = TokenBucketRateLimiter(max_tokens=10, refill_interval=1.0)
        # Internal token count should equal max_tokens
        assert limiter._tokens == 10.0
        assert limiter.max_tokens == 10


class TestAcquireConsumesToken:
    """After acquire, tokens decrease."""

    @pytest.mark.asyncio
    async def test_acquire_consumes_token(self):
        limiter = TokenBucketRateLimiter(max_tokens=5, refill_interval=60.0)
        initial_tokens = limiter._tokens
        await limiter.acquire()
        # After one acquire, tokens should have decreased by ~1
        # (may have gained a tiny amount from refill due to elapsed time)
        assert limiter._tokens < initial_tokens
        assert limiter._tokens == pytest.approx(4.0, abs=0.1)


class TestRateLimitBlocks:
    """When tokens exhausted, acquire blocks."""

    @pytest.mark.asyncio
    async def test_rate_limit_blocks(self):
        # Create a limiter with only 1 token and a long refill interval
        limiter = TokenBucketRateLimiter(max_tokens=1, refill_interval=60.0)

        # First acquire should succeed immediately
        await limiter.acquire()

        # Second acquire should block because tokens are exhausted.
        # Use asyncio.wait_for with a short timeout to confirm it blocks.
        with pytest.raises(asyncio.TimeoutError):
            await asyncio.wait_for(limiter.acquire(), timeout=0.1)
