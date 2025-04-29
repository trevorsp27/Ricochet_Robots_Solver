#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>
#include <tbb/concurrent_queue.h> 
#include <tbb/concurrent_hash_map.h> 
#include <tbb/parallel_for.h> 
#include <bitset>
#include <cmath>
#include <algorithm>
#include <array>
#include <chrono>
#include <atomic> 
#include <string>
#include <limits>
#include <cctype>
#include <map>
#include <tuple>
#include <optional>
#include <queue> 
#include <iomanip> 

using State = uint64_t;

enum class Direction : uint8_t {
    UP    = 1 << 0,
    DOWN  = 1 << 1,
    LEFT  = 1 << 2,
    RIGHT = 1 << 3
};

enum class DiagonalOrientation {
    NW_SE,
    NE_SW
};

inline int dirToIndex(Direction dir) {
    return static_cast<int>(log2(static_cast<double>(dir)));
}

class Board {
public:
    static const std::map<int, char> robotIndexToColor;
    static const std::map<char, int> robotColorToIndex;
    static const std::map<int, char> fileColorIndexToChar;

    Board(int width, int height)
        : width(width), height(height),
          walls(height, std::vector<uint8_t>(width, 0)),
          targetX(-1), targetY(-1), targetColor('\0'), targetRobot(-1) {}

    void addWall(int x, int y, Direction dir) {
        validateCoordinates(x, y);
        walls[y][x] |= static_cast<uint8_t>(dir);
        int nx = x, ny = y;
        Direction opposite_dir;
        switch (dir) {
            case Direction::UP:    ny--; opposite_dir = Direction::DOWN; break;
            case Direction::DOWN:  ny++; opposite_dir = Direction::UP;   break;
            case Direction::LEFT:  nx--; opposite_dir = Direction::RIGHT; break;
            case Direction::RIGHT: nx++; opposite_dir = Direction::LEFT;  break;
            default: return;
        }
        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
             walls[ny][nx] |= static_cast<uint8_t>(opposite_dir);
        }
    }

    void addDiagonalWall(int x, int y, char color, DiagonalOrientation orientation) {
        validateCoordinates(x, y);
        diagonalWalls[{x, y}] = {color, orientation};
    }

    void addOpening(int x, int y, Direction edge) {
        validateCoordinates(x, y);
        openings.push_back({x, y, edge});
    }

    void setTarget(int x, int y, char color) {
        validateCoordinates(x, y);
        if (robotColorToIndex.find(color) == robotColorToIndex.end()) {
            throw std::invalid_argument("Invalid target color");
        }
        targetX = x;
        targetY = y;
        targetColor = color;
        targetRobot = robotColorToIndex.at(color);
    }

    bool hasWall(int x, int y, Direction dir) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return true;
        return walls[y][x] & static_cast<uint8_t>(dir);
    }

    std::optional<std::pair<char, DiagonalOrientation>> getDiagonalWallInfo(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return std::nullopt;
        auto it = diagonalWalls.find({x, y});
        if (it != diagonalWalls.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool isOpening(int x, int y, Direction edge) const {
        bool on_edge = false;
        switch (edge) {
            case Direction::UP:    on_edge = (y == 0); break;
            case Direction::DOWN:  on_edge = (y == height - 1); break;
            case Direction::LEFT:  on_edge = (x == 0); break;
            case Direction::RIGHT: on_edge = (x == width - 1); break;
        }
        if (!on_edge) return false;

        for (const auto& opening : openings) {
            if (std::get<0>(opening) == x && std::get<1>(opening) == y && std::get<2>(opening) == edge) {
                return true;
            }
        }
        return false;
    }

    char getRobotColor(int index) const {
        try {
            return robotIndexToColor.at(index);
        } catch (const std::out_of_range& oor) {
            return '?';
        }
    }

    std::pair<int, int> getTargetPosition() const {
        return {targetX, targetY};
    }

    char getTargetColor() const {
        return targetColor;
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    void print(const std::array<std::pair<int, int>, 5>& current_robots) const {
        const std::map<int, char> robotIndexToColor = {
            {0, 'R'}, {1, 'B'}, {2, 'G'}, {3, 'Y'}, {4, 'P'}
        };

        std::cout << "   "; 
        for (int x = 0; x < width; ++x) {
            std::cout << " " << std::setw(2) << x << " "; 
        }
        std::cout << std::endl;

        std::cout << "  +"; 
        for (int x = 0; x < width; ++x) {
            std::cout << "---+";
        }
        std::cout << std::endl;

        for (int y = 0; y < height; ++y) {
            std::cout << std::setw(2) << y; 

            for (int x = 0; x < width; ++x) {
                if (hasWall(x, y, Direction::LEFT)) {
                    std::cout << "|";
                } else {
                    std::cout << " ";
                }

                char robotChar = ' ';
                int robotIdx = -1;
                for (int i = 0; i < 5; ++i) {
                    if (current_robots[i].first == x && current_robots[i].second == y) {
                        robotIdx = i;
                        try {
                            robotChar = robotIndexToColor.at(i);
                        } catch (const std::out_of_range& oor) {
                            robotChar = '?';
                        }
                        break;
                    }
                }

                bool isTarget = (x == targetX && y == targetY);

                std::string colorSequence;

                if (isTarget) {
                    colorSequence += std::to_string(getBackgroundColorCode(targetColor)) + ";";
                }

                if (robotChar != ' ') {
                    colorSequence += std::to_string(getForegroundColorCode(robotChar) + 60) + ";";
                } else if (isTarget) {
                    colorSequence += std::to_string(getForegroundColorCode(targetColor)) + ";";
                }

                if (!colorSequence.empty()) {
                    colorSequence.pop_back();
                    std::cout << "\033[" << (robotChar != ' ' ? "1;" : "") << colorSequence << "m";
                }

                if (robotChar != ' ') {
                    std::cout << " " << robotChar << " ";
                } else if (isTarget) {
                    std::cout << " T ";
                } else {
                    auto diag_info = getDiagonalWallInfo(x, y);
                    if (diag_info) {
                        char diag_color_char = diag_info->first;
                        DiagonalOrientation diag_orient = diag_info->second;
                        std::cout << "\033[" << std::to_string(getForegroundColorCode(diag_color_char)) << "m";
                        std::cout << " " << (diag_orient == DiagonalOrientation::NW_SE ? '\\' : '/') << " ";
                        std::cout << "\033[0m";
                    } else {
                        std::cout << "   ";
                    }
                }

                std::cout << "\033[0m";
            }

            if (hasWall(width - 1, y, Direction::RIGHT)) {
                std::cout << "|";
            } else {
                std::cout << " ";
            }
            std::cout << std::endl;

            std::cout << "  +";
            for (int x = 0; x < width; ++x) {
                if (hasWall(x, y, Direction::DOWN)) {
                    std::cout << "---+";
                } else {
                    std::cout << "   +";
                }
            }
            std::cout << std::endl;
        }
    }

friend class Solver;

private:
    void validateCoordinates(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            throw std::out_of_range("Coordinates out of bounds");
        }
    }

    int getForegroundColorCode(char color) const {
        switch (std::toupper(color)) {
            case 'R': return 31;
            case 'G': return 32;
            case 'Y': return 33;
            case 'B': return 34;
            case 'P': return 35;
            default:  return 37;
        }
    }

    int getBackgroundColorCode(char color) const {
        switch (std::toupper(color)) {
            case 'R': return 41;
            case 'G': return 42;
            case 'Y': return 43;
            case 'B': return 44;
            case 'P': return 45;
            default:  return 40;
        }
    }

    int width;
    int height;
    std::vector<std::vector<uint8_t>> walls;
    int targetX;
    int targetY;
    char targetColor;
    int targetRobot;

    std::map<std::pair<int, int>, std::pair<char, DiagonalOrientation>> diagonalWalls;
    std::vector<std::tuple<int, int, Direction>> openings;
};

const std::map<int, char> Board::robotIndexToColor = {
    {0, 'R'}, {1, 'B'}, {2, 'G'}, {3, 'Y'}, {4, 'P'}
};
const std::map<char, int> Board::robotColorToIndex = {
    {'R', 0}, {'B', 1}, {'G', 2}, {'Y', 3}, {'P', 4}
};
const std::map<int, char> Board::fileColorIndexToChar = {
    {0, 'R'}, {1, 'B'}, {2, 'G'}, {3, 'Y'}, {4, 'P'}
};

std::array<std::pair<int, int>, 5> decode(State s) {
    std::array<std::pair<int, int>, 5> robots;
    for (int i = 0; i < 5; ++i) {
        uint8_t bits = (s >> (8*i)) & 0xFF;
        int x = bits & 0x0F;
        int y = (bits >> 4) & 0x0F;
        robots[i] = {x, y};
    }
    return robots;
}

State encode(const std::array<std::pair<int, int>, 5>& robots) {
    State s = 0;
    for (int i = 0; i < 5; ++i) {
        s |= (static_cast<State>(robots[i].first & 0x0F)) << (8*i);
        s |= (static_cast<State>(robots[i].second & 0x0F)) << (8*i + 4);
    }
    return s;
}

struct TbbStateHashCompare {
    static size_t hash(State s) {
        return std::hash<State>()(s);
    }
    static bool equal(State s1, State s2) {
        return s1 == s2;
    }
};

struct Move {
    int robot;
    Direction dir;
};

class Solver {
public:
    Solver(const Board& board, State initial)
        : board(board), initial(initial), targetRobot(board.targetRobot)
    {
        if (targetRobot < 0 || targetRobot >= 5) {
            throw std::runtime_error("Target robot not set or invalid on the board before creating Solver.");
        }
    }

    std::vector<Move> solve() {
        tbb::concurrent_queue<State> queue;
        tbb::concurrent_hash_map<State, std::pair<State, Move>, TbbStateHashCompare> visited;
        std::vector<Move> solution;
        State solution_state = 0;

        queue.push(initial);
        visited.insert({initial, {initial, {-1, Direction::UP}}});

        std::atomic<bool> solutionFound = false;

        while (!solutionFound && !queue.empty()) {
            std::vector<State> current_level;
            State s;
            while (queue.try_pop(s)) current_level.push_back(s);

            if (current_level.empty()) break;

            tbb::parallel_for(tbb::blocked_range<size_t>(0, current_level.size()),
                [&](const auto& r) {
                    for (size_t i = r.begin(); i < r.end(); ++i) {
                        if (solutionFound.load()) return;

                        const State& current = current_level[i];
                        auto robots = decode(current);

                        if (checkSolution(current)) {
                            bool expected = false;
                            if (solutionFound.compare_exchange_strong(expected, true)) {
                                solution_state = current;
                            }
                            return;
                        }

                        for (int robot_idx = 0; robot_idx < 5; ++robot_idx) {
                            for (Direction dir : {Direction::UP, Direction::DOWN,
                                                 Direction::LEFT, Direction::RIGHT}) {

                                if (solutionFound.load()) return;

                                auto [start_x, start_y] = robots[robot_idx];
                                auto [nx, ny] = simulateMove(start_x, start_y, dir, robots, robot_idx);

                                if (start_x == nx && start_y == ny) continue;

                                State new_state = encode(robots, robot_idx, nx, ny);
                                Move move{robot_idx, dir};

                                tbb::concurrent_hash_map<State, std::pair<State, Move>, TbbStateHashCompare>::accessor acc;
                                if (visited.insert(acc, new_state)) {
                                    acc->second = {current, move};
                                    queue.push(new_state);
                                }
                                acc.release();
                            }
                        }
                    }
                });

            if (solutionFound.load()) break;
        }

        if (solutionFound) {
            solution = reconstructPath(visited, solution_state);
        }

        return solution;
    }

    std::vector<Move> solve_sequential() {
        std::queue<State> queue;
        std::unordered_map<State, std::pair<State, Move>> visited; 
        std::vector<Move> solution;
        State solution_state = 0;
        bool solutionFound = false; 

        queue.push(initial);
        visited[initial] = {initial, {-1, Direction::UP}}; 

        while (!solutionFound && !queue.empty()) {
            size_t level_size = queue.size();
            for (size_t i = 0; i < level_size; ++i) {
                State current = queue.front();
                queue.pop();

                auto robots = decode(current);

                if (checkSolution(current)) {
                    solution_state = current;
                    solutionFound = true;
                    break; 
                }

                for (int robot_idx = 0; robot_idx < 5; ++robot_idx) {
                    for (Direction dir : {Direction::UP, Direction::DOWN,
                                         Direction::LEFT, Direction::RIGHT}) {

                        auto [start_x, start_y] = robots[robot_idx];
                        auto [nx, ny] = simulateMove(start_x, start_y, dir, robots, robot_idx);

                        if (start_x == nx && start_y == ny) continue;

                        State new_state = encode(robots, robot_idx, nx, ny);
                        Move move{robot_idx, dir};

                        if (visited.find(new_state) == visited.end()) {
                            visited[new_state] = {current, move};
                            queue.push(new_state); 
                        }
                    }
                }
            }
            if (solutionFound) break;
        }

        if (solutionFound) {
            solution = reconstructPathSequential(visited, solution_state);
        }

        return solution;
    }

    std::vector<Move> reconstructPath(
        const tbb::concurrent_hash_map<State, std::pair<State, Move>, TbbStateHashCompare>& visited,
        State endState) const {

        std::vector<Move> path;
        State current = endState;
        while (true) {
            tbb::concurrent_hash_map<State, std::pair<State, Move>, TbbStateHashCompare>::const_accessor acc;
            if (!visited.find(acc, current)) {
                std::cerr << "Error: State " << current << " not found during parallel path reconstruction!" << std::endl; 
                path.clear(); 
                break;
            }
            const auto& [prev_state, move] = acc->second;
            acc.release(); 

            if (move.robot == -1) { 
                break;
            }

            path.push_back(move);
            if (current == prev_state) { 
                std::cerr << "Error: Path reconstruction loop detected (parallel)! State " << current << " points to itself." << std::endl;
                path.clear(); 
                break;
            }
            current = prev_state;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    std::vector<Move> reconstructPathSequential(
        const std::unordered_map<State, std::pair<State, Move>>& visited,
        State endState) const {

        std::vector<Move> path;
        State current = endState;
        while (true) {
            auto it = visited.find(current);
            if (it == visited.end()) {
                 std::cerr << "Error: State " << current << " not found during sequential path reconstruction!" << std::endl;
                 path.clear(); 
                 break;
            }
            const auto& [prev_state, move] = it->second;

            if (move.robot == -1) { 
                break;
            }

            path.push_back(move);

            if (current == prev_state) { 
                std::cerr << "Error: Path reconstruction loop detected (sequential)! State " << current << " points to itself." << std::endl;
                 path.clear(); 
                 break;
            }
            current = prev_state;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

private:
    State encode(const std::array<std::pair<int, int>, 5>& current_robots,
                 int robot_to_move, int next_x, int next_y) const {
        auto temp_robots = current_robots;
        temp_robots[robot_to_move] = {next_x, next_y};
        return ::encode(temp_robots);
    }

    std::pair<int, int> simulateMove(int start_x, int start_y, Direction initial_dir, 
                                     const std::array<std::pair<int, int>, 5>& current_robots,
                                     int moving_robot_index) const {
        int curr_x = start_x;
        int curr_y = start_y;
        char moving_robot_color = board.getRobotColor(moving_robot_index);
        Direction current_move_dir = initial_dir; 

        while (true) {
            if (board.hasWall(curr_x, curr_y, current_move_dir)) {
                break; 
            }

            int next_x = curr_x;
            int next_y = curr_y;
            Direction opposite_dir;
            switch (current_move_dir) {
                case Direction::UP:    next_y--; opposite_dir = Direction::DOWN; break;
                case Direction::DOWN:  next_y++; opposite_dir = Direction::UP;   break;
                case Direction::LEFT:  next_x--; opposite_dir = Direction::RIGHT; break;
                case Direction::RIGHT: next_x++; opposite_dir = Direction::LEFT;  break;
                default: return {curr_x, curr_y}; 
            }

            if (next_x < 0 || next_x >= board.getWidth() || next_y < 0 || next_y >= board.getHeight()) {
                break; 
            }

            if (board.hasWall(next_x, next_y, opposite_dir)) {
                break; 
            }

            bool collision = false;
            for (int i = 0; i < 5; ++i) {
                if (i == moving_robot_index) continue;
                if (current_robots[i].first == next_x && current_robots[i].second == next_y) {
                    collision = true;
                    break;
                }
            }
            if (collision) {
                break; 
            }

            auto diag_info = board.getDiagonalWallInfo(next_x, next_y);
            if (diag_info) {
                auto [wall_color, orientation] = *diag_info;
                if (wall_color != moving_robot_color) {
                    curr_x = next_x;
                    curr_y = next_y;

                    Direction entry_direction = current_move_dir; 
                    Direction next_move_dir = current_move_dir; 

                    if (orientation == DiagonalOrientation::NW_SE) { 
                        if (entry_direction == Direction::RIGHT) next_move_dir = Direction::DOWN;
                        else if (entry_direction == Direction::LEFT) next_move_dir = Direction::UP;
                        else if (entry_direction == Direction::DOWN) next_move_dir = Direction::RIGHT;
                        else if (entry_direction == Direction::UP) next_move_dir = Direction::LEFT;
                    } else { 
                        if (entry_direction == Direction::RIGHT) next_move_dir = Direction::UP;
                        else if (entry_direction == Direction::LEFT) next_move_dir = Direction::DOWN;
                        else if (entry_direction == Direction::DOWN) next_move_dir = Direction::LEFT;
                        else if (entry_direction == Direction::UP) next_move_dir = Direction::RIGHT;
                    }

                    current_move_dir = next_move_dir;

                    continue; 
                }
            }

            curr_x = next_x;
            curr_y = next_y;

        }
        return {curr_x, curr_y}; 
    }

    bool checkSolution(State s) const {
        auto pos = decode(s)[targetRobot];
        return pos.first == board.targetX && pos.second == board.targetY;
    }

    const Board& board;
    State initial;
    int targetRobot;
};

const std::unordered_map<int, std::vector<Direction>> wallMapping = {
    {0,  {}},
    {1,  {Direction::UP}},
    {2,  {Direction::DOWN}}, 
    {3,  {Direction::UP, Direction::DOWN}}, 
    {4,  {Direction::LEFT}}, 
    {5,  {Direction::UP, Direction::LEFT}}, 
    {6,  {Direction::DOWN, Direction::LEFT}}, 
    {7,  {Direction::UP, Direction::DOWN, Direction::LEFT}}, 
    {8,  {Direction::RIGHT}}, 
    {9,  {Direction::UP, Direction::RIGHT}}, 
    {10, {Direction::DOWN, Direction::RIGHT}}, 
    {11, {Direction::UP, Direction::DOWN, Direction::RIGHT}}, 
    {12, {Direction::LEFT, Direction::RIGHT}}, 
    {13, {Direction::UP, Direction::LEFT, Direction::RIGHT}}, 
    {14, {Direction::DOWN, Direction::LEFT, Direction::RIGHT}}, 
    {15, {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT}} 
};

void loadFromFile(Board& board, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    int y = 0;
    int boardHeight = board.getHeight();
    int boardWidth = board.getWidth();

    while (std::getline(file, line) && y < boardHeight) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        int x = 0;
        int value;

        while (iss >> value && x < boardWidth) {
            if (value >= 0 && value <= 15) {
                if (value & static_cast<int>(Direction::UP))    board.addWall(x, y, Direction::UP);
                if (value & static_cast<int>(Direction::DOWN))  board.addWall(x, y, Direction::DOWN);
                if (value & static_cast<int>(Direction::LEFT))  board.addWall(x, y, Direction::LEFT);
                if (value & static_cast<int>(Direction::RIGHT)) board.addWall(x, y, Direction::RIGHT);
            } else if (value >= 16 && value <= 25) {
                char color = '?';
                DiagonalOrientation orientation = DiagonalOrientation::NW_SE;

                switch (value) {
                    case 16: color = 'Y'; orientation = DiagonalOrientation::NW_SE; break;
                    case 17: color = 'Y'; orientation = DiagonalOrientation::NE_SW; break;
                    case 18: color = 'R'; orientation = DiagonalOrientation::NW_SE; break;
                    case 19: color = 'R'; orientation = DiagonalOrientation::NE_SW; break;
                    case 20: color = 'B'; orientation = DiagonalOrientation::NW_SE; break;
                    case 21: color = 'B'; orientation = DiagonalOrientation::NE_SW; break;
                    case 22: color = 'G'; orientation = DiagonalOrientation::NW_SE; break;
                    case 23: color = 'G'; orientation = DiagonalOrientation::NE_SW; break;
                    case 24: color = 'P'; orientation = DiagonalOrientation::NW_SE; break;
                    case 25: color = 'P'; orientation = DiagonalOrientation::NE_SW; break;
                }
                if (color != '?') {
                    board.addDiagonalWall(x, y, color, orientation);
                } else {
                    std::cerr << "Warning: Unhandled diagonal wall code '" << value
                              << "' at (" << x << ", " << y << "). Skipping." << std::endl;
                }
            } else {
                throw std::runtime_error("Invalid wall value '" + std::to_string(value) +
                                         "' at (" + std::to_string(x) + ", " +
                                         std::to_string(y) + "). Expected 0-25.");
            }
            x++;
        }

        if (x != boardWidth) {
            throw std::runtime_error("Incomplete line at row " + std::to_string(y) +
                                     ". Expected " + std::to_string(boardWidth) +
                                     " values, found " + std::to_string(x));
        }
        y++;
    }

    if (y != boardHeight) {
        throw std::runtime_error("File has incomplete grid. Expected " +
                                 std::to_string(boardHeight) + " rows, found " +
                                 std::to_string(y));
    }
}

int main() {
    const int BOARD_SIZE = 16;
    Board board(BOARD_SIZE, BOARD_SIZE);

    std::array<std::pair<int, int>, 5> initial_positions = {{
        {0, 1}, {15, 1}, {14, 14}, {0, 0}, {7, 8}
    }};

    try {
        loadFromFile(board, "boardstate.txt");
        std::cout << "Board loaded from boardstate.txt (diagonal codes 16-25 supported)" << std::endl;

        std::cout << "\nInitial Board State:" << std::endl;
        board.print(initial_positions); 
        std::cout << std::endl;

        int target_x, target_y;
        char target_color_char;
        std::string target_color_str;
        bool valid_color = false;

        while (!valid_color) {
            std::cout << "Enter target robot color (R, B, G, Y, P): ";
            std::cin >> target_color_str;
            if (target_color_str.length() == 1) {
                target_color_char = std::toupper(target_color_str[0]);
                if (Board::robotColorToIndex.count(target_color_char)) {
                    valid_color = true;
                } else {
                    std::cerr << "Invalid color. Please enter R, B, G, Y, or P." << std::endl;
                }
            } else {
                std::cerr << "Invalid input. Please enter a single character." << std::endl;
            }
            if (!valid_color) {
                 std::cin.clear();
                 std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }

        while (true) {
            std::cout << "Enter target X coordinate (0-" << BOARD_SIZE - 1 << "): ";
            if (!(std::cin >> target_x) || target_x < 0 || target_x >= BOARD_SIZE) {
                std::cerr << "Invalid X coordinate. Please enter a number between 0 and " << BOARD_SIZE - 1 << "." << std::endl;
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            } else {
                 std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
                break;
            }
        }

        while (true) {
            std::cout << "Enter target Y coordinate (0-" << BOARD_SIZE - 1 << "): ";
            if (!(std::cin >> target_y) || target_y < 0 || target_y >= BOARD_SIZE) {
                std::cerr << "Invalid Y coordinate. Please enter a number between 0 and " << BOARD_SIZE - 1 << "." << std::endl;
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            } else {
                 std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
                break;
            }
        }

        board.setTarget(target_x, target_y, target_color_char);
        std::cout << "Target set: Robot " << board.getTargetColor()
                  << " to (" << board.getTargetPosition().first << ", "
                  << board.getTargetPosition().second << ")" << std::endl;


    } catch (const std::exception& e) {
        std::cerr << "Error initializing board: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Initial positions set." << std::endl;

    State initial_state = encode(initial_positions);

    char solver_choice = ' ';
    while (solver_choice != 's' && solver_choice != 'p') {
        std::cout << "\nChoose solver type (s = sequential, p = parallel): ";
        if (!(std::cin >> solver_choice)) {
             std::cerr << "Error reading input. Exiting." << std::endl;
             return 1; 
        }
        solver_choice = std::tolower(solver_choice);
        if (solver_choice != 's' && solver_choice != 'p') {
            std::cerr << "Invalid choice. Please enter 's' or 'p'." << std::endl;
            std::cin.clear(); 
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');


    try {
        Solver solver(board, initial_state);
        std::vector<Move> solution;
        std::chrono::duration<double> elapsed_time;

        if (solver_choice == 's') {
            std::cout << "\n--- Running Sequential Solver ---" << std::endl << std::flush; 
            auto start_seq = std::chrono::high_resolution_clock::now();
            solution = solver.solve_sequential();
            auto end_seq = std::chrono::high_resolution_clock::now();
            elapsed_time = end_seq - start_seq;

            if (solution.empty()) {
                std::cout << "Sequential: No solution found." << std::endl << std::flush; 
            } else {
                std::cout << "Sequential: Solution found in " << solution.size() << " moves ("
                          << elapsed_time.count() << " seconds):\n" << std::flush; 
                char robot_chars[] = {'R', 'B', 'G', 'Y', 'P'};
                for (const auto& move : solution) {
                    std::string dir_str;
                    switch(move.dir) {
                        case Direction::UP:    dir_str = "UP"; break;
                        case Direction::DOWN:  dir_str = "DOWN"; break;
                        case Direction::LEFT:  dir_str = "LEFT"; break;
                        case Direction::RIGHT: dir_str = "RIGHT"; break;
                        default:               dir_str = "?"; break;
                    }
                    if (move.robot >= 0 && move.robot < 5) {
                       std::cout << "  Robot " << robot_chars[move.robot] << " (" << move.robot << ") -> " << dir_str << "\n";
                    } else {
                       std::cout << "  Invalid robot index in move: " << move.robot << "\n";
                    }
                }
                 std::cout << std::flush; 
            }

        } else { 
            std::cout << "\n--- Running Parallel Solver (TBB) ---" << std::endl << std::flush; 
            auto start_par = std::chrono::high_resolution_clock::now();
            solution = solver.solve(); 
            auto end_par = std::chrono::high_resolution_clock::now();
            elapsed_time = end_par - start_par;

            if (solution.empty()) {
                std::cout << "Parallel: No solution found." << std::endl << std::flush; 
            } else {
                std::cout << "Parallel: Solution found in " << solution.size() << " moves ("
                          << elapsed_time.count() << " seconds):\n" << std::flush; 
                char robot_chars[] = {'R', 'B', 'G', 'Y', 'P'};
                for (const auto& move : solution) {
                    std::string dir_str;
                    switch(move.dir) {
                        case Direction::UP:    dir_str = "UP"; break;
                        case Direction::DOWN:  dir_str = "DOWN"; break;
                        case Direction::LEFT:  dir_str = "LEFT"; break;
                        case Direction::RIGHT: dir_str = "RIGHT"; break;
                        default:               dir_str = "?"; break;
                    }
                    if (move.robot >= 0 && move.robot < 5) {
                       std::cout << "  Robot " << robot_chars[move.robot] << " (" << move.robot << ") -> " << dir_str << "\n";
                    } else {
                       std::cout << "  Invalid robot index in move: " << move.robot << "\n";
                    }
                }
                 std::cout << std::flush; 
            }
        }

    } catch (const std::exception& e) {
         std::cerr << "Error during solving: " << e.what() << std::endl;
         return 1;
    }

    return 0;
}
