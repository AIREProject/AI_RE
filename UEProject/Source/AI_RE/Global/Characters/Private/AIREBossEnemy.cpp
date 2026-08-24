#include "AIREBossEnemy.h"

AAIREBossEnemy::AAIREBossEnemy()
{
	BossDisplayName = NSLOCTEXT(
		"AIREBoss",
		"ArmoredRuinCrunch",
		"파멸의 기갑, 크런치");
}

FText AAIREBossEnemy::GetBossDisplayName() const
{
	return BossDisplayName;
}
