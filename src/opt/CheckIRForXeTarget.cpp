/*
  Copyright (c) 2022-2023, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

#include "CheckIRForXeTarget.h"

#ifdef ISPC_XE_ENABLED

namespace ispc {

char CheckIRForXeTarget::ID = 0;

bool CheckIRForXeTarget::checkAndFixIRForXe(llvm::BasicBlock &bb) {
    DEBUG_START_BB("CheckIRForXeTarget");
    bool modifiedAny = false;
    for (llvm::BasicBlock::iterator I = bb.begin(), E = --bb.end(); I != E; ++I) {
        llvm::Instruction *inst = &*I;
        SourcePos pos;
        LLVMGetSourcePosFromMetadata(inst, &pos);
        if (llvm::CallInst *ci = llvm::dyn_cast<llvm::CallInst>(inst)) {
            if (llvm::GenXIntrinsic::getGenXIntrinsicID(ci) == llvm::GenXIntrinsic::genx_lsc_prefetch_stateless) {
                // If prefetch is supported, fix data size parameter
                Assert(ci->arg_size() > 6);
                llvm::Value *dataSizeVal = ci->getArgOperand(6);
                llvm::ConstantInt *dataSizeConst = llvm::dyn_cast<llvm::ConstantInt>(dataSizeVal);
                Assert(dataSizeConst && (dataSizeConst->getBitWidth() == 8));
                int dataSizeNum = dataSizeConst->getSExtValue();
                // 0: invalid
                // 1: d8
                // 2: d16
                // 3: d32
                // 4: d64
                // Valid user's input is 1, 2, 4, 8
                int8_t genSize = 3;
                switch (dataSizeNum) {
                case 1:
                    genSize = 1;
                    break;
                case 2:
                    genSize = 2;
                    break;
                case 4:
                    genSize = 3;
                    break;
                case 8:
                    genSize = 4;
                    break;
                default:
                    Error(pos, "Incorrect data size argument for \'prefetch\'. Valid values are 1, 2, 4, 8");
                }
                llvm::Value *dataSizeGen = llvm::ConstantInt::get(LLVMTypes::Int8Type, genSize);
                ci->setArgOperand(6, dataSizeGen);
            }
        }
        // Report error if double type is not supported by the target
        if (!g->target->hasFp64Support()) {
            for (int i = 0; i < (int)inst->getNumOperands(); ++i) {
                llvm::Type *t = inst->getOperand(i)->getType();
                if (t == LLVMTypes::DoubleType || t == LLVMTypes::DoublePointerType ||
                    t == LLVMTypes::DoubleVectorType || t == LLVMTypes::DoubleVectorPointerType) {
                    Error(pos, "\'double\' type is not supported by the target\n");
                }
            }
        }
    }
    DEBUG_END_BB("CheckIRForXeTarget");
    return modifiedAny;
}

bool CheckIRForXeTarget::runOnFunction(llvm::Function &F) {
    llvm::TimeTraceScope FuncScope("CheckIRForXeTarget::runOnFunction", F.getName());

    bool modifiedAny = false;
    for (llvm::BasicBlock &BB : F) {
        modifiedAny |= checkAndFixIRForXe(BB);
    }
    return modifiedAny;
}

llvm::Pass *CreateCheckIRForXeTarget() { return new CheckIRForXeTarget(); }

} // namespace ispc

#endif
