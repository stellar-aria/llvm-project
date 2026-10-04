add_llvm_component_library(LLVMSHARCDisassembler
  SHARCDisassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  SHARCDesc
  SHARCInfo
  Support

  ADD_TO_COMPONENT
  SHARC
  )
