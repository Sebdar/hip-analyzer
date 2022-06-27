/** \file gpu_functions.cpp
 * \brief Functions with device code (isolating slow compilations)
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include <chrono>
#include <numeric>

#include "hip_instrumentation/gpu_info.hpp"
#include "hip_instrumentation/hip_instrumentation.hpp"
#include "hip_instrumentation/hip_utils.hpp"
#include "hip_instrumentation/reduction_kernels.hpp"

unsigned int hip::Instrumenter::reduceFlops(const counter_t* device_ptr,
                                            hipStream_t stream) const {

    if (blocks.empty()) {
        // The block database has to be loaded prior to reduction!
        throw std::runtime_error("BasicBlock::normalized : Empty vector");
    }

    unsigned int num_blocks = 128u; // Ultimately, how many last reductions will
                                    // we have to do on the cpu
    unsigned int threads_per_block = 128u;

    // ----- Malloc & memcopy ----- //

    // Temporary buffer
    auto buf_size = num_blocks * threads_per_block * kernel_info.basic_blocks *
                    sizeof(hip::BlockUsage);

    hip::BlockUsage* buffer_ptr;
    hip::check(hipMalloc(&buffer_ptr, buf_size));

    // Output buffer

    std::vector<hip::BlockUsage> output(num_blocks * kernel_info.basic_blocks,
                                        {0u, 0u});

    auto output_size = output.size() * sizeof(hip::BlockUsage);

    hip::BlockUsage* output_ptr;
    hip::check(hipMalloc(&output_ptr, buf_size));

    // Launch geometry

    hip::LaunchGeometry geometry{kernel_info.total_threads_per_blocks,
                                 kernel_info.total_blocks,
                                 kernel_info.basic_blocks};

    // Basic blocks
    auto blocks_info = hip::BasicBlock::normalized(blocks);
    auto blocks_info_size = blocks_info.size() * sizeof(hip::BasicBlock);

    hip::BasicBlock* blocks_info_ptr;
    hip::check(hipMalloc(&blocks_info_ptr, blocks_info_size));

    hip::check(hipMemcpy(blocks_info_ptr, blocks_info.data(), blocks_info_size,
                         hipMemcpyHostToDevice));

    // ----- Synchronization ----- //
    if (!stream) {
        hip::check(hipDeviceSynchronize());
    }

    // ----- Launch kernel ----- //

    ::hip::
        reduceFlops<<<dim3(num_blocks), dim3(threads_per_block), 0, stream>>>(
            device_ptr, geometry, blocks_info_ptr, buffer_ptr, output_ptr);

    // ----- Fetch back data ----- //

    if (!stream) {
        hip::check(hipDeviceSynchronize());
    } else {
        hip::check(hipStreamSynchronize(stream));
    }

    hip::check(hipMemcpy(output.data(), output_ptr,
                         output.size() * sizeof(hip::BlockUsage),
                         hipMemcpyDeviceToHost));

    // ----- Free device memory ----- //

    hip::check(hipFree(output_ptr));
    hip::check(hipFree(buffer_ptr));
    hip::check(hipFree(blocks_info_ptr));

    // ----- Final reduction ----- //

    unsigned int flops = 0u;
    for (auto i = 0u; i < num_blocks; ++i) {
        for (auto bb = 0u; bb < kernel_info.basic_blocks; ++bb) {
            auto block_output = output[i * kernel_info.basic_blocks + bb];

            // std::cout << i << ' ' << bb << " : " << block_output.count <<
            // '\n';

            flops += block_output.flops;
        }
    }

    return flops;
}

namespace hip {
namespace benchmark {

double average(const std::vector<double>& vec) {
    auto sum = std::accumulate(vec.begin(), vec.end(), 0.);

    return sum / static_cast<double>(vec.size());
}

/** \fn performBenchmark
 * \brief Execute the benchmarking function benchmark a total of nb_repeats
 * times, returns the average in terms of ChronoUnit
 */
template <typename ChronoUnit = std::chrono::microseconds>
double performBenchmark(std::function<void(void)> benchmark,
                        unsigned int nb_repeats) {
    std::vector<double> times;
    times.reserve(nb_repeats);

    for (auto i = 0u; i < nb_repeats; ++i) {
        auto t0 = std::chrono::steady_clock::now();

        benchmark();

        auto t1 = std::chrono::steady_clock::now();
        auto time = std::chrono::duration_cast<ChronoUnit>(t1 - t0).count();

        times.emplace_back(static_cast<double>(time));
    }

    return average(times);
}

// ----- Memory benchmarks ----- //

MemoryRoof benchmarkMemoryBandwidth(unsigned int nb_repeats) {
    constexpr auto bytes_per_thread = 1024u;
    constexpr auto threads = 1024u;
    constexpr auto blocks = 1024u;

    double avg_time = performBenchmark(
        [&]() {
            // TODO
        },
        nb_repeats);

    unsigned int total_bytes = bytes_per_thread * threads * blocks;

    double bytes_per_second = static_cast<double>(total_bytes) / avg_time;

    return {"memory", bytes_per_second};
}

// ----- Compute benchmarks ----- //

constexpr unsigned int flop_number = 65536u - 1u; // 2^16 - 1

/** \fn benchmark_operation
 * \brief Compute kernel to benchmark an operation.
 *
 * \param Operation template parameter, requires an empty struct with a call
 * operator (operator()). Prefer using <functional> types
 * (std::multiplies<float>, etc.)
 *
 * \param data Vector of size blockDim.x * gridDim.x
 */
template <typename Operation> __global__ void benchmark_operation(float* data) {
    Operation op{};

    size_t index = blockIdx.x * blockDim.x + threadIdx.x;

    // We need to "simulate" a real access to the memory in order to prevent the
    // compiler from optimizing our efforts
    float lhs = data[index];
    float rhs = data[index + 1];

    for (auto i = 0u; i < flop_number; ++i) {
        lhs = op(lhs, rhs);
    }

    // Store to prevent dead code optimization
    data[index] = lhs;
}

float* allocEmptyVector(size_t size, float init = 1.f) {
    std::vector<float> vec_cpu(size, init);

    size_t vec_size = size * sizeof(float);

    float* data;
    hip::check(hipMalloc(&data, vec_size));
    hip::check(
        hipMemcpy(data, vec_cpu.data(), vec_size, hipMemcpyHostToDevice));

    return data;
}

ComputeRoof benchmarkMultiplyFlops(unsigned int nb_repeats) {
    constexpr auto flops_per_thread = 1024u;
    constexpr auto threads = 1024u;
    constexpr auto blocks = 1024u;

    // Alloc

    float* data = allocEmptyVector(blocks * threads * 2);

    double avg_time = performBenchmark(
        [&]() {
            benchmark_operation<std::multiplies<float>>
                <<<blocks, threads>>>(data);

            hip::check(hipDeviceSynchronize());
        },
        nb_repeats);

    // Free

    hip::check(hipFree(data));

    // Compute perf

    unsigned int total_flops = flops_per_thread * threads * blocks;

    double flops_per_second = static_cast<double>(total_flops) / avg_time;

    return {"multiply", flops_per_second};
}

ComputeRoof benchmarkAddFlops(unsigned int nb_repeats) {
    constexpr auto flops_per_thread = 1024u;
    constexpr auto threads = 1024u;
    constexpr auto blocks = 1024u;

    double avg_time = performBenchmark(
        [&]() {
            // TODO
        },
        nb_repeats);

    unsigned int total_flops = flops_per_thread * threads * blocks;

    double flops_per_second = static_cast<double>(total_flops) / avg_time;

    return {"add", flops_per_second};
}

ComputeRoof benchmarkFmaFlops(unsigned int nb_repeats) {
    constexpr auto flops_per_thread = 1024u;
    constexpr auto threads = 1024u;
    constexpr auto blocks = 1024u;

    double avg_time = performBenchmark(
        [&]() {
            // TODO
        },
        nb_repeats);

    unsigned int total_flops = flops_per_thread * threads * blocks;

    double flops_per_second = static_cast<double>(total_flops) / avg_time;

    return {"fma", flops_per_second};
}

} // namespace benchmark

} // namespace hip
