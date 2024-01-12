// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef USE_PDEVUNIT
#include <pdevunit/pdevunit.h>
#include <logging_base/logging.h>

int main(int argc, char *argv[])
{
    logging_base::configure(argc, argv);
    return pdevunit::run(argc, argv);
}

#else

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

int main( int argc, char **argv)
{
  CppUnit::TextUi::TestRunner runner;
  CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();
  runner.addTest( registry.makeTest() );
  bool wasSuccessful = runner.run( "", false );
  return !wasSuccessful;
}
#endif
