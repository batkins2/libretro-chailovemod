#include "Joystick.h"
#include "SDL.h"
#include "../../../ChaiLove.h"
#include <string>

namespace love {
namespace Types {
namespace Input {

std::string Joystick::getName() {
	return name;
}

Joystick::Joystick() {
	clearStates();
}

Joystick::Joystick(int i) : m_index(i) {
	clearStates();
}

void Joystick::clearStates() {
	for (int i = 0; i < 14; i++) {
		m_state[i] = 0;
	}
	for (int i = 0; i < 6; i++) {
		m_analogState[i] = 0.0f;
	}
}

bool Joystick::isDown(int button) {
	return static_cast<bool>(m_state[button]);
}

bool Joystick::isDown(const std::string& button) {
	int key = ChaiLove::getInstance()->joystick.getButtonKey(button);
	return isDown(key);
}

bool Joystick::isConnected() {
	return m_connected;
}

int Joystick::getID() {
	return m_index;
}

float Joystick::getAxis(int axis) {
	if (axis < 0 || axis >= 6) {
		return 0.0f;
	}
	return m_analogState[axis];
}

float Joystick::getAxis(const std::string& axis) {
	if (axis == "leftx") {
		return m_analogState[0];
	} else if (axis == "lefty") {
		return m_analogState[1];
	} else if (axis == "rightx") {
		return m_analogState[2];
	} else if (axis == "righty") {
		return m_analogState[3];
	} else if (axis == "triggerleft" || axis == "lefttrigger") {
		return m_analogState[4];
	} else if (axis == "triggerright" || axis == "righttrigger") {
		return m_analogState[5];
	}
	return 0.0f;
}

void Joystick::update() {
	if (!isConnected()) {
		return;
	}

	int16_t state;
	// Loop through each button.
	for (int u = 0; u < 14; u++) {
		// Retrieve the state of the button.
		state = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_JOYPAD, 0, u);

		// Check if there's a change of state.
		if (m_state[u] != state) {
			m_state[u] = state;

			std::string name = ChaiLove::getInstance()->joystick.getButtonName(u);
			if (state == 1) {
				ChaiLove::getInstance()->script->joystickpressed(m_index, name);
			} else if (state == 0) {
				ChaiLove::getInstance()->script->joystickreleased(m_index, name);
			}
		}
	}

	// Update analog sticks
	// Left stick
	int16_t leftX = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
	int16_t leftY = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
	// Right stick
	int16_t rightX = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
	int16_t rightY = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

	// Convert from int16 range [-32768, 32767] to float range [-1.0, 1.0]
	m_analogState[0] = leftX / 32768.0f;
	m_analogState[1] = leftY / 32768.0f;
	m_analogState[2] = rightX / 32768.0f;
	m_analogState[3] = rightY / 32768.0f;

	// Read trigger values (L2/R2 as analog axes)
	// Note: In libretro, triggers can be read as buttons (L2/R2) or as analog values
	// We read them as buttons here and normalize to [0.0, 1.0] range for trigger semantics
	int16_t leftTrigger = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
	int16_t rightTrigger = ChaiLove::input_state_cb(m_index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);
	// Convert button state (0 or 1) to trigger range [0.0, 1.0]
	m_analogState[4] = leftTrigger > 0 ? 1.0f : 0.0f;
	m_analogState[5] = rightTrigger > 0 ? 1.0f : 0.0f;
}

}  // namespace Input
}  // namespace Types
}  // namespace love
