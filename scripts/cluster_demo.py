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
import socket

BINARY = "./cpp/build/distributed_cache"
NODE_A_PORT = 6384
NODE_B_PORT = 6385
NODE_A_ID = "nodeA"
NODE_B_ID = "nodeB"

# regex to parse a MOVED error: e.g. "-MOVED 1234 127.0.0.1:6385"
MOVED_RE = re.compile(r"MOVED\s+(\d+)\s+([0-9\.]+):(\d+)")


def wait_for_port(host, port, timeout=3.0):
    """Wait until a TCP port is accepting connections or timeout."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except Exception:
            time.sleep(0.05)
    return False

proc_a = None
proc_b = None

try:
    # Start node B first
    print("Starting Node B on port", NODE_B_PORT)
    # Start node B and capture logs
    proc_b = subprocess.Popen([BINARY, "tcp", "--port", str(NODE_B_PORT), "--node-id", NODE_B_ID, "--seed", f"127.0.0.1:{NODE_B_PORT}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    time.sleep(0.2)

    # Start node A and capture logs
    print("Starting Node A on port", NODE_A_PORT)
    proc_a = subprocess.Popen([BINARY, "tcp", "--port", str(NODE_A_PORT), "--node-id", NODE_A_ID, "--seed", f"127.0.0.1:{NODE_B_PORT}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # start threads to stream logs from both servers
    import threading
    ready_event = threading.Event()
    def stream_out(name, pipe, ready_event=None):
        for line in iter(pipe.readline, ''):
            sline = line.rstrip()
            print(f"[{name}] {sline}")
            if ready_event is not None and "[server] READY AND ROUTING" in sline:
                ready_event.set()
    t1 = threading.Thread(target=stream_out, args=("nodeA", proc_a.stdout, None), daemon=True)
    t2 = threading.Thread(target=stream_out, args=("nodeA-err", proc_a.stderr, ready_event), daemon=True)
    t3 = threading.Thread(target=stream_out, args=("nodeB", proc_b.stdout, None), daemon=True)
    t4 = threading.Thread(target=stream_out, args=("nodeB-err", proc_b.stderr, None), daemon=True)
    t1.start(); t2.start(); t3.start(); t4.start();

    # allow servers to fully initialize
    time.sleep(0.5)

    # Give Node A time to accept connections
    if not wait_for_port('127.0.0.1', NODE_A_PORT, timeout=3.0):
        print(f"Node A not listening on port {NODE_A_PORT}")
        sys.exit(1)

    # Use raw TCP RESP sends with socket timeouts to avoid blocking in client libs
    def send_recv_raw(host, port, payload, connect_timeout=0.2, read_timeout=0.1):
        try:
            with socket.create_connection((host, port), timeout=connect_timeout) as s:
                s.settimeout(read_timeout)
                s.sendall(payload.encode('utf-8'))
                # Read until CRLF or until read_timeout elapses
                data = b""
                deadline = time.monotonic() + read_timeout
                while time.monotonic() < deadline:
                    try:
                        chunk = s.recv(4096)
                        if not chunk:
                            break
                        data += chunk
                        if b'\r\n' in data:
                            break
                    except socket.timeout:
                        break
                return True, data.decode('utf-8', errors='ignore')
        except Exception:
            return False, None

    # Deterministic approach: ask the local binary to find a key that maps to the remote node.
    # This uses the same in-process hashing and avoids network scanning.
    print("Requesting a deterministic key that maps to the owner node via the helper CLI")
    helper_cmd = [BINARY, "--find-key-for-node", f"127.0.0.1:{NODE_B_PORT}", "--max-tries", "10000", "--seed", f"127.0.0.1:{NODE_B_PORT}", "--node-id", NODE_A_ID, "--port", str(NODE_A_PORT)]
    try:
        proc = subprocess.run(helper_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=2.0)
        if proc.returncode != 0 or not proc.stdout:
            print("Helper failed to produce a key:", proc.stderr.strip())
            sys.exit(2)
        target_key = proc.stdout.strip().splitlines()[0]
        print(f"Deterministic target key: {target_key}")
        # Ensure Node A is still running before issuing the SET
        if proc_a.poll() is not None:
            print(f"Node A has exited unexpectedly with code {proc_a.returncode}")
            try:
                print('nodeA stderr:', proc_a.stderr.read())
            except Exception:
                pass
            sys.exit(5)

        # Wait for server readiness marker from Node A stderr
        print('Waiting up to 5s for Node A READY marker...')
        if not ready_event.wait(timeout=5.0):
            print('Node A did not signal READY; aborting')
            sys.exit(6)

        # As a quick health check, PING Node A first to ensure it responds
        pingreq = "*1\r\n$4\r\nPING\r\n"
        ok, pong = send_recv_raw('127.0.0.1', NODE_A_PORT, pingreq, connect_timeout=0.5, read_timeout=0.5)
        print('PING response:', pong)
        if not ok or not pong or '+PONG' not in pong:
            print('Node A did not reply to PING; aborting')
            sys.exit(7)

        # Issue SET to Node A and expect a MOVED (no retry loops)
        setreq = f"*3\r\n$3\r\nSET\r\n${len(target_key)}\r\n{target_key}\r\n$1\r\nv\r\n"
        success, resp = send_recv_raw('127.0.0.1', NODE_A_PORT, setreq, connect_timeout=0.5, read_timeout=0.5)
        if not success or not resp:
            print("No response from Node A when setting deterministic key (no retries)")
            try:
                import select
                fd = proc_a.stderr.fileno()
                r,_,_ = select.select([fd], [], [], 0.1)
                if r:
                    partial = proc_a.stderr.read(4096)
                    print('nodeA stderr (partial):', partial)
                else:
                    print('nodeA stderr: <no data available>')
            except Exception as e:
                print('nodeA stderr read failed:', e)
            sys.exit(3)
        print("Response from Node A:", resp.strip())
        m = MOVED_RE.search(resp)
        if not m:
            print("Expected MOVED response but did not get one; response:", resp)
            sys.exit(4)
        slot = m.group(1); host = m.group(2); port = int(m.group(3))
        print(f"Received MOVED for {target_key}: slot={slot} -> {host}:{port}")
        moved_info = (slot, host, port)
    except subprocess.TimeoutExpired:
        print("Helper timed out")
        sys.exit(2)
    except Exception as e:
        print("Helper invocation failed:", e)
        sys.exit(2)

    # Follow the redirection to the indicated node
    slot, host, port = moved_info
    print(f"Following redirection to {host}:{port} and re-issuing SET")

    # Wait for remote node to be listening
    if not wait_for_port(host, port, timeout=3.0):
        print(f"Remote node {host}:{port} is not accepting connections")
        sys.exit(1)

    client_remote = redis.Redis(host=host, port=port, decode_responses=True,
                                 socket_connect_timeout=0.5, socket_timeout=0.5)

    try:
        r = client_remote.set(target_key, 'v')
        print("SET on remote returned:", r)

        # Verify by GET on the remote
        v = client_remote.get(target_key)
        print("GET on remote returned:", v)
        if v == 'v':
            print("Demo successful: key was redirected and stored on the owner node.")
        else:
            print("Demo failed: value mismatch")
    except Exception as e:
        print("Failed to contact remote owner node:", e)
        sys.exit(1)

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
