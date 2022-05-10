/** \file hip_instrumentation.cpp
 * \brief Kernel instrumentation embedded code
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include "hip_instrumentation.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace hip {

inline void check(hipError_t err) {
    if (err != hipSuccess) {
        std::cerr << "error : " << hipGetErrorString(err) << " (" << err
                  << ")\n";
        throw std::runtime_error(std::string("Encountered hip error ") +
                                 hipGetErrorString(err));
    }
}

// GCN Assembly

/** \brief Saves the EXEC registers in two VGPRs (variables h & l)
 */
constexpr auto save_register =
    "asm volatile (\"s_mov_b32 s6, exec_lo\\n s_mov_b32 s7, exec_hi\\n "
    "v_mov_b32 %0, s6\\n v_mov_b32 %1, s7\":\"=v\" (l), \"=v\" (h):);";

// ---- Instrumentation ----- //

void KernelInfo::dump() const {
    std::cout << "Kernel info (" << name << ") :\n"
              << "\tTotal blocks : " << total_blocks << '\n'
              << "\tTotal threads : " << total_threads_per_blocks << '\n'
              << "\tBasic blocks : " << basic_blocks << '\n'
              << "\tInstr size : " << instr_size << '\n';
}

Instrumenter::Instrumenter(KernelInfo& ki)
    : kernel_info(ki), host_counters(ki.instr_size, 0u) {

    // Get the timestamp for unique identification
    auto now = std::chrono::steady_clock::now();
    stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch())
                .count();
}

void* Instrumenter::toDevice() {
    void* data_device;
    hip::check(
        hipMalloc(&data_device, kernel_info.instr_size * sizeof(counter_t)));

    hip::check(hipMemcpy(data_device, host_counters.data(),
                         kernel_info.instr_size * sizeof(counter_t),
                         hipMemcpyHostToDevice));

    return data_device;
}

void Instrumenter::fromDevice(void* device_ptr) {
    hip::check(hipMemcpy(host_counters.data(), device_ptr,
                         kernel_info.instr_size * sizeof(counter_t),
                         hipMemcpyDeviceToHost));
}

std::string Instrumenter::autoFilenamePrefix() const {
    std::stringstream ss;
    ss << kernel_info.name << '_' << stamp;

    return ss.str();
}
void Instrumenter::dumpCsv(const std::string& filename_in) {
    std::string filename;

    if (filename_in.empty()) {
        filename = autoFilenamePrefix() + ".csv";
    } else {
        filename = filename_in;
    }

    std::ofstream out(filename);
    out << "block,thread,bblock,count\n";

    for (auto block = 0; block < kernel_info.total_blocks; ++block) {
        for (auto thread = 0; thread < kernel_info.total_threads_per_blocks;
             ++thread) {
            for (auto bblock = 0; bblock < kernel_info.basic_blocks; ++bblock) {
                auto index = block * kernel_info.total_threads_per_blocks *
                                 kernel_info.basic_blocks +
                             thread * kernel_info.basic_blocks + bblock;
                out << block << ',' << thread << ',' << bblock << ','
                    << static_cast<unsigned int>(host_counters[index]) << '\n';
            }
        }
    }

    out.close();
}

void Instrumenter::dumpBin(const std::string& filename_in) {
    std::string filename;

    if (filename_in.empty()) {
        filename = autoFilenamePrefix() + ".hiptrace";
    } else {
        filename = filename_in;
    }

    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(host_counters.data()),
              host_counters.size() * sizeof(counter_t));

    out.close();
}

} // namespace hip
