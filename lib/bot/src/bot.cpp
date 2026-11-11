#include "bot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "blast.h"
#include "upgrade.h"

namespace Bomberman
{

namespace
{

constexpr std::array<Direction, 4> Directions = {Direction::Up, Direction::Down, Direction::Left, Direction::Right};

// Goal scores are value / (travel ticks + this): a goal twice as valuable
// is worth going twice as far, and the constant keeps a goal next door from
// dwarfing everything else.
constexpr long long TravelBias = 20;
// Wandering bots pick a cell at most this far (ticks).
constexpr int WanderReach = 90;

std::vector<Position> opponentPositions(const GameEngine& engine, PlayerId id)
{
    std::vector<Position> positions;
    for (PlayerId other = 0; other < engine.getPlayerCount(); ++other)
        if (other != id && engine.getCharacter(other).isAlive())
            positions.push_back(engine.getCharacter(other).getPosition());
    return positions;
}

bool contains(const std::vector<Position>& cells, Position cell)
{
    return std::find(cells.begin(), cells.end(), cell) != cells.end();
}

int manhattan(Position a, Position b)
{
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

// Picking it up would raise a stat (it is consumed for nothing at the cap).
bool isUseful(UpgradeType type, const PlayerStats& stats, const UpgradeRules& rules)
{
    switch (type)
    {
    case UpgradeType::ExtraBomb:
        return stats.maxBombs < rules.maxBombs;
    case UpgradeType::PushStrength:
        return stats.pushDistance < rules.maxPushDistance;
    case UpgradeType::BlastRadius:
        return stats.blastRadius < rules.maxBlastRadius;
    }
    return false;
}

const Upgrade* upgradeAt(const GameEngine& engine, Position cell)
{
    const std::vector<Upgrade>& upgrades = engine.getUpgrades();
    const auto it = std::find_if(upgrades.begin(), upgrades.end(),
                                 [cell](const Upgrade& upgrade) { return upgrade.position == cell; });
    return it == upgrades.end() ? nullptr : &*it;
}

const Bomb* bombAt(const BombManager& bombs, Position cell)
{
    const std::vector<Bomb>& all = bombs.getBombs();
    const auto it = std::find_if(all.begin(), all.end(), [cell](const Bomb& bomb) { return bomb.getPosition() == cell; });
    return it == all.end() ? nullptr : &*it;
}

int countIn(const std::vector<Position>& cells, const std::vector<Position>& positions)
{
    return static_cast<int>(
        std::count_if(positions.begin(), positions.end(), [&](Position p) { return contains(cells, p); }));
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
Bot::Bot(BotProfile profile, std::uint64_t seed) noexcept
    : m_profile(profile),
      m_random(seed)
{
}

Bot::Bot(BotDifficulty difficulty, std::uint64_t seed) noexcept
    : Bot(BotProfile::forDifficulty(difficulty), seed)
{
}

// =========================
// Public Methods
// =========================
std::optional<CharacterCommand> Bot::decide(const GameEngine& engine, PlayerId id)
{
    const std::uint64_t tick = engine.getTick();
    const Character& me = engine.getCharacter(id);

    // 1. Danger first, every tick, even while idle: the danger map is cheap.
    const DangerMap danger = perceive(engine, id, m_profile.reactionTicks, engine.getBombManager());
    if (danger.isThreatened(me.getPosition()))
    {
        m_idleUntil = 0;
        return escape(engine, id, danger);
    }

    // 2. Idle or hesitating: think again later.
    if (tick < m_idleUntil)
        return std::nullopt;
    if (m_random.percent(m_profile.hesitationPercent))
    {
        m_intent = Intent::Hesitate;
        m_target.reset();
        m_idleUntil = tick + static_cast<std::uint64_t>(std::max(m_profile.hesitationTicks, 0));
        return std::nullopt;
    }

    const PlayerStats& stats = engine.getStats(id);
    const bool canBomb =
        engine.getBombManager().countActiveBombs(id) < std::min(stats.maxBombs, m_profile.maxBombs);

    // 3. Act where it stands.
    if (const std::optional<Decision> decision = actWhereItStands(engine, id, danger, canBomb))
        return *decision;

    // 4. Go somewhere worth it, 5. or roam.
    const GameRules& rules = engine.getRules();
    const ReachMap reach = ReachMap::explore(danger, me.getPosition(), 0, rules.moveTicks);
    if (const std::optional<Decision> decision = goSomewhere(engine, id, danger, reach, canBomb))
        return *decision;
    return roam(engine, id, reach);
}

const BotProfile& Bot::getProfile() const noexcept
{
    return m_profile;
}

Bot::Intent Bot::getIntent() const noexcept
{
    return m_intent;
}

std::optional<Position> Bot::getTarget() const noexcept
{
    return m_target;
}

DangerMap Bot::perceive(const GameEngine& engine, PlayerId id, int reactionTicks, const BombManager& bombs)
{
    const int tickRate = engine.getRules().tickRate;
    return DangerMap::predict(bombs.getBombs(), engine.getGrid(), tickRate, [&](const Bomb& bomb) {
        if (bomb.getOwnerId() == id || reactionTicks <= 0)
            return true;
        const long elapsed =
            std::lround((bomb.getFuseDuration() - bomb.getTimeRemaining()) * static_cast<float>(tickRate));
        return elapsed >= reactionTicks;
    });
}

// =========================
// Private Methods
// =========================
int Bot::bombValue(const GameEngine& engine, PlayerId id, const DangerMap& danger, Position cell,
                   bool* hitsOpponent) const
{
    const Grid& grid = engine.getGrid();
    const Blast blast = Blast::compute(cell, engine.getStats(id).blastRadius, grid);
    const std::vector<Position> opponents = opponentPositions(engine, id);

    int value = 0;
    for (const Position hit : blast.cells())
    {
        // A block another explosion is already going to destroy is no gain.
        if (grid.getTile(hit).isDestructible() && !danger.isThreatened(hit))
            value += m_profile.blockValue;
        if (upgradeAt(engine, hit))
            value -= m_profile.upgradeValue;
        if (m_profile.attackValue > 0 && contains(opponents, hit))
        {
            value += m_profile.attackValue;
            if (hitsOpponent)
                *hitsOpponent = true;
        }
    }
    return value;
}

bool Bot::trapsAnOpponent(const GameEngine& engine, PlayerId id, Position cell) const
{
    const PlayerStats& stats = engine.getStats(id);
    BombManager bombs = engine.getBombManager();
    if (!bombs.placeBomb(cell, engine.getGrid(), id, stats.maxBombs, stats.blastRadius))
        return false;

    // Assume the opponent is perfect: it sees every bomb at once.
    const GameRules& rules = engine.getRules();
    const DangerMap danger = DangerMap::predict(bombs.getBombs(), engine.getGrid(), rules.tickRate);
    for (PlayerId other = 0; other < engine.getPlayerCount(); ++other)
    {
        const Character& opponent = engine.getCharacter(other);
        if (other == id || !opponent.isAlive() || !danger.isThreatened(opponent.getPosition())
            || manhattan(opponent.getPosition(), cell) > stats.blastRadius + 4)
            continue;
        const ReachMap reach = ReachMap::explore(danger, opponent.getPosition(), engine.getCooldownTicks(other),
                                                 rules.moveTicks, ReachMap::Scope::UntilSafe);
        if (!reach.canSurvive())
            return true;
    }
    return false;
}

bool Bot::canEscapeAfterPlacing(const GameEngine& engine, PlayerId id, Position cell) const
{
    const PlayerStats& stats = engine.getStats(id);
    const Character& me = engine.getCharacter(id);
    const int delay = engine.getRules().placeBombTicks;
    BombManager bombs = engine.getBombManager();
    if (!bombs.placeBomb(cell, engine.getGrid(), id, stats.maxBombs, stats.blastRadius)
        || !canEscapeWith(engine, id, bombs, delay))
        return false;

    // The obvious answer of a nearby opponent: a bomb of its own, right now
    // where it stands, that could cut the way out.
    for (PlayerId other = 0; other < engine.getPlayerCount() && m_profile.anticipationRange > 0; ++other)
    {
        const Character& opponent = engine.getCharacter(other);
        if (other == id || !opponent.isAlive()
            || manhattan(opponent.getPosition(), me.getPosition()) > m_profile.anticipationRange)
            continue;
        const PlayerStats& theirs = engine.getStats(other);
        BombManager answered = bombs;
        if (answered.placeBomb(opponent.getPosition(), engine.getGrid(), other, theirs.maxBombs, theirs.blastRadius)
            && !canEscapeWith(engine, id, answered, delay))
            return false;
    }
    return true;
}

bool Bot::canEscapeWith(const GameEngine& engine, PlayerId id, const BombManager& bombs, int delay) const
{
    const GameRules& rules = engine.getRules();
    const DangerMap danger = perceive(engine, id, m_profile.reactionTicks, bombs);
    return ReachMap::explore(danger, engine.getCharacter(id).getPosition(), delay, rules.moveTicks,
                             ReachMap::Scope::UntilSafe)
        .canSurvive();
}

Bot::Decision Bot::escape(const GameEngine& engine, PlayerId id, const DangerMap& danger)
{
    const GameRules& rules = engine.getRules();
    const Character& me = engine.getCharacter(id);
    const Position position = me.getPosition();

    // Standing next to a bomb always means being in its blast: punching it
    // onto an opponent is part of getting away.
    if (m_profile.pushesBombs)
        if (const std::optional<Decision> decision = pushOntoOpponent(engine, id))
            return *decision;

    const ReachMap reach = ReachMap::explore(danger, position, 0, rules.moveTicks, ReachMap::Scope::UntilSafe);
    if (const std::optional<Position> safe = reach.nearestSafeCell())
        return headFor(reach, *safe, Intent::Escape);

    // No way out on foot: punch a neighbouring bomb away if that opens one
    // (turning towards it first when needed: moving towards a bomb only
    // turns).
    const Grid& grid = engine.getGrid();
    for (const Direction dir : Directions)
    {
        const Position bombCell = step(position, dir);
        BombManager bombs = engine.getBombManager();
        if (!bombs.pushBombAt(bombCell, dir, grid, engine.getStats(id).pushDistance, opponentPositions(engine, id)))
            continue;
        const bool facing = dir == me.getFacing();
        const int delay = (facing ? 0 : rules.turnTicks) + rules.pushBombTicks;
        if (canEscapeWith(engine, id, bombs, delay))
        {
            m_intent = Intent::PushBomb;
            m_target = bombCell;
            return facing ? CharacterCommand::pushBomb() : CharacterCommand::move(dir);
        }
    }

    m_intent = Intent::Escape; // doomed: nothing helps
    m_target.reset();
    return std::nullopt;
}

std::optional<Bot::Decision> Bot::actWhereItStands(const GameEngine& engine, PlayerId id, const DangerMap& danger,
                                                   bool canBomb)
{
    // The bomb is dropped where the bot stands.
    const Position here = engine.getCharacter(id).getPosition();
    const std::uint64_t tick = engine.getTick();

    if (canBomb && !engine.getBombManager().hasBombAt(here) && !isRejected(here, tick))
    {
        int value = bombValue(engine, id, danger, here);
        if (m_profile.trapValue > 0 && trapsAnOpponent(engine, id, here))
            value += m_profile.trapValue;
        if (value > 0)
        {
            if (canEscapeAfterPlacing(engine, id, here))
            {
                m_intent = Intent::PlaceBomb;
                m_target = here;
                return Decision{CharacterCommand::placeBomb()};
            }
            reject(here, tick); // worth it but deadly: look elsewhere for a while
        }
    }
    return std::nullopt;
}

std::optional<Bot::Decision> Bot::pushOntoOpponent(const GameEngine& engine, PlayerId id)
{
    const Character& me = engine.getCharacter(id);
    const Position faced = step(me.getPosition(), me.getFacing());
    const Bomb* bomb = bombAt(engine.getBombManager(), faced);
    if (!bomb)
        return std::nullopt;

    const Grid& grid = engine.getGrid();
    const std::vector<Position> opponents = opponentPositions(engine, id);
    BombManager bombs = engine.getBombManager();
    const std::optional<Position> landing =
        bombs.pushBombAt(faced, me.getFacing(), grid, engine.getStats(id).pushDistance, opponents);
    if (!landing)
        return std::nullopt;

    // Only worth it if more opponents end up in the blast than before.
    const int before = countIn(Blast::compute(faced, bomb->getBlastRadius(), grid).cells(), opponents);
    const int after = countIn(Blast::compute(*landing, bomb->getBlastRadius(), grid).cells(), opponents);
    if (after <= before || !canEscapeWith(engine, id, bombs, engine.getRules().pushBombTicks))
        return std::nullopt;

    m_intent = Intent::PushBomb;
    m_target = *landing;
    return Decision{CharacterCommand::pushBomb()};
}

std::optional<Bot::Decision> Bot::goSomewhere(const GameEngine& engine, PlayerId id, const DangerMap& danger,
                                              const ReachMap& reach, bool canBomb)
{
    struct Goal
    {
        Position cell; // where to go (and drop the bomb, for BreakBlocks and Attack)
        int value;
        Intent intent;
        int arrival;
    };

    const Grid& grid = engine.getGrid();
    const BombManager& bombs = engine.getBombManager();
    const PlayerStats& stats = engine.getStats(id);
    const Character& me = engine.getCharacter(id);
    const std::uint64_t tick = engine.getTick();

    // Bomb values per target cell, computed once.
    constexpr int Unknown = std::numeric_limits<int>::min();
    std::vector<int> values(static_cast<std::size_t>(grid.getWidth() * grid.getHeight()), Unknown);
    std::vector<char> attacks(values.size(), 0);
    auto valueAt = [&](Position cell, bool& attack) {
        const std::size_t i = static_cast<std::size_t>(cell.y * grid.getWidth() + cell.x);
        if (values[i] == Unknown)
        {
            bool hits = false;
            values[i] = bombValue(engine, id, danger, cell, &hits);
            attacks[i] = hits ? 1 : 0;
        }
        attack = attacks[i] != 0;
        return values[i];
    };

    std::vector<Goal> goals;
    for (int y = 0; y < grid.getHeight(); ++y)
        for (int x = 0; x < grid.getWidth(); ++x)
        {
            const Position cell{x, y};
            const int arrival = reach.safeArrival(cell);
            if (arrival == ReachMap::Unreachable)
                continue;

            if (const Upgrade* upgrade = upgradeAt(engine, cell);
                upgrade && m_profile.upgradeValue > 0 && isUseful(upgrade->type, stats, engine.getRules().upgrades))
                goals.push_back(Goal{cell, m_profile.upgradeValue, Intent::Collect, arrival});

            // A cell to drop a bomb on. Where it stands: already handled by
            // actWhereItStands() (not worth it, or deadly).
            if (!canBomb || cell == me.getPosition() || bombs.hasBombAt(cell) || isRejected(cell, tick))
                continue;
            bool attack = false;
            const int value = valueAt(cell, attack);
            if (value > 0)
                goals.push_back(Goal{cell, value, attack ? Intent::Attack : Intent::BreakBlocks, arrival});
        }
    if (goals.empty())
        return std::nullopt;

    auto score = [](const Goal& goal) {
        return static_cast<long long>(goal.value) * 1000 / (goal.arrival + TravelBias);
    };

    // Trappers look whether the best attack spots would leave an opponent
    // no way out (as things stand now: it is re-checked on the spot).
    if (m_profile.trapValue > 0)
    {
        std::vector<std::size_t> attacks;
        for (std::size_t i = 0; i < goals.size(); ++i)
            if (goals[i].intent == Intent::Attack)
                attacks.push_back(i);
        std::stable_sort(attacks.begin(), attacks.end(),
                         [&](std::size_t a, std::size_t b) { return score(goals[a]) > score(goals[b]); });
        if (attacks.size() > static_cast<std::size_t>(TrapChecksPerDecision))
            attacks.resize(static_cast<std::size_t>(TrapChecksPerDecision));
        for (const std::size_t i : attacks)
            if (trapsAnOpponent(engine, id, goals[i].cell))
                goals[i].value += m_profile.trapValue;
    }

    std::size_t chosen = 0;
    if (m_random.percent(m_profile.mistakePercent))
        chosen = static_cast<std::size_t>(m_random.below(static_cast<int>(goals.size())));
    else
    {
        long long bestScore = std::numeric_limits<long long>::min();
        for (std::size_t i = 0; i < goals.size(); ++i)
            if (score(goals[i]) > bestScore)
            {
                bestScore = score(goals[i]);
                chosen = i;
            }
    }
    const Goal& goal = goals[chosen];
    return headFor(reach, goal.cell, goal.intent);
}

Bot::Decision Bot::roam(const GameEngine& engine, PlayerId id, const ReachMap& reach)
{
    const Grid& grid = engine.getGrid();
    const Position position = engine.getCharacter(id).getPosition();
    const std::uint64_t tick = engine.getTick();

    if (m_profile.huntsOpponents)
    {
        const std::vector<Position> opponents = opponentPositions(engine, id);
        if (opponents.empty())
            return idle(tick);

        // As close to an opponent as it can safely get, the soonest.
        std::optional<Position> best;
        int bestDistance = std::numeric_limits<int>::max();
        int bestArrival = std::numeric_limits<int>::max();
        for (int y = 0; y < grid.getHeight(); ++y)
            for (int x = 0; x < grid.getWidth(); ++x)
            {
                const int arrival = reach.safeArrival({x, y});
                if (arrival == ReachMap::Unreachable)
                    continue;
                int distance = std::numeric_limits<int>::max();
                for (const Position opponent : opponents)
                    distance = std::min(distance, manhattan({x, y}, opponent));
                if (distance < bestDistance || (distance == bestDistance && arrival < bestArrival))
                {
                    best = Position{x, y};
                    bestDistance = distance;
                    bestArrival = arrival;
                }
            }
        if (!best || *best == position)
            return idle(tick);
        return headFor(reach, *best, Intent::Hunt);
    }

    // Wander: keep a random target until it is reached or cut off.
    if (m_wanderTarget && (*m_wanderTarget == position || !reach.isSafelyReachable(*m_wanderTarget)))
        m_wanderTarget.reset();
    if (!m_wanderTarget)
    {
        std::vector<Position> candidates;
        for (int y = 0; y < grid.getHeight(); ++y)
            for (int x = 0; x < grid.getWidth(); ++x)
            {
                const int arrival = reach.safeArrival({x, y});
                if (arrival != ReachMap::Unreachable && arrival <= WanderReach && Position{x, y} != position)
                    candidates.push_back({x, y});
            }
        if (candidates.empty())
            return idle(tick);
        m_wanderTarget = candidates[static_cast<std::size_t>(m_random.below(static_cast<int>(candidates.size())))];
    }
    return headFor(reach, *m_wanderTarget, Intent::Wander);
}

bool Bot::isRejected(Position cell, std::uint64_t tick) const
{
    return std::any_of(m_rejected.begin(), m_rejected.end(),
                       [&](const Rejected& rejected) { return rejected.cell == cell && rejected.until > tick; });
}

void Bot::reject(Position cell, std::uint64_t tick)
{
    m_rejected.erase(std::remove_if(m_rejected.begin(), m_rejected.end(),
                                    [&](const Rejected& rejected) { return rejected.until <= tick || rejected.cell == cell; }),
                     m_rejected.end());
    m_rejected.push_back(Rejected{cell, tick + static_cast<std::uint64_t>(RejectTicks)});
}

Bot::Decision Bot::headFor(const ReachMap& reach, Position cell, Intent intent)
{
    m_intent = intent;
    m_target = cell;
    const std::optional<Direction> dir = reach.firstStep(cell);
    if (!dir)
        return std::nullopt; // waiting is the fastest way (an explosion has to pass first)
    return CharacterCommand::move(*dir);
}

Bot::Decision Bot::idle(std::uint64_t tick)
{
    m_intent = Intent::Idle;
    m_target.reset();
    m_idleUntil = tick + IdleRethinkTicks;
    return std::nullopt;
}

} // namespace Bomberman
