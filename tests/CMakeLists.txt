add_executable(maccy_core_smoke
  core_smoke.cpp
)

target_link_libraries(maccy_core_smoke PRIVATE maccy_core)

add_test(NAME maccy_core_smoke COMMAND maccy_core_smoke)

add_executable(maccy_history_store_test
  history_store_test.cpp
)

target_link_libraries(maccy_history_store_test PRIVATE maccy_core)

add_test(NAME maccy_history_store_test COMMAND maccy_history_store_test)

add_executable(maccy_search_modes_test
  search_modes_test.cpp
)

target_link_libraries(maccy_search_modes_test PRIVATE maccy_core)

add_test(NAME maccy_search_modes_test COMMAND maccy_search_modes_test)

add_executable(maccy_search_highlight_test
  search_highlight_test.cpp
)

target_link_libraries(maccy_search_highlight_test PRIVATE maccy_core)

add_test(NAME maccy_search_highlight_test COMMAND maccy_search_highlight_test)

add_executable(maccy_history_persistence_test
  history_persistence_test.cpp
)

target_link_libraries(maccy_history_persistence_test PRIVATE maccy_core)

add_test(NAME maccy_history_persistence_test COMMAND maccy_history_persistence_test)

add_executable(maccy_settings_test
  settings_test.cpp
)

target_link_libraries(maccy_settings_test PRIVATE maccy_core)

add_test(NAME maccy_settings_test COMMAND maccy_settings_test)

add_executable(maccy_ignore_rules_test
  ignore_rules_test.cpp
)

target_link_libraries(maccy_ignore_rules_test PRIVATE maccy_core)

add_test(NAME maccy_ignore_rules_test COMMAND maccy_ignore_rules_test)
