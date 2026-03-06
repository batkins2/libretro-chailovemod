#ifndef SRC_LOVE_TYPES_INPUT_JOYSTICK_H_
#define SRC_LOVE_TYPES_INPUT_JOYSTICK_H_

#include <string>

namespace love {
namespace Types {
namespace Input {

/**
 * Represents a physical joystick.
 */
class Joystick {
	public:
	Joystick();
	Joystick(int index);

	/**
	 * Gets the name of the joystick.
	 *
	 * @return string The name of the Joystick.
	 */
	std::string getName();

	void clearStates();

	/**
	 * Checks if a button on the Joystick is pressed.
	 *
	 * @param button The index of the button to be checked
	 *
	 * @return bool Whether or not the given button is down.
	 */
	bool isDown(int button);

	/**
	 * Checks if a button on the Joystick is pressed.
	 *
	 * @param button The button to be checked
	 *
	 * @return bool Whether or not the given button is down.
	 */
	bool isDown(const std::string& button);

	void update();

	/**
	 * Gets whether the Joystick is connected.
	 *
	 * @return True if the given joystick is connected.
	 */
	bool isConnected();

	/**
	 * Gets the joystick's unique identifier.
	 *
	 * @return The index number of the Joystick.
	 */
	int getID();

	/**
	 * Gets the position of an analog axis.
	 *
	 * @param axis The axis index (0=left X, 1=left Y, 2=right X, 3=right Y).
	 *
	 * @return The current position of the axis, normalized to [-1.0, 1.0].
	 */
	float getAxis(int axis);

	/**
	 * Gets the position of an analog axis by name.
	 *
	 * @param axis The axis name.
	 *
	 * @return The current position of the axis, normalized to [-1.0, 1.0].
	 */
	float getAxis(const std::string& axis);

private:
	int m_index = 0;
	std::string name = "RetroPad";
	int16_t m_state[14];
	float m_analogState[6]; // left X, left Y, right X, right Y, left trigger, right trigger
	bool m_connected = true;
};

}  // namespace Input
}  // namespace Types
}  // namespace love

#endif  // SRC_LOVE_TYPES_INPUT_JOYSTICK_H_
