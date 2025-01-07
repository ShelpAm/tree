set_languages("cxxlatest", "clatest")
add_requires("boost", {
    system = false,
    configs = { serialization = true, thread = true, regex = true, },
})
add_requires("openssl")

if is_plat("windows") then
    add_syslinks("libcrypto", "libssl", "Crypt32", "Ws2_32", "Advapi32", "Kernel32", "Ole32", "User32")
end

target("libtree")
set_kind("static")
add_files("libtree/tree.cpp")
add_headerfiles("libtree/tree.hpp", "libtree/print.hpp")
add_includedirs(".")
add_packages("boost", "openssl")

target("tree")
set_kind("binary")
add_files("tree/main.cpp")
add_includedirs(".")
add_deps("libtree")
add_packages("boost", "openssl")
