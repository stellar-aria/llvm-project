add_llvm_component_library(LLVMSHARCInfo
  SHARCTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  SHARC
  )
