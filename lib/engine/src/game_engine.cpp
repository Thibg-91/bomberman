#include "game_engine.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
GameEngine::GameEngine(Grid grid, GameRules rules)
    : m_rules(rules),
      m_grid(std::move(grid)),
      m_timestep(rules.tickRate)
{
    m_rules.tickRate = m_timestep.getTickRate(); // clamped, keep both in sync
}

// =========================
// Public Methods
// =========================
const GameRules& GameEngine::getRules() const noexcept
{
    return m_rules;
}

const Grid& GameEngine::getGrid() const noexcept
{
    return m_grid;
}

const BombManager& GameEngine::getBombManager() const noexcept
{
    return m_bombManager;
}

std::uint64_t GameEngine::getTick() const noexcept
{
    return m_tick;
}

std::optional<PlayerId> GameEngine::addPlayer()
{
    const std::vector<Position>& spawns = m_grid.getSpawnPoints();
    if (m_players.size() >= spawns.size())
        return std::nullopt;

    const auto id = static_cast<PlayerId>(m_players.size());
    // The walk lasts exactly moveTicks ticks, so the character is free to
    // move again right when its cooldown ends.
    m_players.push_back(Player{id, Character(spawns[m_players.size()], m_rules.secondsFor(m_rules.moveTicks)), 0,
                               std::nullopt});
    return id;
}

int GameEngine::getPlayerCount() const noexcept
{
    return static_cast<int>(m_players.size());
}

bool GameEngine::hasPlayer(PlayerId id) const noexcept
{
    return id >= 0 && id < getPlayerCount();
}

int GameEngine::countAlivePlayers() const noexcept
{
    return static_cast<int>(std::count_if(m_players.begin(), m_players.end(),
                                          [](const Player& p) { return p.character.isAlive(); }));
}

const Character& GameEngine::getCharacter(PlayerId id) const
{
    return player(id).character;
}

int GameEngine::getCooldownTicks(PlayerId id) const
{
    return player(id).cooldownTicks;
}

bool GameEngine::isReady(PlayerId id) const
{
    return isReady(player(id));
}

void GameEngine::submitCommand(PlayerId id, const CharacterCommand& command)
{
    if (hasPlayer(id))
        m_players[static_cast<std::size_t>(id)].pendingCommand = command;
}

int GameEngine::advance(float realSeconds)
{
    const int ticks = m_timestep.consume(realSeconds);
    if (ticks == 0)
        return 0; // keep the commands for the next frame's ticks

    for (int i = 0; i < ticks; ++i)
        runTick();
    clearCommands();
    return ticks;
}

void GameEngine::tick()
{
    runTick();
    clearCommands();
}

float GameEngine::getInterpolationAlpha() const noexcept
{
    return m_timestep.getAlpha();
}

std::vector<GameEvent> GameEngine::takeEvents()
{
    return std::exchange(m_events, {});
}

// =========================
// Private Methods
// =========================
const GameEngine::Player& GameEngine::player(PlayerId id) const
{
    if (!hasPlayer(id))
        throw std::out_of_range("GameEngine: unknown player id " + std::to_string(id));
    return m_players[static_cast<std::size_t>(id)];
}

bool GameEngine::isReady(const Player& player) noexcept
{
    return player.character.isAlive() && player.cooldownTicks == 0 && !player.character.isMoving();
}

void GameEngine::runTick()
{
    // 1. Commands. Repeating one on a later tick of the same frame is
    // harmless: a successful action makes the player busy, a failed one
    // has no effect.
    for (Player& p : m_players)
    {
        if (p.pendingCommand && isReady(p))
            execute(p, *p.pendingCommand);
    }

    // 2. Walking and cooldowns.
    const float dt = m_rules.tickDuration();
    for (Player& p : m_players)
    {
        p.character.update(dt);
        if (p.cooldownTicks > 0)
            --p.cooldownTicks;
    }

    // 3. Bombs: the explosion first, then its victims.
    const std::vector<Position> hitCells = m_bombManager.update(dt, m_grid);
    if (!hitCells.empty())
    {
        emit(GameEvent::Type::Explosion, -1, hitCells.front(), hitCells);
        for (Player& p : m_players)
        {
            if (p.character.isAlive()
                && std::find(hitCells.begin(), hitCells.end(), p.character.getPosition()) != hitCells.end())
            {
                p.character.kill();
                emit(GameEvent::Type::PlayerKilled, p.id, p.character.getPosition());
            }
        }
    }

    ++m_tick;
}

void GameEngine::clearCommands() noexcept
{
    for (Player& p : m_players)
        p.pendingCommand.reset();
}

void GameEngine::execute(Player& p, const CharacterCommand& command)
{
    Character& character = p.character;
    const Position facedCell = step(character.getPosition(), character.getFacing());

    switch (command.type)
    {
    case CharacterCommand::Type::Move:
    {
        const Direction facingBefore = character.getFacing();
        // Bombs block the way like blocks do.
        if (character.execute(command, m_grid, m_bombManager.getBombPositions()))
        {
            p.cooldownTicks = m_rules.moveTicks;
            emit(GameEvent::Type::PlayerMoved, p.id, character.getPosition());
        }
        else if (character.getFacing() != facingBefore)
        {
            p.cooldownTicks = m_rules.turnTicks;
            emit(GameEvent::Type::PlayerTurned, p.id, character.getPosition());
        }
        break;
    }
    case CharacterCommand::Type::PlaceBomb:
        // The bomb is dropped on the cell the character faces.
        if (m_bombManager.placeBomb(facedCell, m_grid, p.id, m_rules.maxActiveBombs))
        {
            p.cooldownTicks = m_rules.placeBombTicks;
            emit(GameEvent::Type::BombPlaced, p.id, facedCell);
        }
        break;
    case CharacterCommand::Type::PushBomb:
        // Does nothing without a bomb in front of the character.
        if (m_bombManager.pushBombAt(facedCell, character.getFacing(), m_grid, m_rules.pushDistance))
        {
            Position target = facedCell;
            for (int i = 0; i < m_rules.pushDistance; ++i)
                target = step(target, character.getFacing());
            p.cooldownTicks = m_rules.pushBombTicks;
            emit(GameEvent::Type::BombPushed, p.id, target);
        }
        break;
    }
}

void GameEngine::emit(GameEvent::Type type, PlayerId id, Position position, std::vector<Position> cells)
{
    m_events.push_back(GameEvent{type, m_tick, id, position, std::move(cells)});
}

} // namespace Bomberman
