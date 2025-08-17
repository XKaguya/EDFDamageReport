#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <cstdint>

class Player
{
private:
    int index;
    std::string name;
    uintptr_t pointer;

public:
    Player(int idx = -1, const char* playerName = "", uintptr_t ptr = 0);

    int getIndex() const;
    const std::string& getName() const;
    uintptr_t getPointer() const;
    void resetPlayer();
};

class PlayerList
{
private:
    mutable std::mutex player_mutex;
    std::vector<Player> players;
    static const int MAX_PLAYERS = 4;

public:
    PlayerList();

    void addOrUpdatePlayer(int idx, const char* name, uintptr_t pointer);
    bool addOrUpdatePlayer(const char* name, uintptr_t pointer);
    Player getPlayerByIndex(int idx) const;
    Player getPlayerByPointer(uintptr_t ptr) const;
    void resetPlayers();
    std::vector<Player> getAllPlayers() const;
};

class DamageSummary
{
private:
    mutable std::mutex data_mutex;
    PlayerList playerList;
    std::vector<float> player_damages;
    static const int MAX_PLAYERS = 4;

public:
    DamageSummary();

    void addDamage(int playerIndex, float damage);
    void addDamage(uintptr_t playerPointer, float damage);
    void setPlayer(int idx, const char* name, uintptr_t pointer);
    bool setPlayer(const char* name, uintptr_t pointer);
    float getDamageForPlayer(int playerIndex) const;
    float getDamageForPlayer(uintptr_t playerPointer) const;
    std::vector<std::pair<int, float>> getAllDamages() const;
    void resetDamages();
    void resetAll();
    PlayerList& getPlayerList();
    std::string formatAllDamages() const;
};