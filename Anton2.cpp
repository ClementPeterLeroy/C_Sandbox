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
    "All white: the classic start. Try 11,000 steps to see the highway.",
    "A 17 x 17 checkerboard centered on the ant.",
    "A square ring, with a white interior and the ant at its center.",
    "Two black lines crossing underneath the ant.",
    "Repeatable scattered cells in a 25 x 25 patch (fixed seed)."
};

// An unbounded board: only black cells need to be stored.
struct AntSimulation {
    std::set<std::pair<int, int>> blackCells;
    int x = 0, y = 0;
    int direction = 0; // 0: north, 1: east, 2: south, 3: west
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    std::uint64_t steps = 0;
    StartPreset preset = StartPreset::Blank;

    // Every pattern starts at (0, 0), facing north, with zero steps.
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
            const auto cell = std::make_pair(x, y);
            const auto found = blackCells.find(cell);
            if (found == blackCells.end()) {
                // White: turn right and paint black.
                direction = (direction + 1) % 4;
                blackCells.insert(cell);
            } else {
                // Black: turn left and paint white.
                direction = (direction + 3) % 4;
                blackCells.erase(found);
            }
            x += dx[direction];
            y += dy[direction];
            ++steps;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
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

    sf::RenderWindow window(sf::VideoMode({1000, 900}), "Langton's Ant",
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
        drawText("Langton's Ant", 24, 14, 28, sf::Color::White);
        drawText("White: turn right. Black: turn left. Flip the cell, then move.",
                 24, 51, 16, sf::Color(183, 196, 210));

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

        drawText("Steps: " + std::to_string(simulation.steps) +
                 "     Black cells: " + std::to_string(simulation.blackCells.size()) +
                 "     Ant: (" + std::to_string(simulation.x) + ", " +
                 std::to_string(simulation.y) + ")", 24, 237, 18, sf::Color::White);

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

        // A red arrow shows both the ant's cell and its heading.
        const float radius = std::max(3.f, cellSize * 0.43f);
        sf::ConvexShape ant(3);
        ant.setPoint(0, {0.f, -radius});
        ant.setPoint(1, {radius * 0.8f, radius});
        ant.setPoint(2, {-radius * 0.8f, radius});
        ant.setFillColor(sf::Color(235, 62, 59));
        ant.setPosition({originX + (simulation.x + 0.5f) * cellSize,
                         originY + (simulation.y + 0.5f) * cellSize});
        ant.setRotation(sf::degrees(simulation.direction * 90.f));
        window.draw(ant);

        drawText("Keys 1 / 2 / 3 / 4: advance 1 / 10 / 100 / 1000 steps.   R: reset.",
                 24, 844, 16, sf::Color(183, 196, 210));
        drawText("Reset restores the selected starting pattern. The view fits automatically.",
                 24, 869, 15, sf::Color(183, 196, 210));
        window.display();
    }
    return 0;
}
