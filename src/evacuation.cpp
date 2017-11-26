/**
 * @file evacuation.cpp
 * Cellular automaton definition.
 */

#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

#include <iostream>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cassert>

#include <climits>
#include <queue>
#include <sstream>

#include "evacuation.h"
#include "bitmap.h"

#define shuffle(arr) \
    std::random_shuffle(arr.begin(), arr.end())

#define RAND (((double) rand()) / (RAND_MAX))
#define PROB(val) val > RAND

using namespace Evacuation;

CA::CA(unsigned height, unsigned width) :
    height{height}, width{width},
    cells(height, std::vector<Cell>(width))
{}

std::vector<CellPosition> CA::cell_neighbourhood(
    size_t row, size_t col, int cell_types
) const
{{{
    std::vector<CellPosition> neighbours;

    push_if(neighbours, row - 1, col, cell_types);
    push_if(neighbours, row, col - 1, cell_types);
    push_if(neighbours, row, col + 1, cell_types);
    push_if(neighbours, row + 1, col, cell_types);
    push_if(neighbours, row - 1, col - 1, cell_types);
    push_if(neighbours, row - 1, col + 1, cell_types);
    push_if(neighbours, row + 1, col - 1, cell_types);
    push_if(neighbours, row + 1, col + 1, cell_types);

    return neighbours;
}}}

std::vector<CellPosition> CA::cell_neighbourhood(
    CellPosition position, int cell_types
) const
{{{
    return cell_neighbourhood(position.first, position.second, cell_types);
}}}

bool CA::evolve()
{{{
    bool res = false;
    stat.time++;

    std::vector<CellPosition> people;
    for (size_t row = 0; row < this->height; row++) {
        for (size_t col = 0; col < this->width; col++) {
            auto &current = cells[row][col];
            switch (current.type) {
                case PersonAppearance:
                case Obstacle:
                case Empty:
                case Person:
                {
                    // propagation of smoke
                    int smoke_neigh =
                        cell_neighbourhood(row, col, SmokeCells).size();
                    if (PROB( smoke_neigh/ 8.0 * smoke_spreading_rate)) {
                        if (current.type == Obstacle) {
                            current.type = ObstacleWithSmoke;
                        }
                        else if (current.type == Person) {
                            current.type = PersonWithSmoke;
                            stat.smoke_exposed++;
                        }
                        else {
                            current.type = Smoke;
                        }
                    }
                    if (current.type == Person) {
                        // remember person position
                        people.push_back(CellPosition(row, col));
                    }
                    break;
                }
                case PersonAtExit:
                    // remove people at exits
                    stat.evacuated++;
                    stat.total_time += stat.time;
                    current.type = Exit;
                    break;
                case PersonWithSmoke:
                    stat.smoke_exposed++;
                    // remember person position
                    people.push_back(CellPosition(row, col));
                default:
                    ;
            }
        }
    }

    // Recompute exit distances
    recompute_shortest_paths();

    // Propagate people
    res = !people.empty();
    shuffle(people);
    for (auto person : people) {
        auto neighbours = cell_neighbourhood(person);
        if (!neighbours.empty()) {
            // non-deterministic choice of the minimum
            shuffle(neighbours);
            // find min value
            CellPosition next_cell = neighbours[0];
            for (auto c : neighbours) {
                if (distance(next_cell) > distance(c)) {
                    next_cell = c;
                }
            }

            // distance to the next cell
            int person_distance = distance(person);
            int next_distance = distance(next_cell);
            int diff = person_distance - next_distance;
            // assert (diff == 0 || diff == -1 || diff == 1);
            // move to the cell with lesser exit distance or same distance
            // with some probability
            if (diff >= 1 ||
                (diff == 0 && PROB(chaos_rate)))
            {
                // move from empty or smoke cell
                stat.moves++;
                cell(person).type =
                    cell(person).type == Person ? Empty : Smoke;
                auto &next_type = cell(next_cell).type;
                if (next_type == Smoke) {
                    next_type = PersonWithSmoke;
                }
                else if (next_type == Exit) {
                    next_type = PersonAtExit;
                }
                else {
                    next_type = Person;
                }
            }
        }
    }

    return res;
}}}

void CA::add_smoke(int smoke)
{{{
    std::vector<CellPosition> empty_cells;
    for (size_t i = 0; i < this->height; i++) {
        for (size_t j = 0; j < this->width; j++) {
            if (cells[i][j].type == Empty) {
                empty_cells.push_back(CellPosition(i,j));
            }
        }
    }

    shuffle(empty_cells);
    if (smoke > 0 && smoke > empty_cells.size()) {
        throw std::logic_error("cannot have more smoke cells than empty cells");
    }

    while (!empty_cells.empty() && smoke-- > 0) {
        cell(empty_cells.back()).type = Smoke;
        empty_cells.pop_back();
    }

}}}

// somehow distribute people over empty cells
void CA::add_people(int people_count)
{{{ 
    stat.pedestrians = people_count;
    std::vector<CellPosition> empty_cells;
    std::vector<CellPosition> empty_priority_cells;
    // mark all cells with possible person appearance
    for (size_t i = 0; i < this->height; i++) {
        for (size_t j = 0; j < this->width; j++) {
            if (cells[i][j].type == Empty) {
                empty_cells.push_back(CellPosition(i,j));
            }
            else if (cells[i][j].type == PersonAppearance) {
                empty_priority_cells.push_back(CellPosition(i,j));
            }
        }
    }

    if (people_count > empty_cells.size() + empty_priority_cells.size()) {
        throw std::logic_error("cannot have more people than empty cells");
    }

    shuffle(empty_priority_cells);
    shuffle(empty_cells);

    empty_cells.insert(
        empty_cells.end(), empty_priority_cells.begin(),
        empty_priority_cells.end());

    // placing people according to the priority
    while (!empty_cells.empty() && people_count-- > 0) {
        cell(empty_cells.back()).type = Person;
        empty_cells.pop_back();
    }
}}}

void CA::recompute_shortest_paths()
{{{
    // Reset exit distances
    std::vector<std::vector<double>> min_distances(
		this->height, std::vector<double>(this->width)
	);
    for (unsigned row = 0; row < this->height; row++) {
        for (unsigned col = 0; col < this->width; col++) {
            min_distances[row][col] = (double)UINT_MAX;
        }
    }
    
    // Apply Dijkstra for each exit state and keep minimum distances
    for(auto es: exits) {
    	// Initialize current distances
        std::vector<std::vector<double>> distances(
			this->height, std::vector<double>(this->width)
		);
        for (unsigned row = 0; row < this->height; row++) {
        	for (unsigned col = 0; col < this->width; col++) {
                distances[row][col] = (double) UINT_MAX;
            }
        }
        distances[es.first][es.second] = 0.0;

        // Vector of visited states
        std::vector<std::vector<bool>> visited(
			this->height, std::vector<bool>(this->width)
		);
        for (unsigned row = 0; row < this->height; row++) {
            for (unsigned col = 0; col < this->width; col++) {
                visited[row][col] = false;
            }
        }

        // Queue of unprocessed successors
        std::vector<CellPosition> unprocessed;
        unprocessed.push_back(es);

        // Compute distances
        while(!unprocessed.empty()) {
			// Find unprocessed state with minimum exit distance
			CellPosition current = unprocessed[0];
			int min = 0;
			double current_distance = distances[current.first][current.second];
			for(int i = 1; i < unprocessed.size(); i++) {
				CellPosition cp = unprocessed[i];
                double d = distances[cp.first][cp.second];
                if(d < current_distance) {
                    min = i;
                    current = cp;
                    current_distance = d;
				}
			}

			// Visit minimum
            unprocessed[min] = unprocessed.back();
			unprocessed.pop_back();
			visited[current.first][current.second] = true;
            
            // Cell types that can be considered adjacent
            constexpr int succTypes =  Empty | Exit | Person | Smoke
               | PersonAppearance | PersonAtExit | PersonWithSmoke;

            // Generate successors
            std::vector<CellPosition> successors;
            successors = cell_neighbourhood(current, succTypes);

            // Compute successor distance
            double accrual = 1.0;
            if(cell(current).type & (Person | PersonWithSmoke)) {
                accrual *= occupied_distance;
            }
            if(cell(current).type & (Smoke | PersonWithSmoke)) {
                accrual *= smoke_distance;
            }
            double next_distance = current_distance + accrual;

            // Process all successors
            for(CellPosition successor: successors) {
                // Skip processed successors
                int r = successor.first, c = successor.second;
                if(!visited[r][c]) {
                    visited[r][c] = true;
                    // Update distance
                    if(next_distance < distances[r][c]) {
                        distances[r][c] = next_distance;
                    }
                    unprocessed.push_back(successor);
                }
            }
        }

        // Pick best distance
        for(unsigned row = 0; row < this->height; row++) {
            for(unsigned col = 0; col < this->width; col++) {
                if(distances[row][col] < min_distances[row][col]) {
                    min_distances[row][col] = distances[row][col];
                }
            }
        }
    }

    // Store final result
    for(unsigned row = 0; row < this->height; row++) {
        for(unsigned col = 0; col < this->width; col++) {
        	cell(row,col).exit_distance = min_distances[row][col];
        }
    }
}}}

CA CA::load(const std::string &filename)
{{{
    // Load from image
    CA ca = Bitmap::load(filename);

    // Identify exit states
    for(unsigned row = 0; row < ca.height; row++) {
        for(unsigned col = 0; col < ca.width; col++) {
        	if(ca.cell(row, col).type == Exit) {
                ca.exits.push_back(CellPosition(row,col));
            }
        }
    }

    // Resolve distances
    ca.recompute_shortest_paths();

    // Success
    return ca;
}}}

void CA::show()
{{{
    Bitmap::store(*this, "output.bmp");
}}}

void CA::print_statistics() const noexcept
{{{
	std::cout << stat.str();
}}}


std::string Statistics::str() const noexcept
{{{
	std::ostringstream ss;
    // TODO exits - number, size, loading
    ss << "*********************************************************\n";
    float traveled = moves * cell_width;
    float realtime = time * time_step;
    ss << "Total pedestrians                 : " << pedestrians
        << std::endl;
    ss << "People evacuated                  : " << evacuated
        << std::endl;
//    std::cout << "Exit total size                   : " << std::endl;
    ss << "Total evacuation time             : " << realtime << " s"
        << std::endl;
    ss << "Mean time per person in smoke     : "
        << smoke_exposed * time_step / evacuated << " s" << std::endl;
    ss << "Mean evacuation time per person   : "
        << total_time * time_step / evacuated << " s" << std::endl;
    ss << "Total distance traveled           : " << traveled << " m"
        << std::endl;
    ss << "Mean distance traveled per person : "
        << traveled / pedestrians << " m" << std::endl;
    ss << "*********************************************************\n";

    return ss.str();
}}}