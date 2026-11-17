#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Each .cpp in this folder is a separate program. Build this file on its own.
constexpr int COLS = 64, ROWS = 56, AREA = COLS * ROWS;
constexpr float FOOD_CAP = 6.f, EAT_LIMIT = 3.f, MOVE_COST = 0.25f;
struct Parameters {
    float speed = 20.f;
    float regrowth = 0.08f;
    float startingPopulation = 80.f; // per species; used on reset
    std::array<float, 2> upkeep = {0.55f, 0.30f};
    std::array<float, 2> birthEnergy = {18.f, 26.f};
    std::array<float, 2> moveChance = {0.9f, 0.35f};
};
struct Organism {
    int cell, species;
    float energy = 12.f;
};
struct Population { int red = 0, blue = 0; };

struct Ecosystem {
    Parameters parameters;
    std::uint32_t seed = 42;
    std::mt19937 random{seed};
    std::array<float, AREA> food{};
    std::array<int, AREA> occupied{}; // -1: empty; -2: newborn this round
    std::vector<Organism> organisms;
    std::deque<Population> history;
    std::uint64_t rounds = 0;
    Population population;
    float averageFood = 0.f;

    Ecosystem() { reset(); }
    static int cellAt(int x, int y) {
        return ((y % ROWS + ROWS) % ROWS) * COLS + (x % COLS + COLS) % COLS;
    }
    std::array<int, 4> neighbors(int cell) const {
        const int x = cell % COLS, y = cell / COLS;
        return {cellAt(x, y - 1), cellAt(x + 1, y), cellAt(x, y + 1), cellAt(x - 1, y)};
    }
    float chance() { return std::generate_canonical<float, 24>(random); }
    void measure() {
        population = {};
        for (const auto& organism : organisms) {
            if (organism.species == 0) ++population.red;
            else ++population.blue;
        }
        averageFood = std::accumulate(food.begin(), food.end(), 0.f) / AREA;
    }
    void reset() {
        random.seed(seed);
        rounds = 0;
        organisms.clear();
        history.clear();
        occupied.fill(-1);
        // Both species share the same randomly distributed food supply.
        for (auto& amount : food) amount = chance() * FOOD_CAP;
        std::vector<int> cells(AREA);
        std::iota(cells.begin(), cells.end(), 0);
        std::shuffle(cells.begin(), cells.end(), random);
        const int count = std::clamp(static_cast<int>(std::round(parameters.startingPopulation)), 1, AREA / 2);
        for (int i = 0; i < count * 2; ++i) {
            occupied[cells[i]] = static_cast<int>(organisms.size());
            organisms.push_back({cells[i], i % 2, 12.f});
        }
        measure();
        history.push_back(population);
    }
    void advance() {
        // Only unoccupied cells regrow food. Food is not created by organisms.
        for (int cell = 0; cell < AREA; ++cell)
            if (occupied[cell] == -1)
                food[cell] = std::min(FOOD_CAP, food[cell] + parameters.regrowth);
        std::vector<int> order(organisms.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), random);
        std::vector<Organism> babies;
        for (int index : order) {
            auto& organism = organisms[index];
            const int species = organism.species;
            const float meal = std::min(food[organism.cell], EAT_LIMIT);
            food[organism.cell] -= meal;
            organism.energy += meal - parameters.upkeep[species];
            if (organism.energy <= 0.f) {
                occupied[organism.cell] = -1;
                continue;
            }
            auto nearby = neighbors(organism.cell);
            std::shuffle(nearby.begin(), nearby.end(), random);
            if (organism.energy > MOVE_COST && chance() < parameters.moveChance[species]) {
                int destination = -1;
                for (int cell : nearby)
                    if (occupied[cell] == -1 && (destination < 0 || food[cell] > food[destination]))
                        destination = cell;
                if (destination >= 0) {
                    occupied[organism.cell] = -1;
                    organism.cell = destination;
                    occupied[destination] = index;
                    organism.energy -= MOVE_COST;
                }
            }
            // Reproduction costs two energy, then splits the rest equally.
            // Newborns occupy space immediately but first act next round.
            if (organism.energy >= parameters.birthEnergy[species]) {
                nearby = neighbors(organism.cell);
                std::shuffle(nearby.begin(), nearby.end(), random);
                for (int cell : nearby) {
                    if (occupied[cell] == -1) {
                        organism.energy = (organism.energy - 2.f) / 2.f;
                        babies.push_back({cell, species, organism.energy});
                        occupied[cell] = -2;
                        break;
                    }
                }
            }
        }
        organisms.erase(std::remove_if(organisms.begin(), organisms.end(),
            [](const Organism& organism) { return organism.energy <= 0.f; }), organisms.end());
        organisms.insert(organisms.end(), babies.begin(), babies.end());
        occupied.fill(-1);
        for (std::size_t i = 0; i < organisms.size(); ++i)
            occupied[organisms[i].cell] = static_cast<int>(i);
        ++rounds;
        measure();
        history.push_back(population);
        if (history.size() > 600) history.pop_front();
    }
};

// Run without opening a window: Ecosystem1.exe --self-test
void selfTest() {
    auto require = [](bool condition) { if (!condition) throw std::runtime_error("Ecosystem regression failed"); };
    Ecosystem a, b;
    require(a.population.red == 80 && a.population.blue == 80);
    require(Ecosystem::cellAt(-1, -1) == AREA - 1);
    require(Ecosystem::cellAt(COLS, ROWS) == 0);
    for (int round = 0; round < 200; ++round) {
        a.advance(); b.advance();
        require(a.food == b.food && a.organisms.size() == b.organisms.size());
        std::array<bool, AREA> seen{};
        for (std::size_t i = 0; i < a.organisms.size(); ++i) {
            const auto& agent = a.organisms[i];
            const auto& other = b.organisms[i];
            require(agent.cell == other.cell && agent.species == other.species && agent.energy == other.energy);
            require(agent.cell >= 0 && agent.cell < AREA && !seen[agent.cell] && agent.energy > 0.f);
            seen[agent.cell] = true;
            require(a.occupied[agent.cell] == static_cast<int>(i));
        }
        for (float amount : a.food) require(amount >= 0.f && amount <= FOOD_CAP);
        require(a.population.red + a.population.blue == static_cast<int>(a.organisms.size()));
    }
    a.reset(); b.reset();
    require(a.rounds == 0 && a.history.size() == 1 && a.food == b.food);
    // Without food, all organisms must eventually die.
    a.food.fill(0.f); a.parameters.regrowth = 0.f;
    for (int i = 0; i < 100; ++i) a.advance();
    require(a.organisms.empty());
    for (int value : a.occupied) require(value == -1);
    // One parent produces one child; the child has not eaten or acted yet.
    a.reset(); a.organisms = {{0, 0, 30.f}}; a.occupied.fill(-1); a.occupied[0] = 0;
    a.food.fill(0.f); a.parameters.moveChance[0] = 0.f;
    a.advance();
    require(a.organisms.size() == 2);
    require(a.organisms[0].energy == a.organisms[1].energy);
    a.seed = 99; a.reset(); require(a.food != b.food);
    std::cout << "Passed: seeded replay, wrapping, occupancy, food bounds, reset, starvation and newborn timing.\n";
}

std::string number(float value, int decimals = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}
struct Slider {
    std::string label;
    float* value;
    float minimum, maximum;
    int decimals;
    sf::FloatRect track;
};

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        try { selfTest(); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    sf::Font font;
    bool loaded = false;
    for (const auto& path : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})
        if (std::filesystem::exists(path) && font.openFromFile(path)) { loaded = true; break; }
    if (!loaded) { std::cerr << "No UI font found.\n"; return 1; }
    sf::RenderWindow window(sf::VideoMode({1200, 920}), "Two Species Ecosystem",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    Ecosystem world;
    bool running = false;
    float accumulator = 0.f;
    sf::Clock clock;
    int dragging = -1;
    const sf::Color red(242, 100, 95), blue(92, 169, 255), muted(169, 184, 202);
    const sf::FloatRect playButton({834, 70}, {104, 40});
    const sf::FloatRect stepButton({948, 70}, {104, 40});
    const sf::FloatRect hundredButton({1062, 70}, {104, 40});
    const sf::FloatRect resetButton({834, 120}, {161, 36});
    const sf::FloatRect seedButton({1005, 120}, {161, 36});
    std::vector<Slider> sliders;
    auto slider = [&](std::string label, float& value, float minimum, float maximum, int decimals) {
        const float y = 280.f + sliders.size() * 57.f;
        sliders.push_back({label, &value, minimum, maximum, decimals, {{834, y}, {332, 6}}});
    };
    slider("Speed: rounds / second", world.parameters.speed, 1, 120, 0);
    slider("Food regrowth / round", world.parameters.regrowth, 0, 0.5f, 2);
    slider("Starting count EACH (on reset)", world.parameters.startingPopulation, 10, 300, 0);
    slider("Red: energy cost / round", world.parameters.upkeep[0], 0.1f, 2, 2);
    slider("Blue: energy cost / round", world.parameters.upkeep[1], 0.1f, 2, 2);
    slider("Red: reproduction energy", world.parameters.birthEnergy[0], 12, 50, 1);
    slider("Blue: reproduction energy", world.parameters.birthEnergy[1], 12, 50, 1);
    slider("Red: move probability", world.parameters.moveChance[0], 0, 1, 2);
    slider("Blue: move probability", world.parameters.moveChance[1], 0, 1, 2);
    auto updateSlider = [&](int index, float x) {
        auto& item = sliders[index];
        const float ratio = std::clamp((x - item.track.position.x) / item.track.size.x, 0.f, 1.f);
        *item.value = item.minimum + ratio * (item.maximum - item.minimum);
        if (item.decimals == 0) *item.value = std::round(*item.value);
    };
    auto reset = [&] { running = false; accumulator = 0; world.reset(); };
    auto text = [&](const std::string& string, float x, float y, unsigned size, sf::Color color) {
        sf::Text label(font, string, size);
        label.setPosition({x, y}); label.setFillColor(color); window.draw(label);
    };
    while (window.isOpen()) {
        const float elapsed = std::min(clock.restart().asSeconds(), 0.1f);
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->is<sf::Event::FocusLost>()) { running = false; dragging = -1; accumulator = 0; }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Space) { running = !running; accumulator = 0; }
                if (key->code == sf::Keyboard::Key::R) reset();
                if (key->code == sf::Keyboard::Key::N) { running = false; accumulator = 0; world.advance(); }
            }
            if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    const auto point = window.mapPixelToCoords(click->position);
                    if (playButton.contains(point)) { running = !running; accumulator = 0; }
                    if (stepButton.contains(point) || hundredButton.contains(point)) {
                        running = false; accumulator = 0;
                        const int count = hundredButton.contains(point) ? 100 : 1;
                        for (int i = 0; i < count; ++i) world.advance();
                    }
                    if (resetButton.contains(point)) reset();
                    if (seedButton.contains(point)) { ++world.seed; reset(); }
                    for (std::size_t i = 0; i < sliders.size(); ++i) {
                        const auto& track = sliders[i].track;
                        if (sf::FloatRect({track.position.x - 8, track.position.y - 12},
                                          {track.size.x + 16, 30}).contains(point)) {
                            dragging = static_cast<int>(i); updateSlider(dragging, point.x); break;
                        }
                    }
                }
            }
            if (const auto* move = event->getIf<sf::Event::MouseMoved>())
                if (dragging >= 0) updateSlider(dragging, window.mapPixelToCoords(move->position).x);
            if (const auto* release = event->getIf<sf::Event::MouseButtonReleased>())
                if (release->button == sf::Mouse::Button::Left) dragging = -1;
        }
        if (!window.isOpen()) break;
        if (running) {
            accumulator += elapsed;
            const float interval = 1.f / world.parameters.speed;
            while (accumulator >= interval) { world.advance(); accumulator -= interval; }
            if (world.organisms.empty()) running = false;
        } else accumulator = 0;

        window.clear(sf::Color(19, 25, 34));
        text("A shared world", 24, 12, 27, sf::Color::White);
        text("Green: food    Red / blue: organisms    Brighter organisms have more energy", 24, 49, 15, muted);
        constexpr float CELL = 12.f, BOARD_X = 24.f, BOARD_Y = 80.f;
        sf::RectangleShape tile({CELL - 1, CELL - 1});
        for (int i = 0; i < AREA; ++i) {
            const float richness = world.food[i] / FOOD_CAP;
            tile.setPosition({BOARD_X + (i % COLS) * CELL, BOARD_Y + (i / COLS) * CELL});
            tile.setFillColor(sf::Color(23, static_cast<std::uint8_t>(32 + 105 * richness), 37));
            window.draw(tile);
        }
        for (const auto& organism : world.organisms) {
            const float brightness = 0.4f + 0.6f * std::clamp(organism.energy / 25.f, 0.f, 1.f);
            const auto color = organism.species == 0 ? red : blue;
            tile.setFillColor(sf::Color(static_cast<std::uint8_t>(color.r * brightness),
                                        static_cast<std::uint8_t>(color.g * brightness),
                                        static_cast<std::uint8_t>(color.b * brightness)));
            tile.setPosition({BOARD_X + (organism.cell % COLS) * CELL, BOARD_Y + (organism.cell / COLS) * CELL});
            window.draw(tile);
        }
        text("Population history - last " + std::to_string(world.history.size()) + " samples", 24, 763, 16, muted);
        const sf::Vector2f graphPosition(24, 792), graphSize(768, 85);
        sf::RectangleShape graph(graphSize); graph.setPosition(graphPosition);
        graph.setFillColor(sf::Color(28, 36, 48)); window.draw(graph);
        int peak = 10;
        for (auto point : world.history) peak = std::max({peak, point.red, point.blue});
        for (int species = 0; species < 2; ++species) {
            sf::VertexArray line(sf::PrimitiveType::LineStrip);
            for (std::size_t i = 0; i < world.history.size(); ++i) {
                const int count = species == 0 ? world.history[i].red : world.history[i].blue;
                const float x = graphPosition.x + i * graphSize.x / std::max<std::size_t>(1, world.history.size() - 1);
                const float y = graphPosition.y + graphSize.y - 2 - count * (graphSize.y - 4) / peak;
                line.append(sf::Vertex{{x, y}, species == 0 ? red : blue});
            }
            window.draw(line);
        }
        text("Scale: 0 to " + std::to_string(peak) + " organisms per species", 24, 883, 13, muted);
        sf::RectangleShape panel({380, 920}); panel.setPosition({820, 0});
        panel.setFillColor(sf::Color(28, 36, 48)); window.draw(panel);
        text("Ecosystem controls", 834, 16, 24, sf::Color::White);
        const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        auto button = [&](const sf::FloatRect& bounds, const std::string& label) {
            sf::RectangleShape shape(bounds.size); shape.setPosition(bounds.position);
            shape.setFillColor(bounds.contains(mouse) ? sf::Color(65, 106, 139) : sf::Color(43, 73, 97));
            window.draw(shape); text(label, bounds.position.x + 12, bounds.position.y + 8, 16, sf::Color::White);
        };
        button(playButton, running ? "Pause" : "Play"); button(stepButton, "+1 round");
        button(hundredButton, "+100"); button(resetButton, "Reset same seed"); button(seedButton, "New seed + reset");
        text(std::string(running ? "RUNNING" : "PAUSED") + "   Round " + std::to_string(world.rounds), 834, 169, 18, sf::Color::White);
        text("Red: " + std::to_string(world.population.red), 834, 199, 18, red);
        text("Blue: " + std::to_string(world.population.blue), 1010, 199, 18, blue);
        text("Food avg: " + number(world.averageFood, 1) + " / 6    Seed: " + std::to_string(world.seed), 834, 229, 15, muted);
        for (const auto& item : sliders) {
            text(item.label, 834, item.track.position.y - 25, 14, muted);
            text(number(*item.value, item.decimals), 1118, item.track.position.y - 25, 14, sf::Color::White);
            sf::RectangleShape track(item.track.size); track.setPosition(item.track.position);
            track.setFillColor(sf::Color(63, 77, 95)); window.draw(track);
            const float ratio = (*item.value - item.minimum) / (item.maximum - item.minimum);
            sf::CircleShape handle(7); handle.setOrigin({7, 7});
            handle.setPosition({item.track.position.x + ratio * item.track.size.x, item.track.position.y + 3});
            handle.setFillColor(sf::Color(128, 208, 185)); window.draw(handle);
        }
        text("Sliders apply live, except starting count.", 834, 778, 14, muted);
        text("Reset keeps parameters and repeats the seed.", 834, 801, 14, muted);
        text("Space: play/pause   N: step   R: reset", 834, 836, 14, sf::Color::White);
        text("Wrapping edges. Newborns act next round.", 834, 862, 14, muted);
        text("Focus loss pauses. Extinct species stay extinct.", 834, 885, 13, muted);
        window.display();
    }
    return 0;
}
