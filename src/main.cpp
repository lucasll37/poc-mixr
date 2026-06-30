//------------------------------------------------------------------------------
// poc-mixr  v0.1
//
// Demonstração com:
//   - JSBSimModel (6DOF completo)
//   - Navigation com Steerpoints
//   - Radar
//   - Gravação ACMI 2.2 para Tacview
//
// Uso:
//   ./poc-mixr [duracao_s] [output.acmi]
//   Padrão: 60 segundos, saída em "output/flight.acmi"
//------------------------------------------------------------------------------

#include "poc/factory.hpp"
#include "poc/MyStation.hpp"
#include "poc/AcmiWriter.hpp"

#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/Component.hpp"
#include "mixr/simulation/Station.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>
#include <sys/stat.h>

static void sleep_s(double dt)
{
    using namespace std::chrono;
    std::this_thread::sleep_for(duration_cast<nanoseconds>(duration<double>(dt)));
}

int main(int argc, char* argv[])
{
    // ── Argumentos ─────────────────────────────────────────────────────────
    double      simDuration { 60.0 };
    std::string acmiPath    { "output/flight.acmi" };

    if (argc >= 2) simDuration = std::atof(argv[1]);
    if (argc >= 3) acmiPath    = argv[2];

    std::cout << "==========  poc-mixr v0.1  ==========\n"
              << "  Duração:  " << simDuration << "s\n"
              << "  ACMI:     " << acmiPath    << "\n"
              << "======================================\n";

    // ── Cria diretório de saída ─────────────────────────────────────────────
    ::mkdir("output", 0755);

    // ── Fábrica encadeada ───────────────────────────────────────────────────
    mixr::base::Object::setFactory(poc::factory);

    // ── Carrega EDL ────────────────────────────────────────────────────────
    int errors {};
    auto* pair = mixr::base::edl_parser("config/sim.edl", poc::factory, &errors);
    if (pair == nullptr || errors > 0) {
        std::cerr << "Falha ao carregar config/sim.edl (" << errors << " erros)\n";
        return 1;
    }

    auto* station = dynamic_cast<mixr::simulation::Station*>(pair->object());
    if (station == nullptr) {
        std::cerr << "Raiz do EDL não é uma Station\n";
        pair->unref(); return 1;
    }
    station->ref();
    pair->unref();

    // ── Reset ───────────────────────────────────────────────────────────────
    station->event(mixr::base::Component::RESET_EVENT);
    std::cout << "Reset concluído.\n";

    // ── AcmiWriter ──────────────────────────────────────────────────────────
    poc::AcmiWriter acmi(acmiPath);
    if (!acmi.open(station)) {
        std::cerr << "Não foi possível abrir o arquivo ACMI\n";
        station->unref(); return 1;
    }

    // ── Thread TC ───────────────────────────────────────────────────────────
    station->createTimeCriticalProcess();

    // ── Laço principal (background + gravação ACMI) ─────────────────────────
    const double BG_HZ      { 10.0 };
    const double BG_DT      { 1.0 / BG_HZ };
    const double ACMI_HZ    { 10.0 };  // frequência de gravação ACMI (10 Hz)
    const double ACMI_DT    { 1.0 / ACMI_HZ };

    double t_sim      { 0.0 };
    double acmiTimer  { 0.0 };

    const int total_steps { static_cast<int>(simDuration / BG_DT) };

    auto wall_t0 = std::chrono::steady_clock::now();
    auto next_bg = wall_t0;

    std::cout << "Simulando " << simDuration << "s"
              << " · ACMI a " << ACMI_HZ << " Hz...\n";

    for (int step = 0; step < total_steps; ++step) {
        // Avança o tempo simulado
        t_sim    += BG_DT;
        acmiTimer += BG_DT;

        // Background
        station->updateData(BG_DT);

        // Grava ACMI na frequência configurada
        if (acmiTimer >= ACMI_DT) {
            acmi.writeFrame(t_sim, station);
            acmiTimer = 0.0;
        }

        // Progresso no console a cada 10 s
        if (step % static_cast<int>(10.0 / BG_DT) == 0) {
            std::cout << "  t=" << std::fixed << std::setprecision(1)
                      << t_sim << "s / " << simDuration << "s\n";
        }

        // Dorme até o próximo frame de BG (best-effort)
        next_bg += std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::duration<double>(BG_DT));
        std::this_thread::sleep_until(next_bg);
    }

    auto wall_t1 = std::chrono::steady_clock::now();
    double wall  = std::chrono::duration<double>(wall_t1 - wall_t0).count();

    std::cout << "\nConcluído em " << std::fixed << std::setprecision(2)
              << wall << "s de tempo real"
              << "  (fator = " << simDuration / wall << "x)\n";

    // ── Shutdown ────────────────────────────────────────────────────────────
    acmi.close();
    station->event(mixr::base::Component::SHUTDOWN_EVENT);
    station->unref();

    std::cout << "Arquivo ACMI salvo em: " << acmiPath << "\n"
              << "Abra no Tacview para visualizar a trajetória.\n"
              << "==========  fim  ==========\n";
    return 0;
}
