add_llvm_component_library(LLVMSHARCDesc
  SHARCAsmBackend.cpp
  SHARCELFObjectWriter.cpp
  SHARCInstPrinter.cpp
  SHARCMCAsmInfo.cpp
  SHARCMCCodeEmitter.cpp
  SHARCMCTargetDesc.cpp
  SHARCSyntax.cpp
  SHARCTargetStreamer.cpp

  LINK_COMPONENTS
  MC
  Support
  SHARCInfo
  TargetParser

  ADD_TO_COMPONENT
  SHARC
  )
