/**
 * @file evacuation.h
 * Cell & CA interfaces.
 */

#ifndef __evacuation_h
#define __evacuation_h

#include <vector>
#include <list>
#include <iostream>
#include <climits>
#include <cassert>

namespace Evacuation {

// Simulation parameters:

/// Real seconds per simulation step
const float time_step = 0.3;
/// Width of a cell in meters
const float cell_width = 0.4;
/// Probability that person moves to cell with same exit distance
const float chaos_rate = 0.25;
/// Coefficient of smoke spreading.
const float smoke_spreading_rate = 0.2;
/// Occupied cell distance factor (1.0 => usual distance)
const float occupied_distance = 2.0;
/// Effect of smoke on exit distance (1.0 => usual distance)
const float smoke_distance = 5.0;

/// Position in matrix.
using CellPosition = std::pair<size_t, size_t>;

/** Type of a cell. */
enum CellType {
    Empty =             0b0000000001,
    Exit =              0b0000000010,
    Wall =              0b0000000100,
    Obstacle =          0b0000001000,
    Person =            0b0000010000,
    Smoke =             0b0000100000,
    PersonAppearance =  0b0001000000,
    PersonAtExit =      0b0010000000,
    PersonWithSmoke =   0b0100000000,
    ObstacleWithSmoke = 0b1000000000
};

// Groups of cell types used for cell filtering

/// Cells where person can move into.
constexpr int EmptyCells = Empty | Smoke | PersonAppearance | Exit;
/// Cells which have an impact of smoke propagation.
constexpr int SmokeCells = Smoke | ObstacleWithSmoke | PersonWithSmoke;

/** Cell structure. */
struct Cell {
    /** Cell type. */
    CellType type;
    /** Distance to upper border. */
    int row;
    /** Distance to left border. */
    int col;
    /** Distance (in pseudo-hops) to nearest exit. */
    unsigned exit_distance;

    Cell() :
        type{Empty}, row{-1}, col{-1}, exit_distance{UINT_MAX}
    {}
};

/**
 * Cellular automaton.
 * The state space is assumed to be 2-dimensional and constant.
 */
class CA {
public:
    /// Number of rows
    unsigned height;
    /// Number of columns in EACH row
    unsigned width;
    /// Precomuted vector of exit states
    std::vector<CellPosition> exits;

    CA(unsigned height, unsigned width);
    ~CA() = default;

    /// Apply transition function on CA states.
    /// @return false if there are no people to evacuate, true otherwise
    bool evolve();

    /// Distribute people over the building.
    void add_people(int people);

    /// Distribute smoke over the building.
    void add_smoke(int smoke);

    /**
     * Load model description from a bitmap.
     * @param filename name of input file
     * @return instance of CA class
     * @throw invalid_argument if failed to process input file
     * @note loaded model might be populated
     */
    static CA load(const std::string &filename);

    /// Store model description to "output.bmp".
    void show();

    /// Display simulation statistics. TODO
    void print_statistics() const noexcept;

    // Inline methods:

    /// Retrieve a cell at a specified position
    inline Cell& cell(int row, int col) {
        return cells[row][col];
    }

private:
    /// 2D matrix of cells.
    std::vector<std::vector<Cell>> cells;

    /// variables for computing some statistics TODO
    int pedestrians;
    int evacuated;
    int time;
    int smoke_exposed;
    int moves;
    int total_time;

    // methods

    /// Return Moore neighbourhood of cells of specified type at current
    /// position. Default returned cell types are defined above.
    std::vector<CellPosition> cell_neighbourhood(
        CellPosition position, int cell_types = EmptyCells
    ) const;
    std::vector<CellPosition> cell_neighbourhood(
        size_t row, size_t col, int cell_types = EmptyCells
    ) const;

    /// Recompute exit distances.
    void recompute_shortest_paths();

    // Inline methods:

    /// @ return true if cell coordinates are valid
    inline bool cell_check(int row, int col) const {
        if(row < 0 || col < 0) {
            return false;
        }
        return (unsigned) row < this->height && (unsigned) col < this->width;
    }

    /// push an position(row,col) to the vector if cell at this position is
    /// empty
    inline void push_if(
        std::vector<CellPosition> &vec, size_t row, size_t col,
        int cell_types) const
    {
        if (cell_check(row, col) && (cells[row][col].type & cell_types)) {
            vec.push_back(CellPosition(row, col));
        }
    }

    /// return a distance to exit from cell at specified position
    inline int distance(CellPosition pos) const {
        return distance(pos.first, pos.second);
    }

    /// return a distance to exit from cell at specified position
    inline int distance(size_t row, size_t col) const {
        return cells[row][col].exit_distance;
    }

    /// Retrieve a cell at a specified position
    inline Cell& cell(CellPosition pos) {
        return cell(pos.first, pos.second);
    }
};

class Statistics {

};
} // end of namespace

#endif
