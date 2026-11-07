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
const float CELL_SIZE = 30.0f;

int main(){
    // Create the window
    sf::RenderWindow window(
        sf::VideoMode({800, 800}),
        "Conway Game of Life"
    );

    // Create the matrix, a vector of vector
    std::vector<std::vector<int>> grid(GRID_SIZE, std::vector<int>(GRID_SIZE, 0));
    grid[3][5] = 1;
    grid[4][5] = 1;
    grid[5][5] = 1;
    // fill with dead cells
    sf::RectangleShape cell({CELL_SIZE - 1, CELL_SIZE - 1});
    cell.setFillColor(sf::Color(40, 60, 70));

    //main loop
    while (window.isOpen())
    {
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

    window.clear(sf::Color::Black);
    for (int row = 0; row < GRID_SIZE; row++){
        for (int col = 0; col < GRID_SIZE; col++)
        {
            float x = 100 + col * CELL_SIZE;
            float y = 100 + row * CELL_SIZE;

            cell.setPosition({x, y});
            // if cell is alive, green, otherwise stay black
            if (grid[row][col] == 1){
                    cell.setFillColor(sf::Color::Green);
                }
            else{
                    cell.setFillColor(sf::Color(40, 60, 70));
                }
            window.draw(cell);
        }
    }

    window.display();
    }

    return 0;
}