#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "Renderer/TileRenderer.h"
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Compressor.h"
#include "Cave/Entity/Genom.h"
#include "Cave/Entity/GenomEgg.h"
#include "Cave/Entity/HiveLarva.h"
#include "Cave/Entity/HiveMatriarch.h"
#include "Cave/Entity/HiveHunterBubble.h"
#include "Cave/Entity/HiveWorker.h"
#include "Cave/Entity/Jim.h"
#include "Cave/Entity/Space.h"
#include "Cave/Entity/Spinner.h"
#include "Cave/Entity/Type.h"

namespace {

static void clearTileVerts(sf::Vertex* tri, int count) {
    for (int v = 0; v < count; ++v) {
        tri[v].position = { 0.f, 0.f };
        tri[v].texCoords = { 0.f, 0.f };
        tri[v].color = sf::Color::Transparent;
    }
}

static void writeTileQuad(sf::Vertex* tri, float px1, float py1, float px2, float py2,
    float tx1, float ty1, float tx2, float ty2, sf::Color tint) {
    tri[0].texCoords = { tx1, ty1 };
    tri[1].texCoords = { tx2, ty1 };
    tri[2].texCoords = { tx2, ty2 };
    tri[3].texCoords = { tx1, ty1 };
    tri[4].texCoords = { tx2, ty2 };
    tri[5].texCoords = { tx1, ty2 };
    tri[0].position = { px1, py1 };
    tri[1].position = { px2, py1 };
    tri[2].position = { px2, py2 };
    tri[3].position = { px1, py1 };
    tri[4].position = { px2, py2 };
    tri[5].position = { px1, py2 };
    for (int v = 0; v < 6; ++v) tri[v].color = tint;
}

struct OverlayQuad {
    float px1 = 0.f, py1 = 0.f, px2 = 0.f, py2 = 0.f;
    float tx1 = 0.f, ty1 = 0.f, tx2 = 0.f, ty2 = 0.f;
    sf::Color tint = sf::Color::White;
    int cellIndex = 0;
};

}

Renderer::TileRenderer::TileRenderer(Image::Manager* imageManager): m_imageManager(imageManager) {}

bool Renderer::TileRenderer::load(const Image::Texture& texture, const sf::Vector2u& tileSize) {
    m_tileset = m_imageManager->getTexture(texture);

    m_tilesize = tileSize;
    m_vertices.setPrimitiveType(sf::PrimitiveType::Triangles);
    m_tilesetCols = m_tileset.getSize().x / tileSize.x;

    return true;
}

void Renderer::TileRenderer::updateTexture(const std::vector<Cave::Entity::Base>& entities, const sf::Vector2i& position, const sf::IntRect& gridRange, int gap, sf::Color jimTint) {
    struct WormSlide {
        float px = 0.f;
        float py = 0.f;
        int tileIndex = Cave::Entity::NO_TEXTURE_INDEX;
        sf::Color tint = sf::Color::White;
    };
    WormSlide wormSlides[96];
    int wormSlideCount = 0;
    std::vector<OverlayQuad> overlays;
    overlays.reserve(64);

    auto pushGenomLayers = [&](const Cave::Entity::Base& entity, int look, int frame,
        float baseX, float baseY, const sf::IntRect& ep, const sf::IntRect& tp, int cellIndex) {
        int layers[Cave::Entity::Genom::LAYER_COUNT];
        Cave::Entity::Genom::layerTextures(look, frame, layers);
        const float px1 = baseX + ep.position.x;
        const float py1 = baseY + ep.position.y;
        const float px2 = baseX + ep.position.x + ep.size.x;
        const float py2 = baseY + ep.position.y + ep.size.y;
        for (int i = 0; i < Cave::Entity::Genom::LAYER_COUNT; ++i) {
            const int tileIndex = layers[i];
            if (tileIndex == Cave::Entity::NO_TEXTURE_INDEX) continue;
            const sf::Vector2i textureCoords = { tileIndex % m_tilesetCols, tileIndex / m_tilesetCols };
            OverlayQuad q;
            q.px1 = px1; q.py1 = py1; q.px2 = px2; q.py2 = py2;
            q.tx1 = textureCoords.x * m_tilesize.x + tp.position.x;
            q.ty1 = textureCoords.y * m_tilesize.y + tp.position.y;
            q.tx2 = textureCoords.x * m_tilesize.x + tp.position.x + tp.size.x;
            q.ty2 = textureCoords.y * m_tilesize.y + tp.position.y + tp.size.y;
            // Spikes stay B&W; wings/eyes tint with highlight preserve (alpha 0 flag).
            q.tint = Cave::Entity::Genom::layerTint(look, i);
            q.cellIndex = cellIndex;
            overlays.push_back(q);
        }
        // Court particles sit above every look layer for the 3s breeding hold.
        const int breedTex = Cave::Entity::Genom::breedingTexture(entity, frame);
        if (breedTex != Cave::Entity::NO_TEXTURE_INDEX) {
            const sf::Vector2i textureCoords = { breedTex % m_tilesetCols, breedTex / m_tilesetCols };
            OverlayQuad q;
            q.px1 = px1; q.py1 = py1; q.px2 = px2; q.py2 = py2;
            q.tx1 = textureCoords.x * m_tilesize.x + tp.position.x;
            q.ty1 = textureCoords.y * m_tilesize.y + tp.position.y;
            q.tx2 = textureCoords.x * m_tilesize.x + tp.position.x + tp.size.x;
            q.ty2 = textureCoords.y * m_tilesize.y + tp.position.y + tp.size.y;
            q.tint = sf::Color::White;
            q.cellIndex = cellIndex;
            overlays.push_back(q);
        }
    };

    auto pushGenomEggStack = [&](const Cave::Entity::Base& entity, int look, int eggTex, int babyFrame,
        float baseX, float baseY, const sf::IntRect& ep, const sf::IntRect& tp, int cellIndex) {
        pushGenomLayers(entity, look, babyFrame, baseX, baseY, ep, tp, cellIndex);
        if (eggTex == Cave::Entity::NO_TEXTURE_INDEX) return;
        const sf::Vector2i textureCoords = { eggTex % m_tilesetCols, eggTex / m_tilesetCols };
        OverlayQuad q;
        q.px1 = baseX + ep.position.x;
        q.py1 = baseY + ep.position.y;
        q.px2 = baseX + ep.position.x + ep.size.x;
        q.py2 = baseY + ep.position.y + ep.size.y;
        q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x);
        q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y);
        q.tx2 = (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x + tp.size.x);
        q.ty2 = (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y + tp.size.y);
        q.tint = Cave::Entity::Genom::tintFromLook(look);
        q.cellIndex = cellIndex;
        overlays.push_back(q);
    };

    int index = 0;
    const int cellCount = gridRange.size.x * gridRange.size.y;
    m_vertices.resize(static_cast<std::size_t>(cellCount) * 12);
    for (int y = gridRange.position.y; y < gridRange.position.y + gridRange.size.y; ++y) {
        for (int x = gridRange.position.x; x < gridRange.position.x + gridRange.size.x; ++x) {
            const Cave::Entity::Base& entity = entities[index];
            sf::Vertex* tri = &m_vertices[index * 12];

            const float baseX = (gap == 0)
                ? (float)(x * (int)m_tilesize.x + position.x)
                : (float)((x - gridRange.position.x) * ((int)m_tilesize.x + gap) + gap + position.x);
            const float baseY = (gap == 0)
                ? (float)(y * (int)m_tilesize.y + position.y)
                : (float)((y - gridRange.position.y) * ((int)m_tilesize.y + gap) + gap + position.y);
            // Tint only Jim sprite layers — never the empty remainder of a slide cell
            // (that made the pyrobe/ruby pulse crawl across the tile ahead of Jim).
            // Buff tints use alpha==1 so tint.frag skips the cave colour filter.
            const bool hasJimBuff = jimTint.a == 1;
            auto tintForTex = [&](int tex) -> sf::Color {
                if (!hasJimBuff) return sf::Color::White;
                if (tex != Cave::Entity::NO_TEXTURE_INDEX
                    && Cave::Entity::Jim::isJimTexture(tex))
                    return jimTint;
                return sf::Color::White;
            };
            const sf::Color tint = (entity.getType() == Cave::Entity::Type::Jim && hasJimBuff)
                ? jimTint : sf::Color::White;

            const int prevTex = entity.getPreviousTextureIndex();

            // Compressor suck: prey/Genom into-slides under a full-tile open mouth.
            // Checked before fall slides so MODE_SUCKING into is not treated as a fall.
            const bool compressorSuckRide = entity.getType() == Cave::Entity::Type::Compressor
                && entity.isIntoTransition()
                && Cave::Entity::Compressor::mode(entity) == Cave::Entity::Compressor::MODE_SUCKING
                && prevTex != Cave::Entity::NO_TEXTURE_INDEX;
            if (compressorSuckRide) {
                const int floorIndex = Cave::Entity::Space::TEXTURE_INDEX;
                const sf::Vector2i floorCoords = { floorIndex % m_tilesetCols, floorIndex / m_tilesetCols };
                writeTileQuad(tri,
                    baseX, baseY, baseX + (float)m_tilesize.x, baseY + (float)m_tilesize.y,
                    (float)(floorCoords.x * (int)m_tilesize.x), (float)(floorCoords.y * (int)m_tilesize.y),
                    (float)((floorCoords.x + 1) * (int)m_tilesize.x), (float)((floorCoords.y + 1) * (int)m_tilesize.y),
                    sf::Color::White);
                clearTileVerts(&m_vertices[index * 12 + 6], 6);

                const int rideLook = (entity.homeIndex != -1
                    && (entity.homeIndex & Cave::Entity::Genom::LOOK_VALID) != 0)
                    ? entity.homeIndex : 0;
                const sf::IntRect moveEp = entity.getCurrentPosition();
                const sf::IntRect moveTp = entity.getCurrentTextureCoords();
                const bool preyIsCompressor = prevTex >= Cave::Entity::Compressor::IDLE_BASE
                    && prevTex < Cave::Entity::Compressor::OPEN_BASE + Cave::Entity::Compressor::OPEN_COUNT;
                if (rideLook != 0 && Cave::Entity::Genom::isGenomTexture(prevTex)) {
                    const int frame = Cave::Entity::Genom::frameFromBgTexture(prevTex);
                    pushGenomLayers(entity, rideLook, frame, baseX, baseY, moveEp, moveTp, index);
                }
                else if (preyIsCompressor) {
                    // Full-tile prey compressor under the mouth (avoids black crop seams).
                    const int disp = entity.getTransitionDisplacement();
                    float ox = 0.f, oy = 0.f;
                    switch (entity.getTransitionDirection()) {
                    case Cave::Entity::Direction::RIGHT: ox = (float)(disp - 32); break;
                    case Cave::Entity::Direction::LEFT:  ox = (float)(32 - disp); break;
                    case Cave::Entity::Direction::DOWN:  oy = (float)(disp - 32); break;
                    case Cave::Entity::Direction::UP:    oy = (float)(32 - disp); break;
                    default: break;
                    }
                    const sf::Vector2i textureCoords = { prevTex % m_tilesetCols, prevTex / m_tilesetCols };
                    const float tw = (float)m_tilesize.x;
                    const float th = (float)m_tilesize.y;
                    OverlayQuad q;
                    q.px1 = baseX + ox;
                    q.py1 = baseY + oy;
                    q.px2 = baseX + ox + tw;
                    q.py2 = baseY + oy + th;
                    q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x);
                    q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y);
                    q.tx2 = (float)((textureCoords.x + 1) * (int)m_tilesize.x);
                    q.ty2 = (float)((textureCoords.y + 1) * (int)m_tilesize.y);
                    q.tint = sf::Color::White;
                    q.cellIndex = index;
                    overlays.push_back(q);
                }
                else if (moveEp.size.x > 0 && moveEp.size.y > 0) {
                    const sf::Vector2i textureCoords = { prevTex % m_tilesetCols, prevTex / m_tilesetCols };
                    OverlayQuad q;
                    q.px1 = baseX + moveEp.position.x;
                    q.py1 = baseY + moveEp.position.y;
                    q.px2 = baseX + moveEp.position.x + moveEp.size.x;
                    q.py2 = baseY + moveEp.position.y + moveEp.size.y;
                    q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x + moveTp.position.x);
                    q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y + moveTp.position.y);
                    q.tx2 = (float)(textureCoords.x * (int)m_tilesize.x + moveTp.position.x + moveTp.size.x);
                    q.ty2 = (float)(textureCoords.y * (int)m_tilesize.y + moveTp.position.y + moveTp.size.y);
                    q.tint = sf::Color::White;
                    q.cellIndex = index;
                    overlays.push_back(q);
                }

                const int mouthTex = entity.getCurrentTextureIndex();
                if (mouthTex != Cave::Entity::NO_TEXTURE_INDEX) {
                    const sf::Vector2i textureCoords = { mouthTex % m_tilesetCols, mouthTex / m_tilesetCols };
                    OverlayQuad q;
                    q.px1 = baseX;
                    q.py1 = baseY;
                    q.px2 = baseX + (float)m_tilesize.x;
                    q.py2 = baseY + (float)m_tilesize.y;
                    q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x);
                    q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y);
                    q.tx2 = (float)((textureCoords.x + 1) * (int)m_tilesize.x);
                    q.ty2 = (float)((textureCoords.y + 1) * (int)m_tilesize.y);
                    q.tint = sf::Color::White;
                    q.cellIndex = index;
                    overlays.push_back(q);
                }
                index++;
                continue;
            }

            // Full-tile slides avoid crop seams through opaque black sprite backgrounds
            // (Spinner / Compressor) and keep Worm bodies whole while moving.
            // Skip when a hive hauler is spitting the package — that path needs the
            // collectorDropInto body+bubble stack (compressor intoSlide would steal it).
            auto haulerAwayAtEarly = [&](int ni, Cave::Entity::Direction awayDir) -> bool {
                if (ni < 0 || ni >= cellCount) return false;
                const auto& c = entities[static_cast<size_t>(ni)];
                return Cave::Entity::isHiveDepositSpitter(c.getType())
                    && c.isAwayTransition()
                    && c.getTransitionDirection() == awayDir;
            };
            bool haulerSpittingOnto = false;
            if (entity.isIntoTransition()) {
                const auto td = entity.getTransitionDirection();
                const int viewWEarly = gridRange.size.x;
                if (td == Cave::Entity::Direction::DOWN
                    && haulerAwayAtEarly(index - viewWEarly, td))
                    haulerSpittingOnto = true;
                else if (td == Cave::Entity::Direction::UP
                    && haulerAwayAtEarly(index + viewWEarly, td))
                    haulerSpittingOnto = true;
                else if (td == Cave::Entity::Direction::LEFT
                    && haulerAwayAtEarly(index + 1, td))
                    haulerSpittingOnto = true;
                else if (td == Cave::Entity::Direction::RIGHT
                    && haulerAwayAtEarly(index - 1, td))
                    haulerSpittingOnto = true;
            }
            const bool spinnerFrame = prevTex >= Cave::Entity::Spinner::FRAME_BASE
                && prevTex < Cave::Entity::Spinner::FRAME_BASE + Cave::Entity::Spinner::FRAME_COUNT;
            const bool compressorFrame = prevTex >= Cave::Entity::Compressor::IDLE_BASE
                && prevTex < Cave::Entity::Compressor::OPEN_BASE + Cave::Entity::Compressor::OPEN_COUNT;
            const bool intoSlide = entity.isIntoTransition()
                && !haulerSpittingOnto
                && (Cave::Entity::isWorm(entity.getType())
                    || entity.getType() == Cave::Entity::Type::Spinner
                    || entity.getType() == Cave::Entity::Type::Compressor);
            const bool awaySlide = entity.isAwayTransition()
                && entity.getType() == Cave::Entity::Type::Space
                && (spinnerFrame || compressorFrame);
            if (intoSlide || awaySlide) {
                const int floorIndex = Cave::Entity::Space::TEXTURE_INDEX;
                const sf::Vector2i floorCoords = { floorIndex % m_tilesetCols, floorIndex / m_tilesetCols };
                writeTileQuad(tri,
                    baseX, baseY, baseX + (float)m_tilesize.x, baseY + (float)m_tilesize.y,
                    (float)(floorCoords.x * (int)m_tilesize.x), (float)(floorCoords.y * (int)m_tilesize.y),
                    (float)((floorCoords.x + 1) * (int)m_tilesize.x), (float)((floorCoords.y + 1) * (int)m_tilesize.y),
                    sf::Color::White);
                clearTileVerts(&m_vertices[index * 12 + 6], 6);

                const int disp = entity.getTransitionDisplacement();
                float ox = 0.f;
                float oy = 0.f;
                int slideTex = entity.getCurrentTextureIndex();
                if (awaySlide) {
                    slideTex = prevTex;
                    switch (entity.getTransitionDirection()) {
                    case Cave::Entity::Direction::RIGHT: ox = (float)disp; break;
                    case Cave::Entity::Direction::LEFT:  ox = (float)-disp; break;
                    case Cave::Entity::Direction::DOWN:  oy = (float)disp; break;
                    case Cave::Entity::Direction::UP:    oy = (float)-disp; break;
                    default: break;
                    }
                }
                else {
                    switch (entity.getTransitionDirection()) {
                    case Cave::Entity::Direction::RIGHT: ox = (float)(disp - 32); break;
                    case Cave::Entity::Direction::LEFT:  ox = (float)(32 - disp); break;
                    case Cave::Entity::Direction::DOWN:  oy = (float)(disp - 32); break;
                    case Cave::Entity::Direction::UP:    oy = (float)(32 - disp); break;
                    default: break;
                    }
                }
                if (awaySlide && compressorFrame) {
                    // Compressor away must stay under later tiles (receiving mouth below),
                    // unlike Spinner/Worm which intentionally draw above everything.
                    const sf::Vector2i textureCoords = { slideTex % m_tilesetCols, slideTex / m_tilesetCols };
                    const float tw = (float)m_tilesize.x;
                    const float th = (float)m_tilesize.y;
                    OverlayQuad q;
                    q.px1 = baseX + ox;
                    q.py1 = baseY + oy;
                    q.px2 = baseX + ox + tw;
                    q.py2 = baseY + oy + th;
                    q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x);
                    q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y);
                    q.tx2 = (float)((textureCoords.x + 1) * (int)m_tilesize.x);
                    q.ty2 = (float)((textureCoords.y + 1) * (int)m_tilesize.y);
                    q.tint = tint;
                    q.cellIndex = index;
                    overlays.push_back(q);
                }
                else if (wormSlideCount < 96) {
                    wormSlides[wormSlideCount++] = { baseX + ox, baseY + oy, slideTex, tint };
                }
                index++;
                continue;
            }

            const bool isGenom = entity.getType() == Cave::Entity::Type::Genom
                && Cave::Entity::Genom::hasLook(entity);
            const bool isGenomEgg = entity.getType() == Cave::Entity::Type::GenomEgg
                && Cave::Entity::Genom::hasLook(entity);
            const int gateGhostLook = (entity.homeIndex != -1
                && (entity.homeIndex & Cave::Entity::Genom::LOOK_VALID) != 0)
                ? entity.homeIndex : 0;
            // Vacating Space (look in spawnCredit) or restored open gate (look in homeIndex).
            const bool genomGhost = entity.isAwayTransition()
                && Cave::Entity::Genom::isGenomTexture(prevTex)
                && ((entity.getType() == Cave::Entity::Type::Space
                        && Cave::Entity::Genom::hasLook(entity))
                    || ((entity.getType() == Cave::Entity::Type::Gate
                            || entity.getType() == Cave::Entity::Type::PrivateGate)
                        && gateGhostLook != 0));
            // Predator into-slide carrying a Genom look in homeIndex.
            // homeIndex==-1 must not pass LOOK_VALID (bit 31 is set on -1).
            // Hive haulers / matriarch use their own cargo+bubble stacks (look is also in homeIndex).
            const bool genomPreyRide = entity.isIntoTransition()
                && entity.getType() != Cave::Entity::Type::Compressor
                && !Cave::Entity::isHiveHauler(entity.getType())
                && entity.getType() != Cave::Entity::Type::HiveMatriarch
                && entity.homeIndex != -1
                && (entity.homeIndex & Cave::Entity::Genom::LOOK_VALID) != 0
                && prevTex != Cave::Entity::NO_TEXTURE_INDEX
                && Cave::Entity::Genom::isGenomTexture(prevTex);
            // GenomEgg sliding off a cell: layered tint must travel with the previous crop.
            const bool eggAwayGhost = entity.isAwayTransition()
                && Cave::Entity::GenomEgg::isEggTexture(prevTex)
                && entity.getType() == Cave::Entity::Type::Space
                && Cave::Entity::Genom::hasLook(entity);
            const bool eggPushGhost = entity.isPushTransition()
                && Cave::Entity::GenomEgg::isEggTexture(prevTex)
                && gateGhostLook != 0;

            if (genomPreyRide) {
                int tileIndex = entity.getCurrentTextureIndex();
                if (tileIndex == Cave::Entity::NO_TEXTURE_INDEX) {
                    clearTileVerts(tri, 6);
                }
                else {
                    const sf::Vector2i textureCoords = { tileIndex % m_tilesetCols, tileIndex / m_tilesetCols };
                    const sf::IntRect ep = entity.getCurrentPosition();
                    const sf::IntRect tp = entity.getCurrentTextureCoords();
                    writeTileQuad(tri,
                        baseX + ep.position.x, baseY + ep.position.y,
                        baseX + ep.position.x + ep.size.x, baseY + ep.position.y + ep.size.y,
                        (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x),
                        (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y),
                        (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x + tp.size.x),
                        (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y + tp.size.y),
                        tint);
                }
                clearTileVerts(&m_vertices[index * 12 + 6], 6);
                const int frame = Cave::Entity::Genom::frameFromBgTexture(prevTex);
                pushGenomLayers(entity, entity.homeIndex, frame, baseX, baseY,
                    entity.getPreviousPosition(), entity.getPreviousTextureCoords(), index);
                index++;
                continue;
            }

            if (eggPushGhost) {
                // Pusher current (Jim) + layered egg previous (suppress raw shell previous).
                int tileIndex = entity.getCurrentTextureIndex();
                if (tileIndex == Cave::Entity::NO_TEXTURE_INDEX) {
                    clearTileVerts(tri, 6);
                }
                else {
                    const sf::Vector2i textureCoords = { tileIndex % m_tilesetCols, tileIndex / m_tilesetCols };
                    const sf::IntRect ep = entity.getCurrentPosition();
                    const sf::IntRect tp = entity.getCurrentTextureCoords();
                    writeTileQuad(tri,
                        baseX + ep.position.x, baseY + ep.position.y,
                        baseX + ep.position.x + ep.size.x, baseY + ep.position.y + ep.size.y,
                        (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x),
                        (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y),
                        (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x + tp.size.x),
                        (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y + tp.size.y),
                        tint);
                }
                clearTileVerts(&m_vertices[index * 12 + 6], 6);
                const int babyFrame = std::min(
                    Cave::Entity::GenomEgg::frameFromTexture(prevTex),
                    Cave::Entity::Genom::FRAME_COUNT - 1);
                pushGenomEggStack(entity, gateGhostLook, prevTex, babyFrame, baseX, baseY,
                    entity.getPreviousPosition(), entity.getPreviousTextureCoords(), index);
                index++;
                continue;
            }

            if (eggAwayGhost) {
                const int floorIndex = Cave::Entity::Space::TEXTURE_INDEX;
                const sf::Vector2i floorCoords = { floorIndex % m_tilesetCols, floorIndex / m_tilesetCols };
                writeTileQuad(tri,
                    baseX, baseY, baseX + (float)m_tilesize.x, baseY + (float)m_tilesize.y,
                    (float)(floorCoords.x * (int)m_tilesize.x), (float)(floorCoords.y * (int)m_tilesize.y),
                    (float)((floorCoords.x + 1) * (int)m_tilesize.x), (float)((floorCoords.y + 1) * (int)m_tilesize.y),
                    sf::Color::White);
                clearTileVerts(&m_vertices[index * 12 + 6], 6);
                const int babyFrame = std::min(
                    Cave::Entity::GenomEgg::frameFromTexture(prevTex),
                    Cave::Entity::Genom::FRAME_COUNT - 1);
                pushGenomEggStack(entity, entity.spawnCredit, prevTex, babyFrame, baseX, baseY,
                    entity.getPreviousPosition(), entity.getPreviousTextureCoords(), index);
                index++;
                continue;
            }

            if (isGenomEgg) {
                const int floorIndex = Cave::Entity::Space::TEXTURE_INDEX;
                const sf::Vector2i floorCoords = { floorIndex % m_tilesetCols, floorIndex / m_tilesetCols };
                const bool eggInto = entity.isIntoTransition();
                const sf::IntRect ep = entity.getCurrentPosition();
                const sf::IntRect tp = entity.getCurrentTextureCoords();
                const int babyFrame = std::min(
                    entity.getAnimation().currentFrame, Cave::Entity::Genom::FRAME_COUNT - 1);
                const int eggTex = entity.getCurrentTextureIndex();

                if (eggInto) {
                    clearTileVerts(tri, 6);
                    if (ep.size.x > 0 && ep.size.y > 0) {
                        writeTileQuad(tri,
                            baseX + ep.position.x, baseY + ep.position.y,
                            baseX + ep.position.x + ep.size.x, baseY + ep.position.y + ep.size.y,
                            (float)(floorCoords.x * (int)m_tilesize.x + ep.position.x),
                            (float)(floorCoords.y * (int)m_tilesize.y + ep.position.y),
                            (float)(floorCoords.x * (int)m_tilesize.x + ep.position.x + ep.size.x),
                            (float)(floorCoords.y * (int)m_tilesize.y + ep.position.y + ep.size.y),
                            sf::Color::White);
                    }
                    clearTileVerts(&m_vertices[index * 12 + 6], 6);
                    pushGenomEggStack(entity, entity.spawnCredit, eggTex, babyFrame,
                        baseX, baseY, ep, tp, index);
                    const int destPrev = entity.getPreviousTextureIndex();
                    if (destPrev != Cave::Entity::NO_TEXTURE_INDEX) {
                        const sf::Vector2i textureCoords = { destPrev % m_tilesetCols, destPrev / m_tilesetCols };
                        const sf::IntRect prevEp = entity.getPreviousPosition();
                        const sf::IntRect prevTp = entity.getPreviousTextureCoords();
                        OverlayQuad q;
                        q.px1 = baseX + prevEp.position.x;
                        q.py1 = baseY + prevEp.position.y;
                        q.px2 = baseX + prevEp.position.x + prevEp.size.x;
                        q.py2 = baseY + prevEp.position.y + prevEp.size.y;
                        q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x + prevTp.position.x);
                        q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y + prevTp.position.y);
                        q.tx2 = (float)(textureCoords.x * (int)m_tilesize.x + prevTp.position.x + prevTp.size.x);
                        q.ty2 = (float)(textureCoords.y * (int)m_tilesize.y + prevTp.position.y + prevTp.size.y);
                        q.tint = sf::Color::White;
                        q.cellIndex = index;
                        overlays.push_back(q);
                    }
                }
                else {
                    writeTileQuad(tri,
                        baseX, baseY, baseX + (float)m_tilesize.x, baseY + (float)m_tilesize.y,
                        (float)(floorCoords.x * (int)m_tilesize.x), (float)(floorCoords.y * (int)m_tilesize.y),
                        (float)((floorCoords.x + 1) * (int)m_tilesize.x), (float)((floorCoords.y + 1) * (int)m_tilesize.y),
                        sf::Color::White);
                    clearTileVerts(&m_vertices[index * 12 + 6], 6);
                    pushGenomEggStack(entity, entity.spawnCredit, eggTex, babyFrame,
                        baseX, baseY, ep, tp, index);
                }
                index++;
                continue;
            }

            if (isGenom || genomGhost) {
                const int floorIndex = Cave::Entity::Space::TEXTURE_INDEX;
                const sf::Vector2i floorCoords = { floorIndex % m_tilesetCols, floorIndex / m_tilesetCols };
                const bool genomInto = isGenom && entity.isIntoTransition();
                const int ghostLook = Cave::Entity::Genom::hasLook(entity)
                    ? entity.spawnCredit : gateGhostLook;

                if (genomInto) {
                    // Match other monsters: current under, previous (gate) on top.
                    // Layers act as "current"; gate previous is appended after layers.
                    clearTileVerts(tri, 6);
                    const sf::IntRect curEp = entity.getCurrentPosition();
                    if (curEp.size.x > 0 && curEp.size.y > 0) {
                        writeTileQuad(tri,
                            baseX + curEp.position.x, baseY + curEp.position.y,
                            baseX + curEp.position.x + curEp.size.x, baseY + curEp.position.y + curEp.size.y,
                            (float)(floorCoords.x * (int)m_tilesize.x + curEp.position.x),
                            (float)(floorCoords.y * (int)m_tilesize.y + curEp.position.y),
                            (float)(floorCoords.x * (int)m_tilesize.x + curEp.position.x + curEp.size.x),
                            (float)(floorCoords.y * (int)m_tilesize.y + curEp.position.y + curEp.size.y),
                            sf::Color::White);
                    }
                    clearTileVerts(&m_vertices[index * 12 + 6], 6);
                    const int frame = Cave::Entity::Genom::frameFromBgTexture(entity.getCurrentTextureIndex());
                    pushGenomLayers(entity, entity.spawnCredit, frame, baseX, baseY,
                        entity.getCurrentPosition(), entity.getCurrentTextureCoords(), index);
                    const int destPrev = entity.getPreviousTextureIndex();
                    if (destPrev != Cave::Entity::NO_TEXTURE_INDEX) {
                        const sf::Vector2i textureCoords = { destPrev % m_tilesetCols, destPrev / m_tilesetCols };
                        const sf::IntRect ep = entity.getPreviousPosition();
                        const sf::IntRect tp = entity.getPreviousTextureCoords();
                        OverlayQuad q;
                        q.px1 = baseX + ep.position.x;
                        q.py1 = baseY + ep.position.y;
                        q.px2 = baseX + ep.position.x + ep.size.x;
                        q.py2 = baseY + ep.position.y + ep.size.y;
                        q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x);
                        q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y);
                        q.tx2 = (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x + tp.size.x);
                        q.ty2 = (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y + tp.size.y);
                        q.tint = sf::Color::White;
                        q.cellIndex = index;
                        overlays.push_back(q);
                    }
                }
                else if (genomGhost) {
                    // Restored gate or Space: show tile as current, layered ghost as previous.
                    int tileIndex = entity.getCurrentTextureIndex();
                    if (tileIndex == Cave::Entity::NO_TEXTURE_INDEX)
                        tileIndex = floorIndex;
                    {
                        const sf::Vector2i textureCoords = { tileIndex % m_tilesetCols, tileIndex / m_tilesetCols };
                        writeTileQuad(tri,
                            baseX, baseY, baseX + (float)m_tilesize.x, baseY + (float)m_tilesize.y,
                            (float)(textureCoords.x * (int)m_tilesize.x), (float)(textureCoords.y * (int)m_tilesize.y),
                            (float)((textureCoords.x + 1) * (int)m_tilesize.x), (float)((textureCoords.y + 1) * (int)m_tilesize.y),
                            sf::Color::White);
                    }
                    clearTileVerts(&m_vertices[index * 12 + 6], 6);
                    const int frame = Cave::Entity::Genom::frameFromBgTexture(prevTex);
                    pushGenomLayers(entity, ghostLook, frame, baseX, baseY,
                        entity.getPreviousPosition(), entity.getPreviousTextureCoords(), index);
                }
                else {
                    // Stationary Genom: full floor under the stack.
                    writeTileQuad(tri,
                        baseX, baseY, baseX + (float)m_tilesize.x, baseY + (float)m_tilesize.y,
                        (float)(floorCoords.x * (int)m_tilesize.x), (float)(floorCoords.y * (int)m_tilesize.y),
                        (float)((floorCoords.x + 1) * (int)m_tilesize.x), (float)((floorCoords.y + 1) * (int)m_tilesize.y),
                        sf::Color::White);
                    clearTileVerts(&m_vertices[index * 12 + 6], 6);
                    const int frame = Cave::Entity::Genom::frameFromBgTexture(entity.getCurrentTextureIndex());
                    pushGenomLayers(entity, entity.spawnCredit, frame, baseX, baseY,
                        entity.getCurrentPosition(), entity.getCurrentTextureCoords(), index);
                }
                // Hauler spit onto this Genom: cosmic shell is suppressed — draw bubble +
                // body here so deposit matches other bubbled monsters.
                if (haulerSpittingOnto && genomInto) {
                    const auto td = entity.getTransitionDirection();
                    const int viewW = gridRange.size.x;
                    int ni = -1;
                    float ox = 0.f, oy = 0.f;
                    if (td == Cave::Entity::Direction::DOWN) {
                        ni = index - viewW; oy = -(float)m_tilesize.y;
                    }
                    else if (td == Cave::Entity::Direction::UP) {
                        ni = index + viewW; oy = (float)m_tilesize.y;
                    }
                    else if (td == Cave::Entity::Direction::LEFT) {
                        ni = index + 1; ox = (float)m_tilesize.x;
                    }
                    else if (td == Cave::Entity::Direction::RIGHT) {
                        ni = index - 1; ox = -(float)m_tilesize.x;
                    }
                    if (ni >= 0 && ni < cellCount) {
                        const auto& hauler = entities[static_cast<size_t>(ni)];
                        if (Cave::Entity::hiveHaulerIsDepositAwayBubbled(hauler)) {
                            using B = Cave::Entity::HiveHunterBubble;
                            const sf::IntRect ep = entity.getCurrentPosition();
                            const sf::IntRect tp = entity.getCurrentTextureCoords();
                            OverlayQuad q;
                            const int btex = B::FRAME_BASE + B::FRAME_LAST;
                            const sf::Vector2i bc = { btex % m_tilesetCols, btex / m_tilesetCols };
                            q.px1 = baseX + ep.position.x;
                            q.py1 = baseY + ep.position.y;
                            q.px2 = baseX + ep.position.x + ep.size.x;
                            q.py2 = baseY + ep.position.y + ep.size.y;
                            q.tx1 = (float)(bc.x * (int)m_tilesize.x + tp.position.x);
                            q.ty1 = (float)(bc.y * (int)m_tilesize.y + tp.position.y);
                            q.tx2 = (float)(bc.x * (int)m_tilesize.x + tp.position.x + tp.size.x);
                            q.ty2 = (float)(bc.y * (int)m_tilesize.y + tp.position.y + tp.size.y);
                            q.tint = sf::Color::White;
                            q.cellIndex = index;
                            overlays.push_back(q);
                        }
                        const int htex = hauler.getCurrentTextureIndex();
                        if (htex != Cave::Entity::NO_TEXTURE_INDEX) {
                            const sf::Vector2i hc = { htex % m_tilesetCols, htex / m_tilesetCols };
                            OverlayQuad q;
                            q.px1 = baseX + ox;
                            q.py1 = baseY + oy;
                            q.px2 = baseX + ox + (float)m_tilesize.x;
                            q.py2 = baseY + oy + (float)m_tilesize.y;
                            q.tx1 = (float)(hc.x * (int)m_tilesize.x);
                            q.ty1 = (float)(hc.y * (int)m_tilesize.y);
                            q.tx2 = (float)((hc.x + 1) * (int)m_tilesize.x);
                            q.ty2 = (float)((hc.y + 1) * (int)m_tilesize.y);
                            q.tint = sf::Color::White;
                            q.cellIndex = index;
                            overlays.push_back(q);
                        }
                    }
                }
                index++;
                continue;
            }

            // Hive birth: larva slides out under a full matriarch (away wipe would
            // only keep a top strip of her, so the larva would show through).
            const bool hiveBirthAway = entity.getType() == Cave::Entity::Type::HiveMatriarch
                && entity.isAwayTransition()
                && prevTex >= Cave::Entity::HiveLarva::FRAME_BASE
                && prevTex < Cave::Entity::HiveLarva::FRAME_BASE + Cave::Entity::HiveLarva::FRAME_COUNT;

            using HC = Cave::Entity::HiveCollector;
            // Hauler / builder drop: cargo slides out under a full body.
            const bool collectorDropAway = Cave::Entity::isHiveDepositSpitter(entity.getType())
                && entity.isAwayTransition()
                && prevTex != Cave::Entity::NO_TEXTURE_INDEX;
            const bool collectorDropAwayBubbled = collectorDropAway
                && Cave::Entity::hiveHaulerIsDepositAwayBubbled(entity);

            // Vacated Space away-slide stamped with haul so cargo travels with the hauler.
            const bool collectorCargoGhost = entity.isAwayTransition()
                && Cave::Entity::hiveHaulerIsCargoGhost(entity)
                && prevTex != Cave::Entity::NO_TEXTURE_INDEX;
            const int cargoTex = Cave::Entity::hiveHaulerCargoTexture(entity);
            const int cargoLook = Cave::Entity::hiveHaulerCargoLook(entity);
            const bool cargoGenom = cargoLook != 0
                && (cargoLook & Cave::Entity::Genom::LOOK_VALID) != 0
                && Cave::Entity::Genom::isGenomTexture(cargoTex);
            const bool collectorCargo = cargoTex != Cave::Entity::NO_TEXTURE_INDEX
                && cargoTex >= 0
                && Cave::Entity::hiveHaulerIsCarrying(entity);
            const bool collectorCargoBubbled = collectorCargo
                && Cave::Entity::hiveHaulerIsCarryingBubbled(entity);
            const bool collectorGhostBubbled = collectorCargoGhost
                && (entity.spawnCredit == HC::MODE_CARRY_BUBBLED
                    || entity.spawnCredit == Cave::Entity::HiveServant::MODE_CARRY_BUBBLED);

            // Larva into-slide on the cell below also overlaps the matriarch tile and
            // would draw after her — re-stamp a full matriarch on top.
            const int viewW = gridRange.size.x;
            const int aboveIndex = index - viewW;
            const bool hiveBirthInto = entity.getType() == Cave::Entity::Type::HiveLarva
                && entity.isIntoTransition()
                && entity.getTransitionDirection() == Cave::Entity::Direction::DOWN
                && aboveIndex >= 0
                && entities[static_cast<size_t>(aboveIndex)].getType() == Cave::Entity::Type::HiveMatriarch
                && entities[static_cast<size_t>(aboveIndex)].isAwayTransition();

            // Deposit may slide any direction out of the collector.
            int collectorDropNeighbor = -1;
            float collectorDropOx = 0.f;
            float collectorDropOy = 0.f;
            if (entity.isIntoTransition()) {
                const auto td = entity.getTransitionDirection();
                auto collectorAwayAt = [&](int ni, Cave::Entity::Direction awayDir) -> bool {
                    if (ni < 0 || ni >= cellCount) return false;
                    const auto& c = entities[static_cast<size_t>(ni)];
                    return Cave::Entity::isHiveDepositSpitter(c.getType())
                        && c.isAwayTransition()
                        && c.getTransitionDirection() == awayDir;
                };
                if (td == Cave::Entity::Direction::DOWN && collectorAwayAt(aboveIndex, td)) {
                    collectorDropNeighbor = aboveIndex;
                    collectorDropOy = -(float)m_tilesize.y;
                }
                else if (td == Cave::Entity::Direction::UP
                    && collectorAwayAt(index + viewW, td)) {
                    collectorDropNeighbor = index + viewW;
                    collectorDropOy = (float)m_tilesize.y;
                }
                else if (td == Cave::Entity::Direction::LEFT
                    && collectorAwayAt(index + 1, td)) {
                    collectorDropNeighbor = index + 1;
                    collectorDropOx = (float)m_tilesize.x;
                }
                else if (td == Cave::Entity::Direction::RIGHT
                    && collectorAwayAt(index - 1, td)) {
                    collectorDropNeighbor = index - 1;
                    collectorDropOx = -(float)m_tilesize.x;
                }
            }
            const bool collectorDropInto = collectorDropNeighbor >= 0;

            auto writeLayer = [&](sf::Vertex* dest, int tex, const sf::IntRect& ep, const sf::IntRect& tp) {
                if (tex == Cave::Entity::NO_TEXTURE_INDEX) {
                    clearTileVerts(dest, 6);
                    return;
                }
                const sf::Vector2i textureCoords = { tex % m_tilesetCols, tex / m_tilesetCols };
                const float px1 = baseX + ep.position.x;
                const float py1 = baseY + ep.position.y;
                const float px2 = baseX + ep.position.x + ep.size.x;
                const float py2 = baseY + ep.position.y + ep.size.y;
                const float tx1 = textureCoords.x * m_tilesize.x + tp.position.x;
                const float ty1 = textureCoords.y * m_tilesize.y + tp.position.y;
                const float tx2 = textureCoords.x * m_tilesize.x + tp.position.x + tp.size.x;
                const float ty2 = textureCoords.y * m_tilesize.y + tp.position.y + tp.size.y;
                writeTileQuad(dest, px1, py1, px2, py2, tx1, ty1, tx2, ty2, tintForTex(tex));
            };

            auto pushSpriteOverlay = [&](float mx, float my, int spriteTex, int cellIndex,
                const sf::IntRect& ep = { { 0, 0 }, { 32, 32 } },
                const sf::IntRect& tp = { { 0, 0 }, { 32, 32 } }) {
                if (spriteTex == Cave::Entity::NO_TEXTURE_INDEX) return;
                const sf::Vector2i textureCoords = { spriteTex % m_tilesetCols, spriteTex / m_tilesetCols };
                OverlayQuad q;
                q.px1 = mx + (float)ep.position.x;
                q.py1 = my + (float)ep.position.y;
                q.px2 = mx + (float)ep.position.x + (float)ep.size.x;
                q.py2 = my + (float)ep.position.y + (float)ep.size.y;
                q.tx1 = (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x);
                q.ty1 = (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y);
                q.tx2 = (float)(textureCoords.x * (int)m_tilesize.x + tp.position.x + tp.size.x);
                q.ty2 = (float)(textureCoords.y * (int)m_tilesize.y + tp.position.y + tp.size.y);
                q.tint = sf::Color::White;
                q.cellIndex = cellIndex;
                overlays.push_back(q);
            };

            sf::Vertex* tri2 = &m_vertices[index * 12 + 6];
            if (hiveBirthAway || collectorDropAway) {
                // Cargo/larva away crop underneath; solid body stamped over it.
                const sf::IntRect pep = entity.getPreviousPosition();
                const sf::IntRect ptp = entity.getPreviousTextureCoords();
                const int dropLook = Cave::Entity::hiveHaulerCargoLook(entity);
                const bool dropGenom = dropLook != 0
                    && Cave::Entity::Genom::isGenomTexture(prevTex);
                if (dropGenom) {
                    const int frame = Cave::Entity::Genom::frameFromBgTexture(prevTex);
                    clearTileVerts(tri, 6);
                    pushGenomLayers(entity, dropLook, frame, baseX, baseY, pep, ptp, index);
                }
                else {
                    writeLayer(tri, prevTex, pep, ptp);
                }
                clearTileVerts(tri2, 6);
                if (collectorDropAwayBubbled) {
                    using B = Cave::Entity::HiveHunterBubble;
                    pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, pep, ptp);
                }
                pushSpriteOverlay(baseX, baseY, entity.getCurrentTextureIndex(), index);
            }
            else if (collectorCargoGhost) {
                // Away half of a carrying move: space fills the tile, haul+body slide out together.
                const sf::IntRect pep = entity.getPreviousPosition();
                const sf::IntRect ptp = entity.getPreviousTextureCoords();
                writeLayer(tri, entity.getCurrentTextureIndex(),
                    entity.getCurrentPosition(), entity.getCurrentTextureCoords());
                clearTileVerts(tri2, 6);
                if (cargoGenom) {
                    const int frame = Cave::Entity::Genom::frameFromBgTexture(cargoTex);
                    pushGenomLayers(entity, cargoLook, frame, baseX, baseY, pep, ptp, index);
                    if (collectorGhostBubbled) {
                        using B = Cave::Entity::HiveHunterBubble;
                        pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, pep, ptp);
                    }
                    pushSpriteOverlay(baseX, baseY, prevTex, index, pep, ptp);
                }
                else if (collectorGhostBubbled) {
                    using B = Cave::Entity::HiveHunterBubble;
                    pushSpriteOverlay(baseX, baseY, cargoTex, index, pep, ptp);
                    pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, pep, ptp);
                    pushSpriteOverlay(baseX, baseY, prevTex, index, pep, ptp);
                }
                else {
                    writeLayer(tri2, cargoTex, pep, ptp);
                    pushSpriteOverlay(baseX, baseY, prevTex, index, pep, ptp);
                }
            }
            else if (collectorCargo) {
                // Keep the transition remainder filled (prev layer) so the editor
                // background never shows through as grey streak tiles.
                const sf::IntRect ep = entity.getCurrentPosition();
                const sf::IntRect tp = entity.getCurrentTextureCoords();
                const sf::IntRect pep = entity.getPreviousPosition();
                const sf::IntRect ptp = entity.getPreviousTextureCoords();
                const int bodyTex = entity.getCurrentTextureIndex();
                const int cargoFrame = cargoGenom
                    ? Cave::Entity::Genom::frameFromBgTexture(cargoTex) : 0;
                // Haul stack (cargo + optional bubble + body) always shares one crop so the
                // shell stays glued under the collector while walking / during into.
                auto pushBubbledHaul = [&](const sf::IntRect& rp, const sf::IntRect& rt) {
                    using B = Cave::Entity::HiveHunterBubble;
                    if (cargoGenom)
                        pushGenomLayers(entity, cargoLook, cargoFrame, baseX, baseY, rp, rt, index);
                    else
                        pushSpriteOverlay(baseX, baseY, cargoTex, index, rp, rt);
                    pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, rp, rt);
                    pushSpriteOverlay(baseX, baseY, bodyTex, index, rp, rt);
                };
                if (prevTex != Cave::Entity::NO_TEXTURE_INDEX) {
                    if (cargoGenom && Cave::Entity::Genom::isGenomTexture(prevTex)) {
                        const int prevFrame = Cave::Entity::Genom::frameFromBgTexture(prevTex);
                        pushGenomLayers(entity, cargoLook, prevFrame, baseX, baseY, pep, ptp, index);
                        clearTileVerts(tri, 6);
                    }
                    else {
                        writeLayer(tri, prevTex, pep, ptp);
                    }
                    // Eat only: package remnant still shows its shell (prevTex is the cargo).
                    // Walk-into Space must NOT get a trailing bubble on the empty wedge.
                    if (collectorCargoBubbled && prevTex == cargoTex) {
                        using B = Cave::Entity::HiveHunterBubble;
                        pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, pep, ptp);
                    }
                    clearTileVerts(tri2, 6);
                    if (collectorCargoBubbled)
                        pushBubbledHaul(ep, tp);
                    else if (cargoGenom) {
                        pushGenomLayers(entity, cargoLook, cargoFrame, baseX, baseY, ep, tp, index);
                        pushSpriteOverlay(baseX, baseY, bodyTex, index, ep, tp);
                    }
                    else {
                        writeLayer(tri2, cargoTex, ep, tp);
                        pushSpriteOverlay(baseX, baseY, bodyTex, index, ep, tp);
                    }
                }
                else if (collectorCargoBubbled) {
                    if (cargoGenom) {
                        clearTileVerts(tri, 6);
                        pushBubbledHaul(ep, tp);
                    }
                    else {
                        writeLayer(tri, cargoTex, ep, tp);
                        clearTileVerts(tri2, 6);
                        using B = Cave::Entity::HiveHunterBubble;
                        pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, ep, tp);
                        pushSpriteOverlay(baseX, baseY, bodyTex, index, ep, tp);
                    }
                }
                else if (cargoGenom) {
                    clearTileVerts(tri, 6);
                    clearTileVerts(tri2, 6);
                    pushGenomLayers(entity, cargoLook, cargoFrame, baseX, baseY, ep, tp, index);
                    pushSpriteOverlay(baseX, baseY, bodyTex, index, ep, tp);
                }
                else {
                    // Idle haul: cargo under, collector body in the window layer — no overlay.
                    writeLayer(tri, cargoTex, ep, tp);
                    writeLayer(tri2, bodyTex, ep, tp);
                }
            }
            else {
                const sf::IntRect ep = entity.getCurrentPosition();
                const sf::IntRect tp = entity.getCurrentTextureCoords();
                const sf::IntRect pep = entity.getPreviousPosition();
                const sf::IntRect ptp = entity.getPreviousTextureCoords();
                const bool matriarchEatBubbled = entity.getType() == Cave::Entity::Type::HiveMatriarch
                    && entity.isIntoTransition()
                    && Cave::Entity::HiveMatriarch::isEatingBubbled(entity)
                    && prevTex != Cave::Entity::NO_TEXTURE_INDEX;
                if (matriarchEatBubbled) {
                    // Prey fills the remainder with its shell; matriarch body rides the into wedge.
                    const int mealLook = (entity.homeIndex != -1
                        && (entity.homeIndex & Cave::Entity::Genom::LOOK_VALID) != 0)
                        ? entity.homeIndex : 0;
                    if (mealLook != 0 && Cave::Entity::Genom::isGenomTexture(prevTex)) {
                        const int frame = Cave::Entity::Genom::frameFromBgTexture(prevTex);
                        clearTileVerts(tri, 6);
                        pushGenomLayers(entity, mealLook, frame, baseX, baseY, pep, ptp, index);
                    }
                    else {
                        writeLayer(tri, prevTex, pep, ptp);
                    }
                    clearTileVerts(tri2, 6);
                    using B = Cave::Entity::HiveHunterBubble;
                    pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index, pep, ptp);
                    pushSpriteOverlay(baseX, baseY, entity.getCurrentTextureIndex(), index, ep, tp);
                }
                else {
                    writeLayer(tri, entity.getCurrentTextureIndex(), ep, tp);
                    writeLayer(tri2, entity.getPreviousTextureIndex(), pep, ptp);
                }
                if (hiveBirthInto) {
                    const auto& matriarch = entities[static_cast<size_t>(aboveIndex)];
                    pushSpriteOverlay(baseX, baseY - (float)m_tilesize.y,
                        matriarch.getCurrentTextureIndex(), index);
                }
                if (collectorDropInto) {
                    const auto& collector = entities[static_cast<size_t>(collectorDropNeighbor)];
                    // Shell rides the cargo into-wedge under the hauler body so it
                    // keeps emerging from inside (cosmic draw is suppressed mid-spit).
                    if (Cave::Entity::hiveHaulerIsDepositAwayBubbled(collector)) {
                        using B = Cave::Entity::HiveHunterBubble;
                        pushSpriteOverlay(baseX, baseY, B::FRAME_BASE + B::FRAME_LAST, index,
                            entity.getCurrentPosition(), entity.getCurrentTextureCoords());
                    }
                    pushSpriteOverlay(baseX + collectorDropOx, baseY + collectorDropOy,
                        collector.getCurrentTextureIndex(), index);
                }
            }

            index++;
        }
    }

    // Interleave Genom layers after their owning cell so later tiles (portals, etc.)
    // still draw on top — matching normal monster slides instead of always-on-top overlays.
    std::vector<sf::Vertex> built;
    built.reserve(static_cast<std::size_t>(cellCount) * 12
        + overlays.size() * 6
        + static_cast<std::size_t>(wormSlideCount) * 6);
    std::size_t oi = 0;
    for (int i = 0; i < cellCount; ++i) {
        const sf::Vertex* src = &m_vertices[static_cast<std::size_t>(i) * 12];
        built.insert(built.end(), src, src + 12);
        while (oi < overlays.size() && overlays[oi].cellIndex == i) {
            const OverlayQuad& q = overlays[oi++];
            const std::size_t at = built.size();
            built.resize(at + 6);
            writeTileQuad(&built[at],
                q.px1, q.py1, q.px2, q.py2, q.tx1, q.ty1, q.tx2, q.ty2, q.tint);
        }
    }
    for (int i = 0; i < wormSlideCount; ++i) {
        const WormSlide& slide = wormSlides[i];
        const std::size_t at = built.size();
        built.resize(at + 6);
        if (slide.tileIndex == Cave::Entity::NO_TEXTURE_INDEX) {
            clearTileVerts(&built[at], 6);
            continue;
        }
        const sf::Vector2i textureCoords = { slide.tileIndex % m_tilesetCols, slide.tileIndex / m_tilesetCols };
        const float tw = (float)m_tilesize.x;
        const float th = (float)m_tilesize.y;
        writeTileQuad(&built[at],
            slide.px, slide.py, slide.px + tw, slide.py + th,
            (float)(textureCoords.x * (int)m_tilesize.x), (float)(textureCoords.y * (int)m_tilesize.y),
            (float)((textureCoords.x + 1) * (int)m_tilesize.x), (float)((textureCoords.y + 1) * (int)m_tilesize.y),
            slide.tint);
    }
    m_vertices.resize(built.size());
    for (std::size_t i = 0; i < built.size(); ++i)
        m_vertices[i] = built[i];
}

void Renderer::TileRenderer::updateLoadingTexture(const std::vector<bool>& loaded, const sf::Vector2i& position, const sf::IntRect& gridRange) {
    int index = 0;
    m_vertices.resize(gridRange.size.x * gridRange.size.y * 6);
    for (int y = gridRange.position.y; y < gridRange.position.y + gridRange.size.y; ++y) {
        for (int x = gridRange.position.x; x < gridRange.position.x + gridRange.size.x; ++x) {
            float px1 = x * m_tilesize.x + position.x;
            float py1 = y * m_tilesize.y + position.y;
            float px2 = x * m_tilesize.x + position.x + m_tilesize.x;
            float py2 = y * m_tilesize.y + position.y + m_tilesize.y;
            float tx1 = 0;
            float ty1 = 0;
            float tx2 = m_tilesize.x;
            float ty2 = m_tilesize.y;
            if (loaded[index]) {
                tx1 += 32;
                tx2 += 32;
            }

            // Pointer to the 6 vertices for this tile
            sf::Vertex* tri = &m_vertices[index * 6];

            // First triangle
            tri[0].texCoords = { tx1, ty1 };
            tri[1].texCoords = { tx2, ty1 };
            tri[2].texCoords = { tx2, ty2 };

            // Second triangle
            tri[3].texCoords = { tx1, ty1 };
            tri[4].texCoords = { tx2, ty2 };
            tri[5].texCoords = { tx1, ty2 };

            // First triangle
            tri[0].position = { px1, py1 };
            tri[1].position = { px2, py1 };
            tri[2].position = { px2, py2 };

            // Second triangle
            tri[3].position = { px1, py1 };
            tri[4].position = { px2, py2 };
            tri[5].position = { px1, py2 };

            index++;
        }
    }
}

void Renderer::TileRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    states.transform *= getTransform();
    states.texture = &m_tileset;
    target.draw(m_vertices, states);
}

void Renderer::TileRenderer::render(sf::RenderTarget& target, sf::RenderStates states) const {
    states.transform *= getTransform();
    states.texture = &m_tileset;
    target.draw(m_vertices, states);
}
