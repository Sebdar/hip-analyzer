/** \file llvm_ir_consumer.hpp
 * \brief LLVM Intermediate representation handler
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#pragma once

#include <memory>

#include "clang/Tooling/Tooling.h"

#include "clang/CodeGen/CodeGenAction.h"

std::unique_ptr<clang::tooling::ToolAction>
makeLLVMAction(const std::string& kernel_name);
