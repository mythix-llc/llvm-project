add_mlir_library(MLIRPresburger
  IntegerPolyhedron.cpp
  LinearTransform.cpp
  Matrix.cpp
  PresburgerSet.cpp
  PresburgerSpace.cpp
  PWMAFunction.cpp
  Simplex.cpp
  Utils.cpp

  LINK_LIBS PUBLIC
  MLIRSupport
  )
