/** \file llvm_instruction_counters.cpp
 * \brief LLVM Instruction counters for basic block static analysis
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include "llvm_instr_counters.h"

#include "llvm/IR/Instructions.h"

namespace hip {

unsigned int FlopCounter::operator()(llvm::BasicBlock& bb) {
    using namespace llvm;

    for (const auto& instr : bb) {
        switch (instr.getOpcode()) {
        case Instruction::FAdd:
        case Instruction::FSub:
        case Instruction::FMul:
        case Instruction::FDiv:
        case Instruction::FRem:
        case Instruction::FPToUI:
        case Instruction::FPToSI:
        case Instruction::UIToFP:
        case Instruction::SIToFP:
        case Instruction::FPTrunc:
        case Instruction::FPExt:
        case Instruction::FCmp:
            ++count;
        default:;
        }
    }

    return getCount();
}

constexpr auto bits_per_byte = 8u;

unsigned int StoreCounter::operator()(llvm::BasicBlock& bb) {
    for (const auto& instr : bb) {
        if (const auto* store_inst = llvm::dyn_cast<llvm::StoreInst>(&instr)) {
            // store_inst->print(llvm::errs());

            const auto* type =
                store_inst->getPointerOperandType()->getContainedType(0);

            // TODO : handle possible compound types
            count += type->getPrimitiveSizeInBits() / bits_per_byte;
        }
    }

    return getCount();
}

unsigned int LoadCounter::operator()(llvm::BasicBlock& bb) {
    // TODO : handle possible compound types
    for (const auto& instr : bb) {
        if (const auto* array_deref =
                llvm::dyn_cast<llvm::GetElementPtrInst>(&instr)) {
            // array_deref->print(llvm::errs());

            const auto* type =
                array_deref->getPointerOperandType()->getContainedType(0);

            count += type->getPrimitiveSizeInBits() / bits_per_byte;
        } else if (const auto* load_inst =
                       llvm::dyn_cast<llvm::LoadInst>(&instr)) {
            // load_inst->print(llvm::errs());

            const auto* type =
                load_inst->getPointerOperandType()->getContainedType(0);

            count += type->getPrimitiveSizeInBits() / bits_per_byte;
        }
    }

    return getCount();
}

} // namespace hip
