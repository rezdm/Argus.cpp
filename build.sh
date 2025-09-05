#!/bin/sh

#debug
cmake -S src/ -B build-debug/ -DCMAKE_BUILD_TYPE=Debug -DHTTPLIB_REQUIRE_OPENSSL=ON -DHTTPLIB_USE_OPENSSL_IF_AVAILABLE=ON
cmake --build build-debug/ -j4

#release
#cmake -S src/ -B build-release/ -DCMAKE_BUILD_TYPE=Release -DHTTPLIB_REQUIRE_OPENSSL=ON -DHTTPLIB_USE_OPENSSL_IF_AVAILABLE=ON
#cmake --build build-release/ -j4

