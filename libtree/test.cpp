#include <boost/archive/text_iarchive.hpp>
#include <filesystem>
#include <fstream>

int main()
{
    std::filesystem::path p;

    std::ifstream ifs{"/tmp/cv_debug.log"};
    boost::archive::text_iarchive ia{ifs};
    ia & p;
}
