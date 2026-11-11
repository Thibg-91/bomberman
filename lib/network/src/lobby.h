#ifndef LOBBY_H
#define LOBBY_H

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "protocol.h"

namespace Bomberman
{

// Seats of the waiting room, as the host sees them (pure logic, tested
// without sockets). Seat 0 is the host's and is always taken; clients get
// the first free seat when they join. Every player has a distinct crew
// model (models are indexes in AllCharacterModels): a wanted model already
// taken is replaced by the next free one in crew order.
class Lobby
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    // modelCount: size of the crew; seatCount: how many players the map can
    // take (its spawn points), clamped to [1, MaxPlayers].
    Lobby(int modelCount, int hostModel, int seatCount = MaxPlayers);

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] int getSeatCount() const noexcept;
    [[nodiscard]] int getModelCount() const noexcept;
    // Model on seat, nothing when it is free (or out of range).
    [[nodiscard]] std::optional<int> getModel(int seat) const noexcept;
    [[nodiscard]] bool isTaken(int seat) const noexcept;
    [[nodiscard]] int countPlayers() const noexcept;
    // Taken seats in increasing order: the players of the match, by PlayerId.
    [[nodiscard]] std::vector<int> getTakenSeats() const;

    // Seats a newcomer on the first free seat with wantedModel (or the next
    // free one); nothing when the room is full.
    std::optional<int> join(int wantedModel);
    // Frees seat (never the host's).
    void leave(int seat) noexcept;
    // Moves seat to the next (step > 0) or previous (step < 0) free model,
    // wrapping around; returns whether it changed.
    bool changeModel(int seat, int step) noexcept;

    // At least two players.
    [[nodiscard]] bool canStart() const noexcept;

    // What the clients are told.
    [[nodiscard]] std::array<SeatInfo, MaxPlayers> toSeatInfos() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    [[nodiscard]] bool isModelTaken(int model) const noexcept;
    // model itself when free, otherwise the next free one in crew order.
    [[nodiscard]] int freeModelFrom(int model, int step) const noexcept;

    // =========================
    // Members
    // =========================
    int m_modelCount;
    int m_seatCount;
    std::array<std::optional<int>, MaxPlayers> m_seats{};
};

} // namespace Bomberman

#endif // LOBBY_H
