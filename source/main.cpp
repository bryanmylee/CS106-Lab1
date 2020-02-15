#include "MicroBit.h"

MicroBit uBit;

// MARK 1: Question 1
class ExtendedButton {
    private:
        unsigned long lastPressed;
    public:
        unsigned long repeatDelay;
        MicroBitButton *uButton;

        ExtendedButton(MicroBitButton *uButton, unsigned long repeatDelay) {
            this->uButton = uButton;
            this->repeatDelay = repeatDelay;
            lastPressed = uBit.systemTime();
        }

        bool onPress() {
            if (uButton->isPressed() && uBit.systemTime() - lastPressed > repeatDelay) {
                lastPressed = uBit.systemTime();
                return true;
            }
            /*
             * If the key is lifted before the key repeat delay is over, then reset the key event
             * by shifting the lastPressed back by KEY_REPEAT_DELAY
             */
            if (!uButton->isPressed() && uBit.systemTime() - lastPressed < repeatDelay) {
                lastPressed = uBit.systemTime() - repeatDelay;
            }
            return false;
        }
};

class TimeForEverything {
    private:
        ExtendedButton buttonA = ExtendedButton(&uBit.buttonA, 500);
        ExtendedButton buttonB = ExtendedButton(&uBit.buttonB, 500);

        void countdown(int start) {
            while (start > 0) {
                uBit.sleep(1000);
                start--;
                uBit.display.print(start);
            }
        }
    public:
        void run() {
            int x = 5;

            while (1) {
                uBit.display.print(x);
                if (buttonA.onPress()) {
                    if (x > 1) x--;
                }
                if (buttonB.onPress()) {
                    if (x < 9) x++;
                }
                if (uBit.buttonAB.isPressed()) {
                    countdown(x);
                    break;
                }
            }
        }
} timeForEverything;


// MARK 2: Question 2
/* 
 * The margin of error for what we define as vertical.
 *   0   : perfectly horizontal
 *   1000: perfectly vertical
 */
#define HORIZONTAL_MARGIN 600
// Defines how many consecutive clean data points are captured before we register a change in verticality
#define ORIENTATION_SENS 20
#define SQRT3 1.7320508

enum Orientation { HORIZONTAL = 0, VERTICAL = 1 };

class Math {
    public:
        static int abs(int x) {
            return x >= 0 ? x : -x;
        }

        static double arctan(double x) {
            if (x < 0) return -arctan(-x);
            if (x > 1) return PI / 2 - arctan(1 / x);
            if (x > 2 - SQRT3) return PI / 6 + arctan((SQRT3 * x - 1) / (SQRT3 + x));
            return x - x * x * x / 3 + x * x * x * x * x / 5;
        }

        static double getRadians(int x, int y) {
            /*
             * If x ~= 0, then y / x becomes very large.
             * To avoid that, we take the smaller ratio, and offset from PI / 2 instead.
             */
            if (y >= 0) {
                if (x >= y) {
                    return arctan((double) y / x);
                }
                return PI / 2 - arctan((double) x / y);
            }
            // tan has a periodicity of PI, so we have to offset by PI to get the full 2PI radians.
            return PI + getRadians(-x, -y);
        }

        static double getDegrees(int x, int y) {
            return getRadians(x, y) / PI * 180;
        }

    private:
        Math() {}
};

class OrientationManager {
    private:
        Orientation orientationReal = HORIZONTAL;
        int changedCount = 0;

        /* 
         * We can check for orientation by checking the magnitude of the vector <z>
         *      0: perfectly vertical
         *   1023: perfectly horizontal
         */
        Orientation getOrientationRaw() {
            if (Math::abs(uBit.accelerometer.getZ()) > HORIZONTAL_MARGIN) {
                return HORIZONTAL;
            }
            return VERTICAL;
        }
    public:
        /*
         * Wait until we get VERT_SENS data points before registering a change in orientation
         *
         * @param (*onChange)() callback function when orientation changes
         */
        Orientation getOrientationBuffered(void (*onChange)()) {
            if (getOrientationRaw() ^ orientationReal) {
                changedCount++;
            } else {
                changedCount = 0;
                return orientationReal;
            }
            if (changedCount > ORIENTATION_SENS) {
                orientationReal = (Orientation) !orientationReal;
                (*onChange)();
            }
            return orientationReal;
        }
} orientationManager;

class VerticalParadox {
    public:
        void run() {
            int degrees = (int) Math::getDegrees(uBit.accelerometer.getX(), uBit.accelerometer.getY());
            int modDegrees = degrees % 360;
            uBit.serial.send(modDegrees);
            uBit.serial.send("\r\n");
        }

        void reset() {
        }
} vertParadox;

class HorizontalParadox {
    public:
        void run() {
            uBit.serial.send("run hori\r\n");
        }

        void reset() {
        }
} horiParadox;

class ParadoxThatDrivesUsAll {
    private:
        static void onOrientationChange() {
            uBit.display.printChar(' ');
            vertParadox.reset();
            horiParadox.reset();
        }
    public:
        void run() {
            while (1) {
                Orientation currOrient = orientationManager.getOrientationBuffered(&onOrientationChange);
                if (currOrient == VERTICAL) {
                    vertParadox.run();
                } else {
                    horiParadox.run();
                }
            }
        }
} paradoxThatDrivesUsAll;

int main() {
    uBit.init();

    paradoxThatDrivesUsAll.run();

    release_fiber();
}
