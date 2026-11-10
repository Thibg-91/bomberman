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
      m_random(rules.seed),
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

const std::vector<Upgrade>& GameEngine::getUpgrades() const noexcept
{
    return m_upgradeManager.getUpgrades();
}

bool GameEngine::addUpgrade(Upgrade upgrade)
{
    return m_upgradeManager.add(upgrade, m_grid);
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
    m_players.push_back(Player{id, Character(spawns[m_players.size()], m_rules.secondsFor(m_rules.moveTicks)),
                               m_rules.startingStats, 0, std::nullopt, nullptr});
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

const PlayerStats& GameEngine::getStats(PlayerId id) const
{
    return player(id).stats;
}

int GameEngine::getCooldownTicks(PlayerId id) const
{
    return player(id).cooldownTicks;
}

bool GameEngine::isReady(PlayerId id) const
{
    return isReady(player(id));
}

void GameEngine::setController(PlayerId id, std::unique_ptr<PlayerController> controller)
{
    if (hasPlayer(id))
        m_players[static_cast<std::size_t>(id)].controller = std::move(controller);
}

bool GameEngine::hasController(PlayerId id) const
{
    return player(id).controller != nullptr;
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
    // has no effect. A controller is only asked when its player is ready,
    // and sees the actions of the players with a lower id.
    for (Player& p : m_players)
    {
        if (!isReady(p))
            continue;
        if (p.controller)
        {
            if (const std::optional<CharacterCommand> command = p.controller->decide(*this, p.id))
                execute(p, *command);
        }
        else if (p.pendingCommand)
            execute(p, *p.pendingCommand);
    }

    // 2. Upgrades under the players' feet.
    collectUpgrades();

    // 3. Walking and cooldowns.
    const float dt = m_rules.tickDuration();
    for (Player& p : m_players)
    {
        p.character.update(dt);
        if (p.cooldownTicks > 0)
            --p.cooldownTicks;
    }

    // 4. Bombs.
    updateBombs(dt);

    ++m_tick;
}

void GameEngine::collectUpgrades()
{
    for (Player& p : m_players)
    {
        if (!p.character.isAlive())
            continue;
        const Position position = p.character.getPosition();
        if (const std::optional<UpgradeType> type = m_upgradeManager.takeAt(position))
        {
            p.stats.apply(*type, m_rules.upgrades); // consumed even at the cap
            emit(GameEvent::Type::UpgradeCollected, p.id, Upgrade{position, *type});
        }
    }
}

void GameEngine::updateBombs(float deltaSeconds)
{
    std::vector<Position> destroyedBlocks;
    std::vector<Blast> blasts;
    const std::vector<Position> hitCells = m_bombManager.update(deltaSeconds, m_grid, &destroyedBlocks, &blasts);
    if (hitCells.empty())
        return;

    // The explosions first, then what they did.
    for (const Blast& blast : blasts)
        emitExplosion(blast);

    // Upgrades already lying in the blast are destroyed; the ones revealed
    // below appear afterwards, so this explosion spares them.
    for (const Upgrade& upgrade : m_upgradeManager.destroyAt(hitCells))
        emit(GameEvent::Type::UpgradeDestroyed, -1, upgrade);
    for (const Upgrade& upgrade :
         m_upgradeManager.spawnInDestroyedBlocks(destroyedBlocks, m_grid, m_rules.upgrades, m_random))
        emit(GameEvent::Type::UpgradeSpawned, -1, upgrade);

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

std::vector<Position> GameEngine::otherPlayerPositions(const Player& player) const
{
    std::vector<Position> positions;
    for (const Player& other : m_players)
    {
        if (other.id != player.id && other.character.isAlive())
            positions.push_back(other.character.getPosition());
    }
    return positions;
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
        // The bomb is dropped on the cell the character faces, with the
        // character's current blast radius.
        if (m_bombManager.placeBomb(facedCell, m_grid, p.id, p.stats.maxBombs, p.stats.blastRadius))
        {
            p.cooldownTicks = m_rules.placeBombTicks;
            emit(GameEvent::Type::BombPlaced, p.id, facedCell);
        }
        break;
    case CharacterCommand::Type::PushBomb:
        // Does nothing without a bomb in front of the character. The bomb
        // slides up to pushDistance cells, stopping before obstacles.
        if (const std::optional<Position> target = m_bombManager.pushBombAt(
                facedCell, character.getFacing(), m_grid, p.stats.pushDistance, otherPlayerPositions(p)))
        {
            p.cooldownTicks = m_rules.pushBombTicks;
            emit(GameEvent::Type::BombPushed, p.id, *target);
        }
        break;
    }
}

void GameEngine::emit(GameEvent::Type type, PlayerId id, Position position)
{
    GameEvent event;
    event.type = type;
    event.tick = m_tick;
    event.playerId = id;
    event.position = position;
    m_events.push_back(std::move(event));
}

void GameEngine::emit(GameEvent::Type type, PlayerId id, const Upgrade& upgrade)
{
    emit(type, id, upgrade.position);
    m_events.back().upgrade = upgrade.type;
}

void GameEngine::emitExplosion(const Blast& blast)
{
    emit(GameEvent::Type::Explosion, -1, blast.center);
    m_events.back().cells = blast.cells();
    m_events.back().blast = blast;
}

} // namespace Bomberman
