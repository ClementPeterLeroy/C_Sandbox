# C++ Sandbox

Small projects made while learning C++.

## Projects

- `CSnake1.cpp`: a console Snake game.
- `CSnake2.cpp` and `CSnake3.cpp`: graphical Snake versions using SFML, with buffered turns, score tracking, and restart controls.
- `game_of_life1.cpp`: a starting point for a Game of Life project; the simulation is not implemented yet.

Each source file is a separate program with its own `main()` function. Build one at a time.

## Build and run

The graphical projects require a C++17 compiler and SFML 3. The included VS Code configuration uses GCC and GDB from MSYS2 UCRT64, installed in `C:\msys64\ucrt64`.

With those tools installed, open a graphical project's `.cpp` file in VS Code and press **F5** to build and run it. Use **Ctrl+Shift+B** to build only.

You can also build from a terminal where the compiler and SFML libraries are available:

```sh
g++ -std=c++17 CSnake3.cpp -o CSnake3.exe -lsfml-graphics -lsfml-window -lsfml-system
```

On Windows PowerShell, run it with:

```powershell
.\CSnake3.exe
```

For the console version:

```sh
g++ -std=c++17 CSnake1.cpp -o CSnake1.exe
```

The graphical Snake games use arrow keys or WASD to turn and **R** to restart after a game ends. Compiled executables are excluded from version control.
