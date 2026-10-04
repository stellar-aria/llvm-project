add_llvm_component_library(LLVMSHARCDesc
  SHARCInstPrinter.cpp
  SHARCMCAsmInfo.cpp
  SHARCMCTargetDesc.cpp
  SHARCSyntax.cpp

  LINK_COMPONENTS
  MC
  Support
  SHARCInfo
  TargetParser

  ADD_TO_COMPONENT
  SHARC
  )
