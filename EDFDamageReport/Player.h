#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <cstdint>
#include <utility>
#include <unordered_map>

class Player
{
    private:
        int index;
        std::string name;
		std::string typeName;
        uintptr_t pointer;

    public:
        Player(int idx = -1, const char* playerName = "", uintptr_t ptr = 0);
        int getIndex() const;
        const std::string& getName() const;
		const std::string& getTypeName() const;
        uintptr_t getPointer() const;
        void resetPlayer();


        void setIndex(int idx);
        void setName(const char* playerName);
		void setTypeName(const char* typeName);
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

class RecentAttackLogger {
    public:
        struct AttackEvent {
            uintptr_t attacker_ptr;
            uintptr_t target_ptr;
            float damage;
        };

        void recordAttack(uintptr_t attacker, uintptr_t target, float damage);
        std::vector<std::string> getFormattedAttacks(const PlayerList& playerList);

    private:
        static const int MAX_ATTACKS = 5;
        AttackEvent attacks[MAX_ATTACKS];
        int writeIndex = 0;
        std::mutex logMutex;
};

class DamageSummary 
{
    private:
        mutable std::mutex data_mutex;
        PlayerList* globalPlayerList;
        RecentAttackLogger* attackLogger;
        std::vector<float> player_damages;
        std::vector<std::unordered_map<uintptr_t, float>> player_friendly_damage_detail;
        static const int MAX_PLAYERS = 4;

    public:
        DamageSummary(PlayerList* globalList, RecentAttackLogger* attackLogger);
        DamageSummary(const RecentAttackLogger&) = delete;
        DamageSummary(const DamageSummary&) = delete;
        DamageSummary& operator=(const RecentAttackLogger&) = delete;
        DamageSummary& operator=(const DamageSummary&) = delete;

        void addDamage(uintptr_t playerPointer, uintptr_t targetPointer, float damage);
        float getDamageForPlayer(int playerIndex) const;
        float getDamageForPlayer(uintptr_t playerPointer) const;

        void addFriendlyDamage(uintptr_t attackerPointer, uintptr_t targetPointer, float damage);
        float getFriendlyDamageForPlayer(int playerIndex) const;
        float getFriendlyDamageForPlayer(uintptr_t playerPointer) const;
        float getFriendlyDamageToTarget(int attackerIndex, uintptr_t targetPointer) const;

        std::vector<std::pair<int, float>> getAllDamages() const;

        void resetDamages();
		void resetFriendlyDamages();
        void resetAll();

        std::string formatAllDamagesSummary() const;

        void recordAttack(uintptr_t attacker_ptr, uintptr_t target_ptr, float damage);
        std::vector<std::string> getFormattedAttacks();

};