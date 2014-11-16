

#include <iostream>
#include <fstream>

#include "testing.h"

int main (int argc, char * const argv[])
{
  
  // insert code here...

  std::cout << "TEST NOW STARTING...\n\n";

  Testing::runAllTests();
  Testing::output();

  if (0 != Testing::getGlobalFailedVar()) {
    return -1;
  }

  return 0;
}
