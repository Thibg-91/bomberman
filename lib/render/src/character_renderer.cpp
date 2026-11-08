#include "character_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;
constexpr float HopAmplitude = 0.12f;      // fraction of a tile
constexpr float BodyRadiusFraction = 0.33f; // fraction of a tile

constexpr float LegStepAmplitude = 0.09f; // fraction of a tile, vertical lift
constexpr float MaxRaiseAngleDeg = 150.f; // arms, bomb-drop pose
constexpr float TailFlapFrequency = 4.f;  // wiggles per cell moved
constexpr float TailFlapAmplitudeDeg = 14.f;
constexpr float FaceNudge = 0.08f; // fraction of a tile, towards facing

sf::Vector2f cellCenter(Position position, float tileSize)
{
    return {(static_cast<float>(position.x) + 0.5f) * tileSize, (static_cast<float>(position.y) + 0.5f) * tileSize};
}

sf::Vector2f directionVector(Direction direction)
{
    const Position offset = step({0, 0}, direction);
    return {static_cast<float>(offset.x), static_cast<float>(offset.y)};
}

float angleDegrees(sf::Vector2f from, sf::Vector2f to)
{
    return std::atan2(to.y - from.y, to.x - from.x) * 180.f / Pi;
}

float lengthOf(sf::Vector2f from, sf::Vector2f to)
{
    return std::hypot(to.x - from.x, to.y - from.y);
}

} // namespace

CharacterRenderer::Palette::Palette() noexcept : skin(219, 163, 107), bandana(195, 59, 59)
{
}

// =========================
// Static Methods
// =========================
CharacterRenderer::Palette CharacterRenderer::paletteForPlayer(int playerIndex) noexcept
{
    static const std::array<Palette, PlayerCount> palettes = {
        Palette{sf::Color(219, 163, 107), sf::Color(195, 59, 59)},  // red
        Palette{sf::Color(219, 163, 107), sf::Color(47, 111, 174)}, // blue
        Palette{sf::Color(219, 163, 107), sf::Color(58, 138, 78)},  // green
        Palette{sf::Color(219, 163, 107), sf::Color(201, 146, 47)}, // gold
    };
    return palettes[static_cast<std::size_t>(std::clamp(playerIndex, 0, PlayerCount - 1))];
}

// =========================
// Constructors / Destructor
// =========================
CharacterRenderer::CharacterRenderer(float tileSize, Palette palette)
    : m_tileSize(tileSize),
      m_center(0.f, 0.f),
      m_bombAnimElapsed(BombAnimDuration),
      m_body(tileSize * BodyRadiusFraction),
      m_armLeft(1.f),
      m_armRight(1.f),
      m_legLeft(),
      m_legRight(),
      m_knot(1.f),
      m_tailLeft(3),
      m_tailRight(3),
      m_eye(1.f),
      m_eyeShine(1.f),
      m_blushLeft(1.f),
      m_blushRight(1.f),
      m_smile(sf::PrimitiveType::LineStrip, 5),
      m_eyepatch(1.f),
      m_earOuter(1.f),
      m_earInner(1.f),
      m_backPanel(1.f),
      m_spark(tileSize * 0.1f),
      m_facing(Direction::Down)
{
    // "27 units" is the body radius in the original mock-up; every other
    // shape was authored relative to it, so every length below is expressed
    // the same way and converted with this single scale factor.
    const float unit = (tileSize * BodyRadiusFraction) / 27.f;
    const auto relative = [unit](float x, float y) { return sf::Vector2f(x * unit, y * unit); };

    const sf::Color dark(28, 20, 13);
    const sf::Color strapColor = dark;

    // Outline thickness scales with the tile size instead of being a fixed
    // pixel count, so it stays proportionally the same (and visible) at any
    // resolution, including a fullscreen window with much bigger tiles.
    const float outlineMain = std::max(1.f, tileSize * 0.045f);
    const float outlineThin = std::max(1.f, tileSize * 0.028f);

    m_body.setFillColor(palette.skin);
    m_body.setOutlineColor(dark);
    m_body.setOutlineThickness(outlineMain);
    m_body.setOrigin({m_body.getRadius(), m_body.getRadius()});

    // Arms: ellipses (a scaled circle) that rotate around their own center,
    // only during the bomb-drop pose.
    for (sf::CircleShape* arm : {&m_armLeft, &m_armRight})
    {
        arm->setRadius(7.f * unit);
        arm->setScale({1.f, 11.f / 7.f});
        arm->setOrigin({arm->getRadius(), arm->getRadius()});
        arm->setFillColor(palette.skin);
        arm->setOutlineColor(dark);
        arm->setOutlineThickness(outlineMain);
    }

    // Legs: simple boots, lifted one at a time while walking.
    for (sf::RectangleShape* leg : {&m_legLeft, &m_legRight})
    {
        leg->setSize({12.f * unit, 14.f * unit});
        leg->setFillColor(dark);
    }

    // Bandana cap: the upper half of the head, a convex dome sitting flat on
    // the forehead (the pointed trim below is drawn as separate pennants so
    // every piece stays a simple convex shape). Sampled finely so the dome
    // reads as a smooth curve rather than a faceted polygon.
    constexpr int CapSegments = 16;
    m_bandanaCap.setPointCount(CapSegments + 1);
    for (int i = 0; i <= CapSegments; ++i)
    {
        const float theta = Pi - Pi * (static_cast<float>(i) / static_cast<float>(CapSegments));
        m_bandanaCap.setPoint(static_cast<std::size_t>(i),
                               relative(28.f * std::cos(theta), -6.f - 28.f * std::sin(theta)));
    }
    m_bandanaCap.setFillColor(palette.bandana);
    m_bandanaCap.setOutlineColor(dark);
    m_bandanaCap.setOutlineThickness(outlineMain);

    constexpr std::array<float, 4> pennantCenters = {-18.f, -6.f, 6.f, 18.f};
    for (std::size_t i = 0; i < m_bandanaPennants.size(); ++i)
    {
        sf::ConvexShape& pennant = m_bandanaPennants[i];
        const float cx = pennantCenters[i];
        pennant.setPointCount(3);
        pennant.setPoint(0, relative(cx - 5.f, -6.f));
        pennant.setPoint(1, relative(cx + 5.f, -6.f));
        pennant.setPoint(2, relative(cx, 2.f));
        pennant.setFillColor(palette.bandana);
        pennant.setOutlineColor(dark);
        pennant.setOutlineThickness(outlineThin);
    }

    m_knot.setRadius(7.f * unit);
    m_knot.setOrigin({m_knot.getRadius(), m_knot.getRadius()});
    m_knot.setFillColor(palette.bandana);
    m_knot.setOutlineColor(dark);
    m_knot.setOutlineThickness(outlineMain);

    // Tails flap around the knot, so their points are expressed relative to
    // it (0, 0) rather than relative to the body center.
    m_tailLeft.setPoint(0, relative(0.f, 0.f));
    m_tailLeft.setPoint(1, relative(-14.f, -10.f));
    m_tailLeft.setPoint(2, relative(-6.f, -16.f));
    m_tailRight.setPoint(0, relative(0.f, 0.f));
    m_tailRight.setPoint(1, relative(6.f, -14.f));
    m_tailRight.setPoint(2, relative(14.f, -6.f));
    for (sf::ConvexShape* tail : {&m_tailLeft, &m_tailRight})
    {
        tail->setFillColor(palette.bandana);
        tail->setOutlineColor(dark);
        tail->setOutlineThickness(outlineThin);
    }

    for (sf::CircleShape& dot : m_bandanaDots)
    {
        dot.setRadius(2.1f * unit);
        dot.setOrigin({dot.getRadius(), dot.getRadius()});
        dot.setFillColor(sf::Color(255, 243, 223, 215));
    }

    // Cute face: one big expressive eye (the other is covered by the
    // eyepatch), a shine, blush and a small smile.
    m_eye.setRadius(6.4f * unit);
    m_eye.setScale({1.f, 7.4f / 6.4f});
    m_eye.setOrigin({m_eye.getRadius(), m_eye.getRadius()});
    m_eye.setFillColor(dark);

    m_eyeShine.setRadius(1.6f * unit);
    m_eyeShine.setOrigin({m_eyeShine.getRadius(), m_eyeShine.getRadius()});
    m_eyeShine.setFillColor(sf::Color::White);

    for (sf::CircleShape* blush : {&m_blushLeft, &m_blushRight})
    {
        blush->setRadius(5.f * unit);
        blush->setScale({1.f, 3.2f / 5.f});
        blush->setOrigin({blush->getRadius(), blush->getRadius()});
        blush->setFillColor(sf::Color(240, 132, 138, 140));
    }

    m_smileBase = {relative(-4.f, 9.f), relative(-2.f, 10.125f), relative(0.f, 10.5f), relative(2.f, 10.125f),
                   relative(4.f, 9.f)};
    for (std::size_t i = 0; i < m_smileBase.size(); ++i)
        m_smile[i].color = dark;

    // Eyepatch: a diagonal strap that stops right under the bandana, and a
    // tilted patch over the other eye.
    const sf::Vector2f strapStart = relative(-9.f, -7.5f);
    const sf::Vector2f strapEnd = relative(27.f, 9.f);
    m_eyepatchStrap.setSize({lengthOf(strapStart, strapEnd), tileSize * 0.026f});
    m_eyepatchStrap.setOrigin({0.f, m_eyepatchStrap.getSize().y / 2.f});
    m_eyepatchStrap.setRotation(sf::degrees(angleDegrees(strapStart, strapEnd)));
    m_eyepatchStrap.setFillColor(strapColor);

    m_eyepatch.setRadius(7.4f * unit);
    m_eyepatch.setScale({1.f, 8.4f / 7.4f});
    m_eyepatch.setOrigin({m_eyepatch.getRadius(), m_eyepatch.getRadius()});
    m_eyepatch.setRotation(sf::degrees(25.f));
    m_eyepatch.setFillColor(dark);

    const sf::Vector2f scarStart = relative(-16.f, 4.f);
    const sf::Vector2f scarEnd = relative(-17.f, 9.f);
    m_scar.setSize({lengthOf(scarStart, scarEnd), tileSize * 0.016f});
    m_scar.setOrigin({0.f, m_scar.getSize().y / 2.f});
    m_scar.setRotation(sf::degrees(angleDegrees(scarStart, scarEnd)));
    m_scar.setFillColor(sf::Color(176, 90, 82));

    // Single centered ear, peeking through the gap in the bandana trim; only
    // used facing down/left/right (facing up shows the back of the head).
    m_earOuter.setRadius(4.2f * unit);
    m_earOuter.setScale({1.f, 5.2f / 4.2f});
    m_earOuter.setOrigin({m_earOuter.getRadius(), m_earOuter.getRadius()});
    m_earOuter.setFillColor(palette.skin);
    m_earOuter.setOutlineColor(dark);
    m_earOuter.setOutlineThickness(outlineMain);

    m_earInner.setRadius(1.9f * unit);
    m_earInner.setScale({1.f, 2.6f / 1.9f});
    m_earInner.setOrigin({m_earInner.getRadius(), m_earInner.getRadius()});
    m_earInner.setFillColor(sf::Color(201, 138, 90, 180));

    // Back-of-head panel: covers most of the head when facing up, leaving
    // only a thin skin rim showing near the chin.
    m_backPanel.setRadius(27.f * unit);
    m_backPanel.setScale({29.f / 27.f, 17.f / 27.f});
    m_backPanel.setOrigin({m_backPanel.getRadius(), m_backPanel.getRadius()});
    m_backPanel.setFillColor(palette.bandana);
    m_backPanel.setOutlineColor(dark);
    m_backPanel.setOutlineThickness(outlineMain);

    m_spark.setOrigin({m_spark.getRadius(), m_spark.getRadius()});
    m_spark.setFillColor(sf::Color(255, 207, 77, 0));
}

// =========================
// Public Methods
// =========================
void CharacterRenderer::update(const Character& character, float deltaSeconds)
{
    const float progress = character.getMoveProgress();
    const sf::Vector2f from = cellCenter(character.getPreviousPosition(), m_tileSize);
    const sf::Vector2f to = cellCenter(character.getPosition(), m_tileSize);
    m_center = from + (to - from) * progress;

    // Hop while walking, flat when idle (progress is 1 then).
    const float hop = std::sin(progress * Pi) * HopAmplitude * m_tileSize;
    const sf::Vector2f bodyPos(m_center.x, m_center.y - hop);
    m_body.setPosition(bodyPos);

    if (deltaSeconds > 0.f && m_bombAnimElapsed < BombAnimDuration)
        m_bombAnimElapsed = std::min(m_bombAnimElapsed + deltaSeconds, BombAnimDuration);
    const bool playingBomb = isPlayingBombAnimation();
    const float bombWave = playingBomb ? std::sin((m_bombAnimElapsed / BombAnimDuration) * Pi) : 0.f;

    m_facing = character.getFacing();
    const bool facingUp = m_facing == Direction::Up;
    const bool facingSide = m_facing == Direction::Left || m_facing == Direction::Right;
    const float mirror = m_facing == Direction::Right ? -1.f : 1.f; // Left is authored directly, Right mirrors it

    // The head cluster (bandana + face) nudges slightly towards where the
    // character is facing, the same way the previous single eye dot did.
    const sf::Vector2f headPos = bodyPos + directionVector(character.getFacing()) * (m_tileSize * FaceNudge);

    const float unit = (m_tileSize * BodyRadiusFraction) / 27.f;
    const auto relative = [unit](float x, float y) { return sf::Vector2f(x * unit, y * unit); };
    const auto relativeM = [unit, mirror](float x, float y) { return sf::Vector2f(mirror * x * unit, y * unit); };

    m_bandanaCap.setPosition(headPos); // same dome for every facing

    const float tailAngle = character.isMoving() ? std::sin(progress * TailFlapFrequency * Pi) * TailFlapAmplitudeDeg
                                                   : 0.f;

    if (facingUp)
    {
        // Back of the head: the dome plus a wide panel cover almost all of
        // it, the knot sits centered and the tails hang down on each side.
        m_backPanel.setPosition(headPos + relative(0.f, 6.f));

        const sf::Vector2f knotOffset = relative(0.f, -18.f);
        m_knot.setPosition(headPos + knotOffset);
        m_tailLeft.setPoint(0, relative(-8.f, 8.f));
        m_tailLeft.setPoint(1, relative(-10.f, 30.f));
        m_tailLeft.setPoint(2, relative(-4.f, 18.f));
        m_tailRight.setPoint(0, relative(8.f, 8.f));
        m_tailRight.setPoint(1, relative(10.f, 30.f));
        m_tailRight.setPoint(2, relative(4.f, 18.f));
        m_tailLeft.setPosition(headPos + knotOffset);
        m_tailLeft.setRotation(sf::degrees(tailAngle));
        m_tailRight.setPosition(headPos + knotOffset);
        m_tailRight.setRotation(sf::degrees(-tailAngle * 0.7f));

        const std::array<sf::Vector2f, 4> dotOffsets = {relative(-14.f, -12.f), relative(14.f, -12.f),
                                                          relative(-8.f, 6.f), relative(8.f, 6.f)};
        for (std::size_t i = 0; i < m_bandanaDots.size(); ++i)
            m_bandanaDots[i].setPosition(headPos + dotOffsets[i]);
    }
    else
    {
        for (sf::ConvexShape& pennant : m_bandanaPennants)
            pennant.setPosition(headPos);

        // Down keeps the knot trailing to its own right; profiles trail it
        // towards the back (away from the leading/visible side).
        const sf::Vector2f knotOffset = facingSide ? relativeM(24.f, -12.f) : relative(20.f, -16.f);
        m_knot.setPosition(headPos + knotOffset);

        if (facingSide)
        {
            m_tailLeft.setPoint(0, relativeM(-8.f, -8.f));
            m_tailLeft.setPoint(1, relativeM(6.f, 2.f));
            m_tailLeft.setPoint(2, relativeM(-4.f, -2.f));
            m_tailRight.setPoint(0, relativeM(-4.f, -4.f));
            m_tailRight.setPoint(1, relativeM(4.f, 12.f));
            m_tailRight.setPoint(2, relativeM(-6.f, 6.f));
        }
        else
        {
            m_tailLeft.setPoint(0, relative(0.f, 0.f));
            m_tailLeft.setPoint(1, relative(-14.f, -10.f));
            m_tailLeft.setPoint(2, relative(-6.f, -16.f));
            m_tailRight.setPoint(0, relative(0.f, 0.f));
            m_tailRight.setPoint(1, relative(6.f, -14.f));
            m_tailRight.setPoint(2, relative(14.f, -6.f));
        }
        m_tailLeft.setPosition(headPos + knotOffset);
        m_tailLeft.setRotation(sf::degrees(tailAngle));
        m_tailRight.setPosition(headPos + knotOffset);
        m_tailRight.setRotation(sf::degrees(-tailAngle * 0.7f));

        const std::array<sf::Vector2f, 3> dotOffsets = {relative(-12.f, -14.f), relative(0.f, -18.f),
                                                          relative(12.f, -14.f)};
        for (std::size_t i = 0; i < 3; ++i)
            m_bandanaDots[i].setPosition(headPos + dotOffsets[i]);
        m_bandanaDots[3].setPosition(m_bandanaDots[0].getPosition()); // unused here, hide by stacking

        // Single centered ear (not shown facing up).
        m_earOuter.setPosition(headPos);
        m_earInner.setPosition(headPos + relative(0.f, 0.5f));

        if (facingSide)
        {
            // Only the leading cheek is visible: one eye, its blush and the
            // scar; the eyepatch/strap are on the hidden cheek.
            m_eye.setScale({1.f, 7.6f / 6.8f});
            m_eye.setPosition(headPos + relativeM(-20.f, -1.f));
            m_eyeShine.setPosition(headPos + relativeM(-22.f, -4.f));
            m_blushLeft.setPosition(headPos + relativeM(-24.f, 6.f));

            const std::array<sf::Vector2f, 5> sideSmile = {relativeM(-25.f, 9.f), relativeM(-22.56f, 10.19f),
                                                             relativeM(-20.25f, 10.75f), relativeM(-18.06f, 10.69f),
                                                             relativeM(-16.f, 10.f)};
            for (std::size_t i = 0; i < sideSmile.size(); ++i)
                m_smile[i].position = headPos + sideSmile[i];

            const sf::Vector2f scarStart = relativeM(-30.f, 2.f);
            const sf::Vector2f scarEnd = relativeM(-32.f, 8.f);
            m_scar.setSize({lengthOf(scarStart, scarEnd), m_tileSize * 0.016f});
            m_scar.setRotation(sf::degrees(angleDegrees(scarStart, scarEnd)));
            m_scar.setPosition(headPos + scarStart);
        }
        else
        {
            m_eye.setScale({1.f, 7.4f / 6.4f});
            m_eye.setPosition(headPos + relative(-9.f, -1.f));
            m_eyeShine.setPosition(headPos + relative(-11.f, -4.f));
            m_blushLeft.setPosition(headPos + relative(-14.f, 6.f));
            m_blushRight.setPosition(headPos + relative(14.f, 6.f));
            for (std::size_t i = 0; i < m_smileBase.size(); ++i)
                m_smile[i].position = headPos + m_smileBase[i];

            const sf::Vector2f scarStart = relative(-16.f, 4.f);
            const sf::Vector2f scarEnd = relative(-17.f, 9.f);
            m_scar.setSize({lengthOf(scarStart, scarEnd), m_tileSize * 0.016f});
            m_scar.setRotation(sf::degrees(angleDegrees(scarStart, scarEnd)));
            m_scar.setPosition(headPos + scarStart);

            m_eyepatchStrap.setPosition(headPos + relative(-9.f, -7.5f));
            m_eyepatch.setPosition(headPos + relative(10.f, -1.f));
        }
    }

    // Legs: one lifts, then the other, once per cell crossed; both rest flat
    // when idle. Facing left/right only shows a single centered leg.
    const float leftLift = progress < 0.5f ? std::sin(progress / 0.5f * Pi) : 0.f;
    const float rightLift = progress >= 0.5f ? std::sin((progress - 0.5f) / 0.5f * Pi) : 0.f;
    if (facingSide)
    {
        const float lift = std::max(leftLift, rightLift);
        m_legLeft.setPosition(bodyPos + relative(-6.f, 24.f) - sf::Vector2f(0.f, lift * LegStepAmplitude * m_tileSize));
    }
    else
    {
        m_legLeft.setPosition(bodyPos + relative(-18.f, 24.f)
                               - sf::Vector2f(0.f, leftLift * LegStepAmplitude * m_tileSize));
        m_legRight.setPosition(bodyPos + relative(6.f, 24.f)
                                - sf::Vector2f(0.f, rightLift * LegStepAmplitude * m_tileSize));
    }

    // Arms stay still while walking; they only raise for the bomb-drop pose.
    // Facing left/right only shows a single centered arm, which nudges up
    // instead of rotating (it is mostly hidden behind the body).
    const float armAngle = playingBomb ? bombWave * MaxRaiseAngleDeg : 0.f;
    if (facingSide)
    {
        m_armLeft.setScale({1.f, 10.f / 7.f});
        m_armLeft.setRotation(sf::degrees(0.f));
        m_armLeft.setPosition(bodyPos + relative(0.f, 4.f) - sf::Vector2f(0.f, bombWave * 0.1f * m_tileSize));
    }
    else
    {
        m_armLeft.setScale({1.f, 11.f / 7.f});
        m_armLeft.setPosition(bodyPos + relative(-25.f, -2.f));
        m_armLeft.setRotation(sf::degrees(-armAngle));
        m_armRight.setPosition(bodyPos + relative(25.f, -2.f));
        m_armRight.setRotation(sf::degrees(armAngle));
    }

    // Ground spark: flashes where the bomb just landed, in front of the
    // character, then fades back out.
    const sf::Vector2f sparkPos =
        m_center + directionVector(character.getFacing()) * (m_tileSize * 0.55f) + sf::Vector2f(0.f, m_tileSize * 0.12f);
    m_spark.setPosition(sparkPos);
    sf::Color sparkColor = m_spark.getFillColor();
    sparkColor.a = static_cast<std::uint8_t>(bombWave * 230.f);
    m_spark.setFillColor(sparkColor);
}

void CharacterRenderer::triggerBombAnimation() noexcept
{
    m_bombAnimElapsed = 0.f;
}

bool CharacterRenderer::isPlayingBombAnimation() const noexcept
{
    return m_bombAnimElapsed < BombAnimDuration;
}

sf::Vector2f CharacterRenderer::getCenter() const noexcept
{
    return m_center;
}

const sf::CircleShape& CharacterRenderer::getBody() const noexcept
{
    return m_body;
}

// =========================
// Private Methods
// =========================
void CharacterRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();

    const bool facingUp = m_facing == Direction::Up;
    const bool facingSide = m_facing == Direction::Left || m_facing == Direction::Right;
    const bool twoLimbs = !facingSide; // down and up show both arms/legs

    target.draw(m_spark, states);
    target.draw(m_legLeft, states);
    if (twoLimbs)
        target.draw(m_legRight, states);
    target.draw(m_armLeft, states);
    if (twoLimbs)
        target.draw(m_armRight, states);
    target.draw(m_body, states);

    target.draw(m_tailLeft, states);
    target.draw(m_tailRight, states);
    target.draw(m_bandanaCap, states);

    if (facingUp)
    {
        target.draw(m_backPanel, states);
    }
    else
    {
        for (const sf::ConvexShape& pennant : m_bandanaPennants)
            target.draw(pennant, states);
        target.draw(m_earOuter, states);
        target.draw(m_earInner, states);
    }

    target.draw(m_knot, states);
    const std::size_t dotCount = facingUp ? m_bandanaDots.size() : 3;
    for (std::size_t i = 0; i < dotCount; ++i)
        target.draw(m_bandanaDots[i], states);

    if (!facingUp)
    {
        target.draw(m_blushLeft, states);
        if (!facingSide)
            target.draw(m_blushRight, states);
        target.draw(m_eye, states);
        target.draw(m_eyeShine, states);
        target.draw(m_smile, states);
        target.draw(m_scar, states);

        if (!facingSide)
        {
            target.draw(m_eyepatchStrap, states);
            target.draw(m_eyepatch, states);
        }
    }
}

} // namespace Bomberman
