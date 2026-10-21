# MHP2G indexed vertex-upload compaction

## Problem and observed fix

Mountain area 4 exhausted the original 64 MiB vertex arena. Increasing the default
to 128 MiB restored that case, but a swamp area still required approximately
145.5 MiB. A 256 MiB override prevented missing draws while investigating.

Resource accounting located almost all arena use in CPU-decoded direct vertex
payloads. Uniforms, alignment and expanded UI sprites were insignificant in the
captured swamp scene. Interpolation was off. A command trace recorded no BBOX or
BJUMP commands, excluding missing bounding-box conditional rendering as the cause
of this captured workload. These are results for the captured scenes, not a claim
that these GE commands are implemented or unnecessary elsewhere.

The dense-compaction sample at frame 3240 showed:

| Measurement | Bytes | MiB |
|---|---:|---:|
| Original direct payload represented by those draws | 152,583,640 | 145.515 |
| Direct payload actually uploaded after compaction | 1,984,920 | 1.893 |
| Whole sampled arena segment including other allocations | 2,006,224 | 1.913 |

3,725 of 10,194 direct draws were compacted in that sample. Direct upload bytes
fell by 98.699%. No overflow messages appeared in the supplied summary. These are
sampled region-segment observations, not guaranteed session/whole-frame maxima.
This does not demonstrate a proportional process-RAM or frame-time reduction.

## Original processing

The GE receives a source vertex address, vertex format, index address and a
primitive. For an indexed primitive, GeState::draw_primitive reads the indices,
finds their lowest and highest values, and decodes that entire inclusive source
range. It subtracts the lowest index from each index to match the decoded array.
The renderer then expands primitive topology into triangle indices, converts each
decoded vertex to a 40-byte GpuVertex and uploads the whole array for that draw.

That lowest-to-highest cropping removes unused ends, but retains unused interior
vertices. A triangle referencing source indices [100, 2100, 3099] spans 3,000
vertices even though it references only three. After GE rebasing, its indices are
[0, 2000, 2999], and the old renderer uploads all 3,000 decoded vertices.
Many small indexed primitives into shared buffers can repeat this cost.
This is allocation/upload amplification, not evidence of a cross-frame leak.

The first renderer-side range-trimming experiment repeated the cropping already
performed by GeState and measured zero savings. The missing BBOX implementation
was another investigation lead, not the demonstrated fix.

## Dense upload algorithm

For each eligible CPU-decoded direct draw, the renderer traverses its actual
triangle-index list. A reusable lookup array maps each referenced decoded vertex
index to a dense index on first use. A reusable sources vector records the
original decoded index corresponding to each dense index. Later references to
the same index reuse the same dense index.

For the example above, sources become [0, 2000, 2999], the triangle indices become
[0, 1, 2], and only those three full vertices are converted/uploaded. Merged draw
groups rebase these dense indices into their combined vertex run and use the
actual uploaded count for the 16-bit group-size limit. The unmerged path writes
the same dense indices beside the compacted vertices.

For every triangle corner:

    sources[dense_index] == original_decoded_index

The operation preserves positions, normals, UVs, colours, triangle order/winding
and repeated/degenerate triangles. It merges references to the same source index
within a draw; it does not merge distinct indices merely because their vertex
attributes happen to match. It neither discards visible objects nor changes
quest/monster data, texture resolution, camera matrices or HUD layout.

## What it does not optimize yet

GeState still decodes the entire lowest-to-highest range into its CPU vector.
The optimization saves conversion/upload work and GPU-arena space after decoding;
it does not remove that earlier decoding cost. It also does not share uploads
between draws or cache geometry across frames. Scratch mapping adds CPU work,
so performance gains must be measured rather than inferred from bytes saved.

The retained mapping is at most 65,536 entries for the 16-bit triangle indices;
sources hold at most 65,536 entries. Invalid input falls back to the original
upload path. Buffers are reused for later draws, not treated as a persistent
vertex-data cache, so this creates no cache invalidation requirement for guest
memory changes.

## Defaults and current compatibility boundary

Dense compaction is enabled by default for CPU-decoded direct draws when frame
interpolation is OFF. MHP2G_COMPACT_DIRECT_VERTICES=0 disables it for comparisons.
MHP2G_VERTEX_UPLOAD_TRACE=1 enables diagnostic accounting independently.

Raw/GPU-decoded draws and expanded UI primitives keep their original paths.
Frames recorded for interpolation bypass compaction: replay and skinned-vertex
matching need their own validation before changing vertex identity/order there.
The stock capture used interpolation=0 and raw_draws=0; it establishes no result
for these bypass paths. FUComplete requires its own visual/runtime validation.

The global arena default therefore remains 128 MiB per region in this change.
A 64 MiB override is suitable for validating the tested 30 FPS CPU path, but is
not yet a justified global default. Returning globally to 64 MiB must also account
for interpolation and raw decoding, not just this compacted swamp sample. The
existing total storage-buffer-range compatibility guard remains intact.

Changing used bytes does not shrink the allocated arena. Shared-buffer allocation
is three regions plus two 6 MiB scratch regions: 64 MiB -> 204 MiB total;
128 MiB -> 396 MiB; 256 MiB -> 780 MiB. These are allocated buffer sizes, not total
process memory or guaranteed physical residency.

## Validation before release

The helper passed 10,000 deterministic randomized sparse triangle-index cases,
full 16-bit coverage, degenerate references, invalid-input fallback, simulated
merge rebasing and counters. UndefinedBehaviorSanitizer helper checks passed.
These tests establish index/source equivalence, not full shader/rendering or
frame-interpolation compatibility.

Validate default-on behavior with the explicit compaction flag unset. Test the
64 MiB override at 30 FPS in Mountain area 4 and the offending swamp area, then
normal hunting, camera rotation, shadows, status effects, equipment previews,
chest menus, HUD scaling and texture replacements. Validate on macOS and Steam
Deck and separately in FUComplete. Check logs for both vertex and index arena
failures. Test the normal 60 FPS configuration with its original capacity too;
compaction may be bypassed there and must not be claimed as fixed by this change.

Before lowering the global default or committing the final release change,
consolidate the experimental plumbing and remove unrelated BBOX/quest/HP probes
from the feature commit. Preserve upstream attribution and unrelated changes.
