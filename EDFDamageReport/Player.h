#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <cstdint>
#include <utility>

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


        void setIndex(int idx);
        void setName(const char* playerName);
        void setPointer(uintptr_t ptr);
};

class PlayerList
{
    private:
        mutable std::mutex player_mutex;
        std::vector<Player> players;
        static const int MAX_PLAYERS = 4;

    public:
        PlayerList();
        PlayerList(const PlayerList&) = delete;
        PlayerList& operator=(const PlayerList&) = delete;

        void addOrUpdatePlayer(int idx, const char* name, uintptr_t pointer);
        bool addOrUpdatePlayer(const char* name, uintptr_t pointer);
        const Player* getPlayerByIndex(int idx) const;
        const Player* getPlayerByPointer(uintptr_t ptr) const;
        void resetPlayers();
        const std::vector<Player>& getAllPlayers() const;
};

class DamageSummary 
{
    private:
        mutable std::mutex data_mutex;
        PlayerList* globalPlayerList;  // 使用指针指向全局实例
        std::vector<float> player_damages;
        static const int MAX_PLAYERS = 4;

    public:
        DamageSummary(PlayerList* globalList);  // 构造函数接受全局实例指针
        DamageSummary(const DamageSummary&) = delete;
        DamageSummary& operator=(const DamageSummary&) = delete;

        void addDamage(int playerIndex, float damage);
        void addDamage(uintptr_t playerPointer, float damage);
        float getDamageForPlayer(int playerIndex) const;
        float getDamageForPlayer(uintptr_t playerPointer) const;
        std::vector<std::pair<int, float>> getAllDamages() const;
        void resetDamages();
        void resetAll();
        std::string formatAllDamages() const;
};