#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <deque>
#include <algorithm>
#include <string>

using namespace std;

// Board configuration
const int GRID_SIZE = 20;
const int CELL_SIZE = 30;

// A position on the grid
struct Position {
    int x;
    int y;
};

// Validate against the last planned turn, then apply one turn per move.
// A short buffer remembers quick corners without building up input lag.
void queueTurn(deque<Position>& turns, Position direction, Position requested) {
    const Position last = turns.empty() ? direction : turns.back();
    if (turns.size() >= 2 ||
        (requested.x == last.x && requested.y == last.y) ||
        (requested.x == -last.x && requested.y == -last.y))
        return;
    turns.push_back(requested);
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    // Create the window
    sf::RenderWindow window(
        sf::VideoMode({600, 600}),
        "Snake C++"
    );

    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    // Create the snake
    vector<Position> snake = {
        {5, 10},
        {4, 10},
        {3, 10}
    };

    Position direction = {1, 0};
    deque<Position> turns;
    Position food = {12, 10};

    bool gameOver = false;
    int score = 0;
    bool focused = true;
    string endReason;

    // Create our graphical square
    sf::RectangleShape square({28.f, 28.f});

    // Controls how fast the snake moves
    sf::Clock clock;
    const float moveDelay = 0.15f;
    float elapsed = 0.f;

    while (window.isOpen()) {
        // Preserve fractional time, but do not race through missed moves
        // after a long stall (for example, dragging the window).
        elapsed += min(clock.restart().asSeconds(), moveDelay);

        // 1. Handle events
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (event->is<sf::Event::FocusLost>()) {
                focused = false;
                turns.clear();
                elapsed = 0.f;
            }
            if (event->is<sf::Event::FocusGained>()) {
                focused = true;
                elapsed = 0.f;
                clock.restart();
            }

            if (const auto* key =
                    event->getIf<sf::Event::KeyPressed>()) {

                if (key->code == sf::Keyboard::Key::R && gameOver) {
                    snake = {{5, 10}, {4, 10}, {3, 10}};
                    direction = {1, 0};
                    food = {12, 10};
                    turns.clear();
                    score = 0;
                    gameOver = false;
                    endReason.clear();
                    elapsed = 0.f;
                    clock.restart();
                }

                if (!focused || gameOver)
                    continue;

                if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W)
                    queueTurn(turns, direction, {0, -1});
                else if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S)
                    queueTurn(turns, direction, {0, 1});
                else if (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A)
                    queueTurn(turns, direction, {-1, 0});
                else if (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D)
                    queueTurn(turns, direction, {1, 0});
            }
        }

        // 2. Update game
        if (!window.isOpen())
            break;
        if (!focused || gameOver)
            elapsed = 0.f;

        while (focused && !gameOver && elapsed >= moveDelay) {

            elapsed -= moveDelay;
            if (!turns.empty()) {
                direction = turns.front();
                turns.pop_front();
            }

            Position newHead = {
                snake[0].x + direction.x,
                snake[0].y + direction.y
            };

            // Wall collision
            if (newHead.x < 0 || newHead.x >= GRID_SIZE ||
                newHead.y < 0 || newHead.y >= GRID_SIZE) {
                gameOver = true;
                endReason = "Hit the wall";
            }

            if (!gameOver) {
                bool eating =
                    newHead.x == food.x &&
                    newHead.y == food.y;

                // Self collision
                // Moving into the old tail is allowed
                // when the snake is not growing.
                size_t count = snake.size();
                if (!eating)
                    count--;

                for (size_t i = 0; i < count; i++) {
                    if (newHead.x == snake[i].x &&
                        newHead.y == snake[i].y) {
                        gameOver = true;
                        endReason = "Hit your body";
                        break;
                    }
                }

                if (!gameOver) {
                    snake.insert(snake.begin(), newHead);

                    if (eating) {
                        score++;

                        // Generate food in a free cell
                        vector<Position> freeCells;

                        for (int y = 0; y < GRID_SIZE; y++) {
                            for (int x = 0; x < GRID_SIZE; x++) {
                                bool occupied = false;

                                for (const auto& segment : snake) {
                                    if (segment.x == x &&
                                        segment.y == y) {
                                        occupied = true;
                                        break;
                                    }
                                }

                                if (!occupied)
                                    freeCells.push_back({x, y});
                            }
                        }

                        if (freeCells.empty()) {
                            gameOver = true;
                            endReason = "You win!";
                        } else {
                            food = freeCells[
                                rand() % freeCells.size()
                            ];
                        }
                    } else {
                        snake.pop_back();
                    }
                }
            }

            if (gameOver)
                cout << endReason << " Score: " << score
                     << ". Press R to restart." << endl;
        }

        window.setTitle("Snake C++ | Score: " + to_string(score) +
            (gameOver ? " | " + endReason + " | R: restart" :
             !focused ? " | Paused" : " | Arrows / WASD"));

        // 3. Draw the game
        window.clear(sf::Color(25, 25, 25));

        // Draw food
        square.setFillColor(sf::Color::Red);
        square.setPosition({
            float(food.x * CELL_SIZE),
            float(food.y * CELL_SIZE)
        });
        window.draw(square);

        // Draw snake
        square.setFillColor(sf::Color::Green);

        for (const Position& segment : snake) {
            square.setFillColor(&segment == &snake.front()
                ? (gameOver ? sf::Color::Yellow : sf::Color::Cyan)
                : sf::Color::Green);
            square.setPosition({
                float(segment.x * CELL_SIZE),
                float(segment.y * CELL_SIZE)
            });

            window.draw(square);
        }

        window.display();
    }

    return 0;
}
