#!/usr/bin/env python3
"""Verify the temporary HTTP texture-fetch diagnostic is present and safe.

Why this exists: on i-Grid Mom sees a cloud avatar, a missing skin and terrain
that does not look like sand, while other players see her avatar correctly. The
only log evidence was "DECODE FAILED: <uuid> Discard: -1, reason:" with an EMPTY
reason. An empty reason plus removeFromCache() indicates the decoder was handed
an empty or non-image buffer, but the response metadata that would prove it was
never recorded: callbackHttpGet silently discarded both the empty-body case and
the outright-failure case.

The server side (OpenSim/Capabilities/Handlers/GetTexture/GetTextureHandler.cs)
returns int_response_code 404, content_type text/plain and int_bytes 0 when the
asset is not found, which matches that signature exactly. But "not found" was a
hypothesis, not a measurement, so both failure shapes are now logged.

SAFETY REQUIREMENT (enforced below): the diagnostic must never print the URL, the
texture UUID, the region host, or any other identifying or credential-bearing
value. Response status, status type, content type, status message, fetch type and
requested discard level only. No secrets in logs.
"""

import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = (root / "lltexturefetch.cpp").read_text()


def callback_body():
    start = src.index("S32 LLTextureFetchWorker::callbackHttpGet(")
    opening = src.index("{", src.index(")", start))
    depth, end = 1, opening + 1
    while depth:
        depth += (src[end] == "{") - (src[end] == "}")
        end += 1
    return src[start:end]


body = callback_body()

# 1. The callback under test is the real one in this tree.
assert "response->getBody()" in body, "callbackHttpGet shape changed; re-verify diagnostic"

# 2. Both silent failure shapes are now recorded.
assert "empty HTTP body" in body, \
    "empty-body case still unlogged; that is the DECODE FAILED-with-empty-reason signature"
assert "HTTP request failed" in body, \
    "outright HTTP failure still silently discarded"

# 3. The response facts we need must actually be logged.
for needle, why in (
    ("response->getStatus().getStatus()", "HTTP status code is the decisive fact"),
    ("response->getContentType()", "content type distinguishes a real image from text/plain"),
):
    assert needle in body, f"{why}: {needle} missing from the diagnostic"

# 4. It must only fire on failure, never on the success path.
assert body.index("empty HTTP body") < body.index("if (data_size > 0)"), \
    "empty-body diagnostic must precede the success branch"
assert body.index("HTTP request failed") > body.index("mRequestedSize = -1; // error") - 400, \
    "failure diagnostic must sit in the else/failure branch"

# 5. SAFETY: no identifying values may be printed.
log_blocks = body.split('LL_WARNS("OpenSimTexFetch")')
assert len(log_blocks) == 3, "expected exactly two OpenSimTexFetch log statements"
for block in log_blocks[1:]:
    statement = block[: block.index("LL_ENDL")]
    for banned, why in (
        ("mUrl", "the request URL"),
        ("mID", "the texture UUID"),
        ("mTargetHost", "the region host"),
        ("getHost()", "the region host"),
        ("getCapabilities()", "capability URLs"),
    ):
        assert banned not in statement, \
            f"diagnostic must not log {why} ({banned}) in: {statement.strip()}"

# 6. The fetch type and discard level are safe and useful.
assert "mFTType" in body, "fetch type should be logged to distinguish default vs host-bake"
assert "mRequestedDiscard" in body, "discard level explains which resolution was requested"

# 7. This diagnostic must stay behind LL_WARNS so it survives the packaged INFO log level.
assert body.count("LL_WARNS(\"OpenSimTexFetch\")") == 2, \
    "both diagnostics must use LL_WARNS with the OpenSimTexFetch category"

print("PASS: HTTP texture-fetch diagnostic logs status/content-type/type/discard on "
      "empty-body and outright-failure only, uses LL_WARNS, and never logs URL, UUID "
      "or host")