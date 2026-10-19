#include "Simulator.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace pib {
    namespace {
        // Objects the simulation runs through GD's normal collision code.
        bool isSimulated(GameObjectType type) {
            switch (type) {
                case GameObjectType::Solid:
                case GameObjectType::Hazard:
                case GameObjectType::AnimatedHazard:
                case GameObjectType::Slope:
                // Pads and orbs: activation is tracked per player, so the hidden
                // player can use them without consuming them for the real one.
                case GameObjectType::YellowJumpPad:
                case GameObjectType::PinkJumpPad:
                case GameObjectType::GravityPad:
                case GameObjectType::RedJumpPad:
                case GameObjectType::SpiderPad:
                case GameObjectType::YellowJumpRing:
                case GameObjectType::PinkJumpRing:
                case GameObjectType::GravityRing:
                case GameObjectType::GreenRing:
                case GameObjectType::DropRing:
                case GameObjectType::RedJumpRing:
                case GameObjectType::DashRing:
                case GameObjectType::GravityDashRing:
                case GameObjectType::SpiderOrb:
                // Gravity and size portals only change the player itself.
                case GameObjectType::InverseGravityPortal:
                case GameObjectType::NormalGravityPortal:
                case GameObjectType::GravityTogglePortal:
                case GameObjectType::RegularSizePortal:
                case GameObjectType::MiniSizePortal:
                    return true;
                default:
                    return false;
            }
        }

        // Objects whose effect reaches beyond the player (camera, dual mode,
        // teleports, mirroring, speed). Touching one ends the run as Unknown.
        bool endsSimulation(GameObjectType type) {
            switch (type) {
                case GameObjectType::ShipPortal:
                case GameObjectType::CubePortal:
                case GameObjectType::BallPortal:
                case GameObjectType::UfoPortal:
                case GameObjectType::WavePortal:
                case GameObjectType::RobotPortal:
                case GameObjectType::SpiderPortal:
                case GameObjectType::SwingPortal:
                case GameObjectType::InverseMirrorPortal:
                case GameObjectType::NormalMirrorPortal:
                case GameObjectType::DualPortal:
                case GameObjectType::SoloPortal:
                case GameObjectType::TeleportPortal:
                case GameObjectType::TeleportOrb:
                case GameObjectType::Modifier:
                case GameObjectType::Special:
                    return true;
                default:
                    return false;
            }
        }
    }

    Simulator& Simulator::get() {
        static Simulator instance;
        return instance;
    }

    void Simulator::filterObjects(PlayerObject* player, std::vector<GameObject*> const& objects, int count,
        std::vector<GameObject*>& out)
    {
        out.clear();
        auto limit = std::min<size_t>(static_cast<size_t>(std::max(count, 0)), objects.size());
        auto playerRect = player->getObjectRect();
        for (size_t i = 0; i < limit; ++i) {
            auto object = objects[i];
            if (!object) continue;
            auto type = object->m_objectType;
            if (isSimulated(type)) {
                out.push_back(object);
            }
            else if (endsSimulation(type) && playerRect.intersectsRect(object->getObjectRect())) {
                m_unknown = true;
            }
        }
    }

    void Simulator::cloneFrom(PlayerObject* src) {
        auto dst = m_player;
        // Gamemode, gravity, size, speed, position and y-velocity, applied the
        // way GD does it for dual spawns (no effects).
        dst->copyAttributes(src);
        // Everything else the physics reads.
#include "generated/PlayerStateCopy.inc"
        dst->m_touchingRings->removeAllObjects();
        if (src->m_touchingRings) {
            dst->m_touchingRings->addObjectsFromArray(src->m_touchingRings);
        }
        dst->setPosition(src->getPosition());
        dst->setRotation(src->getRotation());
    }

    // One physics tick for one player, in the order GJBaseGameLayer::update
    // runs it in 2.2091 (input has already been applied by the caller).
    void Simulator::step() {
        auto p = m_player;
        float dt = m_tickDelta;

        p->m_totalTime += dt / 60.0;
        p->resetTouchedRings(false);
        p->m_collisionLogTop->removeAllObjects();
        p->m_collisionLogBottom->removeAllObjects();
        p->m_collisionLogLeft->removeAllObjects();
        p->m_collisionLogRight->removeAllObjects();
        p->m_unk50C = p->m_lastCollisionBottom;
        p->m_unk510 = p->m_lastCollisionTop;
        p->m_lastCollisionBottom = -1;
        p->m_lastCollisionTop = -1;
        p->m_lastCollisionLeft = -1;
        p->m_lastCollisionRight = -1;

        p->updateInternalActions(dt);
        p->update(dt);
        m_layer->checkCollisions(p, dt, false);

        // GD keeps the previous rotation and position in these two fields
        // (named m_unkUnused3 / m_shipRotation in the bindings); collision
        // checks on the next tick read them.
        p->m_unkUnused3 = p->getRotation();
        p->updateRotation(dt);
        p->m_shipRotation = p->getPosition();
    }

    SimResult Simulator::run(GJBaseGameLayer* layer, PlayerObject* simPlayer, SimRequest const& request) {
        SimResult result;
        if (!layer || !simPlayer || !request.source) return result;

        m_layer = layer;
        m_player = simPlayer;
        m_dead = false;
        m_unknown = false;

        // Orbs the player is already touching; reaching any other one ends
        // the run, because what happens next depends on the next click.
        std::vector<CCObject*> startingOrbs;
        if (auto rings = request.source->m_touchingRings) {
            for (unsigned i = 0; i < rings->count(); ++i) startingOrbs.push_back(rings->objectAtIndex(i));
        }

        this->cloneFrom(request.source);

        result.outcome = SimOutcome::Survived;
        bool airborne = false;
        size_t nextInput = 0;
        for (int tick = 0; tick < request.horizon; ++tick) {
            while (nextInput < request.inputs.size() && request.inputs[nextInput].tick <= tick) {
                if (request.inputs[nextInput].down) simPlayer->pushButton(PlayerButton::Jump);
                else simPlayer->releaseButton(PlayerButton::Jump);
                ++nextInput;
            }

            auto prevY = simPlayer->getPositionY();
            auto prevVelocity = simPlayer->m_yVelocity;
            auto prevUpsideDown = simPlayer->m_isUpsideDown;
            auto prevDashing = simPlayer->m_isDashing;

            this->step();

            if (m_dead || simPlayer->m_isDead) {
                result.outcome = SimOutcome::Died;
                break;
            }
            if (m_unknown) {
                result.outcome = SimOutcome::Unknown;
                break;
            }
            if (tick < request.evaluatedTick) continue;

            // A jump, orb, flip, dash or spider teleport changes velocity or
            // position far more in one tick than gravity can.
            if (tick <= request.evaluatedTick + request.actWindow && (
                std::abs(simPlayer->m_yVelocity - prevVelocity) > 2.0 ||
                std::abs(simPlayer->getPositionY() - prevY) > 10.f ||
                simPlayer->m_isUpsideDown != prevUpsideDown ||
                (simPlayer->m_isDashing && !prevDashing)))
            {
                result.acted = true;
            }

            if (request.stopOnNewOrb && tick > request.evaluatedTick) {
                bool newOrb = false;
                auto rings = simPlayer->m_touchingRings;
                for (unsigned i = 0; rings && i < rings->count(); ++i) {
                    auto ring = rings->objectAtIndex(i);
                    if (std::find(startingOrbs.begin(), startingOrbs.end(), ring) == startingOrbs.end()) newOrb = true;
                }
                if (newOrb) break;
            }
            if (request.stopOnLanding) {
                if (!simPlayer->m_isOnGround) airborne = true;
                else if (airborne) break;
            }
        }
        if (result.outcome == SimOutcome::Survived && request.stopOnLanding && airborne && !simPlayer->m_isOnGround) {
            // Ran out of ticks mid-air without landing or reaching an orb:
            // nothing proves this timing is safe (e.g. falling into the void).
            result.outcome = SimOutcome::Unknown;
        }

        m_finalX = simPlayer->getPositionX();
        m_finalY = simPlayer->getPositionY();
        m_player = nullptr;
        m_layer = nullptr;
        return result;
    }
}
