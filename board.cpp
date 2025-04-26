#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>

enum class Direction : uint8_t {
    UP    = 1 << 0,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
};

class Board {
public:
    Board(int width, int height) 
        : width(width), height(height), 
          walls(height, std::vector<uint8_t>(width, 0)),
          targetX(-1), targetY(-1), targetColor('\0') {}

    void addWall(int x, int y, Direction dir) {
        validateCoordinates(x, y);
        walls[y][x] |= static_cast<uint8_t>(dir);
    }

    void setTarget(int x, int y, char color) {
        validateCoordinates(x, y);
        targetX = x;
        targetY = y;
        targetColor = color;
    }

    bool hasWall(int x, int y, Direction dir) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return walls[y][x] & static_cast<uint8_t>(dir);
    }

    std::pair<int, int> getTargetPosition() const {
        return {targetX, targetY};
    }

    char getTargetColor() const {
        return targetColor;
    }

    void addRobot(int x, int y, char color) {
        validateCoordinates(x, y);
        robots.emplace_back(std::make_pair(x, y), color);
    }

    void print() const {
        for (int y = 0; y < height; ++y) {
            // Print top walls
            for (int x = 0; x < width; ++x) {
                std::cout << "+";
                if (hasWall(x, y, Direction::UP)) {
                    std::cout << "---";
                } else {
                    std::cout << "   ";
                }
            }
            std::cout << "+" << std::endl;

            // Print cell content and side walls
            for (int x = 0; x < width; ++x) {
                // Left wall
                if (hasWall(x, y, Direction::LEFT)) {
                    std::cout << "|";
                } else {
                    std::cout << " ";
                }

                // Check if there's a robot here
                char robotColor = ' ';
                for (const auto& robot : robots) {
                    if (robot.first.first == x && robot.first.second == y) {
                        robotColor = robot.second;
                        break;
                    }
                }

                // Build color sequence
                std::string colorSequence;
                bool isTarget = (x == targetX && y == targetY);
                
                // Add background color for target
                if (isTarget) {
                    colorSequence += std::to_string(getBackgroundColorCode(targetColor)) + ";";
                }
                
                // Add foreground color for robot
                if (robotColor != ' ') {
                    colorSequence += std::to_string(getColorCode(robotColor)) + ";";
                }
                
                // Apply colors
                if (!colorSequence.empty()) {
                    colorSequence.pop_back(); // Remove trailing ;
                    std::cout << "\033[1;" << colorSequence << "m";
                }

                // Print content
                if (robotColor != ' ') {
                    std::cout << " " << robotColor << " ";
                } else if (isTarget) {
                    std::cout << " \u25A0 "; // ■ symbol for empty target
                } else {
                    std::cout << "   ";
                }

                // Reset colors
                std::cout << "\033[0m";
            }

            // Print right wall for the last cell in the row
            if (hasWall(width-1, y, Direction::RIGHT)) {
                std::cout << "|";
            }
            std::cout << std::endl;
        }

        // Print bottom walls for the last row
        for (int x = 0; x < width; ++x) {
            std::cout << "+";
            if (hasWall(x, height-1, Direction::DOWN)) {
                std::cout << "---";
            } else {
                std::cout << "   ";
            }
        }
        std::cout << "+" << std::endl;
    }

private:
    void validateCoordinates(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            throw std::out_of_range("Coordinates out of bounds");
        }
    }

    static int getColorCode(char c) {
        switch (c) {
            case 'R': return 31; // Red
            case 'B': return 34; // Blue
            case 'G': return 32; // Green
            case 'Y': return 33; // Yellow
            default: return 37;  // Default white
        }
    }

    static int getBackgroundColorCode(char c) {
        switch (c) {
            case 'R': return 41; // Red background
            case 'B': return 44; // Blue background
            case 'G': return 42; // Green background
            case 'Y': return 43; // Yellow background
            default: return 49;  // Default background
        }
    }

    int width;
    int height;
    std::vector<std::vector<uint8_t>> walls;
    int targetX;
    int targetY;
    char targetColor;
    std::vector<std::pair<std::pair<int, int>, char>> robots;
};

// Updated wall mapping with all valid codes
const std::unordered_map<int, std::vector<Direction>> wallMapping = {
    {0,  {}}, // No walls
    {1,  {Direction::LEFT}},
    {2,  {Direction::UP}},
    {3,  {Direction::RIGHT}},
    {4,  {Direction::DOWN}},
    {5,  {Direction::LEFT, Direction::UP}},
    {6,  {Direction::UP, Direction::RIGHT}},
    {7,  {Direction::RIGHT, Direction::DOWN}},
    {8,  {Direction::DOWN, Direction::LEFT}},
    {9,  {Direction::LEFT, Direction::UP, Direction::RIGHT}},
    {10, {Direction::UP, Direction::RIGHT, Direction::DOWN}},
    {11, {Direction::RIGHT, Direction::DOWN, Direction::LEFT}}
};

void loadFromFile(Board& board, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    std::string line;
    int y = 0;
    while (std::getline(file, line) && y < 16) {
        std::istringstream iss(line);
        int x = 0;
        int value;
        
        while (iss >> value && x < 16) {
            auto it = wallMapping.find(value);
            if (it == wallMapping.end()) {
                throw std::runtime_error("Invalid wall value at (" + 
                                         std::to_string(x) + ", " + 
                                         std::to_string(y) + ")");
            }
            
            for (Direction dir : it->second) {
                board.addWall(x, y, dir);
            }
            x++;
        }
        
        if (x != 16) {
            throw std::runtime_error("Incomplete line at row " + std::to_string(y));
        }
        y++;
    }
    
    if (y != 16) {
        throw std::runtime_error("File has incomplete grid");
    }
}

int main() {
    try {
        Board board(16, 16);
        loadFromFile(board, "C:/Users/tspin/OneDrive/Desktop/ricochet-robot-solver-main/test_board.txt");
        
        // Example setup (replace with your actual robot positions)
        board.addRobot(0, 0, 'R');
        board.setTarget(15, 15, 'B'); // Blue target at bottom-right
        
        board.print();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}