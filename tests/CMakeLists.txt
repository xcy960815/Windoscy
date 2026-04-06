add_executable(maccy_core_smoke
  core_smoke.cpp
)

target_link_libraries(maccy_core_smoke PRIVATE maccy_core)

add_test(NAME maccy_core_smoke COMMAND maccy_core_smoke)
