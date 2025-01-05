set_languages("cxxlatest")
add_requires("boost", {
    system = false,
    configs = { serialization = true, thread = true, regex = true, },
})
add_requires("openssl")

target("libtree")
    set_kind("static")
    add_files("libtree/tree.cpp")
    add_includedirs(".")
    add_packages("boost", "openssl")

target("tree")
    set_kind("binary")
    add_files("tree/main.cpp")
    add_includedirs(".")
    add_deps("libtree")
    add_packages("boost")
