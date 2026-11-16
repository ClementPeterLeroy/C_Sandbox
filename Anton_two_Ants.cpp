#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <array>
#include <random>

enum class StartPreset { Blank, Checkerboard, Ring, Cross, Scattered };
const std::array<std::string, 5> presetNames = {
    "Blank", "Checkerboard", "Ring", "Cross", "Scattered"
};
const std::array<std::string, 5> presetDescriptions = {
    "All white: two ants start ten cells apart, both facing north.",
    "A 17 x 17 checkerboard centered on the ant.",
    "A square ring, with a white interior and both ants inside.",
    "Two black lines crossing underneath the ant.",
    "Repeatable scattered cells in a 25 x 25 patch (fixed seed)."
};

// An unbounded board: only black cells need to be stored.
struct Ant {
    int x, y;
    int direction = 0; // 0: north, 1: east, 2: south, 3: west
    bool mirrored = false;
};

struct AntSimulation {
    std::set<std::pair<int, int>> blackCells;
    std::array<Ant, 2> ants = {{{-5, 0, 0, false}, {5, 0, 0, true}}};
    int minX = -5, maxX = 5, minY = 0, maxY = 0;
    std::uint64_t steps = 0;
    StartPreset preset = StartPreset::Blank;

    // Every preset restores both ants and starts the round counter at zero.
    void setStart(StartPreset selected) {
        *this = AntSimulation{};
        preset = selected;
        std::mt19937 random(42);
        for (int row = -12; row <= 12; ++row) {
            for (int col = -12; col <= 12; ++col) {
                bool black = false;
                switch (selected) {
                    case StartPreset::Blank: break;
                    case StartPreset::Checkerboard:
                        black = std::abs(col) <= 8 && std::abs(row) <= 8 &&
                                (col + row) % 2 == 0;
                        break;
                    case StartPreset::Ring:
                        black = std::max(std::abs(col), std::abs(row)) == 8;
                        break;
                    case StartPreset::Cross:
                        black = (col == 0 && std::abs(row) <= 10) ||
                                (row == 0 && std::abs(col) <= 10);
                        break;
                    case StartPreset::Scattered:
                        black = random() % 4 == 0;
                        break;
                }
                if (black) {
                    blackCells.insert({col, row});
                    minX = std::min(minX, col);
                    maxX = std::max(maxX, col);
                    minY = std::min(minY, row);
                    maxY = std::max(maxY, row);
                }
            }
        }
    }

    void advance(int rounds) {
        const int dx[] = {0, 1, 0, -1};
        const int dy[] = {-1, 0, 1, 0};
        for (int i = 0; i < rounds; ++i) {
            // Phase 1: both ants read the unchanged board and choose a turn.
            std::array<std::pair<int, int>, 2> cells;
            for (std::size_t j = 0; j < ants.size(); ++j) {
                auto& ant = ants[j];
                cells[j] = {ant.x, ant.y};
                const bool black = blackCells.count(cells[j]) != 0;
                const bool turnRight = (black == ant.mirrored);
                ant.direction = (ant.direction + (turnRight ? 1 : 3)) % 4;
            }

            // Phase 2: apply the flips. Two flips of the same cell cancel.
            for (const auto& cell : cells) {
                if (!blackCells.erase(cell)) blackCells.insert(cell);
            }

            // Phase 3: move both ants. Sharing cells and crossing are allowed.
            for (auto& ant : ants) {
                ant.x += dx[ant.direction];
                ant.y += dy[ant.direction];
                minX = std::min(minX, ant.x);
                maxX = std::max(maxX, ant.x);
                minY = std::min(minY, ant.y);
                maxY = std::max(maxY, ant.y);
            }
            ++steps;
        }
    }

    void reset() { setStart(preset); }
};

struct Button {
    sf::FloatRect bounds;
    std::string label;
    int rounds; // zero means reset
};

int main() {
    sf::Font font;
    bool fontLoaded = false;
    for (const auto& path : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                            "/System/Library/Fonts/Helvetica.ttc"}) {
        if (std::filesystem::exists(path) && font.openFromFile(path)) {
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        std::cerr << "Cannot find a system font for the controls.\n";
        return 1;
    }

    sf::RenderWindow window(sf::VideoMode({1000, 900}), "Langton's Ant - Two Ants",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    AntSimulation simulation;
    const std::vector<Button> buttons = {
        {{{24.f, 82.f}, {150.f, 44.f}}, "+1", 1},
        {{{186.f, 82.f}, {150.f, 44.f}}, "+10", 10},
        {{{348.f, 82.f}, {150.f, 44.f}}, "+100", 100},
        {{{510.f, 82.f}, {150.f, 44.f}}, "+1000", 1000},
        {{{822.f, 82.f}, {150.f, 44.f}}, "Reset", 0}
    };
    std::vector<Button> presetButtons;
    for (std::size_t i = 0; i < presetNames.size(); ++i) {
        presetButtons.push_back({{{24.f + i * 192.f, 164.f}, {180.f, 36.f}},
                                 presetNames[i], static_cast<int>(i)});
    }

    auto drawText = [&](const std::string& text, float x, float y,
                        unsigned size, sf::Color color) {
        sf::Text label(font, text, size);
        label.setPosition({x, y});
        label.setFillColor(color);
        window.draw(label);
    };

    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
            if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    const auto position = window.mapPixelToCoords(click->position);
                    for (const auto& button : buttons) {
                        if (button.bounds.contains(position)) {
                            if (button.rounds == 0) simulation.reset();
                            else simulation.advance(button.rounds);
                            break;
                        }
                    }
                    for (const auto& button : presetButtons) {
                        if (button.bounds.contains(position)) {
                            simulation.setStart(static_cast<StartPreset>(button.rounds));
                            break;
                        }
                    }
                }
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                switch (key->code) {
                    case sf::Keyboard::Key::Num1: simulation.advance(1); break;
                    case sf::Keyboard::Key::Num2: simulation.advance(10); break;
                    case sf::Keyboard::Key::Num3: simulation.advance(100); break;
                    case sf::Keyboard::Key::Num4: simulation.advance(1000); break;
                    case sf::Keyboard::Key::R: simulation.reset(); break;
                    default: break;
                }
            }
        }
        if (!window.isOpen()) break;

        window.clear(sf::Color(23, 29, 39));
        drawText("Langton's Ant - Two Ants", 24, 14, 28, sf::Color::White);
        drawText("Red: white R / black L", 24, 51, 16, sf::Color(255, 120, 116));
        drawText("Blue: white L / black R", 260, 51, 16, sf::Color(110, 182, 255));
        drawText("1 round = both ants move once", 560, 51, 16, sf::Color(183, 196, 210));

        const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        for (const auto& button : buttons) {
            sf::RectangleShape box(button.bounds.size);
            box.setPosition(button.bounds.position);
            const bool hover = button.bounds.contains(mouse);
            box.setFillColor(button.rounds == 0
                ? (hover ? sf::Color(159, 70, 74) : sf::Color(115, 48, 57))
                : (hover ? sf::Color(55, 110, 155) : sf::Color(39, 68, 96)));
            window.draw(box);
            sf::Text label(font, button.label, 21);
            const auto bounds = label.getLocalBounds();
            label.setOrigin(bounds.position + bounds.size / 2.f);
            label.setPosition(button.bounds.position + button.bounds.size / 2.f);
            window.draw(label);
        }

        drawText("Starting pattern (select to restart):", 24, 136, 16, sf::Color(183, 196, 210));
        for (const auto& button : presetButtons) {
            const bool selected = button.rounds == static_cast<int>(simulation.preset);
            sf::RectangleShape box(button.bounds.size);
            box.setPosition(button.bounds.position);
            box.setFillColor(selected ? sf::Color(42, 123, 107) :
                button.bounds.contains(mouse) ? sf::Color(66, 83, 103) : sf::Color(40, 49, 64));
            box.setOutlineThickness(selected ? 2.f : 0.f);
            box.setOutlineColor(sf::Color(109, 213, 185));
            window.draw(box);
            drawText(button.label, button.bounds.position.x + 12.f,
                     button.bounds.position.y + 6.f, 17, sf::Color::White);
        }
        drawText(presetDescriptions[static_cast<std::size_t>(simulation.preset)],
                 24, 208, 16, sf::Color(183, 196, 210));

        const auto& red = simulation.ants[0];
        const auto& blue = simulation.ants[1];
        drawText("Rounds: " + std::to_string(simulation.steps) +
                 "     Black cells: " + std::to_string(simulation.blackCells.size()) +
                 "     Red: (" + std::to_string(red.x) + ", " + std::to_string(red.y) +
                 ")     Blue: (" + std::to_string(blue.x) + ", " +
                 std::to_string(blue.y) + ")", 24, 237, 18, sf::Color::White);

        // Fit all visited cells, including the highway, without wrapping edges.
        const float boardX = 24.f, boardY = 272.f, boardW = 948.f, boardH = 560.f;
        const int columns = std::max(41, simulation.maxX - simulation.minX + 9);
        const int rows = std::max(31, simulation.maxY - simulation.minY + 9);
        const float cellSize = std::min(boardW / columns, boardH / rows);
        const float centerX = (simulation.minX + simulation.maxX) / 2.f;
        const float centerY = (simulation.minY + simulation.maxY) / 2.f;
        const float originX = boardX + boardW / 2.f - (centerX + 0.5f) * cellSize;
        const float originY = boardY + boardH / 2.f - (centerY + 0.5f) * cellSize;
        sf::RectangleShape board({boardW, boardH});
        board.setPosition({boardX, boardY});
        board.setFillColor(sf::Color(246, 246, 239));
        window.draw(board);

        sf::RectangleShape cell({cellSize, cellSize});
        cell.setFillColor(sf::Color(27, 33, 39));
        for (const auto& [x, y] : simulation.blackCells) {
            cell.setPosition({originX + x * cellSize, originY + y * cellSize});
            window.draw(cell);
        }
        if (cellSize >= 8.f) {
            sf::VertexArray grid(sf::PrimitiveType::Lines);
            const sf::Color gridColor(170, 177, 178, 80);
            for (float x = originX + std::ceil((boardX - originX) / cellSize) * cellSize;
                 x <= boardX + boardW; x += cellSize) {
                grid.append(sf::Vertex{{x, boardY}, gridColor});
                grid.append(sf::Vertex{{x, boardY + boardH}, gridColor});
            }
            for (float y = originY + std::ceil((boardY - originY) / cellSize) * cellSize;
                 y <= boardY + boardH; y += cellSize) {
                grid.append(sf::Vertex{{boardX, y}, gridColor});
                grid.append(sf::Vertex{{boardX + boardW, y}, gridColor});
            }
            window.draw(grid);
        }

        // Arrows show headings. Purple marks a shared cell; both arrows remain visible.
        const float radius = std::max(3.f, cellSize * 0.43f);
        sf::ConvexShape ant(3);
        ant.setPoint(0, {0.f, -radius});
        ant.setPoint(1, {radius * 0.8f, radius});
        ant.setPoint(2, {-radius * 0.8f, radius});
        const bool overlapping = red.x == blue.x && red.y == blue.y;
        for (std::size_t j = 0; j < simulation.ants.size(); ++j) {
            const auto& current = simulation.ants[j];
            ant.setFillColor(overlapping ? sf::Color(190, 80, 235) :
                             j == 0 ? sf::Color(235, 62, 59) : sf::Color(30, 130, 245));
            ant.setPosition({originX + (current.x + 0.5f) * cellSize,
                             originY + (current.y + 0.5f) * cellSize});
            ant.setRotation(sf::degrees(current.direction * 90.f));
            window.draw(ant);
        }

        drawText("Keys 1 / 2 / 3 / 4: advance 1 / 10 / 100 / 1000 rounds.   R: reset.",
                 24, 844, 16, sf::Color(183, 196, 210));
        drawText("Purple: shared cell. Flips cancel there. Reset restores the preset and both ants.",
                 24, 869, 15, sf::Color(183, 196, 210));
        window.display();
    }
    return 0;
}
