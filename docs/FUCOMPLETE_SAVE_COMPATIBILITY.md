# FUComplete US save-key compatibility

## Verified failure and scope

The reviewed FUComplete native build supplied the earlier Poke key while loading
`ULUS10391/MHP2NDG.BIN`. The supplied PPSSPP save uses mode 5 (flag 0x41), is
1,483,024 bytes long and has valid PARAM.SFO hashes. Its data hash fails under
the guest-supplied key but matches exactly under a regional key also present
verbatim in the supplied FUComplete 1.4.0 PRELOAD.BIN. The complete supplied pair
passes SFO verification, file-hash verification, decryption and re-encryption
checks with that regional key. No user save or loader binary is committed as a fixture.

This establishes a regional-key mismatch at the HLE boundary. The precise native
loader patch/dispatch behavior that leaves the earlier key in the request is not
fully audited by this targeted compatibility fix. It does not claim general
self-modifying guest-code or international MHFU support.

## Correction

`effective_save_key` substitutes the regional key only when all conditions hold:

- compiled main ELF provenance is the reviewed FUComplete hash
  `02811bb3a529d48d707072a726e87106d0ecdab62aa7e494b23ecf1469eb662f`;
- the game requests exactly `ULUS10391/MHP2NDG.BIN`;
- the supplied key equals the earlier observed key.

Other regions, files, custom keys, unknown variants and stock builds retain their
existing behavior. The save crypto algorithms and integrity rules are unchanged.
The effective key is used for normal loading and writing. No alternate hash mode
or unchecked decryption is introduced.

## Existing saves made by Poke

Earlier Poke FUComplete builds could save successfully with the earlier key and
reload those saves themselves, while rejecting regional PPSSPP saves. To preserve
that progress, the loader tries the earlier key only on a mode-5 hash mismatch
for this exact reviewed US save, and uses it only if the stored file hash matches.
A corrupted file that matches neither key is still rejected.

Loading never rewrites or migrates the save. A later normal in-game save writes
with the corrected regional key through the existing atomic-write path. Keep a
backup before testing, and test a copied output in PPSSPP before calling that
round-trip game-level interoperability verified.

## FUCDAT is separate

Keep the full FUCDAT directory beside ULUS10391 under the same memory stick:

- `mhfuc/ms0/PSP/SAVEDATA/ULUS10391/`: hunter save.
- `mhfuc/ms0/PSP/SAVEDATA/FUCDAT/`: FUComplete configuration and loader assets.

Do not combine their PARAM.SFO files. This fix does not import, regenerate or
rewrite FUCDAT. The current host Import/Export still recognizes only ULJM05500;
manual folder copying is the tested transfer path for this fix. Regional import
selection and safe treatment of FUCDAT's auxiliary content are separate work.
The supplied FUCDAT SFO lists DUMMY.BIN even though that file is absent; ordinary
hunter-save validation must not simply be applied to that auxiliary folder.

## Build and test

Remove the temporary file-hash probe before applying this fix. The package's
apply script does so, backs up the active US save and changed source files under
ignored out/ folders, applies the source-only patch, and runs:

```sh
cmake --build out/mhfuc --target Poke mhp2g_savedata_tests mhp2g_fuc_save_tests --parallel 2
./out/mhfuc/bin/mhp2g_savedata_tests
./out/mhfuc/bin/mhp2g_fuc_save_tests
python3 run_fuc.py
```

The dedicated public test uses synthetic data for key scoping, regional and
previous-key saves, re-saving, other-file/region isolation and tamper rejection.
It explicitly enables the reviewed variant on its own test target, independently
of the normal application's corpus selection. Production activation still
requires the real build's existing compiled provenance definition.

Preparation checks: the new C++ tests passed for the reviewed variant, no variant
and an unrelated variant. The existing savedata suite passed, and the uploaded
real save pair passed the existing external-save checker with zero failures.
These local checks used the uploaded Poke source with an independent OpenSSL AES
block adapter because the repository's vendored tiny-AES source was not attached.
The real CMake/Tiny-AES application build, game-level load and Steam Deck/macOS
smoke tests must run on the developer's complete checkout.

No main corpus or overlays need regeneration for this HLE/store change. Do not
commit backups, save files, runtime logs, game keys dumped from a session, or the
removed hash probe. The constants in fuc_save_keys.hpp are fixed format facts;
no loader implementation or user game-content fixture is included.
