#include <cstdlib>
#include <filesystem>
#include <libtree/tree.hpp>
#include <print>
#include <string_view>

template <typename... Args>
void errorln(std::format_string<Args...> fmt, Args &&...args)
{
    std::println(std::cerr, fmt, std::forward<Args>(args)...);
}

int main(int argc, char **argv)
{
    using namespace std::string_literals;
    namespace fs = std::filesystem;

    if (argc == 0) {
        errorln("Can't run with argc being 0");
        return EXIT_FAILURE;
    }

    std::span args{argv, static_cast<std::size_t>(argc)};
    auto it{args.begin()};
    auto next_arg{[&it]() { return *it++; }};

    auto show_usage = [program_path{next_arg()}]() {
        errorln("Usage: {} <command> args...", program_path);
        errorln("commands:");
        errorln("    sync   Synchronizes source to destination dir. If source "
                "is a file, the program reads dir info from it. If destination "
                "doesn't exist, it will be created");
        errorln("        args: <source> <dest-dir>");
        errorln("    save   Saves a directory info to file");
        errorln("        args: <source-dir> <saving-file>");
    };

    if (argc == 1) {
        show_usage();
        return EXIT_FAILURE;
    }

    // Processes options
    while (*it != nullptr && (*it)[0] == '-') {
        char const *arg{next_arg()};
        // Intentionally left blank, because we don't have any options to parse.
        ++it;
    }

    if (std::string_view command{next_arg()}; command == "sync") {
        char const *from{next_arg()};
        char const *to{next_arg()};

        errorln("Syncing {} to {} ...", from, to);

        if (!fs::exists(to)) {
            fs::create_directory(to);
        }

        auto const src{MerkleTree::from_path(from)};
        auto dest{MerkleTree::from_directory(to)};

        dest.sync_from(src);

        errorln("Sync ok");
    }
    else if (command == "save") {
        char const *source{next_arg()};
        char const *saving_filepath{next_arg()};

        auto const src{MerkleTree::from_directory(source)};
        src.writeTree(saving_filepath);

        errorln("File saved to {}", saving_filepath);
    }
    else {
        show_usage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
