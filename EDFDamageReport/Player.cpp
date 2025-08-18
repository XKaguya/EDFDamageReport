#include "pch.h"
#include "Player.h"
#include "Utils.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

Player::Player(int idx, const char* playerName, uintptr_t ptr)
    : index(idx), name(playerName ? playerName : ""), typeName(""), pointer(ptr)
{
}

int Player::getIndex() const { return index; }
const std::string& Player::getName() const { return name; }
const std::string& Player::getTypeName() const { return typeName; }
uintptr_t Player::getPointer() const { return pointer; }

void Player::setIndex(int idx) { index = idx; }
void Player::setName(const char* playerName) { name = playerName ? playerName : ""; }
void Player::setTypeName(const char* typeName_) { typeName = typeName_ ? typeName_ : ""; }
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
		players[idx].setTypeName(AttemptReadRTTI(pointer).c_str());
    }
}

bool PlayerList::addOrUpdatePlayer(const char* name, uintptr_t pointer) {
    std::lock_guard<std::mutex> lock(player_mutex);
    for (auto& player : players) {
        if (player.getPointer() == 0 || player.getPointer() == pointer) {
            player.setName(name);
            player.setPointer(pointer);
            player.setTypeName(AttemptReadRTTI(pointer).c_str());
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

void RecentAttackLogger::recordAttack(uintptr_t attacker, uintptr_t target, float damage) {
    std::lock_guard<std::mutex> lock(logMutex);

    attacks[writeIndex] = { attacker, target, damage };
    writeIndex = (writeIndex + 1) % MAX_ATTACKS;
}

std::vector<std::string> RecentAttackLogger::getFormattedAttacks(const PlayerList& playerList) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::vector<std::string> results;

    for (int i = 0; i < MAX_ATTACKS; i++) {
        int index = (writeIndex - 1 - i + MAX_ATTACKS) % MAX_ATTACKS;
        const AttackEvent& event = attacks[index];

        if (event.attacker_ptr == 0 && event.target_ptr == 0) continue;
        const Player* attacker = playerList.getPlayerByPointer(event.attacker_ptr);

        if (!attacker) {
            continue;
        }

        const Player* targetPlayer = playerList.getPlayerByPointer(event.target_ptr);
        std::string targetInfo;

        if (targetPlayer) {
            char targetBuffer[128];
            snprintf(targetBuffer, sizeof(targetBuffer), "Player[%s][%d][%s]",
                targetPlayer->getName().c_str(),
                targetPlayer->getIndex(),
                targetPlayer->getTypeName().c_str());
            targetInfo = targetBuffer;
        }
        else {

            targetInfo = AttemptReadRTTI(event.target_ptr);
        }

        if (targetInfo.empty()) {
            targetInfo = "UnknownTarget";
        }

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Player[%s][%d][%s] Deals %.1f To %s",
            attacker->getName().c_str(),
            attacker->getIndex(),
            attacker->getTypeName().c_str(),
            event.damage,
            targetInfo.c_str());

        results.push_back(buffer);
    }

    return results;
}

DamageSummary::DamageSummary(PlayerList* globalList, RecentAttackLogger* attackLogger)
    : globalPlayerList(globalList),
    attackLogger(attackLogger)
{
    player_damages.resize(MAX_PLAYERS, 0.0f);
    player_friendly_damage_detail.resize(MAX_PLAYERS);
}

void DamageSummary::addDamage(uintptr_t playerPointer, uintptr_t targetPointer, float damage) {
    std::lock_guard<std::mutex> lock(data_mutex);
    if (globalPlayerList) {
        const Player* player = globalPlayerList->getPlayerByPointer(playerPointer);
        if (player && player->getPointer() != 0) {
            player_damages[player->getIndex()] += damage;
			recordAttack(playerPointer, targetPointer, damage);
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

void DamageSummary::addFriendlyDamage(uintptr_t attackerPointer, uintptr_t targetPointer, float damage)
{
    std::lock_guard<std::mutex> lock(data_mutex);
    if (globalPlayerList) {
        const Player* attacker = globalPlayerList->getPlayerByPointer(attackerPointer);
        if (attacker && attacker->getPointer() != 0 && targetPointer != 0) {
            int idx = attacker->getIndex();
            player_friendly_damage_detail[idx][targetPointer] += damage;
            recordAttack(attackerPointer, targetPointer, damage);
        }
    }
}

float DamageSummary::getFriendlyDamageForPlayer(int playerIndex) const
{
    std::lock_guard<std::mutex> lock(data_mutex);
    float total = 0.0f;
    if (playerIndex >= 0 && playerIndex < MAX_PLAYERS) {
        for (const auto& kv : player_friendly_damage_detail[playerIndex]) {
            total += kv.second;
        }
    }
    return total;
}

float DamageSummary::getFriendlyDamageForPlayer(uintptr_t playerPointer) const
{
    if (globalPlayerList) {
        const Player* player = globalPlayerList->getPlayerByPointer(playerPointer);
        if (player) {
            return getFriendlyDamageForPlayer(player->getIndex());
        }
    }
    return 0.0f;
}

float DamageSummary::getFriendlyDamageToTarget(int attackerIndex, uintptr_t targetPointer) const
{
    std::lock_guard<std::mutex> lock(data_mutex);
    if (attackerIndex >= 0 && attackerIndex < MAX_PLAYERS && targetPointer != 0) {
        auto it = player_friendly_damage_detail[attackerIndex].find(targetPointer);
        if (it != player_friendly_damage_detail[attackerIndex].end()) {
            return it->second;
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

void DamageSummary::resetFriendlyDamages()
{
    std::lock_guard<std::mutex> lock(data_mutex);
    for (auto& detail : player_friendly_damage_detail) {
        detail.clear();
    }
}

void DamageSummary::recordAttack(uintptr_t attacker_ptr, uintptr_t target_ptr, float damage) {
    attackLogger->recordAttack(attacker_ptr, target_ptr, damage);
}

std::vector<std::string> DamageSummary::getFormattedAttacks() {
    if (globalPlayerList) {
        return attackLogger->getFormattedAttacks(*globalPlayerList);
    }
    return {};
}

void DamageSummary::resetAll() {
    resetDamages();
    resetFriendlyDamages();
}

std::string DamageSummary::formatAllDamagesSummary() const
{
    std::lock_guard<std::mutex> lock(data_mutex);
    std::ostringstream oss;

    oss << "=== Damage Summary ===\n";
    if (!globalPlayerList) {
        oss << "ERROR: Global PlayerList not available\n";
        return oss.str();
    }

    float totalDamage = 0.0f;
    int validPlayerCount = 0;

    oss << "[Enemy Damage]\n";
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const Player* player = globalPlayerList->getPlayerByIndex(i);
        if (player && player->getPointer() != 0) {
            float damage = player_damages[i];
            totalDamage += damage;
            validPlayerCount++;
            oss << "  - Player[" << i << "]"
                << " Name: " << player->getName()
                << " Type: " << player->getTypeName()
                << " Pointer: 0x" << std::hex << player->getPointer() << std::dec
                << " Damage: " << damage << "\n";
        } else {
            oss << "  - Player[" << i << "] (No Player) Damage: 0\n";
        }
    }
    oss << "Total Players: " << validPlayerCount << "\n";
    oss << "Total Damage: " << totalDamage << "\n";

    oss << "\n[Friendly Damage]\n";
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const Player* player = globalPlayerList->getPlayerByIndex(i);
        if (!player || player->getPointer() == 0) {
            oss << "  - Player[" << i << "] (No Player)\n";
            continue;
        }

        char ptrStr[32];
        snprintf(ptrStr, sizeof(ptrStr), "0x%llX", static_cast<unsigned long long>(player->getPointer()));
        oss << "  - Player " << player->getName()
            << " [" << i << "] "
            << player->getTypeName()
            << " [" << ptrStr << "] ";

        float totalFriendly = 0.0f;
        for (const auto& kv : player_friendly_damage_detail[i]) {
            totalFriendly += kv.second;
        }
        oss << "Total Friendly Damage: " << totalFriendly;

        uintptr_t maxTargetPtr = 0;
        float maxDamage = 0.0f;
        int maxTargetIdx = -1;
        std::string maxTargetTypeName;
        for (const auto& kv : player_friendly_damage_detail[i]) {
            if (kv.second > maxDamage) {
                maxDamage = kv.second;
                maxTargetPtr = kv.first;
                const Player* targetPlayer = globalPlayerList->getPlayerByPointer(kv.first);
                if (targetPlayer) {
                    maxTargetIdx = targetPlayer->getIndex();
                    maxTargetTypeName = targetPlayer->getTypeName();
                } else {
                    maxTargetIdx = -1;
                    maxTargetTypeName = "Unknown";
                }
            }
        }
        if (maxTargetPtr != 0) {
            char targetPtrStr[32];
            snprintf(targetPtrStr, sizeof(targetPtrStr), "0x%llX", static_cast<unsigned long long>(maxTargetPtr));
            oss << ", Max Friendly Damage To Player [" << targetPtrStr << "]";
            oss << " [" << maxTargetIdx << "]";
            oss << " [" << maxTargetTypeName << "]";
            oss << ": " << maxDamage;
        }
        oss << "\n";
    }

    return oss.str();
}