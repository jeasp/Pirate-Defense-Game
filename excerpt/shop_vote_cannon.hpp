// 상점 구매 투표, 대포 발사, 포탄 충돌 처리 (Game.hpp 중 일부)

// 구매 투표 상태
        // 구매 투표
        bool purchaseVoteActive = false;

        int purchaseVoteItem = -1;
        int purchaseVoteProposer = -1;
        int purchaseVoteId = 0;

        float purchaseVoteRemaining = 0.f;

        // -1 미투표 / 0 반대 / 1 찬성
        std::array<int, MaxPlayers> purchaseBallots{
            -1, -1, -1, -1, -1
        };


// 투표 가중치(호스트 1.5표), 투표 결과 처리, 상점 입력 중 투표·구매 제안

        float purchaseVoteWeight(int choice) const
        {
            float total = 0.f;

            for (int id = 0; id < MaxPlayers; ++id)
            {
                if (
                    players[id].active &&
                    purchaseBallots[id] == choice
                    )
                {
                    // Host(player 0) vote = 1.5, every client vote = 1.0.
                    // IMPORTANT: this weight is applied only AFTER that player
                    // explicitly clicks YES or NO. The proposer is never auto-voted.
                    total += (id == 0) ? 1.5f : 1.0f;
                }
            }

            return total;
        }

        void finishPurchaseVote()
        {
            if (!purchaseVoteActive)
                return;

            purchaseVoteActive = false;
            purchaseVoteRemaining = 0.f;

            // 기본값: 부결
            int result = 5;

            // 찬성이 반대보다 많아야 통과
            // 동률은 부결
            if (
                purchaseVoteWeight(1) >
                purchaseVoteWeight(0)
                )
            {
                ShopManager shop(
                    gold,
                    ammo,
                    upgrades
                );

                const auto item =
                    ShopItems[purchaseVoteItem];

                // 배 HP 강화 전 보너스 저장
                int oldHpBonus =
                    upgrades.getShipHpBonus();

                if (shop.isMaxLevel(item))
                {
                    result = 3;
                }
                else if (shop.buyItem(item))
                {
                    result = 1;

                    // 배 HP 강화 구매 시
                    // 실제 Game maxHp에도 적용
                    int newHpBonus =
                        upgrades.getShipHpBonus();

                    int extraHp =
                        newHpBonus - oldHpBonus;

                    if (extraHp > 0)
                    {
                        maxHp +=
                            static_cast<float>(extraHp);

                        hp = std::min(
                            maxHp,
                            hp + static_cast<float>(extraHp)
                        );
                    }
                }
                else
                {
                    // 골드 부족 등
                    result = 2;
                }
            }

            // 모든 플레이어에게 결과 표시
            for (int id = 0; id < MaxPlayers; ++id)
            {
                if (!players[id].active)
                    continue;

                shopResult[id] = result;
                shopResultTime[id] = 3.f;
            }

            purchaseVoteItem = -1;
            purchaseVoteProposer = -1;
        }


        // 상점 업데이트
        void updateShop(
            float dt,
            const std::array<Input, MaxPlayers>& inputs)
        {
            // 구매 투표 중에는 상점 30초 타이머 정지
            if (!purchaseVoteActive)
            {
                shopRemaining =
                    std::max(
                        0.f,
                        shopRemaining - dt
                    );
            }

            // 구매 투표는 별도 10초
            if (purchaseVoteActive)
            {
                purchaseVoteRemaining =
                    std::max(
                        0.f,
                        purchaseVoteRemaining - dt
                    );
            }

            bool proposedThisStep = false;

            // 플레이어별 상점 입력
            if (shopRemaining > 0.f)
            {
                for (int id = 0; id < MaxPlayers; ++id)
                {
                    shopResultTime[id] =
                        std::max(
                            0.f,
                            shopResultTime[id] - dt
                        );

                    if (!players[id].active)
                        continue;

                    const auto& in =
                        inputs[id];

                    // 이전 상점 입력 무시
                    if (in.shopVisit != shopVisit)
                        continue;


                    // 구매 찬반 투표
                    if (
                        purchaseVoteActive &&
                        in.purchaseVoteId == purchaseVoteId &&
                        (
                            in.purchaseVote == 0 ||
                            in.purchaseVote == 1
                            ) &&
                        purchaseBallots[id] < 0
                        )
                    {
                        purchaseBallots[id] =
                            in.purchaseVote;
                    }


                    // 준비 완료한 플레이어는 새 구매 제안 불가
                    if (shopReady[id])
                        continue;

                    // 구매 제안
                    if (
                        in.buy >= 0 &&
                        in.buy < ShopItemCount &&
                        !purchaseVoteActive &&
                        !proposedThisStep &&
                        in.purchaseVoteId == purchaseVoteId
                        )
                    {
                        ShopManager shop(
                            gold,
                            ammo,
                            upgrades
                        );

                        const auto item =
                            ShopItems[in.buy];

                        // 최대 강화
                        if (shop.isMaxLevel(item))
                        {
                            shopResult[id] = 3;
                            shopResultTime[id] = 2.f;
                        }

                        // 골드 부족
                        else if (
                            !gold.canAfford(
                                shop.getPrice(item)
                            )
                            )
                        {
                            shopResult[id] = 2;
                            shopResultTime[id] = 2.f;
                        }

                        else
                        {
                            // 구매 투표 시작
                            purchaseVoteActive = true;
                            proposedThisStep = true;

                            purchaseVoteItem =
                                in.buy;

                            purchaseVoteProposer =
                                id;

                            ++purchaseVoteId;

                            purchaseVoteRemaining =
                                10.f;

                            purchaseBallots.fill(-1);

                            // The proposer must also click YES or NO. No automatic vote.
                        }
                    }


                    // ...
                }
            }


// 전원 투표 또는 10초 만료 시 투표 판정
            // 구매 투표 종료
            if (purchaseVoteActive)
            {
                bool allVoted = true;

                for (int id = 0; id < MaxPlayers; ++id)
                {
                    if (
                        players[id].active &&
                        purchaseBallots[id] < 0
                        )
                    {
                        allVoted = false;
                    }
                }

                if (
                    allVoted ||
                    purchaseVoteRemaining <= 0.f
                    )
                {
                    finishPurchaseVote();
                }
            }
            // ...
        }

// 대포 발사 (포탄별 피해량, 확산탄 5발 각도 분할)
        void fireCannon(Cannon& cannon)
        {
            const int type = cannon.ammo;

            // 포탄 종류별 기본 속도 / 데미지
            const auto stats = shellStats(type);

            // 일반탄만 공격력 강화 적용
            float damage = stats.damage;

            if (type == Normal)
            {
                damage *= upgrades.getNormalDamageMultiplier();
            }

            // 확산탄만 5발 발사
            const int shotCount =
                type == Spread ? 5 : 1;

            for (int i = 0; i < shotCount; ++i)
            {
                // 확산탄:
                // -20도 / -10도 / 0도 / +10도 / +20도
                float angle =
                    cannon.angle +
                    (
                        type == Spread
                        ? (i - 2) * Pi / 18.f
                        : 0.f
                        );

                V velocity{
                    std::cos(angle) * cannon.side * stats.speed,
                    std::sin(angle) * stats.speed
                };

                shots.push_back({
                    cannon.p + unit(velocity) * 16.f,
                    velocity,
                    type,
                    false,
                    damage,
                    4.f,
                    {}
                    });
            }

            // 장전된 포탄 소모
            cannon.ammo = -1;

            ++shotsFired;

            effect(
                cannon.p,
                0,
                0.15f
            );
        }

// 플레이어 포탄 충돌 (관통탄 중복 피해 방지, 폭발탄 범위, 화염탄 지속)
                else
                {
                    for (auto& e : enemies)
                        if (e.hp > 0 && dist(s.p, e.p) < (e.type == 2 ? 21 : 17) &&
                            std::find(s.hit.begin(), s.hit.end(), e.id) == s.hit.end())
                        {
                            s.hit.push_back(e.id);
                            damageEnemy(e, s.damage);
                            effect(e.p, 1);
                            if (s.type == Flame)
                                e.burn = 4;

                            if (s.type == Blast)
                            {
                                effect(s.p, 3);
                                for (auto& other : enemies)
                                    if (other.id != e.id && dist(other.p, e.p) < 65)
                                        damageEnemy(other, 45.f);
                            }
                            if (s.type != Pierce)
                            {
                                s.life = 0;
                                break;
                            }
                        }
                    if (s.life > 0 && boss.active && segmentDistance(boss.p, previous, s.p) < (boss.type == BossType::GhostShip ? 52.f : 43.f) &&
                        std::find(s.hit.begin(), s.hit.end(), -1) == s.hit.end())
                    {
                        boss.hp -= s.damage;
                        s.hit.push_back(-1);
                        effect(s.p, 1);
                        if (s.type == Flame)
                            boss.burn = 4;

                        s.life = 0;
                    }
                }
