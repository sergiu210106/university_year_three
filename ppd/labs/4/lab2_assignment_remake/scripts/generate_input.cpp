#include <fstream>
#include <iostream>
#include <random>
#include <string>

// generate_input <rows> <cols> <output_file> <seed>
int main(int argc, char **argv) {
    if (argc != 5) {
        std::cerr << "usage: generate_input <rows> <cols> <output_file> <seed>\n";
        return 1;
    }
    try {
        int rows = std::stoi(argv[1]), cols = std::stoi(argv[2]);
        if (rows < 1 || cols < 1) throw std::runtime_error("dimensions must be positive");
        std::mt19937 random(std::stoul(argv[4]));
        std::uniform_int_distribution<int> pixel(0, 255), weight(-5, 5);
        std::ofstream output(argv[3]);
        if (!output) throw std::runtime_error("cannot open output file");
        output << rows << ' ' << cols << '\n';
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) output << (j ? " " : "") << pixel(random);
            output << '\n';
        }
        output << "3\n";
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) output << (j ? " " : "") << weight(random);
            output << '\n';
        }
        output.close();
        if (!output) throw std::runtime_error("cannot write output file");
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
