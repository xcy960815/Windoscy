add_library(maccy_core STATIC
  core/history_item.cpp
  core/history_persistence.cpp
  core/history_store.cpp
  core/search.cpp
)

target_include_directories(maccy_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
  target_compile_options(maccy_core PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
  target_compile_options(maccy_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  add_executable(maccy_win WIN32
    app/main_win32.cpp
    app/win32_app.cpp
    platform/win32/clipboard.cpp
    platform/win32/input.cpp
    platform/win32/utf.cpp
  )

  target_compile_definitions(maccy_win PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )

  target_link_libraries(maccy_win PRIVATE
    maccy_core
    advapi32
    comctl32
    gdi32
    ole32
    shell32
    user32
  )

  if(MSVC)
    target_compile_options(maccy_win PRIVATE /W4 /permissive- /EHsc /utf-8 /O1 /Gy /Gw)
    target_link_options(maccy_win PRIVATE /INCREMENTAL:NO /OPT:REF /OPT:ICF)
  endif()

  install(TARGETS maccy_win RUNTIME DESTINATION .)
endif()
