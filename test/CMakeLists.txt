# Hip Analyzer tests


# ----- block_db ----- #

add_executable(
    block_db
    block_db.cpp
)

target_link_libraries(block_db hip_instrumentation)


# ----- load_bin ----- #

add_executable(
    load_bin
    load_bin.cpp
)

target_link_libraries(load_bin hip_instrumentation LLVMSupport)

# ----- load_csv ----- #

add_executable(
    load_csv
    load_csv.cpp
)

target_link_libraries(load_csv hip_instrumentation LLVMSupport)

# ----- arithmetic_intensity ----- #

add_executable(
    arithmetic_intensity
    arithmetic_intensity.cpp
)

target_link_libraries(arithmetic_intensity hip_instrumentation LLVMSupport)

# ----- gpu_benchmark ----- #

add_executable(
    gpu_benchmark
    gpu_benchmark.cpp
)

target_link_libraries(gpu_benchmark hip_instrumentation LLVMSupport)

# ----- recover_arrays ----- #

add_executable(
    recover_arrays
    recover_arrays.cpp
)

target_link_libraries(recover_arrays hip_instrumentation)
