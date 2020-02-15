#include "MicroBit.h"

MicroBit uBit;
/* 
 * Initialization of MicroBitImage seems to be a little broken,
 * so we will settle with reusing one image instead.
 */
MicroBitImage im(5, 5);

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
#define SQRT3 1.7320508
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

        /*
         * @return degrees from 0 to 359
         */
        static double getDegrees(int x, int y) {
            return getRadians(x, y) / PI * 180;
        }

        static int realMod(int x, int radix) {
            const int result = x % radix;
            return result >= 0 ? result : result + radix;
        }

        static bool diffSign(int x, int y) {
            return (x ^ y) >> 31;
        }

    private:
        Math() {}
};

struct Coord {
    int x;
    int y;
};

/*
 * Defines how sensitive the dot movement is to tilting
 * Lower values result in more sensitive movement
 */
#define TILT_SENS 100
#define BLINK_DUR 250
#define PERIMETER_LEN 18
#define CHANGE_DIR_SENS 3
/*
 * Constraints movement to either the X_AXIS or Y_AXIS.
 * Movement between the two axis can only occur at (0, 0)
 */
enum TiltAxis { X_AXIS, Y_AXIS };
enum RotationDir { COUNTERCLOCKWISE = -1, CLOCKWISE = 1 };
class HorizontalParadox {
    private:
        TiltAxis currTiltAxis = X_AXIS;
        Coord pos = {0, 0};
        unsigned long lastBlink = uBit.systemTime();
        int nTurns = 0;
        int prevHeading;
        int currHeading = -1; // uninitialized value
        int initialHeading = -1;
        int firstHeading = -1;
        RotationDir currUBitDir;
        int nTurnBacks = 0;

        void drawTilt() {
            unsigned long currTime = uBit.systemTime();
            int brightness;
            if (currTime - lastBlink > 2 * BLINK_DUR) {
                lastBlink = currTime;
            }
            if (currTime - lastBlink > BLINK_DUR) {
                brightness = 255;
            } else {
                brightness = 0;
            }
            im.setPixelValue(2 + pos.x, 2 + pos.y, brightness);
        }

        void drawRotation() {
        }

        void drawComposite() {
            im.clear();
            drawRotation();
            drawTilt();
        }

        /*
         * Account for noise in data by validating resets.
         * If the number of turnbacks > CHANGE_DIR_SENS, then we register a reset
         */
        void dirResetBuffered() {
            if (Math::realMod(currHeading + currUBitDir, PERIMETER_LEN) == prevHeading) {
                nTurnBacks++;
            } else if (Math::realMod(prevHeading + currUBitDir, PERIMETER_LEN) == currHeading) {
                nTurnBacks--;
            }
            if (nTurnBacks < 0) nTurnBacks = 0;
            if (nTurnBacks > CHANGE_DIR_SENS) {
                nTurns = 0;
                nTurnBacks = 0;
            }
        }

        void setCurrUBitDir() {
            if (Math::realMod(firstHeading - initialHeading, PERIMETER_LEN) == 1) {
                currUBitDir = CLOCKWISE;
            } else {
                currUBitDir = COUNTERCLOCKWISE;
            }
        }

        void setFirstHeading() {
            if (firstHeading == -1 && currHeading != initialHeading) {
                firstHeading = currHeading;
            } else if (currHeading == initialHeading) {
                firstHeading = -1;
            }
        }

        void setInitialHeading() {
            if (initialHeading == -1) initialHeading = currHeading;
        }

        void setCurrentHeading() {
            currHeading = uBit.compass.heading() / 20;
        }

        void setPrevHeading() {
            if (currHeading == -1) {
                prevHeading = uBit.compass.heading() / 20;
            } else {
                prevHeading = currHeading;
            }
        }

        void setHeadings() {
            setPrevHeading();
            setCurrentHeading();
            setInitialHeading();
            setFirstHeading();
            setCurrUBitDir();
        }

        void setTilt() {
            Coord posRaw = {
                uBit.accelerometer.getX() / TILT_SENS,
                uBit.accelerometer.getY() / TILT_SENS
            };

            if (currTiltAxis == X_AXIS) {
                if (posRaw.x != 0) {
                    pos.x = posRaw.x;
                    pos.y = 0;
                } else if (posRaw.y != 0) {
                    pos.x = 0;
                    pos.y = posRaw.y;
                    currTiltAxis = Y_AXIS;
                } else {
                    pos.x = 0;
                    pos.y = 0;
                }
            } else {
                if (posRaw.y != 0) {
                    pos.x = 0;
                    pos.y = posRaw.y;
                } else if (posRaw.x != 0) {
                    pos.x = posRaw.x;
                    pos.y = 0;
                    currTiltAxis = X_AXIS;
                } else {
                    pos.x = 0;
                    pos.y = 0;
                }
            }

            if (pos.x > 2) pos.x = 2;
            if (pos.x < -2) pos.x = -2;
            if (pos.y > 2) pos.y = 2;
            if (pos.y < -2) pos.y = -2;

            if (Math::abs(pos.x) == 2 || Math::abs(pos.y) == 2) nTurns = 0;
        }
    public:
        void runFrame() {
            setTilt();
            setHeadings();
            dirResetBuffered();
            drawComposite();
            uBit.display.print(im);
        }

        void reset() {
            pos.x = 0;
            pos.y = 0;
            lastBlink = uBit.systemTime();
            nTurns = 0;
            initialHeading = -1;
            currHeading = -1;
            firstHeading = -1;
        }
} horiParadox;

class VerticalParadox {
    private:
        int currIndex;
        int initialIndex = -1; // uninitialized value
        int firstIndex = -1;
        RotationDir currUBitDir; // direction of the uBit device, not the ring drawn
        bool endRun = false;

        /*
         * 0 represents the LED closest to buttonA.
         * Every subsequent index wraps around the perimeter clockwise.
         */
        Coord getPixelCoord(int index) {
            Coord c;
            switch (index) {
                case 0: c.x = 0; c.y = 2; break;
                case 1: c.x = 0; c.y = 1; break;
                case 2: c.x = 0; c.y = 0; break;
                case 3: c.x = 1; c.y = 0; break;
                case 4: c.x = 2; c.y = 0; break;
                case 5: c.x = 3; c.y = 0; break;
                case 6: c.x = 4; c.y = 0; break;
                case 7: c.x = 4; c.y = 1; break;
                case 8: c.x = 4; c.y = 2; break;
                case 9: c.x = 4; c.y = 3; break;
                case 10: c.x = 4; c.y = 4; break;
                case 11: c.x = 3; c.y = 4; break;
                case 12: c.x = 2; c.y = 4; break;
                case 13: c.x = 1; c.y = 4; break;
                case 14: c.x = 0; c.y = 4; break;
                case 15: c.x = 0; c.y = 3; break;
                case 16: c.x = 0; c.y = 2; break;
                case 17: c.x = 0; c.y = 1; break;
            }
            return c;
        }

        // Set a pixel given an index of the ring around the perimeter.
        void setImagePixel(int index) {
            Coord c = getPixelCoord(index);
            im.setPixelValue(c.x, c.y, 255);
        }

        // Draw the image required to print the ring around the perimeter.
        void drawRing() {
            im.clear();

            int i = initialIndex;
            while (i != currIndex) {
                setImagePixel(i);
                i -= currUBitDir;
                i = Math::realMod(i, PERIMETER_LEN);
            };
            setImagePixel(i);
        }

        void drawCenter() {
            im.clear();
            im.setPixelValue(2, 2, 255);
        }

        void checkEndRun() {
            if (currIndex == Math::realMod(initialIndex + currUBitDir, PERIMETER_LEN)) {
                endRun = true;
            }
        }

        void setCurrUBitDir() {
            if (Math::realMod(firstIndex - initialIndex, PERIMETER_LEN) == 1) {
                currUBitDir = COUNTERCLOCKWISE;
            } else {
                currUBitDir = CLOCKWISE;
            }
        }

        void setFirstIndex() {
            /*
             * Whenever stepping out from the initialIndex, we store 
             * the firstIndex to keep track of our intended direction.
             */
            if (firstIndex == -1 && currIndex != initialIndex) {
                firstIndex = currIndex;
            } else if (currIndex == initialIndex) {
                firstIndex = -1;
            }
        }

        void setInitialIndex() {
            if (initialIndex == -1) initialIndex = currIndex;
        }

        void setCurrIndex() {
            int currDegrees = (int) Math::getDegrees(uBit.accelerometer.getX(), uBit.accelerometer.getY());
            currIndex = currDegrees / 20; // 20 degrees per pixel
        }
    public:
        void runFrame() {
            setCurrIndex();
            setInitialIndex();
            setFirstIndex();
            setCurrUBitDir();
            checkEndRun();

            if (endRun) {
                drawCenter();
            } else {
                drawRing();
            }
            uBit.display.print(im);
        }

        void reset() {
            initialIndex = -1;
            currIndex = -1;
            firstIndex = -1;
            endRun = false;
        }
} vertParadox;

/* 
 * The margin of error for what we define as horizontal.
 *   |z| > HORIZONTAL_MARGIN: horizontal
 *   |z| < HORIZONTAL_MARGIN: vertical
 */
#define HORIZONTAL_MARGIN 700
// Defines how many consecutive clean data points are captured before we register a change in verticality
#define ORIENTATION_SENS 20
enum Orientation { HORIZONTAL = 0, VERTICAL = 1 };
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

class ParadoxThatDrivesUsAll {
    private:
        static void onOrientationChange() {
            vertParadox.reset();
            horiParadox.reset();
        }
    public:
        void run() {
            while (1) {
                Orientation currOrient = orientationManager.getOrientationBuffered(&onOrientationChange);
                if (currOrient == VERTICAL) {
                    vertParadox.runFrame();
                } else {
                    horiParadox.runFrame();
                }
            }
        }
} paradoxThatDrivesUsAll;

int main() {
    uBit.init();
    /* We can arbitrarily calibrate the compass since:
     *   - no significant z-axis error is being introduced
     *   - we don't care about the actual North, but relative rotation only
     */
    // uBit.compass.setCalibration(CompassCalibration());
    uBit.compass.calibrate();

    paradoxThatDrivesUsAll.run();

    release_fiber();
}
