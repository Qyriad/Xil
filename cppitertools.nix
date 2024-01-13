{
  lib,
  stdenv,
  cmake,
  boost,
  catch2,
  fetchFromGitHub,
}:

stdenv.mkDerivation (self: {
  pname = "cppitertools";
  version = "2.1";

  src = fetchFromGitHub {
    owner = "ryanhaining";
    repo = "cppitertools";
    rev = "v${self.version}";
    sha256 = "sha256-mii4xjxF1YC3H/TuO/o4cEz8bx2ko6U0eufqNVw5LNA=";
    name = "cppitertools-source";
  };

  nativeBuildInputs = [
    cmake
  ];

  buildInputs = [
    boost
    # For cppitertools
    catch2
  ];

  prePatch = ''
    # Required for tests
    cp -v ${catch2}/include/catch2/catch.hpp test/
  '';

  doCheck = true;

  checkPhase = ''
    runHook preCheck
    cmake -B build-test -S ../test
    cmake --build build-test
    runHook postCheck
  '';

  meta = {
    description = "Implementation of python itertools and builtin iteration functions for C++17";
    homepage = "https://github.com/ryanhaining/cppitertools";
    licenses = lib.licenses.bsd2;
  };
})

