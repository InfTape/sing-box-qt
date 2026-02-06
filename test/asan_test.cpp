#include <cstdlib>
#include <cstring>

int main() {
  char* p = static_cast<char*>(std::malloc(8));
  std::memset(p, 0, 16); // out-of-bounds write
  std::free(p);
  return 0;
}
