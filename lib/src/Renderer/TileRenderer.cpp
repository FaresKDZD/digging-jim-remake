#include <algorithm>
#include <memory>
#include "Renderer/TileRenderer.h"
#include "Cave/Entity/Base.h"

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
    int index = 0;
    m_vertices.resize(gridRange.size.x * gridRange.size.y * 12);
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
            const sf::Color tint = (entity.getType() == Cave::Entity::Type::Jim) ? jimTint : sf::Color::White;

            int tileIndex = entity.getCurrentTextureIndex();
            if (tileIndex == Cave::Entity::NO_TEXTURE_INDEX) {
                clearTileVerts(tri, 6);
            }
            else {
                const sf::Vector2i textureCoords = { tileIndex % m_tilesetCols, tileIndex / m_tilesetCols };
                const sf::IntRect ep = entity.getCurrentPosition();
                const sf::IntRect tp = entity.getCurrentTextureCoords();
                const float px1 = baseX + ep.position.x;
                const float py1 = baseY + ep.position.y;
                const float px2 = baseX + ep.position.x + ep.size.x;
                const float py2 = baseY + ep.position.y + ep.size.y;
                const float tx1 = textureCoords.x * m_tilesize.x + tp.position.x;
                const float ty1 = textureCoords.y * m_tilesize.y + tp.position.y;
                const float tx2 = textureCoords.x * m_tilesize.x + tp.position.x + tp.size.x;
                const float ty2 = textureCoords.y * m_tilesize.y + tp.position.y + tp.size.y;
                writeTileQuad(tri, px1, py1, px2, py2, tx1, ty1, tx2, ty2, tint);
            }

            tileIndex = entity.getPreviousTextureIndex();
            sf::Vertex* tri2 = &m_vertices[index * 12 + 6];
            if (tileIndex == Cave::Entity::NO_TEXTURE_INDEX) {
                clearTileVerts(tri2, 6);
            }
            else {
                const sf::Vector2i textureCoords = { tileIndex % m_tilesetCols, tileIndex / m_tilesetCols };
                const sf::IntRect ep = entity.getPreviousPosition();
                const sf::IntRect tp = entity.getPreviousTextureCoords();
                const float px1 = baseX + ep.position.x;
                const float py1 = baseY + ep.position.y;
                const float px2 = baseX + ep.position.x + ep.size.x;
                const float py2 = baseY + ep.position.y + ep.size.y;
                const float tx1 = textureCoords.x * m_tilesize.x + tp.position.x;
                const float ty1 = textureCoords.y * m_tilesize.y + tp.position.y;
                const float tx2 = textureCoords.x * m_tilesize.x + tp.position.x + tp.size.x;
                const float ty2 = textureCoords.y * m_tilesize.y + tp.position.y + tp.size.y;
                writeTileQuad(tri2, px1, py1, px2, py2, tx1, ty1, tx2, ty2, tint);
            }

            index++;
        }
    }
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