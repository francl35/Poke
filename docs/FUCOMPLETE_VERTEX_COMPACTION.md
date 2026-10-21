# FUComplete renderer vertex compaction

## Scope

This ports the validated stock-Poke dense CPU vertex upload algorithm to the
shared profiles/mhp2g renderer used by FUComplete. It does not add/change a game
profile, executable hash, generated game corpus, overlay address, quest layout,
save encryption, FUCDAT handling, texture-pack path, user-data directory or launch
script. The actual FUComplete build/rendering must still be tested locally.

Large indexed ranges previously produced roughly 145.5 MiB of vertex uploads in
a stock swamp scene. Dense compaction reduced the sampled 30 FPS arena usage to
about 1.91 MiB. The stock rigid-replay extension also showed compacted draws while
interpolation was active, with actual blended presentations and no overflow
messages in its supplied summary. The user observed a 7.6 MiB overlay peak in
that 60 FPS run; sparse log samples peaked lower. These measurements do not prove
maximum usage across every quest/platform or FUComplete.

## Algorithm

GeState reads a source vertex/index buffer, trims to the lowest/highest index,
and decodes every vertex in that range. A triangle indexing 100, 2100 and 3099
therefore retains 3000 decoded vertices, including interior vertices it never
references. The renderer used to convert/upload that whole range for each draw.

The compactor builds a reusable original-index to dense-index mapping and a
first-use ordered list of referenced decoded vertices. It converts/uploads only
those complete vertices and remaps the triangle indices to the dense run. Every
triangle corner still selects exactly its original source vertex. Distinct
source indices are not merged because their attributes happen to match. Triangle
order/winding, normals, UVs, colours and repeated/degenerate references remain.
Merged and unmerged draws use the actual uploaded count and correct index bases.
No visibility/occlusion rejection is introduced.

The decoder still processes the original range: CPU decoding of unused interior
vertices is not yet optimized. There is no geometry cache or upload reuse across
draws/frames. Mapping adds CPU work; a reduction in bytes is not a proportional
frame-time or total process-memory guarantee. The scratch index map is bounded
to 65536 entries, matching the renderer's 16-bit indices. Invalid references fall
back to the previous upload path.

## Eligibility

Compaction is enabled by default for CPU-decoded direct draws. For interpolation,
it is allowed only when the renderer's `skinned` flag is false: rigid replay keeps
its own recorded vertices/indices and interpolates transforms, not per-vertex
array positions. Weighted/skinned CPU replay keeps original vertex ordering and
arrays so its existing older[v]/newer[v] interpolation is untouched. Raw/GPU
vertex decoding and expanded sprite draws keep their original paths.

MHP2G_COMPACT_DIRECT_VERTICES=0 disables compaction for comparisons.
MHP2G_VERTEX_UPLOAD_TRACE=1 enables sampled payload counters. The fresh standalone
port logs source_direct_bytes, uploaded_direct_bytes, expanded_bytes, raw_bytes,
compacted_draws, observed_draws, compact_enabled and interpolation. An upgraded
experimental implementation may expose additional diagnostic counters.

Counters cover successful draws in the arena segment being reset, sampled every
120 renderer frames. A framebuffer readback can split a frame into segments.
They omit uniforms/alignment/inline indices and are not guaranteed frame/session
maxima. Existing performance memory counters provide broader usage and failures.

## Buffer capacity and validation

The port preserves the checkout's existing default capacity and supported
environment overrides. It does not raise or lower the default. Keep 256 MiB for
an initial FUComplete comparison in the affected swamp scene if needed, then
validate the 64 MiB override at 30 and 60 FPS before considering a lower global
default. 64/128/256 MiB per region mean 204/396/780 MiB total for this shared buffer
including its three regions and scratch regions, not total process memory.
A lower used counter alone does not shrink the allocated buffer.

Use the existing FUComplete launcher, its normal game folder and its own
user-data/save configuration. Validate terrain and shadows while rotating the
camera, player/monster animations, combat/effects, Mountain area 4, the affected
swamp area, equipment previews/chest menus, HUD scaling, FOV and texture packs.
Repeat on macOS and Steam Deck and confirm real interpolated presentations at
60 FPS. Check both vertex and index allocation failures. Save/load normally to
confirm the existing launch configuration was preserved.

The index-mapping helper is covered by 10000 deterministic randomized sparse
triangle tests, degenerate/repeated references, full 16-bit coverage, invalid-range
fallback, simulated group rebasing and counter checks. Those are helper tests;
they do not replace a full build, native Vulkan/MoltenVK and in-game visual tests.
