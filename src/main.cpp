#include <filesystem>
#include <iostream>

#include "Wrapper.h"

int main(int argc, char **argv) {
  auto pAssignment = Wrapper::GetSingleton();

  pAssignment->EnterMainLoop(argc, argv);
  delete pAssignment;

#ifdef _MSC_VER
  cout << "Press any key..." << endl;
  cin.get();
#endif
}