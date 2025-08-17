    #include "pch.h"
    #include <vector>
    #include <mutex>
    #include <string>
    #include <cstdint>
    #include <algorithm>
    #include <memory>
    #include <map>

    class Player
    {
    private:
        int index;
        std::string name;
        uintptr_t pointer;

    public:
        Player(int idx = -1, const char* playerName = "", uintptr_t ptr = 0)
            : index(idx), name(playerName ? playerName : ""), pointer(ptr) {
        }

        int getIndex() const { return index; }
        const std::string& getName() const { return name; }
        uintptr_t getPointer() const { return pointer; }

        void resetPlayer() {
            index = -1;
            name.clear();
            pointer = 0;
        }
    };

    class PlayerList
    {
    private:
        mutable std::mutex player_mutex;
        std::vector<Player> players;
        static const int MAX_PLAYERS = 4;

    public:
        PlayerList() {
            players.reserve(MAX_PLAYERS);
            for (int i = 0; i < MAX_PLAYERS; i++) {
                players.emplace_back();
            }
        }

        void addOrUpdatePlayer(int idx, const char* name, uintptr_t pointer) {
            std::lock_guard<std::mutex> lock(player_mutex);

            if (idx >= 0 && idx < MAX_PLAYERS) {
                players[idx] = Player(idx, name, pointer);
            }
        }

        bool addOrUpdatePlayer(const char* name, uintptr_t pointer) {
            std::lock_guard<std::mutex> lock(player_mutex);

            for (auto& player : players) {
                if (player.getPointer() == 0 || player.getPointer() == pointer) {
                    player = Player(player.getIndex(), name, pointer);
                    return true;
                }
            }
            return false;
        }

        Player getPlayerByIndex(int idx) const {
            std::lock_guard<std::mutex> lock(player_mutex);

            if (idx >= 0 && idx < players.size()) {
                return players[idx];
            }
            return Player();
        }

        Player getPlayerByPointer(uintptr_t ptr) const {
            std::lock_guard<std::mutex> lock(player_mutex);

            for (const auto& player : players) {
                if (player.getPointer() == ptr) {
                    return player;
                }
            }
            return Player();
        }

        void resetPlayers() {
            std::lock_guard<std::mutex> lock(player_mutex);

            for (auto& player : players) {
                player.resetPlayer();
            }
        }

        std::vector<Player> getAllPlayers() const {
            std::lock_guard<std::mutex> lock(player_mutex);
            return players;
        }
    };

    class DamageSummary
    {
    private:
        mutable std::mutex data_mutex;
        PlayerList playerList;
        std::vector<float> player_damages;
        static const int MAX_PLAYERS = 4;

    public:
        DamageSummary() {
            player_damages.resize(MAX_PLAYERS, 0.0);
        }

        void addDamage(int playerIndex, float damage) {
            std::lock_guard<std::mutex> lock(data_mutex);

            if (playerIndex >= 0 && playerIndex < MAX_PLAYERS) {
                player_damages[playerIndex] += damage;
            }
        }

        void addDamage(uintptr_t playerPointer, float damage) {
            std::lock_guard<std::mutex> lock(data_mutex);

            Player player = playerList.getPlayerByPointer(playerPointer);
            if (player.getPointer() != 0) {
                player_damages[player.getIndex()] += damage;
            }
        }

        void setPlayer(int idx, const char* name, uintptr_t pointer) {
            playerList.addOrUpdatePlayer(idx, name, pointer);
        }

        bool setPlayer(const char* name, uintptr_t pointer) {
            return playerList.addOrUpdatePlayer(name, pointer);
        }

        float getDamageForPlayer(int playerIndex) const {
            std::lock_guard<std::mutex> lock(data_mutex);

            if (playerIndex >= 0 && playerIndex < MAX_PLAYERS) {
                return player_damages[playerIndex];
            }
            return 0.0;
        }

        float getDamageForPlayer(uintptr_t playerPointer) const {
            Player player = playerList.getPlayerByPointer(playerPointer);
            return getDamageForPlayer(player.getIndex());
        }

        std::vector<std::pair<int, float>> getAllDamages() const {
            std::lock_guard<std::mutex> lock(data_mutex);

            std::vector<std::pair<int, float>> result;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                result.emplace_back(i, player_damages[i]);
            }
            return result;
        }

        void resetDamages() {
            std::lock_guard<std::mutex> lock(data_mutex);
            std::fill(player_damages.begin(), player_damages.end(), 0.0);
        }

        void resetAll() {
            resetDamages();
            playerList.resetPlayers();
        }

        PlayerList& getPlayerList() { return playerList; }

        std::string formatAllDamages() const {
            std::lock_guard<std::mutex> lock(data_mutex);
            std::string result = "Damage Summary:\n";
            for (int i = 0; i < MAX_PLAYERS; i++) {
                Player player = playerList.getPlayerByIndex(i);
                double damage = player_damages[i];
                if (player.getPointer() != 0) {
                    result += "Player " + std::to_string(i) + " (" + player.getName() + "): " + std::to_string(damage) + "\n";
                }
                else {
                    result += "Player " + std::to_string(i) + " (No Player): " + std::to_string(damage) + "\n";
                }
            }
            return result;
        }
    };