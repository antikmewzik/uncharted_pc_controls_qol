> **Unofficial fork of [SuperMedo's Uncharted PC port](https://github.com/Supermedo/uncharted_pc).**
> Not affiliated with SuperMedo. Please do not report problems with this fork to him: use the
> [Issues](../../issues) tab here.

**[⬇ Download the latest release](../../releases)**

---

**Is this fork still being worked on?**

YES ✅ (as of 21 Oct 2026)

# Uncharted PC - Full mouse and keyboard controls and user experience QoL changes

This fork aims to transform **Uncharted 1** _(Uncharted 2 and 3 will follow)_ by adding full keyboard and mouse controls in game and in the menus, with dynamic button prompts and instant input method switching.

## Full mouse and keyboard control

### Gameplay

<table>
  <tr>
    <td><img src="https://github.com/user-attachments/assets/aac36114-f668-4217-be89-7c7748fd0ff7" width="270" alt="Mouse look"></td>
    <td><img src="https://github.com/user-attachments/assets/86887212-8b3b-41f2-b909-7d93c83d6b61" width="270" alt="Grenade with the mouse"></td>
    <td><img src="https://github.com/user-attachments/assets/d53b034b-da9b-4329-9be1-03469a8091a1" width="270" alt="Mouse gameplay"></td>
  </tr>
</table>

The game is now fully playable using mouse and keyboard. Looking around, aiming and shooting while standing or in cover works naturally. Mouse and aim sensitivity sliders have been added to the options to personalise your experience. Some gameplay sequences have been improved so you don't have to press keys that feel unnatural.

<table>
  <tr>
    <td><img src="https://github.com/user-attachments/assets/9f8d18f1-e8f5-4d78-9cd5-80bf6f57007d" width="270" alt="No more weird key bindings"></td>
  </tr>
  <tr>
    <td align="center"><em>No more weird key bindings</em></td>
  </tr>
</table>

### Menus

<table>
  <tr>
    <td><img src="https://github.com/user-attachments/assets/22c5eafe-9f28-46e1-bb5a-1bd13e2e6238" width="405" alt="Mouse navigation in the menus"></td>
    <td><img src="https://github.com/user-attachments/assets/f75fb9f0-ac87-4266-b1a8-1d4164917554" width="405" alt="Mouse navigation in the menus"></td>
  </tr>
</table>

Menus are now fully usable using mouse and keyboard. Inputs have been rewritten so that when a menu is open it allows you to navigate the menu using your mouse cursor. Every option in every menu and sub-menu is clickable, including adjusting sliders or moving the scrollbar. The navigation is made more natural by making the selected option jump directly where the cursor is, preventing the whole menu from scrolling to where the cursor is located. The mouse's side (thumb) button can also be used to return to the previous menu.

Inputs have also been rewritten for the keyboard. The Select and Back buttons (as well as the Title Menu button) are now using keys that feel more natural, akin to a proper PC game. The game dynamically switches between the menu and in-game control schemes to avoid breaking inputs during gameplay.

### Button prompts

<img width="405" alt="Key and mouse button prompts" src="https://github.com/user-attachments/assets/43e7e740-c536-4bab-ab7e-887cb9246369" />

All the button prompts have been remade using my own vectorized design. The game is able to dynamically create each button using the game's font, the keys are not hardcoded. In addition, the in-game hints will reflect those changes in real time, with some hints rewritten to read more naturally when displaying PC button prompts.

Key bindings can also be changed in game using a new menu under Options > Controls. In the menus, the keyboard and mouse button prompts will cycle from one to another, indicating that navigation is possible with both.

<table>
  <tr>
    <td><img src="https://github.com/user-attachments/assets/6da82665-c5c1-4ebd-835e-82c97657a376" width="405" alt="Key bindings menu"></td>
    <td><img src="https://github.com/user-attachments/assets/224a809b-3750-4c8f-8d94-44bcf5258f2d" width="405" alt="Key bindings menu"></td>
  </tr>
</table>

---

## Switching input method and button prompts

<table>
  <tr>
    <td><img src="https://github.com/user-attachments/assets/4be239a0-c014-4438-b541-25950585d395" width="405" alt="Switching between keyboard and controller"></td>
    <td><img src="https://github.com/user-attachments/assets/8e6e1561-1f4c-473c-b3a1-e94b90632e50" width="405" alt="Switching between keyboard and controller"></td>
  </tr>
</table>

You can now switch from keyboard and mouse to a controller and back, the game will dynamically switch based on the last input. If you feel like platforming using a controller and then switch to mouse and keyboard for shooting, you can now do that. When switching to another input method, the game will display the correct button prompts.

The Xbox control layout has also been added under Options > Controls > Controller layout.

<img width="405" alt="Xbox controller layout" src="https://github.com/user-attachments/assets/06aafc7c-c400-4c92-91e8-5d10e55b22ec" />

---

## More info

- **Download:** see the [Releases](../../releases) page. Unzip, run `Uncharted.exe`, pick your
  game folder and press PLAY.
- **Requirements:** same as the original port (see the original README below).
- **Source:** branch [`u1-mkb-qol`](../../tree/u1-mkb-qol). Licensed under the GPL v2 like the
  original.
- **No game files are included.** You need your own decrypted dump of the collection.
- **ONLY TESTED WITH CUSA02320 (US), version 1.02.** I don't know if it works using other dump versions.

The full list of changes and the known problems are in each release's notes.

---

<details>
<summary>Original README of SuperMedo's port (requirements, features, credits)</summary>

# Uncharted: The Nathan Drake Collection for Windows

**Uncharted 1, 2 and 3 from the PS4 collection running natively on Windows 10 and 11, by [SuperMedo](https://github.com/Supermedo).** Mohammed Albarghouthi

Same idea as my [Bloodborne port](https://github.com/Supermedo/bloodborne_pc): the original PS4 game
code runs directly on your PC and the graphics are translated to Vulkan. Unpack the zip, start
`Uncharted.exe`, pick your game folder, choose a game and press **PLAY**.

**[Download the latest version](https://github.com/Supermedo/uncharted_pc/releases/latest)** · **[Join the Discord](https://discord.gg/s3RgH6cRfJ)**

> **No game files are included.** You need your own decrypted dump of the collection
> (CUSA02343, CUSA02320, ...: the folder with eboot.bin, big2-ps4_Shipping.elf,
> big3-ps4_Shipping.elf, sce_sys, u1data, u2data and u3data).
> This project is not affiliated with Sony Interactive Entertainment or Naughty Dog.

## Features

- All three games from one launcher: Drake's Fortune, Among Thieves and Drake's Deception.
- 60 FPS like on PS4, or 120 FPS (higher rates are experimental).
- Render resolution above 1080p (up to 3200x1800) and ultrawide aspect ratios: the games draw a real larger or wider picture, set in the launcher.
- Anti-aliasing with TAA, AMD FSR 3.1, AMD FSR 4 or NVIDIA DLSS (DLAA).
- Frame generation: AMD FSR 3 (any GPU) or NVIDIA DLSS (RTX 40 2x, RTX 50 up to 4x).
- Keyboard, mouse and controller together, all remappable. Mouse look in U3.
- Stereo, 5.1 or 7.1 sound: the games are told the real speaker layout (it follows the Windows setting, or pick it in the launcher).
- Trophies: the games' own trophy lists in the launcher, and a notice in the game when you earn one.
- Button prompts for keyboard and mouse, Xbox or PlayStation controllers; controller vibration settings.
- Cheats page (infinite health and ammo per game), launcher languages, your own covers and Steam art.
- A Fixes page for fixes that cost speed: Automatic turns each one on only where it helps.
- Add to Steam and desktop shortcuts from the launcher, automatic updates.

## Requirements

- Windows 10 (1903 or later) or Windows 11, 64-bit.
- An Intel Core 3rd generation (Ivy Bridge) or newer, an AMD Ryzen, or an AMD FX. Older Intel CPUs
  with AVX (Core 2nd and 3rd generation, Xeon E3 v1/v2): experimental, see Known problems.
- A graphics card with Vulkan 1.3 and a current driver, with **6 GB of video memory or more**.
  4 GB cards (GTX 1650, RX 570) run out of video memory and crash after a while; on those, keep
  Anti-aliasing and Frame generation off.
- About 8 GB of free memory (RAM + page file).

## Known problems

- **Uncharted 3, Cartagena square (chapter 2):** at high frame rates (240 Hz setting, ~80 FPS)
  the game could freeze and close with "GPU crash: the graphics device was lost". It came from
  the speed fix for that area and is fixed in 0.3. If it still happens to you, set Frame rate to
  60 on the Display page and open an issue with your `user\last_run.log`.
- **Uncharted 3:** green or black shapes flashing on Sully's shirt in Cartagena: the cloth
  solvers raced on NVIDIA's 32-lane groups. Fixed in 0.3.9 with BilalReloaded's fix.
- **Crash with "ASSERTION: GetRenderFrameParams(frameToFreeInstancesFor)->m_gfxEopTick"**
  (mostly U1 after the first cutscene): the game expected the GPU side to be done with a frame it
  wanted to reuse, and stopped when it wasn't yet (shaders building on a first run). Fixed in
  0.3.2 (0.3.4 for the 1.02 versions): the game now waits for it. If you still see it, open an
  issue with your log.
- **Intel HD / UHD graphics:** a black screen on the HD 630 is fixed in 0.3.3. Older Intel chips
  (HD 520) can still crash inside Intel's own driver.
- **Uncharted 3 London bar crash** (fault at `0xd9c59b`, or device lost in the bar fight): fixed in
  0.3.4.
- **Uncharted 2 loading crashes** (`ReadSync file (req/read: ...)`, white textures then
  `iSubMesh < m_numSubMeshInstances`): file reads could come back cut short. Fixed in 0.3.5.
- **Older CPUs:** the games use instructions of the PS4's processor (AVX, BMI1, MOVBE, LZCNT).
  Since 0.3.7 they are rewritten on CPUs that lack them, so Intel Core 3rd generation (Ivy Bridge)
  runs, as does AMD FX. Sandy Bridge (2nd generation) also lacks F16C and is not supported.
- **Random crashes with "execute access to 0" or a fault at `0x63f5d0` (U1):** Windows wrote its
  exception data over values the game keeps just below the stack pointer. Fixed in 0.3.7.
- **Uncharted 2, train wreck:** `IsFinite(deltaTransform)` in character-leg-ik.cpp, fixed in 0.3.6
  for the EU 1.00 version.
- Frame rates above 120 are experimental.

## Building

MSYS2 CLANG64 on Windows: `bash build.sh` builds `out\bb-probe.exe`, and
`bash packaging/windows/package.sh` makes `dist\uncharted-windows` and the zip.

## Credits

Built on [bbport](https://github.com/deadinside28/bloodborne_pc), the native Linux port of
Bloodborne by deadinside28, and on the [shadPS4](https://github.com/shadps4-emu/shadPS4)
renderer. Video decoding (Videodec2) and the DLSS bridge come from
[IFreemz/shadPS4-Bloodborne-DLSS-FSR](https://github.com/IFreemz/shadPS4-Bloodborne-DLSS-FSR).
Thanks to Mindro for the shadPS4 Uncharted build that showed which problems to look for.
Several fixes come from BilalReloaded's
[Uncharted port](https://github.com/bc160201983/Uncharted-The-Nathan-Drake-Collection-PC-port)
(GPL v2): the Uncharted 3 cloth solver barriers (Sully's shirt), guest memory committed as the
game allocates it (error 1450), the Uncharted 3 look-at reset and the garbage descriptor fixes.

Also used: [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) and the AMD FidelityFX SDK (FSR),
FSR 4 assets from [FireBurn/Q2RTX](https://github.com/FireBurn/Q2RTX), the
[NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS) (`nvngx_dlss.dll` under NVIDIA's license),
[Intel XeSS SDK](https://github.com/intel/xess) (`libxess.dll` under Intel's license),
[LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), [SDL3](https://github.com/libsdl-org/SDL),
[FFmpeg](https://ffmpeg.org), [Dear ImGui](https://github.com/ocornut/imgui),
[sirit](https://github.com/shadps4-emu/sirit), [magic_enum](https://github.com/Neargye/magic_enum),
[xbyak](https://github.com/herumi/xbyak),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
[MSYS2](https://www.msys2.org) / [LLVM](https://llvm.org), [PyInstaller](https://pyinstaller.org),
[Pillow](https://python-pillow.org) and the Cinzel font (SIL Open Font License).

NVIDIA, GeForce RTX and DLSS are trademarks of NVIDIA Corporation. The default covers and the
icon are original artwork; no official art is included.

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses
(see `licenses\` in the download).
