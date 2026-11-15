#include "../cpp/common.hpp"
#include <cstdint>
#include <random>
#include <string>

int main(int argc, char **argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: generate_input <digits> <output> <seed>");
        int length = std::stoi(argv[1]);
        if (length < 1 || length > MAX_DIGITS)
            throw std::runtime_error("digit count must be 1..100000");
        std::mt19937 random(static_cast<std::uint32_t>(std::stoul(argv[3])));
        std::ofstream output(argv[2]);
        if (!output) throw std::runtime_error("cannot open generated input");
        output << length << '\n';
        for (int i = 0; i < length; ++i) {
            unsigned int digit = i + 1 == length ? 1 + random() % 9 : random() % 10;
            output << digit << (i + 1 == length ? '\n' : ' ');
        }
        output.close();
        if (!output) throw std::runtime_error("cannot finish generated input");
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
