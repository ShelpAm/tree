{
    files = {
        "tree/main.cpp"
    },
    values = {
        "/usr/lib64/ccache/gcc",
        {
            "-m64",
            "-std=c++26",
            "-I.",
            "-isystem",
            "/home/shelpam/.xmake/packages/b/boost/1.86.0/8ca5197e991b4e538d5bb7e9ad08922c/include",
            "-isystem",
            "/home/shelpam/.xmake/packages/i/icu4c/75.1/0bd2e46011f44dc580c82667cc8aa3e5/include"
        }
    },
    depfiles_gcc = "main.o: tree/main.cpp libtree/tree.hpp\
"
}