# Replication & Sharding Design

This document outlines the planned replication and sharding strategy for the distributed cache.

Goals
- High availability and resilience to node failures
- Low-latency reads and writes
- Tunable consistency model (eventual by default; optional stronger modes)
- Easy scale-out via sharding

Sharding
- Use consistent hashing to map keys to shards (ring with virtual nodes).
- Allow configurable number of virtual nodes per host for better load distribution.

Replication
- Each shard will be replicated to `R` nodes (replication factor).
- Replication strategy: leader + followers (primary + replicas).
  - Writes go to the leader, which replicates to followers asynchronously (with ack options).
  - Reads can be served from leader or followers depending on client-configured consistency level.

Consistency modes
- Eventual (default): leader replicates asynchronously; reads from nearest replica.
- Strong (optional): leader waits for majority ack before returning success (quorum writes).

Failover & Recovery
- Leader election via simple lease + heartbeat or integrate a consensus layer (e.g., Raft) for stronger guarantees.
- Rebuild and catch-up using a WAL or snapshot-based sync.

Design trade-offs
- Choosing eventual consistency improves throughput but increases staleness risk.
- Strong consistency requires consensus (adds latency) but simplifies correctness.

Next steps
- Implement a lightweight replication interface and in-memory replication simulator for local testing.
- Add sharding layer and consistent hashing utilities.
- Add tests for failover, replication lag, and shard rebalancing.
