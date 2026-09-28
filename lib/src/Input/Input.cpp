#include "Input/Input.h"

namespace {

bool isWasd(sf::Keyboard::Scan scan) {
    return scan == sf::Keyboard::Scan::W
        || scan == sf::Keyboard::Scan::A
        || scan == sf::Keyboard::Scan::S
        || scan == sf::Keyboard::Scan::D;
}

bool isJimAction(Input::Action action) {
    return action == Input::Action::MoveUp
        || action == Input::Action::MoveDown
        || action == Input::Action::MoveLeft
        || action == Input::Action::MoveRight
        || action == Input::Action::Collect
        || action == Input::Action::SelfDestruct;
}

}

void Input::System::latchIfJimAction(Action action) {
    if (isJimAction(action)) m_latchedActions.insert(action);
}

void Input::System::handleEvent(const sf::Event& event) {

    if (const auto keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        const bool wasd = isWasd(keyPressed->scancode);
        if (m_ignoreWasd && wasd) return;
        for (auto& [key, action] : m_keyMap) {
            if (keyPressed->scancode == key) {
                m_pressedActions.insert(action);
                if (!m_heldActions.count(action))
                    latchIfJimAction(action);
                m_heldActions.insert(action);
                m_keyboardEventOccurred = true;
            }
        }
    }
    if (const auto keyReleased = event.getIf<sf::Event::KeyReleased>()) {
        const bool wasd = isWasd(keyReleased->scancode);
        if (m_ignoreWasd && wasd) return;
        for (auto& [key, action] : m_keyMap) {
            if (keyReleased->scancode == key) {
                m_heldActions.erase(action);
                m_keyboardEventOccurred = true;
            }
        }
    }
    if (const auto mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        for (const auto& [button, action] : m_mouseMap) {
            if (mousePressed->button == button) {
                m_pressedActions.insert(action);
                m_heldActions.insert(action);
                m_keyboardEventOccurred = true;
            }
        }
    }
    if (const auto mouseReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
        for (const auto& [button, action] : m_mouseMap) {
            if (mouseReleased->button == button) {
                m_heldActions.erase(action);
                m_keyboardEventOccurred = true;
            }
        }
    }
}

void Input::System::handleJoystick() {
    if (!m_joystick || !sf::Joystick::isConnected(m_joystickId) || m_keyboardEventOccurred) return;

    float x = sf::Joystick::getAxisPosition(m_joystickId, m_horizontalAxis);
    float y = sf::Joystick::getAxisPosition(m_joystickId, m_verticalAxis);

    if (x < -m_deadzone) {
        if (!isPressed(Action::MoveLeft)) {
            m_pressedActions.insert(Action::MoveLeft);
            latchIfJimAction(Action::MoveLeft);
        }
        m_heldActions.insert(Action::MoveLeft);
    }
    else m_heldActions.erase(Action::MoveLeft);

    if (x > m_deadzone) {
        if (!isPressed(Action::MoveRight)) {
            m_pressedActions.insert(Action::MoveRight);
            latchIfJimAction(Action::MoveRight);
        }
        m_heldActions.insert(Action::MoveRight);
    }
    else m_heldActions.erase(Action::MoveRight);

    if (y < -m_deadzone) {
        if (!isPressed(Action::MoveUp)) {
            m_pressedActions.insert(Action::MoveUp);
            latchIfJimAction(Action::MoveUp);
        }
        m_heldActions.insert(Action::MoveUp);
    }
    else m_heldActions.erase(Action::MoveUp);

    if (y > m_deadzone) {
        if (!isPressed(Action::MoveDown)) {
            m_pressedActions.insert(Action::MoveDown);
            latchIfJimAction(Action::MoveDown);
        }
        m_heldActions.insert(Action::MoveDown);
    }
    else m_heldActions.erase(Action::MoveDown);

    for (auto& [button, action] : m_joystickButtonMap) {
        if (sf::Joystick::isButtonPressed(m_joystickId, button)) {
            if (!isPressed(action)) {
                m_pressedActions.insert(action);
                latchIfJimAction(action);
            }
            m_heldActions.insert(action);
        }
        else {
            m_heldActions.erase(action);
        }
    }
}

void Input::System::update() {
    bool movementPressed = (isPressed(Input::Action::MoveLeft)
        || isPressed(Input::Action::MoveRight)
        || isPressed(Input::Action::MoveUp)
        || isPressed(Input::Action::MoveDown)
        );
    if (!movementPressed) {
        pushTimer = 0;
    }
    m_pressedActions.clear();
    m_keyboardEventOccurred = false;
}

bool Input::System::isPressed(Input::Action action) const {
    return m_heldActions.count(action);
}

bool Input::System::wasPressed(Input::Action action) const {
    return m_pressedActions.count(action);
}

bool Input::System::gameplayHeld(Input::Action action) const {
    return m_heldActions.count(action) || m_latchedActions.count(action);
}

void Input::System::consumeGameplayLatch() {
    m_latchedActions.clear();
}

void Input::System::increasePushTimer() {
    pushTimer++;
}

bool Input::System::registerPush(unsigned int interval) const {
    if (interval < 1) interval = 1;
    if (pushTimer < interval) {
        return false;
    }
    return pushTimer % interval == 0;
}

void Input::System::setJoystick(const bool& toggle) {
    m_joystick = toggle;
    if (m_joystick) detectJoystick();
}

void Input::System::setIgnoreWasd(bool ignore) {
    if (ignore && !m_ignoreWasd) {
        m_heldActions.erase(Action::MoveUp);
        m_heldActions.erase(Action::MoveDown);
        m_heldActions.erase(Action::MoveLeft);
        m_heldActions.erase(Action::MoveRight);
        m_pressedActions.erase(Action::MoveUp);
        m_pressedActions.erase(Action::MoveDown);
        m_pressedActions.erase(Action::MoveLeft);
        m_pressedActions.erase(Action::MoveRight);
        m_latchedActions.erase(Action::MoveUp);
        m_latchedActions.erase(Action::MoveDown);
        m_latchedActions.erase(Action::MoveLeft);
        m_latchedActions.erase(Action::MoveRight);
    }
    m_ignoreWasd = ignore;
}

void Input::System::detectJoystick() {
    m_joystickId = -1;
    for (unsigned int id = 0; id < sf::Joystick::Count; ++id) {
        if (sf::Joystick::isConnected(id)) {
            m_joystickId = static_cast<int>(id);
            break;
        }
    }
}
