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
enum class Environment { Stable, Seasonal, Patchy };
const std::array<std::string, 3> environmentNames = {"Stable", "Seasonal", "Patchy"};
struct Parameters {
    float speed = 20.f;
    float seasonLength = 400.f;
    float mutationChance = 0.15f;
    float mutationStrength = 0.08f;
    float regrowth = 0.08f;
    float startingPopulation = 80.f; // per species; used on reset
    std::array<float, 2> upkeep = {0.55f, 0.30f};
    std::array<float, 2> birthEnergy = {18.f, 26.f};
    std::array<float, 2> moveChance = {0.9f, 0.35f};
};
struct Organism {
    int cell, species;
    float energy = 12.f;
    float moveChance = 0.5f;
    float birthEnergy = 20.f;
    unsigned generation = 0;
};
struct Population { int red = 0, blue = 0; };

struct Ecosystem {
    Parameters parameters;
    Environment environment = Environment::Stable;
    std::array<float, AREA> fertility{};
    std::array<float, 2> meanMove{}, meanBirth{};
    unsigned oldestGeneration = 0;
    std::uint64_t mutations = 0;
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
        meanMove = {}; meanBirth = {}; oldestGeneration = 0;
        for (const auto& organism : organisms) {
            if (organism.species == 0) ++population.red;
            else ++population.blue;
            meanMove[organism.species] += organism.moveChance;
            meanBirth[organism.species] += organism.birthEnergy;
            oldestGeneration = std::max(oldestGeneration, organism.generation);
        }
        for (int s = 0; s < 2; ++s) {
            const int count = s == 0 ? population.red : population.blue;
            if (count) { meanMove[s] /= count; meanBirth[s] /= count; }
        }
        averageFood = std::accumulate(food.begin(), food.end(), 0.f) / AREA;
    }
    void reset() {
        random.seed(seed);
        rounds = 0; mutations = 0;
        organisms.clear();
        history.clear();
        occupied.fill(-1);
        // Both species share the same randomly distributed food supply.
        for (int cell = 0; cell < AREA; ++cell) {
            const float x = static_cast<float>(cell % COLS), y = static_cast<float>(cell / COLS);
            float patch = 0.f;
            // Smooth fertile islands; toroidal distance matches the wrapping world.
            for (const auto& center : std::array<std::pair<float, float>, 4>{{{12, 12}, {46, 15}, {26, 40}, {57, 46}}}) {
                float dx = std::abs(x - center.first), dy = std::abs(y - center.second);
                dx = std::min(dx, COLS - dx); dy = std::min(dy, ROWS - dy);
                patch = std::max(patch, std::exp(-(dx * dx + dy * dy) / 80.f));
            }
            fertility[cell] = environment == Environment::Patchy ? 0.08f + 0.92f * patch : 1.f;
            food[cell] = chance() * FOOD_CAP * fertility[cell];
        }
        std::vector<int> cells(AREA);
        std::iota(cells.begin(), cells.end(), 0);
        std::shuffle(cells.begin(), cells.end(), random);
        const int count = std::clamp(static_cast<int>(std::round(parameters.startingPopulation)), 1, AREA / 2);
        for (int i = 0; i < count * 2; ++i) {
            occupied[cells[i]] = static_cast<int>(organisms.size());
            organisms.push_back({cells[i], i % 2, 12.f, parameters.moveChance[i % 2], parameters.birthEnergy[i % 2], 0});
        }
        measure();
        history.push_back(population);
    }
    float seasonFactor() const {
        if (environment != Environment::Seasonal) return 1.f;
        const double phase = 6.283185307179586 * (rounds % static_cast<std::uint64_t>(parameters.seasonLength)) / parameters.seasonLength;
        return 0.15f + 0.85f * static_cast<float>((1.0 + std::cos(phase)) / 2.0);
    }
    Organism offspring(const Organism& parent, int cell) {
        Organism child = parent;
        child.cell = cell;
        ++child.generation;
        // Each inherited trait has its own chance to mutate. Color is ancestry.
        if (chance() < parameters.mutationChance) {
            child.moveChance = std::clamp(child.moveChance + (chance() * 2.f - 1.f) * parameters.mutationStrength, 0.f, 1.f);
            ++mutations;
        }
        if (chance() < parameters.mutationChance) {
            child.birthEnergy = std::clamp(child.birthEnergy + (chance() * 2.f - 1.f) * parameters.mutationStrength * 38.f, 12.f, 50.f);
            ++mutations;
        }
        return child;
    }
    void advance() {
        const float growth = parameters.regrowth * seasonFactor();
        // Only unoccupied cells regrow food. Food is not created by organisms.
        for (int cell = 0; cell < AREA; ++cell)
            if (occupied[cell] == -1)
                food[cell] = std::min(FOOD_CAP, food[cell] + growth * fertility[cell]);
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
            if (organism.energy > MOVE_COST && chance() < organism.moveChance) {
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
            if (organism.energy >= organism.birthEnergy) {
                nearby = neighbors(organism.cell);
                std::shuffle(nearby.begin(), nearby.end(), random);
                for (int cell : nearby) {
                    if (occupied[cell] == -1) {
                        organism.energy = (organism.energy - 2.f) / 2.f;
                        babies.push_back(offspring(organism, cell));
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


// Headless regression checks: Ecosystem2.exe --self-test
void selfTest() {
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("Ecosystem2 regression failed"); };
    for (auto mode : {Environment::Stable, Environment::Seasonal, Environment::Patchy}) {
        Ecosystem a, b;
        a.environment = b.environment = mode; a.reset(); b.reset();
        const auto initialFood = a.food;
        check(a.population.red == 80 && a.population.blue == 80);
        for (int tick = 0; tick < 300; ++tick) {
            a.advance(); b.advance();
            check(a.food == b.food && a.organisms.size() == b.organisms.size());
            std::array<bool, AREA> seen{};
            for (std::size_t i = 0; i < a.organisms.size(); ++i) {
                const auto& agent = a.organisms[i]; const auto& other = b.organisms[i];
                check(agent.cell >= 0 && agent.cell < AREA && !seen[agent.cell]); seen[agent.cell] = true;
                check(a.occupied[agent.cell] == static_cast<int>(i) && agent.energy > 0);
                check(agent.moveChance >= 0 && agent.moveChance <= 1 && agent.birthEnergy >= 12 && agent.birthEnergy <= 50);
                check(agent.cell == other.cell && agent.energy == other.energy && agent.moveChance == other.moveChance &&
                      agent.birthEnergy == other.birthEnergy && agent.species == other.species && agent.generation == other.generation);
            }
            for (float amount : a.food) check(amount >= 0 && amount <= FOOD_CAP);
        }
        a.reset(); check(a.food == initialFood && a.rounds == 0 && a.mutations == 0 && a.oldestGeneration == 0);
        if (mode == Environment::Patchy) check(*std::min_element(a.fertility.begin(), a.fertility.end()) < 0.2f);
        if (mode == Environment::Seasonal) {
            check(std::abs(a.seasonFactor() - 1.f) < 0.001f);
            a.rounds = 200; check(std::abs(a.seasonFactor() - 0.15f) < 0.001f);
            a.rounds = 400; check(std::abs(a.seasonFactor() - 1.f) < 0.001f);
        }
    }
    Ecosystem world;
    Organism parent{0, 1, 30, 0.6f, 25, 4};
    world.parameters.mutationChance = 0;
    auto child = world.offspring(parent, 1);
    check(child.cell == 1 && child.species == 1 && child.generation == 5 && child.energy == parent.energy);
    check(child.moveChance == parent.moveChance && child.birthEnergy == parent.birthEnergy);
    world.parameters.mutationChance = 1;
    bool changed = false;
    for (int i = 0; i < 1000; ++i) {
        child = world.offspring(child, 1);
        changed |= child.moveChance != parent.moveChance || child.birthEnergy != parent.birthEnergy;
        check(child.moveChance >= 0 && child.moveChance <= 1 && child.birthEnergy >= 12 && child.birthEnergy <= 50);
    }
    check(changed && world.mutations == 2000);
    world.parameters.mutationChance = 0;
    world.organisms = {{0, 0, 30, 0, 18, 0}}; world.occupied.fill(-1); world.occupied[0] = 0;
    world.food.fill(0); world.parameters.regrowth = 0;
    world.advance(); check(world.organisms.size() == 2);
    check(world.organisms[0].energy == world.organisms[1].energy && world.organisms[1].generation == 1);
    world.reset(); world.food.fill(0);
    for (int i = 0; i < 200; ++i) world.advance();
    check(world.organisms.empty());
    check(Ecosystem::cellAt(-1, -1) == AREA - 1);
    std::cout << "Passed: all presets, deterministic evolution, occupancy, food, seasons, inheritance, mutation bounds, newborn timing, reset and starvation.\n";
}

std::string number(float value, int decimals = 2) {
    std::ostringstream out; out << std::fixed << std::setprecision(decimals) << value; return out.str();
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
    sf::RenderWindow window(sf::VideoMode({1200, 920}), "Ecosystem 2 - Environments and Evolution",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60); window.setKeyRepeatEnabled(false);
    Ecosystem world;
    bool running = false, showHabitat = false;
    float accumulator = 0;
    sf::Clock clock;
    int dragging = -1;
    const sf::Color red(242, 100, 95), blue(92, 169, 255), muted(169, 184, 202);
    const sf::FloatRect play({720, 66}, {140, 36}), step({875, 66}, {140, 36}), hundred({1030, 66}, {145, 36});
    const sf::FloatRect resetButton({720, 112}, {218, 36}), seedButton({953, 112}, {222, 36});
    const sf::FloatRect habitatButton({24, 665}, {240, 32});
    std::array<sf::FloatRect, 3> presetButtons;
    for (int i = 0; i < 3; ++i) presetButtons[i] = {{720.f + i * 155, 165}, {145, 36}};
    std::vector<Slider> sliders;
    auto slider = [&](std::string label, float& value, float low, float high, int digits) {
        const float x = 720.f + (sliders.size() % 2) * 235.f;
        const float y = 417.f + (sliders.size() / 2) * 64.f;
        sliders.push_back({label, &value, low, high, digits, {{x, y}, {215, 6}}});
    };
    slider("Speed / second", world.parameters.speed, 1, 120, 0);
    slider("Food regrowth", world.parameters.regrowth, 0, 0.5f, 2);
    slider("Start count each *", world.parameters.startingPopulation, 10, 300, 0);
    slider("Season length", world.parameters.seasonLength, 50, 2000, 0);
    slider("Mutation chance", world.parameters.mutationChance, 0, 1, 2);
    slider("Mutation strength", world.parameters.mutationStrength, 0, 0.3f, 2);
    slider("Red energy cost", world.parameters.upkeep[0], 0.1f, 2, 2);
    slider("Blue energy cost", world.parameters.upkeep[1], 0.1f, 2, 2);
    slider("Red birth energy *", world.parameters.birthEnergy[0], 12, 50, 1);
    slider("Blue birth energy *", world.parameters.birthEnergy[1], 12, 50, 1);
    slider("Red move chance *", world.parameters.moveChance[0], 0, 1, 2);
    slider("Blue move chance *", world.parameters.moveChance[1], 0, 1, 2);
    auto changeSlider = [&](int i, float x) {
        auto& item = sliders[i];
        const float ratio = std::clamp((x - item.track.position.x) / item.track.size.x, 0.f, 1.f);
        *item.value = item.minimum + ratio * (item.maximum - item.minimum);
        if (item.decimals == 0) *item.value = std::round(*item.value);
    };
    auto reset = [&] { running = false; accumulator = 0; dragging = -1; world.reset(); };
    auto text = [&](const std::string& string, float x, float y, unsigned size, sf::Color color) {
        sf::Text label(font, string, size); label.setPosition({x, y}); label.setFillColor(color); window.draw(label);
    };
    const bool preview = argc > 2 && std::string(argv[1]) == "--preview";
    if (preview) {
        world.environment = Environment::Patchy; world.reset();
        for (int i = 0; i < 200; ++i) world.advance();
    }
    while (window.isOpen()) {
        const float elapsed = std::min(clock.restart().asSeconds(), 0.1f);
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->is<sf::Event::FocusLost>()) { running = false; dragging = -1; accumulator = 0; }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Space) { running = !running; accumulator = 0; }
                if (key->code == sf::Keyboard::Key::R) reset();
                if (key->code == sf::Keyboard::Key::N) { running = false; accumulator = 0; world.advance(); }
                if (key->code == sf::Keyboard::Key::H) showHabitat = !showHabitat;
            }
            if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    const auto point = window.mapPixelToCoords(click->position);
                    if (play.contains(point)) { running = !running; accumulator = 0; }
                    if (step.contains(point) || hundred.contains(point)) {
                        running = false; accumulator = 0;
                        for (int i = 0; i < (hundred.contains(point) ? 100 : 1); ++i) world.advance();
                    }
                    if (resetButton.contains(point)) reset();
                    if (seedButton.contains(point)) { ++world.seed; reset(); }
                    if (habitatButton.contains(point)) showHabitat = !showHabitat;
                    for (int i = 0; i < 3; ++i)
                        if (presetButtons[i].contains(point)) { world.environment = static_cast<Environment>(i); reset(); }
                    for (std::size_t i = 0; i < sliders.size(); ++i) {
                        const auto& track = sliders[i].track;
                        if (sf::FloatRect({track.position.x - 8, track.position.y - 12}, {track.size.x + 16, 30}).contains(point)) {
                            dragging = static_cast<int>(i); changeSlider(dragging, point.x); break;
                        }
                    }
                }
            }
            if (const auto* move = event->getIf<sf::Event::MouseMoved>())
                if (dragging >= 0) changeSlider(dragging, window.mapPixelToCoords(move->position).x);
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
        text("Ecosystem 2 / Adaptation", 24, 16, 27, sf::Color::White);
        text("Red and blue track ancestry. Traits evolve within each lineage.", 24, 52, 16, muted);
        constexpr float CELL = 10, BOARD_X = 24, BOARD_Y = 90;
        sf::RectangleShape tile({CELL - 1, CELL - 1});
        for (int cell = 0; cell < AREA; ++cell) {
            const float amount = showHabitat ? world.fertility[cell] : world.food[cell] / FOOD_CAP;
            tile.setPosition({BOARD_X + (cell % COLS) * CELL, BOARD_Y + (cell / COLS) * CELL});
            tile.setFillColor(sf::Color(23, static_cast<std::uint8_t>(32 + 125 * amount), 37));
            window.draw(tile);
        }
        for (const auto& agent : world.organisms) {
            const float bright = 0.4f + 0.6f * std::clamp(agent.energy / 25.f, 0.f, 1.f);
            const auto color = agent.species == 0 ? red : blue;
            tile.setFillColor(sf::Color(static_cast<std::uint8_t>(color.r * bright),
                static_cast<std::uint8_t>(color.g * bright), static_cast<std::uint8_t>(color.b * bright)));
            tile.setPosition({BOARD_X + (agent.cell % COLS) * CELL, BOARD_Y + (agent.cell / COLS) * CELL});
            window.draw(tile);
        }
        sf::RectangleShape panel({500, 920}); panel.setPosition({700, 0}); panel.setFillColor(sf::Color(28, 36, 48)); window.draw(panel);
        const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        auto button = [&](const sf::FloatRect& bounds, const std::string& label, bool active = false) {
            sf::RectangleShape shape(bounds.size); shape.setPosition(bounds.position);
            shape.setFillColor(active ? sf::Color(40, 113, 98) : bounds.contains(mouse) ? sf::Color(65, 106, 139) : sf::Color(43, 73, 97));
            window.draw(shape); text(label, bounds.position.x + 10, bounds.position.y + 7, 15, sf::Color::White);
        };
        button(habitatButton, showHabitat ? "View: habitat fertility [H]" : "View: available food [H]", showHabitat);
        text("Green = " + std::string(showHabitat ? "fertility" : "food") + "; brightness of organisms = energy", 24, 702, 14, muted);
        text("Population / last " + std::to_string(world.history.size()) + " rounds", 24, 737, 16, muted);
        const sf::Vector2f gp(24, 768), gs(640, 105);
        sf::RectangleShape graph(gs); graph.setPosition(gp); graph.setFillColor(sf::Color(28, 36, 48)); window.draw(graph);
        int peak = 10; for (auto point : world.history) peak = std::max({peak, point.red, point.blue});
        for (int s = 0; s < 2; ++s) {
            sf::VertexArray line(sf::PrimitiveType::LineStrip);
            for (std::size_t i = 0; i < world.history.size(); ++i) {
                const int count = s == 0 ? world.history[i].red : world.history[i].blue;
                line.append(sf::Vertex{{gp.x + i * gs.x / std::max<std::size_t>(1, world.history.size() - 1),
                    gp.y + gs.y - 2 - count * (gs.y - 4) / peak}, s == 0 ? red : blue});
            }
            window.draw(line);
        }
        text("Scale: 0 - " + std::to_string(peak) + "    Wrapping world. No fighting or spontaneous life.", 24, 885, 13, muted);
        text("Environment and evolution", 720, 16, 25, sf::Color::White);
        button(play, running ? "Pause" : "Play"); button(step, "+1 round"); button(hundred, "+100 rounds");
        button(resetButton, "Reset same seed"); button(seedButton, "New seed + reset");
        for (int i = 0; i < 3; ++i) button(presetButtons[i], environmentNames[i], i == static_cast<int>(world.environment));
        text(std::string(running ? "RUNNING" : "PAUSED") + "  Round " + std::to_string(world.rounds) + "  Seed " + std::to_string(world.seed), 720, 213, 18, sf::Color::White);
        auto traits = [&](int s, float y, sf::Color color) {
            const int count = s == 0 ? world.population.red : world.population.blue;
            const std::string averages = count ? "move " + number(world.meanMove[s]) + "  birth " + number(world.meanBirth[s], 1) : "extinct";
            text(std::string(s == 0 ? "Red: " : "Blue: ") + std::to_string(count) + "   " + averages, 720, y, 17, color);
        };
        traits(0, 246, red); traits(1, 274, blue);
        text("Generation: " + std::to_string(world.oldestGeneration) + "   Trait mutations: " + std::to_string(world.mutations), 720, 305, 16, muted);
        text("Food avg: " + number(world.averageFood, 1) + " / 6    Growth factor: " + number(world.seasonFactor()), 720, 332, 16, muted);
        const std::array<std::string, 3> descriptions = {"Constant food supply across the world.", "Growth cycles between 100% and 15%.", "Fertile islands surrounded by poor habitat."};
        text(descriptions[static_cast<int>(world.environment)], 720, 360, 15, muted);
        for (const auto& item : sliders) {
            text(item.label, item.track.position.x, item.track.position.y - 24, 13, muted);
            text(number(*item.value, item.decimals), item.track.position.x + 175, item.track.position.y - 24, 13, sf::Color::White);
            sf::RectangleShape track(item.track.size); track.setPosition(item.track.position); track.setFillColor(sf::Color(63, 77, 95)); window.draw(track);
            const float ratio = (*item.value - item.minimum) / (item.maximum - item.minimum);
            sf::CircleShape handle(6); handle.setOrigin({6, 6});
            handle.setPosition({item.track.position.x + ratio * item.track.size.x, item.track.position.y + 3});
            handle.setFillColor(sf::Color(128, 208, 185)); window.draw(handle);
        }
        text("* Founder settings apply on reset. Other sliders apply live.", 720, 778, 14, muted);
        text("Preset selection resets; parameters and seed are kept.", 720, 801, 14, muted);
        text("Movement costs energy; early births mean smaller offspring.", 720, 824, 13, muted);
        text("Space: play/pause   N: step   R: reset   H: food/habitat", 720, 853, 14, sf::Color::White);
        text("Focus loss pauses. Extinct lineages stay extinct.", 720, 883, 14, muted);
        if (preview) {
            sf::Texture capture(window.getSize()); capture.update(window);
            if (!capture.copyToImage().saveToFile(argv[2])) return 2;
            window.close();
        } else window.display();
    }
    return 0;
}
