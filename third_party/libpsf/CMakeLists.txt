# SPICEUnion 维护的 henjo/libpsf CMake 构建配方（vendored）
#
# 上游 https://github.com/henjo/libpsf 是 autotools 工程（configure.ac /
# Makefile.am），没有 CMakeLists.txt。为让本机 local/external/libpsf 与
# 云 CI 使用同一套构建参数，这里固定一份最小 CMake 配方：
#   - 源列表与上游 commit 6efc14f7c5fa7e09a07e354cc54b9135ec353d70 的 src/ 对齐；
#   - 产物为静态库 libpsf.a，安装 include/psf.h 与 include/psfdata.h；
#   - 构建时由 CI 复制到 libpsf 源码根目录后按常规 CMake 流程执行。
cmake_minimum_required(VERSION 3.16)

project(libpsf LANGUAGES CXX)

option(LIBPSF_BUILD_SHARED "Build libpsf as a shared library" OFF)

set(LIBPSF_LIBRARY_TYPE STATIC)
if(LIBPSF_BUILD_SHARED)
  set(LIBPSF_LIBRARY_TYPE SHARED)
endif()

add_library(psf ${LIBPSF_LIBRARY_TYPE}
  src/psf.cc
  src/psfdata.cc
  src/psfproperty.cc
  src/psfpropertyblock.cc
  src/psfchunk.cc
  src/psfcontainer.cc
  src/psfindexedcontainer.cc
  src/psfgroup.cc
  src/psffile.cc
  src/psftype.cc
  src/psfstruct.cc
  src/psfsections.cc
  src/psftrace.cc
  src/psfnonsweepvalue.cc
  src/psfsweepvalue.cc
)

target_include_directories(psf
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(psf PUBLIC cxx_std_11)

include(GNUInstallDirs)

install(TARGETS psf
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
  include/psf.h
  include/psfdata.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
