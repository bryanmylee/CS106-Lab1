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

        /*
         * Given a circular structure with a given radix, determine the order of a and b
         *
         * @param radix the size of the circular structure
         * @return 
         *    1: b is clockwise of a
         *   -1: b is counter clockwise of a
         */
        static int circularCompare(int a, int b, int radix) {
            if (b == a) return 0;
            if (realMod(b - a, radix) < radix / 2) return 1;
            return -1;
        }
    private:
        Math() {}
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

/*
 * Defines how sensitive the dot movement is to tilting
 * Lower values result in more sensitive movement
 */
#define TILT_SENS 250
#define CHANGE_TILT_SENS 100 // How many data points before registering a change in tilt
#define BLINK_DUR 250
#define PERIMETER_LEN 18
#define CHANGE_HEADING_SENS 20
enum RotationDir { COUNTERCLOCKWISE = -1, NO_ROTATION = 0, CLOCKWISE = 1 };
class HorizontalParadox {
    private:
        Coord pos = {0, 0};
        int posChangedCount = 0;
        unsigned long lastBlink = uBit.systemTime();

        int headingChangedCount = 0;
        int prevHeading;
        int currHeading = -1; // uninitialized value
        int initialHeading = -1;
        int firstHeading = -1;
        RotationDir currUBitDir;
        int turnCount = 0;

        void resetHeading() {
            currHeading = -1;
            initialHeading = -1;
            firstHeading = -1;
        }

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

        void drawComposite() {
            im.clear();
            drawRotation();
            drawTilt();
        }

        // void checkTurns() {
        //     if (currHeading == initialHeading) {
        //         turnCount++;
        //         uBit.serial.send(turnCount);
        //         uBit.serial.send("click!\r\n");
        //     }
        // }

        void setCurrUBitDir() {
            if (Math::circularCompare(prevHeading, initialHeading, PERIMETER_LEN) >= 0 
                    && Math::circularCompare(initialHeading, currHeading, PERIMETER_LEN) > 0) {
                if (currUBitDir != CLOCKWISE) {
                    currUBitDir = CLOCKWISE;
                    uBit.serial.send("CHANGED TO CLOCKWISE");
                }
            } else if (Math::circularCompare(prevHeading, initialHeading, PERIMETER_LEN) <= 0
                    && Math::circularCompare(initialHeading, currHeading, PERIMETER_LEN) < 0) {
                if (currUBitDir != COUNTERCLOCKWISE) {
                    currUBitDir = COUNTERCLOCKWISE;
                    uBit.serial.send("CHANGED TO COUNTERCLOCKWISE");
                }
            }
            {
                uBit.serial.send("curr:");
                uBit.serial.send(currHeading);
                uBit.serial.send(", prev:");
                uBit.serial.send(prevHeading);
                uBit.serial.send(", init:");
                uBit.serial.send(initialHeading);
                uBit.serial.send(", cdir:");
                uBit.serial.send(currUBitDir);
                uBit.serial.send(" | resetDirection\r\n");
            }
        }

        void resetDirection() {
            if (Math::realMod(prevHeading + currUBitDir, PERIMETER_LEN) == initialHeading) {
                turnCount += currUBitDir;
                uBit.serial.send(turnCount);
                uBit.serial.send("click!\r\n");
            }
            firstHeading = -1;
        }

        void setInitialHeading() {
            if (initialHeading == -1) initialHeading = currHeading;
            // uBit.serial.send(initialHeading);
            // uBit.serial.send(",");
        }

        void setCurrentHeading() {
            currHeading = uBit.compass.heading() / 20;
            // if (currHeading == -1) {
            //     currHeading = currHeadingRaw;
            //     return;
            // }
            // if (currHeadingRaw != currHeading) {
            //     headingChangedCount++;
            // } else {
            //     headingChangedCount = 0;
            // }
            // if (headingChangedCount > CHANGE_HEADING_SENS) {
            //     currHeading = currHeadingRaw;
            // }
            // uBit.serial.send(currHeading);
            // uBit.serial.send(",");
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
        }

        void setTilt() {
            Coord posRaw = {
                uBit.accelerometer.getX() / TILT_SENS,
                uBit.accelerometer.getY() / TILT_SENS
            };

            /*
             * To reduce flickering between two positions, we wait till the new position 
             * has been sampled CHANGE_TILT_SENS times before changing the position.
             */
            if (posRaw != pos) {
                posChangedCount++;
            } else {
                posChangedCount = 0;
            }
            
            if (posChangedCount > CHANGE_TILT_SENS) {
                pos = posRaw;
            }

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
            setTilt();
            setHeadings();
            setCurrUBitDir();
            // uBit.serial.send("\r\n");
            // checkTurns();
            drawComposite();
            uBit.display.print(im);
        }

        void reset() {
            pos = {0, 0};
            posChangedCount = 0;
            lastBlink = uBit.systemTime();
            turnCount = 0;
            resetHeading();
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
                i -= currUBitDir;
                i = Math::realMod(i, PERIMETER_LEN);
            };
            setImagePixel(i);
        }

        void drawCenter() {
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

            im.clear();
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
 *   |x, y| < HORI_TO_VERT_MARGIN: horizontal
 *   |x, y| > HORI_TO_VERT_MARGIN: vertical
 *   |z| < VERT_TO_HORI_MARGIN: vertical
 *   |z| > VERT_TO_HORI_MARGIN: horizontal
 */
#define HORI_TO_VERT_MARGIN 900
#define VERT_TO_HORI_MARGIN 800
// We ignore accelerometer readings with a magnitude greater than GRAVITY
#define GRAVITY 1250
// Defines how many consecutive clean data points are captured before we register a change in verticality
#define ORIENTATION_SENS 100
enum Orientation { HORIZONTAL = 0, VERTICAL = 1 };
class OrientationManager {
    private:
        Orientation orientationReal = HORIZONTAL;
        int changedCount = 0;

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
            if (orientationReal == HORIZONTAL) {
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
    public:
        /*
         * Wait until we get ORIENTATION_SENS data points before registering a change in orientation
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
                return orientationReal;
            }
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
        Orientation currOrient;

        static void onOrientationChange() {
            vertParadox.reset();
            horiParadox.reset();
        }
    public:
        void run() {
            uBit.compass.calibrate();
            // uBit.compass.setCalibration(CompassCalibration());
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
