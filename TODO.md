# Poke - mhp2G / mhfu recomp

Static recompilation of mhp2g / mhfu based on Yakumo mph3rd recompilation project

### To Do tasks
- [ ] Reduce hub monster life for solo 
- [ ] Change Poke menu to use mhfu menu theme
- [ ] Proper game executable
- [ ] Decomp game to readable and reusable Cpp or Rust and detach logic from data to help mod and alter game
- [ ] Add full character customisation from home 
- [ ] Add retro achievement equivalent

### Completed tasks ✓
- [x] game weapons ui gauge and specific ui scalling
- [x] ui elements wrong place colliding map
- [x] Scale the observed idle and L-held item-selector layouts
- [x] Saved FOV adjustment in the Video menu
- [x] FU complete save compatibility issue
- [x] game weapons ui gauge and specific ui scalling
- [x] ui scalling when changing items
- [x] Command options for improvement (crisp text, textures, maps and menu) 
- [x] UI size adjustment 
- [x] kcx2 format texture pack support
- [x] Correct map scalling for higher res
- [x] Fixed offset on tewture pack sharpness indicator

### FUComplete checkpoint
- [x] Separate pinned executable generation, build, settings and normal saves.
- [x] Fix wider idle/L-held item selector scaling on the observed FUComplete layout.
- [x] Fix the observed `ddeab148` HD sharpness atlas offset.
- [x] Verify normal save/write and subsequent load on macOS.
- [x] Document the separate build/run process and remove temporary traces.
- [ ] Test this cleaned branch on Steam Deck/Linux.
- [ ] Generate matching native corpora for changed FUComplete overlays.
- [ ] Audit host DATA.BIN mod support and remaining module/import stubs.
- [ ] Validate additional quests, transitions, multiplayer and FUComplete features.
- [ ] Add portable macOS/Linux packaging; a Flatpak is not implemented yet.
- [ ] Consider international MHFU as a separate variant after executable review.
