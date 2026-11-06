
#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iostream>

using namespace std;

// Board configuration
const int GRID_SIZE = 20;
const int CELL_SIZE = 30;

// A position on the grid
struct Position {
    int x;
    int y;
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    // Create the window
    sf::RenderWindow window(
        sf::VideoMode({600, 600}),
        "Snake C++"
    );

    window.setFramerateLimit(60);

    // Create the snake
    vector<Position> snake = {
        {5, 10},
        {4, 10},
        {3, 10}
    };

    Position direction = {1, 0};
    Position food = {12, 10};

    bool gameOver = false;
    int score = 0;

    // Create our graphical square
    sf::RectangleShape square({28.f, 28.f});

    // Controls how fast the snake moves
    sf::Clock clock;
    float moveDelay = 0.15f;

    while (window.isOpen()) {

        // 1. Handle events
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* key =
                    event->getIf<sf::Event::KeyPressed>()) {

                if (key->code == sf::Keyboard::Key::Up &&
                    direction.y != 1)
                    direction = {0, -1};

                else if (key->code == sf::Keyboard::Key::Down &&
                         direction.y != -1)
                    direction = {0, 1};

                else if (key->code == sf::Keyboard::Key::Left &&
                         direction.x != 1)
                    direction = {-1, 0};

                else if (key->code == sf::Keyboard::Key::Right &&
                         direction.x != -1)
                    direction = {1, 0};
            }
        }

        // 2. Update game
        if (!gameOver &&
            clock.getElapsedTime().asSeconds() >= moveDelay) {

            clock.restart();

            Position newHead = {
                snake[0].x + direction.x,
                snake[0].y + direction.y
            };

            // Wall collision
            if (newHead.x < 0 || newHead.x >= GRID_SIZE ||
                newHead.y < 0 || newHead.y >= GRID_SIZE) {
                gameOver = true;
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
                            cout << "You win! Score: "
                                 << score << endl;
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
                cout << "Game over! Score: "
                     << score << endl;
        }

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
