//===- I8086.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Intel 8086 ABI Implementation (cdecl, tiny model).
//
// The default ABI already matches what we want: scalars are passed/returned
// directly (small integers promoted), and aggregates are passed by value on
// the stack (indirect byval) and returned via a hidden sret pointer.  The
// backend's calling convention (CC_I8086) does the actual stack/register
// assignment.
//===----------------------------------------------------------------------===//

namespace {

class I8086ABIInfo : public DefaultABIInfo {
public:
  I8086ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}
};

class I8086TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  I8086TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<I8086ABIInfo>(CGT)) {}
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createI8086TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<I8086TargetCodeGenInfo>(CGM.getTypes());
}
