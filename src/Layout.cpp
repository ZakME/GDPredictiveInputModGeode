// Compile-time checks that the Geode bindings match the GD 2.2091 binary this
// mod was built against. Every offset here was read from the decompiled
// game code (GJBaseGameLayer::update, handleButton, copyAttributes,
// pushButton, releaseButton, resetTouchedRings, PlayerObject::create).
// If one fails, the bindings moved and the simulator's field copy is unsafe.

#include <Geode/Geode.hpp>

#include <cstddef>

#define PIB_EXPECT_OFFSET(type, member, offset) \
    static_assert(offsetof(type, member) == (offset), #type "::" #member " is not at " #offset)

PIB_EXPECT_OFFSET(PlayerObject, m_collisionLogTop, 0x5b0);
PIB_EXPECT_OFFSET(PlayerObject, m_collisionLogRight, 0x5c8);
PIB_EXPECT_OFFSET(PlayerObject, m_lastCollisionBottom, 0x5d0);
PIB_EXPECT_OFFSET(PlayerObject, m_unk50C, 0x5e0);
PIB_EXPECT_OFFSET(PlayerObject, m_yVelocity, 0x9a8);
PIB_EXPECT_OFFSET(PlayerObject, m_isShip, 0x9c1);
PIB_EXPECT_OFFSET(PlayerObject, m_isUpsideDown, 0x9c7);
PIB_EXPECT_OFFSET(PlayerObject, m_isDead, 0x9c8);
PIB_EXPECT_OFFSET(PlayerObject, m_isOnGround, 0x9c9);
PIB_EXPECT_OFFSET(PlayerObject, m_isSwing, 0x9cc);
PIB_EXPECT_OFFSET(PlayerObject, m_maybeReverseSpeed, 0x9d8);
PIB_EXPECT_OFFSET(PlayerObject, m_isDashing, 0x9ec);
PIB_EXPECT_OFFSET(PlayerObject, m_vehicleSize, 0x9f8);
PIB_EXPECT_OFFSET(PlayerObject, m_playerSpeed, 0x9fc);
PIB_EXPECT_OFFSET(PlayerObject, m_shipRotation, 0xa00);
PIB_EXPECT_OFFSET(PlayerObject, m_unkUnused3, 0xa10);
PIB_EXPECT_OFFSET(PlayerObject, m_isLocked, 0xa32);
PIB_EXPECT_OFFSET(PlayerObject, m_controlsDisabled, 0xa33);
PIB_EXPECT_OFFSET(PlayerObject, m_lastGroundedPos, 0xa34);
PIB_EXPECT_OFFSET(PlayerObject, m_touchingRings, 0xa40);
PIB_EXPECT_OFFSET(PlayerObject, m_touchedRings, 0xa48);
PIB_EXPECT_OFFSET(PlayerObject, m_position, 0xa98);
PIB_EXPECT_OFFSET(PlayerObject, m_totalTime, 0xaa8);
PIB_EXPECT_OFFSET(PlayerObject, m_platformerXVelocity, 0xb00);
PIB_EXPECT_OFFSET(PlayerObject, m_holdingButtons, 0xbc0);
PIB_EXPECT_OFFSET(PlayerObject, m_actionManager, 0xc38);
static_assert(sizeof(PlayerObject) == 0xc50, "PlayerObject size changed");

PIB_EXPECT_OFFSET(GJBaseGameLayer, m_player1, 0xdb8);
PIB_EXPECT_OFFSET(GJBaseGameLayer, m_player2, 0xdc0);
PIB_EXPECT_OFFSET(GJBaseGameLayer, m_levelSettings, 0xdc8);
PIB_EXPECT_OFFSET(GJBaseGameLayer, m_isPlatformer, 0x317e);
PIB_EXPECT_OFFSET(GJBaseGameLayer, m_useReplay, 0x3280);
// Not checked: the bindings put m_queuedButtons at 0x33f0, but
// processQueuedButtons reads the queue at 0x3400. The fields from
// m_currentStep up to m_clickBetweenSteps are misplaced in the 2.2091
// bindings; this mod doesn't use them.
PIB_EXPECT_OFFSET(GJBaseGameLayer, m_clickBetweenSteps, 0x38f1);
static_assert(offsetof(GJBaseGameLayer, m_gameState) + offsetof(GJGameState, m_isDualMode) == 0x422,
    "GJGameState::m_isDualMode is not at GJBaseGameLayer+0x422");
PIB_EXPECT_OFFSET(LevelSettingsObject, m_twoPlayerMode, 0x154);
