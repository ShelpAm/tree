#include <cstdlib>
#include <libtree/tree.hpp>
#include <print>

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::println(std::cerr, "Usage: {} <from> <to>", *argv);
        return EXIT_FAILURE;
    }

    std::span args{argv, static_cast<std::size_t>(argc)};

    std::println("Syncing {} to {}...", args[1], args[2]);

    MerkleTree const src{args[1]};
    MerkleTree dest{args[2]};

    dest.sync_from(src);
}
