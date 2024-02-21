include_directories(${ParallelDiskANN_SOURCE_DIR}/include)

set(SOURCE_FILES
        hashutil.cpp
		#vamana.cpp
)

add_library(ParallelDiskANNLibrary ${SOURCE_FILES})

target_link_libraries(ParallelDiskANNLibrary PUBLIC Parlay::parlay)
