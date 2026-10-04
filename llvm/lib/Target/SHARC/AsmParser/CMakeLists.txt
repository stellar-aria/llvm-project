add_llvm_component_library(LLVMSHARCAsmParser
  SHARCAsmParser.cpp
  SHARCChecker.cpp

  LINK_COMPONENTS
  MC
  MCParser
  SHARCDesc
  SHARCInfo
  Support

  ADD_TO_COMPONENT
  SHARC
  )
