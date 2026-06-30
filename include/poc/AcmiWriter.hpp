#pragma once

#include <string>
#include <fstream>
#include <ctime>

namespace mixr { namespace simulation { class Station; } }

namespace poc {

//------------------------------------------------------------------------------
// AcmiWriter
//
// Grava a trajetória de todos os players ativos num arquivo ACMI 2.2
// (formato texto UTF-8 reconhecido pelo Tacview).
//
// Formato por frame:
//   #<tempo_s>
//   <id_hex>,T=<lon>|<lat>|<alt_m>|<roll_deg>|<pitch_deg>|<yaw_deg>
//
// Propriedades estáticas declaradas na primeira vez que um player aparece:
//   <id_hex>,T=...,Name=<nome>,Type=Air+FixedWing,Color=Blue|Red
//
// Uso:
//   AcmiWriter writer("saida.acmi");
//   writer.open(station);          // escreve o cabeçalho
//   // a cada frame de updateData:
//   writer.writeFrame(t, station);
//   writer.close();
//------------------------------------------------------------------------------
class AcmiWriter
{
public:
    explicit AcmiWriter(const std::string& path);
    ~AcmiWriter();

    // Abre o arquivo e escreve o cabeçalho ACMI com ReferenceTime = agora (UTC)
    bool open(const mixr::simulation::Station* station);

    // Grava um frame: offsets de tempo + posição/atitude de todos os players
    // t_sim: tempo simulado em segundos desde o início da simulação
    void writeFrame(double t_sim, const mixr::simulation::Station* station);

    void close();

    bool isOpen() const { return file.is_open(); }

private:
    std::string  path;
    std::ofstream file;
    bool          headerWritten { false };

    // Rastreia quais IDs já tiveram suas propriedades estáticas escritas
    // (Name, Type, Color) para não repetir a cada frame
    struct PlayerMeta {
        bool declared { false };
    };
    // mapa simples: id → meta (usa vetor indexado por id, máx 65535)
    static const int MAX_ID { 65536 };
    PlayerMeta meta[MAX_ID] {};

    static std::string nowUtcIso();
    static std::string acmiType(int majorType);  // mapeia Player::MAJOR_TYPE
    static std::string acmiColor(int side);      // mapeia Player::BLUE/RED/...
};

} // namespace poc
