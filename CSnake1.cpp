
#include <iostream>
using namespace std;

int main() {
    // Board dimensions
    const int width = 10;
    const int height = 10;

    // Snake starting position
    int snakeX = 5;
    int snakeY = 5;

    // Food position
    int foodX = 7;
    int foodY = 7;

    int score = 0;
    char direction;
    bool gameOver = false;

    while (!gameOver) {

        // Draw the board
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                if (x == snakeX && y == snakeY)
                    cout << "O ";
                else if (x == foodX && y == foodY)
                    cout << "* ";
                else
                    cout << ". ";
            }
            cout << endl;
        }

        cout << "Score: " << score << endl;
        cout << "Move (W/A/S/D): ";
        cin >> direction;

        // Move the snake
        if (direction == 'w')
            snakeY--;
        else if (direction == 's')
            snakeY++;
        else if (direction == 'a')
            snakeX--;
        else if (direction == 'd')
            snakeX++;

        // Check wall collision
        if (snakeX < 0 || snakeX >= width ||
            snakeY < 0 || snakeY >= height) {
            gameOver = true;
        }

        // Check food collision
        if (snakeX == foodX && snakeY == foodY) {
            score++;
            foodX = (foodX + 3) % width;
            foodY = (foodY + 2) % height;
        }

        cout << endl;
    }

    cout << "GAME OVER! Score: " << score << endl;

    return 0;
}
