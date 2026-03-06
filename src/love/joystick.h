#ifndef SRC_LOVE_JOYSTICK_H_
#define SRC_LOVE_JOYSTICK_H_

#include <string>
#include <vector>

#include "Types/Input/Joystick.h"
#include "SDL2/SDL.h"

using love::Types::Input::Joystick;

namespace love {

/**
 * Provides an interface to connected joysticks.
 */
class joystick {
	public:
	~joystick();
	void load();
	void update();

	/**
	 * Gets a list of connected Joysticks.
	 *
	 * @return Gets a list of connected Joysticks.
	 */
	std::vector<Joystick*>& getJoysticks();

	/**
	 * Gets the number of connected joysticks.
	 *
	 * @return The number of connected joysticks.
	 */
	int getJoystickCount();

	void unload();

	/**
	 * Checks if a button number is pressed on a joystick.
	 *
	 * @param joystick The joystick to be checked.
	 * @param button The button to be checked.
	 *
	 * @return True if the joystick button is pressed.
	 */
	bool isDown(int joystick, int button);

	/**
	 * Checks if a button is pressed on a joystick.
	 *
	 * ## Example
	 *
	 * @code
	 * if (love.joystick.isDown(0, "left")) {
	 *   love.graphics.print("Joystick is pushed LEFT", 100, 100)
	 * }
	 * @endcode
	 *
	 * @param joystick The joystick to be checked.
	 * @param button The button to be checked.
	 *
	 *
	 * @return True if the joystick button is pressed.
	 */
	bool isDown(int joystick, const std::string& button);

	/**
	 * Retrieve the given joystick.
	 *
	 * ## Example
	 *
	 * @code
	 * if (love.joystick[0].isDown("left")) {
	 *   love.graphics.print("Joystick is pushed LEFT", 100, 100)
	 * }
	 * @endcode
	 *
	 * @param joystick The joystick index to be retrieved.
	 *
	 * @return The joystick of the given player number.
	 */
	Joystick* operator[](int joystick);

	int getButtonKey(const std::string& name);
	std::string getButtonName(int key);

	/**
	 * Converts an axis index to its name.
	 *
	 * @param axis The axis index.
	 *
	 * @return The name of the axis.
	 */
	std::string getAxisName(int axis);

	/**
	 * Converts an axis name to its index.
	 *
	 * @param name The axis name.
	 *
	 * @return The axis index, or -1 if not found.
	 */
	int getAxisKey(const std::string& name);

	/**
	 * Gets the position of an axis on a joystick.
	 *
	 * @param joystick The joystick index.
	 * @param axis The axis index (0=left X, 1=left Y, 2=right X, 3=right Y).
	 *
	 * @return The current position of the axis, normalized to [-1.0, 1.0].
	 */
	float getAxis(int joystick, int axis);

	/**
	 * Gets the position of an axis on a joystick by name.
	 *
	 * @param joystick The joystick index.
	 * @param axis The axis name ("leftx", "lefty", "rightx", "righty").
	 *
	 * @return The current position of the axis, normalized to [-1.0, 1.0].
	 */
	float getAxis(int joystick, const std::string& axis);

private:
	std::vector<Joystick*> m_joysticks;
};

}  // namespace love

#endif  // SRC_LOVE_JOYSTICK_H_
