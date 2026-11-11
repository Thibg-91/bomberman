#include "lobby.h"

#include <algorithm>

namespace Bomberman
{

namespace
{

int wrap(int value, int count) noexcept
{
    return count > 0 ? ((value % count) + count) % count : 0;
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
Lobby::Lobby(int modelCount, int hostModel, int seatCount)
    : m_modelCount(std::max(modelCount, 1)),
      m_seatCount(std::clamp(seatCount, 1, MaxPlayers))
{
    m_seats[0] = wrap(hostModel, m_modelCount);
}

// =========================
// Public Methods
// =========================
int Lobby::getSeatCount() const noexcept
{
    return m_seatCount;
}

int Lobby::getModelCount() const noexcept
{
    return m_modelCount;
}

std::optional<int> Lobby::getModel(int seat) const noexcept
{
    if (seat < 0 || seat >= m_seatCount)
        return std::nullopt;
    return m_seats[static_cast<std::size_t>(seat)];
}

bool Lobby::isTaken(int seat) const noexcept
{
    return getModel(seat).has_value();
}

int Lobby::countPlayers() const noexcept
{
    return static_cast<int>(std::count_if(m_seats.begin(), m_seats.end(),
                                          [](const std::optional<int>& seat) { return seat.has_value(); }));
}

std::vector<int> Lobby::getTakenSeats() const
{
    std::vector<int> seats;
    for (int seat = 0; seat < m_seatCount; ++seat)
        if (isTaken(seat))
            seats.push_back(seat);
    return seats;
}

std::optional<int> Lobby::join(int wantedModel)
{
    // Every player needs its own model: no seat without a free model.
    if (countPlayers() >= m_modelCount)
        return std::nullopt;
    for (int seat = 1; seat < m_seatCount; ++seat)
    {
        if (isTaken(seat))
            continue;
        m_seats[static_cast<std::size_t>(seat)] = freeModelFrom(wrap(wantedModel, m_modelCount), 1);
        return seat;
    }
    return std::nullopt;
}

void Lobby::leave(int seat) noexcept
{
    if (seat > 0 && seat < m_seatCount)
        m_seats[static_cast<std::size_t>(seat)].reset();
}

bool Lobby::changeModel(int seat, int step) noexcept
{
    const std::optional<int> current = getModel(seat);
    if (!current || step == 0)
        return false;
    const int direction = step > 0 ? 1 : -1;
    for (int k = 1; k < m_modelCount; ++k)
    {
        const int candidate = wrap(*current + k * direction, m_modelCount);
        if (!isModelTaken(candidate))
        {
            m_seats[static_cast<std::size_t>(seat)] = candidate;
            return true;
        }
    }
    return false; // every other model is taken
}

bool Lobby::canStart() const noexcept
{
    return countPlayers() >= 2;
}

std::array<SeatInfo, MaxPlayers> Lobby::toSeatInfos() const noexcept
{
    std::array<SeatInfo, MaxPlayers> infos{};
    for (int seat = 0; seat < m_seatCount; ++seat)
        if (const std::optional<int> model = getModel(seat))
            infos[static_cast<std::size_t>(seat)] = SeatInfo{true, static_cast<std::uint8_t>(*model)};
    return infos;
}

// =========================
// Private Methods
// =========================
bool Lobby::isModelTaken(int model) const noexcept
{
    return std::find(m_seats.begin(), m_seats.end(), std::optional<int>(model)) != m_seats.end();
}

int Lobby::freeModelFrom(int model, int step) const noexcept
{
    for (int k = 0; k < m_modelCount; ++k)
    {
        const int candidate = wrap(model + k * step, m_modelCount);
        if (!isModelTaken(candidate))
            return candidate;
    }
    return model; // nothing free (join() never gets here)
}

} // namespace Bomberman
