#include "app/ScenarioUpload.hpp"

namespace app {

namespace {

bool contains(const std::string& body, const std::string& token)
{
   return body.find(token) != std::string::npos;
}

void rejectIfContains(const std::string& body, const std::string& token,
                      const std::string& reason, std::vector<std::string>& errors)
{
   if (contains(body, token)) {
      errors.push_back("corpo contem '" + token + "': " + reason);
   }
}

} // namespace

UploadValidation validateScenarioBody(const std::string& body, const std::size_t maxBytes)
{
   UploadValidation result;

   if (body.empty()) {
      result.errors.push_back("corpo vazio");
   }
   if (body.size() > maxBytes) {
      result.errors.push_back("corpo excede o limite de " + std::to_string(maxBytes) + " bytes");
   }
   if (body.find('\0') != std::string::npos) {
      result.errors.push_back("corpo contem byte NUL -- esperado texto EDL, nao binario");
   }

   rejectIfContains(body, "PluginLoader", "o sim-runner ja injeta o dele; dlopen arbitrario nao e permitido", result.errors);
   rejectIfContains(body, "PluginModule", "o sim-runner ja injeta o dele; dlopen arbitrario nao e permitido", result.errors);
   rejectIfContains(body, "networks:", "mantem a execucao hermetica (sem DIS)", result.errors);
   rejectIfContains(body, "dataRecorder:", "evita colisao de porta Tacview entre requisicoes concorrentes", result.errors);

   result.valid = result.errors.empty();
   return result;
}

} // namespace app
