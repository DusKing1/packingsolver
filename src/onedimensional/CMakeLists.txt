find_package(Threads)
add_library(PackingSolver_onedimensional)
target_sources(PackingSolver_onedimensional PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    algorithm_formatter.cpp
    optimize.cpp
    branching_scheme.cpp)
target_include_directories(PackingSolver_onedimensional PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_include_directories(PackingSolver_onedimensional PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_onedimensional PUBLIC
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver
    Threads::Threads)
add_library(PackingSolver::onedimensional ALIAS PackingSolver_onedimensional)

if(PACKINGSOLVER_BUILD_MAIN)
  add_executable(PackingSolver_onedimensional_main)
  target_sources(PackingSolver_onedimensional_main PRIVATE
      main.cpp)
  target_link_libraries(PackingSolver_onedimensional_main PUBLIC
      PackingSolver_onedimensional
      Boost::program_options
      Boost::filesystem)
  set_target_properties(PackingSolver_onedimensional_main PROPERTIES OUTPUT_NAME "packingsolver_onedimensional")
  install(TARGETS PackingSolver_onedimensional_main)
endif()
