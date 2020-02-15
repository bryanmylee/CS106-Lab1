#include "MicroBit.h"

MicroBit uBit;
/* 
 * Initialization of MicroBitImage seems to be a little broken,
 * so we will settle with reusing one image instead.
 */
MicroBitImage im(5, 5);

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
#define IMAGE_SIZE 5

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

    private:
        Math() {}
};

/* 
 * The margin of error for what we define as horizontal.
 *   |z| > HORIZONTAL_MARGIN: horizontal
 *   |z| < HORIZONTAL_MARGIN: vertical
 */
#define HORIZONTAL_MARGIN 800
// Defines how many consecutive clean data points are captured before we register a change in verticality
#define ORIENTATION_SENS 20
enum Orientation { HORIZONTAL = 0, VERTICAL = 1 };
enum RotationDir { COUNTERCLOCKWISE = -1, CLOCKWISE = 1 };
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

struct Coord {
    int x;
    int y;
};

#define PERIMETER_LEN 18
class VerticalParadox {
    private:
        int initialIndex = -1; // uninitialized value
        int currIndex    = -1;
        int firstIndex   = -1;
        RotationDir currDir = CLOCKWISE; // direction of the uBit device, not the ring drawn

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

        /* 
         * Draw the image required to print the ring around the perimeter.
         * @param uBitDir the direction of the uBit device.
         *                the ring will be drawn in the opposite direction
         */
        void drawRing(int startIndex, int endIndex, RotationDir uBitDir) {
            im.clear();

            while (startIndex != endIndex) {
                setImagePixel(startIndex);
                startIndex -= uBitDir;
                startIndex = Math::realMod(startIndex, PERIMETER_LEN);
            };
            setImagePixel(startIndex);
        }
    public:
        void runFrame() {
            int currDegrees = (int) Math::getDegrees(uBit.accelerometer.getX(), uBit.accelerometer.getY());
            currIndex = currDegrees / 20; // 20 degrees per pixel
            if (initialIndex == -1) initialIndex = currIndex;

            if (firstIndex == -1 && currIndex != initialIndex) {
                firstIndex = currIndex;
            } else if (currIndex == initialIndex) {
                firstIndex = -1;
            }

            if (firstIndex > initialIndex || (firstIndex == 0 && initialIndex == 17)) {
                currDir = COUNTERCLOCKWISE;
            } else {
                currDir = CLOCKWISE;
            }

            drawRing(initialIndex, currIndex, currDir);
            uBit.display.print(im);
        }

        void reset() {
            initialIndex = -1;
            currIndex = -1;
            firstIndex = -1;
        }
} vertParadox;

class HorizontalParadox {
    public:
        void runFrame() {
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
                uBit.serial.send("running:");
                Orientation currOrient = orientationManager.getOrientationBuffered(&onOrientationChange);
                if (currOrient == VERTICAL) {
                    vertParadox.runFrame();
                } else {
                    horiParadox.runFrame();
                }
                uBit.serial.send("\r\n");
            }
        }
} paradoxThatDrivesUsAll;

int main() {
    uBit.init();

    paradoxThatDrivesUsAll.run();

    release_fiber();
}
