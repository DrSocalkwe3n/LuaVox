# Resources and asset transfer

This document describes how binary resources are discovered, cached, and transferred from server to client.

## Resource types
- Binary resources are grouped by EnumAssets (nodestate, particle, animation, model, texture, sound, font).
- Each resource is addressed by a domain + key pair and also identified by a SHA-256 hash (Hash_t).
- The hash is the canonical payload identity: if the hash matches, the content is identical.

## High-level flow
1) Server tracks resource usage per client.
2) Server announces bindings (type + id + domain:key + hash) to the client.
3) Client checks local packs and cache; if missing, client requests by hash.
4) Server streams requested resources in chunks.
5) Client assembles the payload, stores it in cache, and marks the resource as loaded.

## Server side
- Resource usage counters are maintained in `RemoteClient::NetworkAndResource_t::ResUses`.
- When content references change, `incrementAssets` and `decrementAssets` update counters.
- `ResourceRequest` is built from per-client usage and sent to `GameServer::stepSyncContent`.
- `GameServer::stepSyncContent` resolves resource data via `AssetsManager` and calls `RemoteClient::informateAssets`.
- `RemoteClient::informateAssets`:
  - Sends bind notifications when a hash for an id changes or is new to the client.
  - Queues full resources only if the client explicitly requested the hash.
- Streaming happens in `RemoteClient::onUpdate`:
  - `InitResSend` announces the total size + hash + type/id + domain/key.
  - `ChunkSend` transmits raw payload slices until complete.

## Client side
- `ServerSession::rP_Resource` handles bind/lost/init/chunk packets.
- `Bind` and `Lost` update the in-memory bindings queue.
- `InitResSend` creates an `AssetLoading` entry keyed by hash.
- `ChunkSend` appends bytes to the loading entry; when finished, the asset is enqueued for cache write.
- The update loop (`ServerSession::update`) pulls assets from `AssetsManager`:
  - If cache miss: sends `ResourceRequest` with the hash.
  - If cache hit: directly marks the resource as loaded.

## Client cache (`AssetsManager`)
- Reads check, in order:
  1) Resource packs on disk (assets directories)
  2) Inline sqlite cache (small resources)
  3) File-based cache (large resources)
- Writes store small resources in sqlite and larger ones as files under `Cache/blobs/`.
- The cache also tracks last-used timestamps for eviction policies.

## Packet types (Resources)
- `Bind`: server -> client mapping of (type, id, domain, key, hash).
- `Lost`: server -> client removing mapping of (type, id).
- `InitResSend`: server -> client resource stream header.
- `ChunkSend`: server -> client resource stream payload.
- `ResourceRequest`: client -> server, list of hashes that are missing.

## Common failure modes
- Missing bind update: client will never request the hash.
- Missing cache entry or stale `AlreadyLoading`: client stops requesting resources it still needs.
- Interrupted streaming: asset stays in `AssetsLoading` without being finalized.

