# ClusterMap / Router Planning Document

Goal: Implement a ClusterMap (Router) that consults the existing ConsistentHash ring and enforces Redis-style redirection for keys not owned locally, enabling a safe Phase 2 rollout with minimal client impact.

## Summary
- Mechanism: Redis-style `-MOVED <host>:<port>` redirection on writes/reads for keys that are not locally owned.
- Router consults a canonical `ConsistentHash` instance (kept up-to-date by Membership/Discovery service).
- The Router will be used by `RespProtocol` (or an upstream Router middleware) to validate ownership before processing commands.
- Start with redirect-only behavior (clients get MOVED and re-issue requests); later we may add server-side proxying for convenience/perf.

## Components
1. MembershipService
   - Responsible for cluster membership / discovery (Gossip, static seed list v1, or both).
   - Maintains node list and notifies listeners of joins/leaves/health changes.
   - Persists node metadata (id, ip, port, weight).

2. ClusterMap (or Router)
   - Wraps a `ConsistentHash` instance and exposes these operations:
     - `bool is_local(const std::string &key)`
     - `std::string owner(const std::string &key)` -> returns node_id
     - `std::optional<std::pair<std::string, int>> owner_address(const std::string &key)` -> returns host+port (for MOVED reply)
     - `void update_nodes(const std::vector<NodeInfo>& nodes)` and listeners for changes
   - Holds a local node id for local checks.

3. Integration Points
   - RespProtocol: before processing a command involving a key, call Router::is_local(key).
     - If true -> process locally.
     - If false -> reply with `-MOVED <ip>:<port>` and do NOT process locally.
   - Reactor/TCPServer: unchanged (Router is called at the protocol level, not in Reactor).

4. Rebalancer / Migrations
   - Membership changes trigger Rebalancer to compute `compute_migrations_for_add/remove` and execute them.
   - Rebalancer will need access to Node's Replicator endpoints.

## RESP / Protocol Changes
- For any operation that references a key (`GET`, `SET`, `DEL`, etc.) the server must:
  1. Call Router::owner(key) and compare with local id.
  2. If owner != local, return `-MOVED <ip>:<port>\r\n` and stop processing.
- For multi-key commands (e.g., `MGET`, `MSET`), if any key is remote for the command, return `-MOVED` for the first remote key (or consider splitting the request into local and remote parts later).

## Testing Plan
- Unit tests for `ClusterMap` behavior using mocked MembershipService and a small `ConsistentHash`.
- Protocol-level tests:
  - Start a server with a ClusterMap configured so that key `k` maps to `node2`.
  - Send `SET k v` and assert the server responds with `-MOVED ip:port\r\n`.
  - Test pipelined requests mixing local and remote keys.
- End-to-end tests for join/leave triggering Rebalancer migrations and eventual ownership.

## Rollout Strategy
1. Implement ClusterMap & Router, defaulting to single-node behavior (empty or only-local node list) so no behavior change until membership is configured.
2. Add protocol-level redirection to `RespProtocol` behind a feature flag.
3. Add tests and run in CI.
4. Deploy to a staging cluster using static seed membership and verify redirects happen as expected.
5. Implement MembershipService (gossip) and hook Rebalancer to execute migrations on events.

## Edge Cases & Notes
- If Router cannot find owner (empty ring): behave as single-node (treat as local) to preserve backward compatibility.
- Avoid routing writes while Rebalancer is in-progress for a key migration if possible (or accept transient MOVED replies).
- For performance: caching owner lookup per-request is cheap; keep it in-memory. For large clusters we may want shard-local caches or consistent-hash with virtual nodes tuning.

## Next Files to Add / Modify
- Add: `cpp/include/sharder/router.h`, `cpp/src/sharder/router.cpp` (Router abstraction)
- Modify: `cpp/src/protocol/resp.cpp` to call Router::is_local and return `-MOVED host:port` when not local.
- Add: `cpp/include/discovery/membership.h` & `cpp/src/discovery/*` (Gossip or seed-list implementation)
- Tests: `cpp/tests/cluster_router_tests.cpp`, `cpp/tests/resp_redirection_tests.cpp`

---

If you approve this approach, I'll draft the concrete API signatures and the first PR (Router + minimal tests) after you confirm the redirection semantics (MOVED only vs proxying) and membership mechanism preference (static seeds first, gossip later).