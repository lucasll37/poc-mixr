#include "poc/AcmiWriter.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/Simulation.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/units/angle_utils.hpp"

#include <ctime>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <iostream>

// Converte radianos para graus
static inline double r2d(double r) { return r * mixr::base::angle::R2DCC; }

namespace poc {

AcmiWriter::AcmiWriter(const std::string& p) : path(p) {}

AcmiWriter::~AcmiWriter() { close(); }

// ── nowUtcIso ───────────────────────────────────────────────────────────────
std::string AcmiWriter::nowUtcIso()
{
    std::time_t t = std::time(nullptr);
    std::tm* utc  = std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
    return buf;
}

// ── acmiType ────────────────────────────────────────────────────────────────
// Mapeia MIXR Player::MAJOR_TYPE para a taxonomia de tipo do ACMI 2.2
// https://www.tacview.net/documentation/acmi/en/#Types
std::string AcmiWriter::acmiType(int majorType)
{
    using P = mixr::simulation::AbstractPlayer;
    switch (majorType) {
        case P::AIR_VEHICLE:    return "Air+FixedWing";
        case P::GROUND_VEHICLE: return "Ground+Vehicle";
        case P::SHIP:           return "Sea+Watercraft";
        case P::WEAPON:         return "Weapon+Missile";
        case P::LIFE_FORM:      return "Ground+LifeForm";
        case P::BUILDING:       return "Ground+Structure";
        case P::EFFECT:         return "Misc+Decoy";
        default:                return "Unknown";
    }
}

// ── acmiColor ───────────────────────────────────────────────────────────────
std::string AcmiWriter::acmiColor(int side)
{
    using P = mixr::simulation::AbstractPlayer;
    switch (side) {
        case P::BLUE: return "Blue";
        case P::RED:  return "Red";
        default:      return "Grey";
    }
}

// ── open ────────────────────────────────────────────────────────────────────
bool AcmiWriter::open(const mixr::simulation::Station* /*station*/)
{
    file.open(path);
    if (!file.is_open()) {
        std::cerr << "[AcmiWriter] Não foi possível abrir: " << path << std::endl;
        return false;
    }

    // Cabeçalho obrigatório ACMI 2.2
    file << "FileType=text/acmi/tacview\n";
    file << "FileVersion=2.2\n";

    // Propriedades globais (objeto 0)
    file << "0,DataSource=poc-mixr v0.1\n";
    file << "0,DataRecorder=poc-mixr\n";
    file << "0,ReferenceTime=" << nowUtcIso() << "\n";
    file << "0,RecordingTime=" << nowUtcIso() << "\n";
    file << "0,Title=poc-mixr simulation\n";
    file << "0,Category=Air Combat\n";
    file << "0,Author=poc-mixr\n";

    headerWritten = true;
    std::cout << "[AcmiWriter] Gravando em " << path << std::endl;
    return true;
}

// ── writeFrame ──────────────────────────────────────────────────────────────
void AcmiWriter::writeFrame(double t_sim,
                            const mixr::simulation::Station* station)
{
    if (!file.is_open() || !headerWritten) return;

    const auto* sim = station->getSimulation();
    if (sim == nullptr) return;

    // Lista de players ativos
    mixr::base::PairStream* players = sim->getPlayers();
    if (players == nullptr) return;

    // Marca o offset de tempo
    file << std::fixed << std::setprecision(3) << "#" << t_sim << "\n";

    // Itera sobre todos os players
    mixr::base::List::Item* item = players->getFirstItem();
    while (item != nullptr) {
        const auto* pair = static_cast<const mixr::base::Pair*>(item->getValue());
        const auto* p    = static_cast<const mixr::simulation::AbstractPlayer*>(
                               pair->object());

        if (p != nullptr && p->isMode(mixr::simulation::AbstractPlayer::ACTIVE)) {
            unsigned short id = p->getID();

            // Posição geodésica
            double lat = p->getLatitude();   // graus
            double lon = p->getLongitude();  // graus
            double alt = p->getAltitudeM(); // metros MSL

            // Atitude em graus (Euler: roll, pitch, yaw)
            // MIXR armazena em radianos — converter para graus
            double roll  = r2d(p->getRollR());
            double pitch = r2d(p->getPitchR());
            double yaw   = r2d(p->getHeadingR());  // yaw = heading verdadeiro

            // Primeira vez: declara as propriedades estáticas do objeto
            if (id < MAX_ID && !meta[id].declared) {
                meta[id].declared = true;

                const char* name = p->getName();
                std::string type  = acmiType(p->getMajorType());
                std::string color = acmiColor(p->getSide());

                file << std::hex << id << std::dec
                     << ",T=" << lon << "|" << lat << "|" << alt
                     << "|" << roll << "|" << pitch << "|" << yaw
                     << ",Name=" << (name ? name : "unknown")
                     << ",Type=" << type
                     << ",Color=" << color
                     << "\n";
            } else {
                // Frames subsequentes: apenas posição + atitude
                file << std::hex << id << std::dec
                     << ",T=" << lon << "|" << lat << "|" << alt
                     << "|" << roll << "|" << pitch << "|" << yaw
                     << "\n";
            }
        }

        item = item->getNext();
    }

    players->unref();
    file.flush();
}

// ── close ───────────────────────────────────────────────────────────────────
void AcmiWriter::close()
{
    if (file.is_open()) {
        file.flush();
        file.close();
        std::cout << "[AcmiWriter] Arquivo fechado: " << path << std::endl;
    }
}

} // namespace poc
