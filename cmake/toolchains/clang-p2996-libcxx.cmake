set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

add_compile_options(-stdlib=libc++ -freflection)
add_link_options(-stdlib=libc++)
