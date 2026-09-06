#include "app/ScenarioFolder.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace app {

namespace {

bool endsWith(const std::string& name, const std::string& sufixo)
{
   return name.size() >= sufixo.size()
       && name.compare(name.size() - sufixo.size(), sufixo.size(), sufixo) == 0;
}

// '.generated.edl' tambem termina em '.edl' -- excluido ANTES de checar o
// sufixo positivo, ou o artefato de saida do proprio pipeline contaria como
// um segundo candidato (ver o comentario do header).
bool hasEdlSuffix(const std::string& name)
{
   if (endsWith(name, ".generated.edl")) return false;
   return endsWith(name, ".edl.in") || endsWith(name, ".edl");
}

} // namespace

std::vector<FolderScenarioEntry> discoverFolderScenarios(const std::string& pasta)
{
   std::vector<FolderScenarioEntry> encontrados;

   std::error_code ec;
   if (!std::filesystem::exists(pasta, ec) || ec) return encontrados;

   for (const auto& sub : std::filesystem::directory_iterator(pasta, ec)) {
      if (ec) break;
      if (!sub.is_directory()) continue;

      const std::filesystem::path configsDir{sub.path() / "configs"};
      std::error_code ecConfigs;
      if (!std::filesystem::exists(configsDir, ecConfigs) || ecConfigs) continue;

      std::string achado;
      int contagem{};
      std::error_code ecArq;
      for (const auto& arq : std::filesystem::directory_iterator(configsDir, ecArq)) {
         if (ecArq) break;
         if (!arq.is_regular_file()) continue;
         if (hasEdlSuffix(arq.path().filename().string())) {
            achado = arq.path().string();
            ++contagem;
         }
      }

      if (contagem == 1) {
         encontrados.push_back(FolderScenarioEntry{sub.path().filename().string(), achado});
      } else {
         std::cerr << "[main] aviso: '" << configsDir.string() << "' tem " << contagem
                   << " arquivo(s) .edl/.edl.in -- precisa ser exatamente 1, pulando"
                   << std::endl;
      }
   }

   std::sort(encontrados.begin(), encontrados.end(),
             [](const FolderScenarioEntry& a, const FolderScenarioEntry& b) { return a.name < b.name; });
   return encontrados;
}

} // namespace app
