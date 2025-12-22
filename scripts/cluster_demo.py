#!/usr/bin/env python3
"""Simple demo that starts two distributed_cache nodes and demonstrates MOVED redirection.

Requirements:
  pip install redis

Usage:
  python3 scripts/cluster_demo.py
"""

import subprocess
import time
import re
import signal
import sys

import redis

BINARY = "./cpp/build/distributed_cache"
NODE_A_PORT = 6384
NODE_B_PORT = 6385
NODE_A_ID = "nodeA"
NODE_B_ID = "nodeB"

# regex to parse a MOVED error: e.g. "-MOVED 1234 127.0.0.1:6385"
MOVED_RE = re.compile(r"MOVED\s+(\d+)\s+([0-9\.]+):(\d+)")

proc_a = None
proc_b = None

try:
    # Start node B first
    print("Starting Node B on port", NODE_B_PORT)
    proc_b = subprocess.Popen([BINARY, "tcp", "--port", str(NODE_B_PORT), "--node-id", NODE_B_ID, "--seed", f"127.0.0.1:{NODE_B_PORT}"])
    time.sleep(0.2)

    # Start node A and point it at node B as seed
    print("Starting Node A on port", NODE_A_PORT)
    proc_a = subprocess.Popen([BINARY, "tcp", "--port", str(NODE_A_PORT), "--node-id", NODE_A_ID, "--seed", f"127.0.0.1:{NODE_B_PORT}"])

    # allow servers to fully initialize
    time.sleep(0.5)

    client = redis.Redis(host='127.0.0.1', port=NODE_A_PORT, decode_responses=True)

    print("Sending SET requests to Node A until we get a MOVED response (or succeed locally)")
    target_key = None
    moved_info = None
    for i in range(1, 10000):
        key = f"demo_key_{i}"
        try:
            # Use low-level execute_command to surface raw MOVED errors as ResponseError
            res = client.execute_command('SET', key, 'v')
            print(f"SET {key} -> {res} (local)")
            if res == 'OK':
                # key stored locally; keep trying until we find a remote-owned key
                continue
        except redis.exceptions.ResponseError as e:
            m = MOVED_RE.search(str(e))
            if m:
                slot = m.group(1)
                host = m.group(2)
                port = int(m.group(3))
                print(f"Received MOVED for key {key}: slot={slot} -> {host}:{port}")
                target_key = key
                moved_info = (slot, host, port)
                break
            else:
                print("Received error", e)
                continue
    if not moved_info:
        print("Could not find a key that triggers MOVED after many tries")
        sys.exit(1)

    # Follow the redirection to the indicated node
    slot, host, port = moved_info
    print(f"Following redirection to {host}:{port} and re-issuing SET")
    client_remote = redis.Redis(host=host, port=port, decode_responses=True)
    r = client_remote.set(target_key, 'v')
    print("SET on remote returned:", r)

    # Verify by GET on the remote
    v = client_remote.get(target_key)
    print("GET on remote returned:", v)
    if v == 'v':
        print("Demo successful: key was redirected and stored on the owner node.")
    else:
        print("Demo failed: value mismatch")

finally:
    # Cleanup
    for p in (proc_a, proc_b):
        if p is None: continue
        try:
            p.terminate()
            p.wait(timeout=1)
        except Exception:
            try: p.kill()
            except Exception: pass
    print("Shut down demo nodes")
