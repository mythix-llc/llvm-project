include(AddMLIRPython)

################################################################################
# Structural groupings.
################################################################################

declare_mlir_python_sources(MLIRPythonSources)
declare_mlir_python_sources(MLIRPythonSources.Dialects
  ADD_TO_PARENT MLIRPythonSources)

################################################################################
# Pure python sources and generated code
################################################################################

declare_mlir_python_sources(MLIRPythonSources.Core
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  ADD_TO_PARENT MLIRPythonSources
  SOURCES
    _mlir_libs/__init__.py
    ir.py
    passmanager.py
    dialects/_ods_common.py

    # The main _mlir module has submodules: include stubs from each.
    _mlir_libs/_mlir/__init__.pyi
    _mlir_libs/_mlir/ir.pyi
    _mlir_libs/_mlir/passmanager.pyi
)

declare_mlir_python_sources(MLIRPythonSources.ExecutionEngine
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  ADD_TO_PARENT MLIRPythonSources
  SOURCES
    execution_engine.py
    _mlir_libs/_mlirExecutionEngine.pyi
  SOURCES_GLOB
    runtime/*.py
)

declare_mlir_python_sources(MLIRPythonSources.Passes
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  ADD_TO_PARENT MLIRPythonSources
  SOURCES_GLOB
    all_passes_registration/*.py
    conversions/*.py
    transforms/*.py
)

declare_mlir_python_sources(MLIRPythonCAPIHeaderSources
  ROOT_DIR "${MLIR_SOURCE_DIR}/include"
  SOURCES_GLOB "mlir-c/*.h"
  DEST_PREFIX "_mlir_libs/include"
)

################################################################################
# Dialect bindings
################################################################################

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/AsyncOps.td
  SOURCES_GLOB dialects/async_dialect/*.py
  DIALECT_NAME async_dialect)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/BuiltinOps.td
  SOURCES
    dialects/builtin.py
    dialects/_builtin_ops_ext.py
  DIALECT_NAME builtin)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/ControlFlowOps.td
  SOURCES
    dialects/cf.py
  DIALECT_NAME cf)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/GPUOps.td
  SOURCES_GLOB dialects/gpu/*.py
  DIALECT_NAME gpu)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/LinalgOps.td
  SOURCES
    dialects/_linalg_ops_ext.py
  SOURCES_GLOB
    dialects/linalg/*.py
  DIALECT_NAME linalg
  DEPENDS LinalgOdsGen)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/MathOps.td
  SOURCES dialects/math.py
  DIALECT_NAME math)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/ArithmeticOps.td
  SOURCES
    dialects/arith.py
    dialects/_arith_ops_ext.py
  DIALECT_NAME arith)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/MemRefOps.td
  SOURCES
    dialects/memref.py
    dialects/_memref_ops_ext.py
  DIALECT_NAME memref)

declare_mlir_python_sources(
  MLIRPythonSources.Dialects.quant
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  SOURCES
    dialects/quant.py
    _mlir_libs/_mlir/dialects/quant.pyi)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/PDLOps.td
  SOURCES
    dialects/pdl.py
    dialects/_pdl_ops_ext.py
    _mlir_libs/_mlir/dialects/pdl.pyi
  DIALECT_NAME pdl)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/SCFOps.td
  SOURCES
    dialects/scf.py
    dialects/_scf_ops_ext.py
  DIALECT_NAME scf)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/ShapeOps.td
  SOURCES dialects/shape.py
  DIALECT_NAME shape)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/SparseTensorOps.td
  SOURCES dialects/sparse_tensor.py
  DIALECT_NAME sparse_tensor)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/StandardOps.td
  SOURCES
    dialects/std.py
    dialects/_std_ops_ext.py
  DIALECT_NAME std)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/TensorOps.td
  SOURCES dialects/tensor.py
  DIALECT_NAME tensor)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/TosaOps.td
  SOURCES dialects/tosa.py
  DIALECT_NAME tosa)

declare_mlir_dialect_python_bindings(
  ADD_TO_PARENT MLIRPythonSources.Dialects
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
  TD_FILE dialects/VectorOps.td
  SOURCES dialects/vector.py
  DIALECT_NAME vector)

################################################################################
# Python extensions.
# The sources for these are all in lib/Bindings/Python, but since they have to
# be rebuilt for each package and integrate with the source setup here, we
# just reference them here instead of having ordered, cross package target
# dependencies.
################################################################################

set(PYTHON_SOURCE_DIR "${MLIR_SOURCE_DIR}/lib/Bindings/Python")
declare_mlir_python_extension(MLIRPythonExtension.Core
  MODULE_NAME _mlir
  ADD_TO_PARENT MLIRPythonSources.Core
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    MainModule.cpp
    IRAffine.cpp
    IRAttributes.cpp
    IRCore.cpp
    IRInterfaces.cpp
    IRModule.cpp
    IRTypes.cpp
    PybindUtils.cpp
    Pass.cpp

    # Headers must be included explicitly so they are installed.
    Globals.h
    IRModule.h
    Pass.h
    PybindUtils.h
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIDebug
    MLIRCAPIIR
    MLIRCAPIInterfaces
    MLIRCAPIRegistration  # TODO: See about dis-aggregating

    # Dialects
    MLIRCAPIStandard
)

declare_mlir_python_extension(MLIRPythonExtension.Dialects.Linalg.Pybind
  MODULE_NAME _mlirDialectsLinalg
  ADD_TO_PARENT MLIRPythonSources.Dialects.linalg
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    DialectLinalg.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIIR
    MLIRCAPILinalg
)

declare_mlir_python_extension(MLIRPythonExtension.Dialects.Quant.Pybind
  MODULE_NAME _mlirDialectsQuant
  ADD_TO_PARENT MLIRPythonSources.Dialects.quant
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    DialectQuant.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIIR
    MLIRCAPIQuant
)

declare_mlir_python_extension(MLIRPythonExtension.Dialects.PDL.Pybind
  MODULE_NAME _mlirDialectsPDL
  ADD_TO_PARENT MLIRPythonSources.Dialects.pdl
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    DialectPDL.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIIR
    MLIRCAPIPDL
)

declare_mlir_python_extension(MLIRPythonExtension.Dialects.SparseTensor.Pybind
  MODULE_NAME _mlirDialectsSparseTensor
  ADD_TO_PARENT MLIRPythonSources.Dialects.sparse_tensor
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    DialectSparseTensor.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIIR
    MLIRCAPISparseTensor
)

declare_mlir_python_extension(MLIRPythonExtension.AllPassesRegistration
  MODULE_NAME _mlirAllPassesRegistration
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    AllPassesRegistration.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIConversion
    MLIRCAPITransforms
)

declare_mlir_python_extension(MLIRPythonExtension.AsyncDialectPasses
  MODULE_NAME _mlirAsyncPasses
  ADD_TO_PARENT MLIRPythonSources.Dialects.async_dialect
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    AsyncPasses.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIAsync
)

declare_mlir_python_extension(MLIRPythonExtension.Conversions
  MODULE_NAME _mlirConversions
  ADD_TO_PARENT MLIRPythonSources.Passes
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    Conversions/Conversions.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
  MLIRCAPIConversion
)

# Only enable the ExecutionEngine if the native target is configured in.
if(TARGET ${LLVM_NATIVE_ARCH})
  declare_mlir_python_extension(MLIRPythonExtension.ExecutionEngine
    MODULE_NAME _mlirExecutionEngine
    ADD_TO_PARENT MLIRPythonSources.ExecutionEngine
    ROOT_DIR "${PYTHON_SOURCE_DIR}"
    SOURCES
      ExecutionEngineModule.cpp
    PRIVATE_LINK_LIBS
      LLVMSupport
    EMBED_CAPI_LINK_LIBS
      MLIRCAPIExecutionEngine
  )
endif()

declare_mlir_python_extension(MLIRPythonExtension.GPUDialectPasses
  MODULE_NAME _mlirGPUPasses
  ADD_TO_PARENT MLIRPythonSources.Dialects.gpu
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    GPUPasses.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPIGPU
)

declare_mlir_python_extension(MLIRPythonExtension.LinalgPasses
  MODULE_NAME _mlirLinalgPasses
  ADD_TO_PARENT MLIRPythonSources.Dialects.linalg
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    LinalgPasses.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPILinalg
)

declare_mlir_python_extension(MLIRPythonExtension.SparseTensorDialectPasses
  MODULE_NAME _mlirSparseTensorPasses
  ADD_TO_PARENT MLIRPythonSources.Dialects.sparse_tensor
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    SparseTensorPasses.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPISparseTensor
)

declare_mlir_python_extension(MLIRPythonExtension.Transforms
  MODULE_NAME _mlirTransforms
  ADD_TO_PARENT MLIRPythonSources.Passes
  ROOT_DIR "${PYTHON_SOURCE_DIR}"
  SOURCES
    Transforms/Transforms.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    MLIRCAPITransforms
)

# TODO: Figure out how to put this in the test tree.
# This should not be included in the main Python extension. However,
# putting it into MLIRPythonTestSources along with the dialect declaration
# above confuses Python module loader when running under lit.
set(_ADDL_TEST_SOURCES)
if(MLIR_INCLUDE_TESTS)
  set(_ADDL_TEST_SOURCES MLIRPythonTestSources)
  declare_mlir_python_sources(MLIRPythonTestSources)
  declare_mlir_python_sources(MLIRPythonTestSources.Dialects
    ADD_TO_PARENT MLIRPythonTestSources)

  # TODO: this uses a tablegen file from the test directory and should be
  # decoupled from here.
  declare_mlir_python_sources(
    MLIRPythonTestSources.Dialects.PythonTest
    ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlir"
    ADD_TO_PARENT MLIRPythonTestSources.Dialects
    SOURCES dialects/python_test.py)
  set(LLVM_TARGET_DEFINITIONS
    "${MLIR_MAIN_SRC_DIR}/test/python/python_test_ops.td")
  mlir_tablegen(
    "dialects/_python_test_ops_gen.py"
    -gen-python-op-bindings
    -bind-dialect=python_test)
  add_public_tablegen_target(PythonTestDialectPyIncGen)
  declare_mlir_python_sources(
    MLIRPythonTestSources.Dialects.PythonTest.ops_gen
    ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    ADD_TO_PARENT MLIRPythonTestSources.Dialects.PythonTest
    SOURCES "dialects/_python_test_ops_gen.py")

  declare_mlir_python_extension(MLIRPythonTestSources.PythonTestExtension
    MODULE_NAME _mlirPythonTest
    ADD_TO_PARENT MLIRPythonTestSources.Dialects
    ROOT_DIR "${MLIR_SOURCE_DIR}/test/python/lib"
    SOURCES
      PythonTestModule.cpp
    PRIVATE_LINK_LIBS
      LLVMSupport
    EMBED_CAPI_LINK_LIBS
      MLIRCAPIPythonTestDialect
  )
endif()

################################################################################
# Common CAPI dependency DSO.
# All python extensions must link through one DSO which exports the CAPI, and
# this must have a globally unique name amongst all embeddors of the python
# library since it will effectively have global scope.
#
# The presence of this aggregate library is part of the long term plan, but its
# use needs to be made more flexible.
#
# TODO: Upgrade to the aggregate utility in https://reviews.llvm.org/D106419
# once ready.
################################################################################

add_mlir_python_common_capi_library(MLIRPythonCAPI
  INSTALL_COMPONENT MLIRPythonModules
  INSTALL_DESTINATION python_packages/mlir_core/mlir/_mlir_libs
  OUTPUT_DIRECTORY "${MLIR_BINARY_DIR}/python_packages/mlir_core/mlir/_mlir_libs"
  RELATIVE_INSTALL_ROOT "../../../.."
  DECLARED_SOURCES
    MLIRPythonSources
    MLIRPythonExtension.AllPassesRegistration
    ${_ADDL_TEST_SOURCES}
)

################################################################################
# The fully assembled package of modules.
# This must come last.
################################################################################

add_mlir_python_modules(MLIRPythonModules
  ROOT_PREFIX "${MLIR_BINARY_DIR}/python_packages/mlir_core/mlir"
  INSTALL_PREFIX "python_packages/mlir_core/mlir"
  DECLARED_SOURCES
    MLIRPythonSources
    MLIRPythonExtension.AllPassesRegistration
    MLIRPythonCAPIHeaderSources
    ${_ADDL_TEST_SOURCES}
  COMMON_CAPI_LINK_LIBS
    MLIRPythonCAPI
  )
