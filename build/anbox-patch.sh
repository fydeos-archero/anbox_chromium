#!/bin/bash

cd ./src/third_party/boost
git submodule init
git submodule update
cd -

#########################################################33333

cat > ./src/third_party/boost/BUILD.gn << EOF
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

config("boost_config") {
  include_dirs = [    
    ".",        
    "./libs/config/include",
    "./libs/log/include",
    "./libs/predef/include",
    "./libs/thread/include",
    "./libs/assert/include",
    "./libs/system/include",
    "./libs/parameter/include",
    "./libs/move/include",
    "./libs/throw_exception/include",
    "./libs/core/include",
    "./libs/smart_ptr/include",
    "./libs/type_index/include",
    "./libs/type_traits/include",
    "./libs/filesystem/include",
    "./libs/static_assert/include",
    "./libs/iterator/include",
    "./libs/date_time/include",
    "./libs/mpl/include",
    "./libs/utility/include",
    "./libs/preprocessor/include",
    "./libs/asio/include",
    "./libs/detail/include",
    "./libs/bind/include",
    "./libs/io/include",
    "./libs/functional/include",
    "./libs/integer/include",
    "./libs/range/include",
    "./libs/locale/include",
    "./libs/atomic/include",
    "./libs/tuple/include",
    "./libs/concept_check/include",
    "./libs/proto/include",
    "./libs/random/include",
    "./libs/exception/include",
    "./libs/optional/include",
    "./libs/array/include",
    "./libs/fusion/include",
    "./libs/function/include",
    "./libs/intrusive/include",
    "./libs/phoenix/include",
    "./libs/algorithm/include",
    "./libs/spirit/include",
    "./libs/typeof/include",
    "./libs/lexical_cast/include",
    "./libs/variant/include",
    "./libs/numeric/conversion/include",
    "./libs/math/include",
    "./libs/foreach/include",
    "./libs/regex/include",
    "./libs/container/include",
    "./libs/tokenizer/include",
    "./libs/format/include",
    "./libs/function_types/include",
    "./libs/signals2/include",
    "./libs/property_tree/include",
    "./libs/any/include",
    "./libs/multi_index/include",
    "./libs/serialization/include",
  ]
}

# Must be in a config because of how GN orders flags (otherwise -Wall will
# appear after this, and turn it back on).
config("clang_warnings") {
  if (is_clang) {
    # Upstream uses self-assignment to avoid warnings.
    cflags = [ 
      "-Wno-self-assign", 
      "-fexceptions",
      "-Wno-error=deprecated-declarations",
      "-Wno-deprecated-declarations"
    ]
    cflags_cc = [
      "-fexceptions",      
      "-mavx", "-mavx2",
      "-msse", "-msse2", "-msse3", "-mssse3",
      "-Wno-error=deprecated-declarations",
      "-Wno-deprecated-declarations"
    ]

    #libs = [ "boost_system" ]
  }
}

#shared_library("boost") {
#static_library("boost") {
component("boost") {
  sources = [    
    "libs/log/src/attribute_name.cpp",
    "libs/log/src/core.cpp",
    "libs/log/src/default_attribute_names.cpp",  
    "libs/log/src/dump.cpp",    
    "libs/log/src/dump_avx2.cpp",    
    "libs/log/src/dump_ssse3.cpp", 
    "libs/log/src/syslog_backend.cpp",
    "libs/log/src/record_ostream.cpp",
    "libs/log/src/severity_level.cpp",    
    "libs/log/src/exceptions.cpp",
    "libs/log/src/once_block.cpp",
    "libs/log/src/attribute_set.cpp",
    "libs/log/src/attribute_value_set.cpp",
    "libs/log/src/thread_specific.cpp",
    "libs/log/src/default_sink.cpp",
    "libs/log/src/thread_id.cpp",
    "libs/log/src/date_time_format_parser.cpp",
    "libs/log/src/text_ostream_backend.cpp",
    "libs/log/src/trivial.cpp",
    "libs/log/src/code_conversion.cpp",
    "libs/log/src/global_logger_storage.cpp",

    "libs/system/src/error_code.cpp",
    "libs/filesystem/src/operations.cpp",
    "libs/filesystem/src/path.cpp",

    "libs/thread/src/pthread/thread.cpp",
    "libs/thread/src/pthread/once_atomic.cpp",    
  ]  

  defines = [
    #"BOOST_ALL_NO_LIB",
    #"BOOST_ATOMIC_DYN_LINK",
    #"BOOST_CHRONO_DYN_LINK",
    #"BOOST_DATE_TIME_DYN_LINK",
    #"BOOST_FILESYSTEM_DYN_LINK",
    #"BOOST_SYSTEM_DYN_LINK",    

    #"BOOST_LOG_DYN_LINK",
    "BOOST_ALL_DYN_LINK",

    #"BOOST_ATOMIC_STATIC_LINK",
    #"BOOST_CHRONO_STATIC_LINK",
    #"BOOST_DATE_TIME_STATIC_LINK",
    #"BOOST_FILESYSTEM_STATIC_LINK",
    #"BOOST_SYSTEM_STATIC_LINK",
    #"BOOST_LOG_STATIC_LINK",

    "BOOST_HAS_ICU",
    "BOOST_LOG_HAS_PTHREAD_MUTEX_ROBUST",    
    "BOOST_LOG_USE_AVX2",     
    "BOOST_LOG_USE_SSSE3",
    "BOOST_LOG_WITHOUT_EVENT_LOG",    
    "BOOST_SPIRIT_USE_PHOENIX_V3",    
    "BOOST_THREAD_DONT_USE_CHRONO",
    "BOOST_THREAD_POSIX",     
    #"BOOST_THREAD_BUILD_DLL",
    #"BOOST_THREAD_USE_DLL",    
    "DATE_TIME_INLINE",
    #"BOOST_LOG_SETUP_DLL",
    "BOOST_SYSTEM_NO_DEPRECATED",    
    "BOOST_LOG_WITHOUT_DEBUG_OUTPUT",
    "BOOST_LOG_BUILDING_THE_LIB",    
    "BOOST_LOG_DLL",    
  ]

  configs -= [ "//build/config/compiler:chromium_code" ]
  configs += [
    "//build/config/compiler:no_chromium_code",    

    # Must be after no_chromium_code for warning flags to be ordered correctly.
    "//build/config/compiler:rtti",
    ":clang_warnings",
  ]
  public_configs = [ ":boost_config" ]
}
EOF

###################################################################

properties_cpp_version=0.0.1+14.10.20140730.orig.tar.gz
wget https://launchpad.net/ubuntu/+archive/primary/+files/properties-cpp_$properties_cpp_version

mkdir -p src/third_party/properties-cpp
tar xf properties-cpp_$properties_cpp_version -C src/third_party/properties-cpp --strip-components=1
rm properties-cpp_$properties_cpp_version

cat > ./src/third_party/properties-cpp/BUILD.gn << EOF
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

config("properties_cpp_config") {
  include_dirs = [    
    "include/",        
  ]
}

# Must be in a config because of how GN orders flags (otherwise -Wall will
# appear after this, and turn it back on).
config("clang_warnings") {
  cflags_cc = [ "-std=c++14" ]
  cflags_objcc = [ "-std=c++14" ]

  if (is_clang) {
    # Upstream uses self-assignment to avoid warnings.
    cflags = [ "-Wno-self-assign" ]    

    cflags += [ "-Wno-exit-time-destructors" ]
    cflags_cc += [ "-Wno-exit-time-destructors" ]
    cflags_objcc += [ "-Wno-exit-time-destructors" ]
  }
}

component("properties_cpp") {
  sources = [    
    
  ]

  configs -= [ "//build/config/compiler:chromium_code" ]
  configs += [
    "//build/config/compiler:no_chromium_code",        

    # Must be after no_chromium_code for warning flags to be ordered correctly.
    ":clang_warnings",
    "//build/config/compiler:exceptions",
  ]
  public_configs = [ ":properties_cpp_config" ]
}
EOF