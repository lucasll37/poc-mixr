#include "app/ScenarioPickerScreen.hpp"

#include "app/ScenarioCatalog.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <vector>

namespace app {

namespace {
using namespace ftxui;
}

std::string runScenarioPicker()
{
   const auto& catalog{scenarioCatalog()};

   std::vector<std::string> labels;
   labels.reserve(catalog.size());
   for (const auto& entry : catalog) labels.push_back(entry.label);

   int selected{};
   bool confirmed{};
   bool cancelled{};

   auto screen = ScreenInteractive::Fullscreen();

   auto menuOption = MenuOption::Vertical();
   menuOption.entries = &labels;
   menuOption.selected = &selected;
   menuOption.on_enter = [&] { confirmed = true; screen.Exit(); };

   const Component menu{Menu(menuOption)};

   const Component root = Renderer(menu, [&] {
      Elements items;
      items.push_back(text("dashboard -- selecione um cenario") | bold | color(Color::CyanLight));
      items.push_back(separator());
      items.push_back(menu->Render() | flex);
      items.push_back(separator());
      const std::string& desc{(selected >= 0 && selected < static_cast<int>(catalog.size()))
                              ? catalog[static_cast<std::size_t>(selected)].description
                              : std::string{}};
      items.push_back(paragraphAlignLeft(desc) | color(Color::GrayLight));
      items.push_back(separator());
      items.push_back(text("[up/down] navegar   [enter] carregar   [q] sair") | dim);

      return vbox(std::move(items)) | border | size(WIDTH, GREATER_THAN, 60)
             | size(HEIGHT, GREATER_THAN, 16) | center;
   });

   const Component withQuit = CatchEvent(root, [&](const Event& event) {
      if (event == Event::Character('q') || event == Event::Character('Q') ||
          event == Event::Escape) {
         cancelled = true;
         screen.Exit();
         return true;
      }
      return false;
   });

   screen.Loop(withQuit);

   if (cancelled || !confirmed) return {};
   if (selected < 0 || selected >= static_cast<int>(catalog.size())) return {};
   return catalog[static_cast<std::size_t>(selected)].key;
}

} // namespace app
