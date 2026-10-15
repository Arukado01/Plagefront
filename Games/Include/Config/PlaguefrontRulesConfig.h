#pragma once

#include "Types/PlaguefrontRulesTypes.h"

namespace Plaguefront {
  struct Config {
    std::array<Stats, 7> classes;
    std::array<int, 5> itemCosts{15, 25, 15, 10, 20};
    std::array<int, 10> terrainCosts{1, 1, 1, 2, 1, 1, 2, 1, 9999, 9999};
    int initialCoins{120}, fortressIncome{10}, houseIncome{5}, neutralReward{20}, killReward{10}, outpostCost{70};
    int rotWait{3}, rotRadius{3}, rotDamage{2}, rotDuration{5}, globalThreshold{3}, connectionDistance{3}, globalDamage{1}, globalDuration{5};
    int weatherPeriod{3}, firePower{2}, icePower{1}, lightningPower{5}, purifyMana{3}, purifierRange{2};
    int houseCapture{40}, outpostCapture{50}, fortressCapture{60};
    int lightningMaxTargets{3}, lightningRadius{1}, fireRadius{1}, fireDuration{2}, fireDamage{2};
    int forestDefense{1}, forestArcherAttack{2}, forestRiderSurcharge{1}, uphillCost{1}, rangedHeightAttack{1}, swampDefensePenalty{1}, ruinsDefense{1};
    int rainMoveSurcharge{1}, stormRiderPenalty{1}, weatherRangePenalty{1}, maxMana{8}, manaRecovery{1}, healAmount{6};
    int potionHeal{8}, largePotionHeal{14}, manaPotionRestore{4}, advantageDamage{2}, damageReductionPercent{50};
    int preciseBonus{3}, markBonus{2}, focusBonus{2}, pikeBonus{2}, thrustBonus{3}, chargeBonus{3}, flankBonus{3}, furiousBonus{3};
    int frenzyAttack{2}, frenzyDefensePenalty{1}, chargeSteps{3}, retreatSteps{2};
    std::array<int, 17> abilityCooldowns{0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 3, 2, 0, 0, 0};
    Config();
    static Config Load(const std::string &filename);
  };
} // espacio de nombres Plaguefront
