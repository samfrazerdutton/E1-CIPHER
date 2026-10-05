# One CLI binary, `e1cipher`, with subcommands (project brief section 31)
# dispatching into these directories' logic -- not one executable per
# subdirectory. Each directory still physically exists per the suggested
# layout; main.cpp just wires them together.
add_library(e1cipher_apps STATIC
    drone_inspection/inspect.cpp
    fleet_gateway/fleet.cpp
)
target_include_directories(e1cipher_apps PUBLIC "${CMAKE_SOURCE_DIR}/apps")
target_link_libraries(e1cipher_apps PUBLIC e1cipher_core)
e1cipher_set_warnings(e1cipher_apps)
e1cipher_apply_sanitizers(e1cipher_apps)

add_executable(e1cipher cli/main.cpp)
target_link_libraries(e1cipher PRIVATE e1cipher_apps e1cipher_core)
e1cipher_set_warnings(e1cipher)
e1cipher_apply_sanitizers(e1cipher)
