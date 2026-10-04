add_llvm_component_group(SHARC)

set(LLVM_TARGET_DEFINITIONS SHARC.td)

tablegen(LLVM SHARCGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM SHARCGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM SHARCGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM SHARCGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM SHARCGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(SHARCCommonTableGen)

add_llvm_target(SHARCCodeGen
  SHARCTargetMachine.cpp

  LINK_COMPONENTS
  CodeGen
  Core
  MC
  SHARCDesc
  SHARCInfo
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  SHARC
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
