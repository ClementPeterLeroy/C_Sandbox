# C++ Sandbox

Small games and simulations made while learning C++ and SFML.

## Projects

- **Snake**: console and graphical Snake games, with food, growth, collision detection, score tracking, buffered turns, and restart controls.
- **Langton's Ant (Anton)**: a grid simulation with black and white cells, selectable starting patterns, and controls to advance by 1, 10, 100, or 1,000 rounds. Includes a two-ant experiment with opposite turning rules on a shared board.
- **Game of Life**: a work in progress that displays a starting cell pattern. Generation updates are not implemented yet.
- **Ecosystem**: two species competing for food and space, with movement, energy use, reproduction, and starvation. A side panel controls speed, food regrowth, starting populations, and species traits. Includes pause, stepping, repeatable seeds, and a population history graph.

Each source file is a separate program with its own `main()` function. Build one at a time.

## Build and run

The graphical projects require a C++17 compiler and SFML 3. The included VS Code configuration uses GCC and GDB from MSYS2 UCRT64, installed in `C:\msys64\ucrt64`.

With those tools installed, open a graphical project's `.cpp` file in VS Code and press **F5** to build and run it. Use **Ctrl+Shift+B** to build only.

You can also build from a terminal where the compiler and SFML libraries are available:

```sh
g++ -std=c++17 Ecosystem1.cpp -o Ecosystem1.exe -lsfml-graphics -lsfml-window -lsfml-system
```

On Windows PowerShell, run it with:

```powershell
.\Ecosystem1.exe
```

Replace the source and executable names to build another project. The console Snake game does not require the SFML libraries.

## Controls

- **Snake**: arrow keys or WASD to turn; **R** to restart after a game ends.
- **Anton**: use the step buttons or keys **1–4** to advance rounds. Select a starting preset to restart; **R** restores the selected preset.
- **Ecosystem**: use the side panel to adjust parameters. **Space** toggles play/pause, **N** advances one round, and **R** resets. Starting-population changes apply on reset; other sliders apply live. Reset keeps the parameters and seed, while **New seed + reset** generates a different starting world.

Compiled executables are excluded from version control.
