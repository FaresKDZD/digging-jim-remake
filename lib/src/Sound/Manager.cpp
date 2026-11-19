#include "Sound/Manager.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>

Sound::Manager::Manager() {}

void Sound::Manager::loadAllSounds(bool includeMusic) {
    if (!m_buffers.empty())
        return;

    // Load sound effects
    loadSound(Sound::Effect::Amoeba, "./assets/sounds/Effect/amoeba.wav");
    loadSound(Sound::Effect::Break, "./assets/sounds/Effect/break.wav");
    loadSound(Sound::Effect::CaveGullExplosion, "./assets/sounds/Effect/cave_gull_explosion.wav");
    loadSound(Sound::Effect::Collect, "./assets/sounds/Effect/collect.wav");
    loadSound(Sound::Effect::DiamondDrop, "./assets/sounds/Effect/diamond_drop.wav");
    loadSound(Sound::Effect::DiamondLand, "./assets/sounds/Effect/diamond_land.wav");
    loadSound(Sound::Effect::Dig, "./assets/sounds/Effect/dig.wav");
    loadSound(Sound::Effect::Drop, "./assets/sounds/Effect/drop.wav");
    loadSound(Sound::Effect::Explosion, "./assets/sounds/Effect/explosion.wav");
    loadSound(Sound::Effect::Land, "./assets/sounds/Effect/land.wav");
    loadSound(Sound::Effect::MagicWall, "./assets/sounds/Effect/magic_wall.wav");
    loadSound(Sound::Effect::Open, "./assets/sounds/Effect/open.wav");
    loadSound(Sound::Effect::Plasma, "./assets/sounds/Effect/plasma.wav");
    loadSound(Sound::Effect::EggHatch, "./assets/sounds/Effect/egg_hatch.wav");
    loadSound(Sound::Effect::Chum, "./assets/sounds/Effect/plasma.wav");
    loadSound(Sound::Effect::Enrage, "./assets/sounds/Effect/enrage.wav");
    loadSound(Sound::Effect::Haze, "./assets/sounds/Effect/haze.wav");
    loadSound(Sound::Effect::Inflate, "./assets/sounds/Effect/inflate.wav");
    loadSound(Sound::Effect::Deflate, "./assets/sounds/Effect/deflate.wav");
    loadSound(Sound::Effect::Tube, "./assets/sounds/Effect/tube.wav");
    loadSound(Sound::Effect::Unlock, "./assets/sounds/Effect/unlock.wav");
    loadSound(Sound::Effect::Yahoo, "./assets/sounds/Effect/yahoo.wav");
    loadSound(Sound::Effect::Yippee, "./assets/sounds/Effect/yippee.wav");
    loadSound(Sound::Effect::Wawa, "./assets/sounds/Effect/wawa.wav");
    loadSound(Sound::Effect::Agent, "./assets/sounds/Effect/agent.wav");
    loadSound(Sound::Effect::Plant, "./assets/sounds/Effect/plant.wav");
    loadSound(Sound::Effect::Bubbles, "./assets/sounds/Effect/bubbles.wav");

    if (includeMusic)
        loadMusic(Sound::Music::MainMenu, "./assets/sounds/Music/main_menu.wav");
}

void Sound::Manager::loadSound(const Sound::Effect& effect, const std::string& filename) {
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(filename)) {
        throw std::runtime_error("Error: Unable to load sound: " + filename + "\n");
    }

    m_buffers[effect] = buffer;

    auto sound = std::make_unique<sf::Sound>(buffer);
    sound->setBuffer(m_buffers[effect]);

    m_sounds[effect] = std::move(sound);
}

void Sound::Manager::loadMusic(const Sound::Music& music, const std::string& filename) {
    auto track = std::make_unique<sf::Music>();
    if (!track->openFromFile(filename)) {
        throw std::runtime_error("Error: Unable to load music: " + filename + "\n");
    }

    m_music[music] = std::move(track);
}

void Sound::Manager::play(const Sound::Effect& effect, float pitch) {
    if (m_playedThisFrame.count(effect)) return;
    m_playedThisFrame.insert(effect);
    m_fadeOuts.erase(effect);

    auto bufIt = m_buffers.find(effect);
    if (bufIt == m_buffers.end()) return;

    auto sound = std::make_unique<sf::Sound>(bufIt->second);
    sound->setLooping(false);
    float vol = m_volume / 2.f;
    // Bubbles sample peaks hot next to other FX.
    if (effect == Sound::Effect::Bubbles)
        vol *= 0.55f;
    sound->setVolume(vol);
    sound->setPitch(pitch);
    sound->play();

    m_activeSounds.push_back(std::move(sound));
}

void Sound::Manager::loop(const Sound::Effect& effect) {
    auto it = m_sounds.find(effect);
    if (it != m_sounds.end()) {
        it->second->setLooping(true);
        it->second->setVolume(m_volume/2);
        it->second->play();
    }
}

void Sound::Manager::applyVolumeToEffect(const Sound::Effect& effect, float volume) {
    auto it = m_sounds.find(effect);
    if (it != m_sounds.end())
        it->second->setVolume(volume);
    auto bufIt = m_buffers.find(effect);
    if (bufIt == m_buffers.end()) return;
    const sf::SoundBuffer* buf = &bufIt->second;
    for (auto& s : m_activeSounds) {
        if (s && &s->getBuffer() == buf)
            s->setVolume(volume);
    }
}

void Sound::Manager::stop(const Sound::Effect& effect) {
    m_fadeOuts.erase(effect);
    auto it = m_sounds.find(effect);
    if (it != m_sounds.end()) {
        it->second->stop();
    }
    // One-shots from play() live in m_activeSounds — stop those too.
    auto bufIt = m_buffers.find(effect);
    if (bufIt == m_buffers.end()) return;
    const sf::SoundBuffer* buf = &bufIt->second;
    for (auto& s : m_activeSounds) {
        if (s && &s->getBuffer() == buf)
            s->stop();
    }
}

void Sound::Manager::fadeOut(const Sound::Effect& effect, float durationSeconds) {
    if (!isPlaying(effect)) {
        m_fadeOuts.erase(effect);
        return;
    }
    if (m_fadeOuts.count(effect))
        return; // already fading

    float startVol = m_volume / 2.f;
    auto it = m_sounds.find(effect);
    if (it != m_sounds.end()
        && it->second->getStatus() == sf::Sound::Status::Playing)
        startVol = it->second->getVolume();
    else {
        auto bufIt = m_buffers.find(effect);
        if (bufIt != m_buffers.end()) {
            const sf::SoundBuffer* buf = &bufIt->second;
            for (const auto& s : m_activeSounds) {
                if (s && &s->getBuffer() == buf
                    && s->getStatus() == sf::Sound::Status::Playing) {
                    startVol = s->getVolume();
                    break;
                }
            }
        }
    }

    FadeOutState fade;
    fade.duration = std::max(0.05f, durationSeconds);
    fade.elapsed = 0.f;
    fade.startVolume = startVol;
    m_fadeOuts[effect] = fade;
}

bool Sound::Manager::isPlaying(const Sound::Effect& effect) const {
    auto it = m_sounds.find(effect);
    if (it != m_sounds.end()) {
        if (it->second->getStatus() == sf::Sound::Status::Playing)
            return true;
    }
    auto bufIt = m_buffers.find(effect);
    if (bufIt != m_buffers.end()) {
        const sf::SoundBuffer* buf = &bufIt->second;
        for (const auto& s : m_activeSounds) {
            if (s && &s->getBuffer() == buf
                && s->getStatus() == sf::Sound::Status::Playing)
                return true;
        }
    }
    return false;
}

void Sound::Manager::playMusic(const Sound::Music& music) {
    auto it = m_music.find(music);
    if (it == m_music.end()) {
        return;
    }

    // Stop currently playing track, if any
    if (m_currentMusic && m_currentMusic->getStatus() == sf::Music::Status::Playing) {
        m_currentMusic->stop();
    }

    m_currentMusic = it->second.get();
    m_currentMusic->play();
}

void Sound::Manager::stopMusic() {
    if (m_currentMusic && m_currentMusic->getStatus() == sf::Music::Status::Playing) {
        m_currentMusic->stop();
    }
    m_currentMusic = nullptr;
}

void Sound::Manager::setVolume(const float& volume) {
    m_volume = volume;
}

void Sound::Manager::update() {
    m_playedThisFrame.clear();

    if (!m_fadeClockStarted) {
        m_fadeClock.restart();
        m_fadeClockStarted = true;
    }
    const float dt = m_fadeClock.restart().asSeconds();

    std::vector<Sound::Effect> finishedFades;
    for (auto& [effect, fade] : m_fadeOuts) {
        fade.elapsed += dt;
        const float t = std::clamp(fade.elapsed / fade.duration, 0.f, 1.f);
        applyVolumeToEffect(effect, fade.startVolume * (1.f - t));
        if (t >= 1.f)
            finishedFades.push_back(effect);
    }
    for (Sound::Effect effect : finishedFades) {
        stop(effect); // also erases m_fadeOuts
    }

    m_activeSounds.erase(
        std::remove_if(
            m_activeSounds.begin(),
            m_activeSounds.end(),
            [](const std::unique_ptr<sf::Sound>& s) {
        return s->getStatus() == sf::Sound::Status::Stopped;
    }),
        m_activeSounds.end()
    );
}