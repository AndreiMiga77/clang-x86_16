add_llvm_component_group(I8086)

# Assembler-only target: we do not call add_llvm_target (no CodeGen library),
# so add the target root (for MCTargetDesc/TargetInfo/*.h and the generated
# *.inc files) to the include path for all subdirectories ourselves.
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(LLVM_TARGET_DEFINITIONS I8086.td)

tablegen(LLVM I8086GenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM I8086GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM I8086GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM I8086GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM I8086GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(I8086CommonTableGen)

# I8086 is assembler-only (no CodeGen library).  Provide a small top-level
# component library so that the "I8086" component resolves for tools that link
# LLVM targets by name (e.g. lld).
add_llvm_component_library(LLVMI8086
  I8086.cpp

  LINK_COMPONENTS
  Support

  ADD_TO_COMPONENT
  I8086
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
add_subdirectory(AsmParser)
