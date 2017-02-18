
# Games used for testing

## Linux

| Name                        | Engine        | Graphics API          | Notes                          | Interesting because..   |
|-----------------------------|---------------|-----------------------|--------------------------------|-------------------------|
| [Overland][]                | Unity         | OpenGL                |                                | 3D OpenGL game, 32/64   |
| [90 Second Portraits][]     | love 0.10     | OpenGL                |                                | love game               |

Wanted:

  * A Vulkan game
  * A game we can force to use ALSA

## Windows

| Name                        | Engine        | Graphics API          | Interesting because..                        |
|-----------------------------|---------------|-----------------------|----------------------------------------------|
| [Teeworlds][]               | Custom        | OpenGL                | Smooth 60FPS goal for 2D OpenGL game         |
| [Sauerbraten][]             | Custom        | OpenGL                | Smooth 60FPS goal for 3D OpenGL game         |
| [Longest Night][]           | Unity 5.3.1f1 | Direct3D 11, DXGI 1.0 | Singlesampled backbuf (on 'Rad' quality)     |
| [Dr Langeskov][]            | Unity 5.1.0p1 | Direct3D 11, DXGI 1.1 | Multisampled backbuf (on 'Emerald' quality)  |
| [Super Ball Brawlers][]     | MonoGame      | Direct3D 11, DXGI 1.1 | Non-hwnd swapchain, RGBA backbuf, fullscreen |

Wanted:

  * D3D9, D3D10, D3D12 games
  * A DirectDraw game
  * A Vulkan game

Random findings:

  * on nVidia cards
    * 1280x720 => pitch is (width * height * components), 1680x1050 isn't (it's re-aligned for perf)

## macOS

| Name                        | Engine        | Graphics API          | Notes                          | Interesting because..   |
|-----------------------------|---------------|-----------------------|--------------------------------|-------------------------|
| [90 Second Portraits][]     | love 0.10     | OpenGL                |                                | love game               |


[Overland]: https://finji.itch.io/overland
[90 Second Portraits]: https://tangramgames.itch.io/90-second-portraits
[Dr Langeskov]: https://crowscrowscrows.itch.io/dr-langeskov-the-tiger-and-the-terribly-cursed-emerald-a-whirlwind-heist
[Longest Night]: https://finji.itch.io/longest-night
[Super Ball Brawlers]: https://redpoint.itch.io/super-ball-brawlers

[Teeworlds]: https://www.teeworlds.com/
[Sauerbraten]: http://sauerbraten.org/