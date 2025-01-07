set_languages("cxxlatest")
add_requires("boost", {
    system = false,
    configs = { serialization = true, thread = true, regex = true, },
})
add_requires("openssl")

add_syslinks("libcrypto", "libssl", "Crypt32", "Ws2_32", "Advapi32", "Kernel32", "Ole32", "User32")
-- libcrypto.lib
-- libssl.lib
-- Crypt32.lib
-- Ws2_32.lib
-- Advapi32.lib
-- Kernel32.lib
-- Ole32.lib

target("libtree")
    set_kind("static")
    add_files("libtree/tree.cpp")
    add_headerfiles("libtree/tree.hpp")
    add_includedirs(".")
    add_packages("boost", "openssl")

target("tree")
    set_kind("binary")
    add_files("tree/main.cpp")
    add_includedirs(".")
    add_deps("libtree")
    add_packages("boost", "openssl")
