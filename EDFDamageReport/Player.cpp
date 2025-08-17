#include "pch.h"
#include "Player.h"
#include <algorithm>
#include <cstdio>

Player::Player(int idx, const char* playerName, uintptr_t ptr)
    : index(idx), name(playerName ? playerName : ""), pointer(ptr)
{
}

int Player::getIndex() const { return index; }
const std::string& Player::getName() const { return name; }
uintptr_t Player::getPointer() const { return pointer; }

void Player::setIndex(int idx) { index = idx; }
void Player::setName(const char* playerName) { name = playerName ? playerName : ""; }
void Player::setPointer(uintptr_t ptr) { pointer = ptr; }

void Player::resetPlayer() {
    index = -1;
    name.clear();
    pointer = 0;
}

PlayerList::PlayerList() {
    players.reserve(MAX_PLAYERS);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players.emplace_back(i, "", 0);
    }
}

void PlayerList::addOrUpdatePlayer(int idx, const char* name, uintptr_t pointer) {
    std::lock_guard<std::mutex> lock(player_mutex);
    if (idx >= 0 && idx < MAX_PLAYERS) {
        players[idx].setIndex(idx);
        players[idx].setName(name);
        players[idx].setPointer(pointer);
    }
}

bool PlayerList::addOrUpdatePlayer(const char* name, uintptr_t pointer) {
    std::lock_guard<std::mutex> lock(player_mutex);
    for (auto& player : players) {
        if (player.getPointer() == 0 || player.getPointer() == pointer) {
            player.setName(name);
            player.setPointer(pointer);
            return true;
        }
    }
    return false;
}

const Player* PlayerList::getPlayerByIndex(int idx) const {
    std::lock_guard<std::mutex> lock(player_mutex);
    if (idx >= 0 && idx < MAX_PLAYERS) {
        return &players[idx];
    }
    return nullptr;
}

const Player* PlayerList::getPlayerByPointer(uintptr_t ptr) const {
    std::lock_guard<std::mutex> lock(player_mutex);
    for (const auto& player : players) {
        if (player.getPointer() == ptr) {
            return &player;
        }
    }
    return nullptr;
}

void PlayerList::resetPlayers() {
    std::lock_guard<std::mutex> lock(player_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].resetPlayer();
    }
}

const std::vector<Player>& PlayerList::getAllPlayers() const {
    return players;
}

DamageSummary::DamageSummary(PlayerList* globalList)
    : globalPlayerList(globalList)
{
    player_damages.resize(MAX_PLAYERS, 0.0f);
}

void DamageSummary::addDamage(int playerIndex, float damage) {
    std::lock_guard<std::mutex> lock(data_mutex);
    if (playerIndex >= 0 && playerIndex < MAX_PLAYERS) {
        player_damages[playerIndex] += damage;
    }
}

void DamageSummary::addDamage(uintptr_t playerPointer, float damage) {
    std::lock_guard<std::mutex> lock(data_mutex);
    if (globalPlayerList) {
        const Player* player = globalPlayerList->getPlayerByPointer(playerPointer);
        if (player && player->getPointer() != 0) {
            player_damages[player->getIndex()] += damage;
        }
    }
}

float DamageSummary::getDamageForPlayer(int playerIndex) const {
    std::lock_guard<std::mutex> lock(data_mutex);
    if (playerIndex >= 0 && playerIndex < MAX_PLAYERS) {
        return player_damages[playerIndex];
    }
    return 0.0f;
}

float DamageSummary::getDamageForPlayer(uintptr_t playerPointer) const {
    if (globalPlayerList) {
        const Player* player = globalPlayerList->getPlayerByPointer(playerPointer);
        if (player) {
            return getDamageForPlayer(player->getIndex());
        }
    }
    return 0.0f;
}

std::vector<std::pair<int, float>> DamageSummary::getAllDamages() const {
    std::lock_guard<std::mutex> lock(data_mutex);
    std::vector<std::pair<int, float>> result;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        result.emplace_back(i, player_damages[i]);
    }
    return result;
}

void DamageSummary::resetDamages() {
    std::lock_guard<std::mutex> lock(data_mutex);
    std::fill(player_damages.begin(), player_damages.end(), 0.0f);
}

void DamageSummary::resetAll() {
    resetDamages();
}

std::string DamageSummary::formatAllDamages() const {
    std::lock_guard<std::mutex> lock(data_mutex);
    std::string result = "Damage Summary:\n";

    if (globalPlayerList) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            const Player* player = globalPlayerList->getPlayerByIndex(i);
            float damage = player_damages[i];
            if (player && player->getPointer() != 0) {
                result += "Player " + std::to_string(i) + " (" + player->getName() + "): " + std::to_string(damage) + "\n";
            }
            else {
                result += "Player " + std::to_string(i) + " (No Player): " + std::to_string(damage) + "\n";
            }
        }
    }
    else {
        result += "ERROR: Global PlayerList not available\n";
    }

    return result;
}