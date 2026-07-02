//------------------------------------------------------------------------------
// mixr-hello
//
// O exemplo mais simples possível usando o framework MIXR: cria uma Station
// com uma Simulation vazia (sem players, sem física) e roda o ciclo
// updateTC()/updateData() por 5 segundos.
//
// Usa apenas os componentes mixr_base + mixr_simulation -- nenhuma classe
// de "models" é necessária para isto funcionar.
//------------------------------------------------------------------------------

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/Simulation.hpp"
#include "mixr/base/Component.hpp"

#include <iostream>
#include <chrono>
#include <thread>

using namespace mixr;

int main()
{
    std::cout << "=== mixr-hello ===" << std::endl;

    // 1. Cria a Station e a Simulation diretamente em C++ (sem EDL)
    auto* sim = new simulation::Simulation();
    sim->ref();

    auto* station = new simulation::Station();
    station->ref();
    station->setSlotSimulation(sim);   // Station "adota" a Simulation
    sim->unref();                       // Station já mantém sua referência

    // 2. Reset: inicializa tudo (mesmo sem players, monta a estrutura interna)
    station->event(base::Component::RESET_EVENT);
    std::cout << "Reset concluído." << std::endl;

    // 3. Laço manual a 50 Hz por 5 segundos (sem threads dedicadas --
    //    chamamos updateTC()/updateData() diretamente)
    const double dt      { 1.0 / 50.0 };
    const int    n_steps { 50 * 5 };   // 5 segundos

    for (int i = 0; i < n_steps; ++i) {
        station->updateTC(dt);

        if (i % 10 == 0) {              // a cada 10 passos (~10 Hz)
            station->updateData(dt * 10.0);
        }

        if (i % 50 == 0) {              // a cada segundo simulado
            std::cout << "  t = " << (i * dt) << "s" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(dt));
    }

    // 4. Shutdown
    station->event(base::Component::SHUTDOWN_EVENT);
    station->unref();

    std::cout << "=== fim ===" << std::endl;
    return 0;
}
