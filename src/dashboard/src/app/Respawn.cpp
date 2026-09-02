#include "app/Respawn.hpp"

#include <climits>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <unistd.h>

namespace app {

void respawnSelf(const std::vector<std::string>& args)
{
   char selfPath[PATH_MAX]{};
   const ssize_t n{readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1)};
   if (n <= 0) {
      std::cerr << "respawnSelf: falha ao resolver /proc/self/exe" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   selfPath[n] = '\0';

   // execv precisa de um array de 'char*' terminado em nullptr -- os
   // std::string ficam vivos ate o execv (ou o exit de falha), entao os
   // c_str() continuam validos.
   std::vector<char*> argv;
   argv.push_back(selfPath);
   for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
   argv.push_back(nullptr);

   execv(selfPath, argv.data());

   // So chega aqui se execv falhar.
   std::cerr << "respawnSelf: execv(" << selfPath << ") falhou" << std::endl;
   std::exit(EXIT_FAILURE);
}

} // namespace app
