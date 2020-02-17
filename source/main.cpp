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
         * @return degrees from 0 to <360
         */
        static double getDegrees(int x, int y) {
            return getRadians(x, y) / PI * 180;
        }

        static int realMod(int x, int radix) {
            const int result = x % radix;
            return result >= 0 ? result : result + radix;
        }
        static int squaredMagnitude(int x, int y) {
            return x * x + y * y;
        }

        static int squaredMagnitude(int x, int y, int z) {
            return x * x + y * y + z * z;
        }
    private:
        Math() {}
};

class Circular {
    public:
        /*
         * Given a circular structure with a given radix, determine the order of a and b
         *
         * @param radix the size of the circular structure
         */
        enum Direction { CLOCKWISE = 1, COUNTERCLOCKWISE = -1, NO_ROTATION = 0, OPPOSITE = 2};
        static Direction compare(int a, int b, int radix) {
            if (b == a) return NO_ROTATION;
            if (Math::realMod(b - a, radix) == radix / 2) return OPPOSITE;
            if (Math::realMod(b - a, radix) < radix / 2) return CLOCKWISE;
            return COUNTERCLOCKWISE;
        }
    private:
        Circular() {}
};

struct Coord {
    int x;
    int y;

    bool operator==(Coord const& other) {
        return x == other.x && y == other.y;
    }

    bool operator!=(Coord const& other) {
        return x != other.x || y != other.y;
    }
};

template <class S, typename T>
class Buffer {
    private:
        int changedCount = 0;
        int bufferSize;
        T currentValue;
        S *s;
        T (S::*getRaw)();
    public:
        Buffer(int bufferSize, T (S::*getRaw)(), S *s) {
            this->bufferSize = bufferSize;
            this->currentValue = (*s.*getRaw)();
            this->s = s;
            this->getRaw = getRaw;
        }

        T value(void (*onChange)()) {
            T raw = (*s.*getRaw)();
            if (raw != currentValue) {
                changedCount++;
            } else {
                changedCount = 0;
                return currentValue;
            }
            if (changedCount > bufferSize) {
                currentValue = raw;
                (*onChange)();
            }
            return currentValue;
        }

        T value() {
            return value([](){});
        }
};

#define PERIMETER_LEN 18
#define TILT_SENS 250        // Defines how sensitive the dot movement is to tilting. Lower values result in more sensitive movement.
#define TILT_BUFFER 100 // How many data points before registering a change in tilt
#define BLINK_DUR 250
#define HEADING_BUFFER 100
class HorizontalParadox {
    private:
        Coord getRawPos() {
            Coord pos = {
                uBit.accelerometer.getX() / TILT_SENS,
                uBit.accelerometer.getY() / TILT_SENS
            };
            return pos;
        }
        Buffer<HorizontalParadox, Coord> *posBuffer
            = new Buffer<HorizontalParadox, Coord>(TILT_BUFFER, &HorizontalParadox::getRawPos, this);
        Coord pos = {0, 0};
        unsigned long lastBlink = uBit.systemTime();

        int getRawHeading() {
            return uBit.compass.heading() / 20;
        }
        Buffer<HorizontalParadox, int> *headingBuffer
            = new Buffer<HorizontalParadox, int>(HEADING_BUFFER, &HorizontalParadox::getRawHeading, this);
        int prevHeading;
        int currHeading = -1; // uninitialized value
        int initialHeading = -1;
        Circular::Direction currUBitDir = Circular::NO_ROTATION;
        int turnCount = 0;

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
            im.print('0' + turnCount);
        }

        void printComposite() {
            im.clear();
            drawRotation();
            drawTilt();
            uBit.display.print(im);
        }

        void setCurrUBitDir() {
            Circular::Direction prevToInit = Circular::compare(prevHeading, initialHeading, PERIMETER_LEN);
            Circular::Direction initToCurr = Circular::compare(initialHeading, currHeading, PERIMETER_LEN);
            if ((prevToInit == Circular::CLOCKWISE || prevToInit == Circular::NO_ROTATION)
                    && initToCurr == Circular::CLOCKWISE) {
                // Left or passed through initialHeading in the clockwise direction
                currUBitDir = Circular::CLOCKWISE;
            } else if ((prevToInit == Circular::COUNTERCLOCKWISE || prevToInit == Circular::NO_ROTATION)
                    && initToCurr == Circular::COUNTERCLOCKWISE) {
                // Left or passed through initialHeading in the counter-clockwise direction
                currUBitDir = Circular::COUNTERCLOCKWISE;
            }
        }

        void checkTurns() {
            if (currUBitDir == Circular::NO_ROTATION) return;

            Circular::Direction prevToInit = Circular::compare(prevHeading, initialHeading, PERIMETER_LEN);
            Circular::Direction initToCurr = Circular::compare(initialHeading, currHeading, PERIMETER_LEN);
            if (prevToInit == Circular::CLOCKWISE
                    && (initToCurr == Circular::CLOCKWISE || initToCurr == Circular::NO_ROTATION)
                    && currUBitDir == Circular::CLOCKWISE) {
                // Touched initialHeading from the clockwise direction
                turnCount++;
                if (turnCount > 9) turnCount = 9;
            } else if (prevToInit == Circular::COUNTERCLOCKWISE
                    && (initToCurr == Circular::COUNTERCLOCKWISE || initToCurr == Circular::NO_ROTATION)
                    && currUBitDir == Circular::COUNTERCLOCKWISE) {
                // Touched initialHeading from the counter-clockwise direction
                turnCount--;
                if (turnCount < 0) turnCount = 0;
            }
        }

        void setInitialHeading() {
            if (initialHeading == -1) initialHeading = currHeading;
        }

        void setCurrentHeading() {
            currHeading = headingBuffer->value();
        }

        void setPreviousHeading() {
            if (currHeading == -1) {
                prevHeading = headingBuffer->value();
            } else {
                prevHeading = currHeading;
            }
        }

        void updateHeadings() {
            setPreviousHeading();
            setCurrentHeading();
            setInitialHeading();
        }

        void updateTilt() {
            pos = posBuffer->value();

            if (pos.x > 2) pos.x = 2;
            if (pos.x < -2) pos.x = -2;
            if (pos.y > 2) pos.y = 2;
            if (pos.y < -2) pos.y = -2;

            if (Math::abs(pos.x) == 2 || Math::abs(pos.y) == 2) {
                turnCount = 0;
            }
        }
    public:
        void runFrame() {
            updateTilt();
            updateHeadings();
            checkTurns();
            setCurrUBitDir();
            printComposite();
        }

        void reset() {
            pos = {0, 0};
            lastBlink = uBit.systemTime();
            currHeading = -1;
            initialHeading = -1;
            currUBitDir = Circular::NO_ROTATION;
            turnCount = 0;
        }
} horiParadox;

#define INDEX_BUFFER 100
class VerticalParadox {
    private:
        int prevIndex;
        int currIndex = -1; // uninitialized value
        int initialIndex = -1;
        Circular::Direction currUBitDir = Circular::NO_ROTATION; // direction of the uBit device, not the ring drawn

        /*
         * 0 represents the LED closest to buttonA.
         * Every subsequent index wraps around the perimeter clockwise.
         */
        Coord getPixelCoord(int index) {
            Coord c;
            switch (index) {
                case 0  : c.x = 0; c.y = 2; break;
                case 1  : c.x = 0; c.y = 1; break;
                case 2  : c.x = 0; c.y = 0; break;
                case 3  : c.x = 1; c.y = 0; break;
                case 4  : c.x = 2; c.y = 0; break;
                case 5  : c.x = 3; c.y = 0; break;
                case 6  : c.x = 4; c.y = 0; break;
                case 7  : c.x = 4; c.y = 1; break;
                case 8  : c.x = 4; c.y = 2; break;
                case 9  : c.x = 4; c.y = 3; break;
                case 10 : c.x = 4; c.y = 4; break;
                case 11 : c.x = 3; c.y = 4; break;
                case 12 : c.x = 2; c.y = 4; break;
                case 13 : c.x = 1; c.y = 4; break;
                case 14 : c.x = 0; c.y = 4; break;
                case 15 : c.x = 0; c.y = 3; break;
                case 16 : c.x = 0; c.y = 2; break;
                case 17 : c.x = 0; c.y = 1; break;
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
            int i = initialIndex;
            while (i != currIndex) {
                setImagePixel(i);
                i = Math::realMod(i + currUBitDir, PERIMETER_LEN);
            };
            setImagePixel(i);
        }

        void printRing() {
            im.clear();
            drawRing();
            uBit.display.print(im);
        }

        void setCurrUBitDir() {
            Circular::Direction prevToInit = Circular::compare(prevIndex, initialIndex, PERIMETER_LEN);
            Circular::Direction initToCurr = Circular::compare(initialIndex, currIndex, PERIMETER_LEN);
            if ((prevToInit == Circular::CLOCKWISE || prevToInit == Circular::NO_ROTATION)
                    && initToCurr == Circular::CLOCKWISE) {
                currUBitDir = Circular::CLOCKWISE;
            } else if ((prevToInit == Circular::COUNTERCLOCKWISE || prevToInit == Circular::NO_ROTATION)
                    && initToCurr == Circular::COUNTERCLOCKWISE) {
                currUBitDir = Circular::COUNTERCLOCKWISE;
            }
        }

        void setInitialIndex() {
            if (initialIndex == -1) initialIndex = currIndex;
        }

        void setCurrentIndex() {
            int currDegrees = (int) Math::getDegrees(uBit.accelerometer.getX(), uBit.accelerometer.getY());
            currIndex = currDegrees / 20; // 20 degrees per pixel
        }

        void setPreviousIndex() {
            if (currIndex == -1) {
                int currDegrees = (int) Math::getDegrees(uBit.accelerometer.getX(), uBit.accelerometer.getY());
                prevIndex = currDegrees / 20; // 20 degrees per pixel
            } else {
                prevIndex = currIndex;
            }
        }

        void updateIndexes() {
            setPreviousIndex();
            setCurrentIndex();
            setInitialIndex();
        }
    public:
        void runFrame() {
            updateIndexes();
            setCurrUBitDir();
            printRing();
        }

        void reset() {
            currIndex = -1;
            initialIndex = -1;
            currUBitDir = Circular::NO_ROTATION;
        }
} vertParadox;

/*
 * The margin of error for what we define as horizontal.
 *   |x, y| < HORI_TO_VERT_MARGIN: horizontal
 *   |x, y| > HORI_TO_VERT_MARGIN: vertical
 *   |z| < VERT_TO_HORI_MARGIN: vertical
 *   |z| > VERT_TO_HORI_MARGIN: horizontal
 */
#define HORI_TO_VERT_MARGIN 950
#define VERT_TO_HORI_MARGIN 950
#define GRAVITY 1250           // We ignore accelerometer readings with a magnitude greater than GRAVITY
#define ORIENTATION_BUFFER 100 // Defines how many consecutive clean data points are captured before we register a change in verticality
enum Orientation { HORIZONTAL = 0, VERTICAL = 1 };
class OrientationManager {
    private:
        Orientation currOrientation = HORIZONTAL;

        /*
         * When VERTICAL, we check for horizontality by checking the magnitude of the vector <z>
         *      0: perfectly vertical
         *   1023: perfectly horizontal
         * When HORIZONTAL, we check for verticality by checking the magnitude of the vector <x, y>
         *      0: perfectly horizontal
         *   1023: perfectly vertical
         * This is so we can avoid issues with moving the uBit.
         */
        Orientation getOrientationRaw() {
            if (currOrientation == HORIZONTAL) {
                if (Math::squaredMagnitude(
                            uBit.accelerometer.getX(),
                            uBit.accelerometer.getY()) > HORI_TO_VERT_MARGIN * HORI_TO_VERT_MARGIN) {
                    return VERTICAL;
                }
                return HORIZONTAL;
            } else {
                if (Math::abs(uBit.accelerometer.getZ()) > VERT_TO_HORI_MARGIN) {
                    return HORIZONTAL;
                }
                return VERTICAL;
            }
        }
        Buffer<OrientationManager, Orientation> *orientationBuffer
            = new Buffer<OrientationManager, Orientation>(ORIENTATION_BUFFER, &OrientationManager::getOrientationRaw, this);
    public:
        /*
         * Wait until we get ORIENTATION_BUFFER data points before registering a change in orientation
         *
         * @param (*onChange)() callback function when orientation changes
         */
        Orientation getOrientationBuffered(void (*onChange)()) {
            /*
             * There are some flaws with using |z| or |x, y| to determine orientation.
             * Moving the uBit along its z-axis will result in increased |z|, even when vertical.
             * To mitigate this, we can ignore all readings where |x, y, z| > GRAVITY
             */
            if (Math::squaredMagnitude(
                        uBit.accelerometer.getX(),
                        uBit.accelerometer.getY(),
                        uBit.accelerometer.getZ()) > GRAVITY * GRAVITY) {
                return currOrientation;
            }
            return orientationBuffer->value(onChange);
        }
} orientationManager;

class ParadoxThatDrivesUsAll {
    private:
        Orientation currOrient;

        static void onOrientationChange() {
            vertParadox.reset();
            horiParadox.reset();
        }
    public:
        void run() {
            // uBit.compass.calibrate();
            while (1) {
                currOrient = orientationManager.getOrientationBuffered(&onOrientationChange);
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

    // Uncomment the lines below to test each question
    // timeForEverything.run();
    paradoxThatDrivesUsAll.run();

    release_fiber();
}
