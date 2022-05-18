/** \file basic_block.hpp
 * \brief Kernel static informations
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

// Std includes

#include <memory>
#include <string>
#include <vector>

namespace hip {

constexpr auto default_database = "hip_analyzer.json";

struct BasicBlock {
    /** ctor
     */
    BasicBlock(unsigned int id, unsigned int flops, const std::string& begin,
               const std::string& end);

    /** \fn json
     * \brief Dump block to JSON
     */
    std::string json() const;

    /** \fn jsonArray
     * \brief Dump blocks to JSON
     */
    static std::string jsonArray(const std::vector<BasicBlock>& blocks);

    /** \fn fromJson
     * \brief Load block from JSON format
     */
    static BasicBlock fromJson(const std::string& json);

    /** \fn fromJsonArray
     * \brief Load block from JSON array format
     */
    static std::vector<BasicBlock> fromJsonArray(const std::string& json);

    unsigned int id;
    unsigned int flops;

    // These are allocated as pointers as to reduce the footprint
    std::unique_ptr<std::string> begin_loc, end_loc;
};

} // namespace hip