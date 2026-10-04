add_llvm_component_library(LLVMSHARCDesc
  SHARCMCAsmInfo.cpp
  SHARCMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  SHARCInfo
  TargetParser

  ADD_TO_COMPONENT
  SHARC
  )
