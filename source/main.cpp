#include "MicroBit.h"

MicroBit uBit;
/*
 * Initialization of MicroBitImage seems to be a little broken,
 * so we will settle with reusing one image instead.
 */
MicroBitImage im(5, 5);

// MARK 0: Helper classes
template <class S>
class ButtonWrapper {
    private:
        unsigned long lastPressed;
        unsigned long repeatDelay;
    public:
        MicroBitButton *uButton;

        ButtonWrapper(MicroBitButton *uButton, unsigned long repeatDelay) {
            this->uButton = uButton;
            this->repeatDelay = repeatDelay;
            lastPressed = uBit.systemTime();
        }

        void onPress(void (S::*op)(), S *s) {
            if (uButton->isPressed() && uBit.systemTime() - lastPressed > repeatDelay) {
                lastPressed = uBit.systemTime();
                (*s.*op)();
            }
            /*
             * If the key is lifted before the key repeat delay is over, then reset the key event
             * by shifting the lastPressed back by KEY_REPEAT_DELAY
             */
            if (!uButton->isPressed() && uBit.systemTime() - lastPressed < repeatDelay) {
                lastPressed = uBit.systemTime() - repeatDelay;
            }
        }
};

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
            if (y >= 0) {
                /*
                 * If x ~= 0, then y / x becomes very large.
                 * To avoid that, we take the smaller ratio, and offset from PI / 2 instead.
                 */
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

enum CircularDirection { CLOCKWISE = 1, COUNTERCLOCKWISE = -1, NO_ROTATION = 0, INDETERMINATE = 2 };
class Circular {
    public:
        /*
         * Given a circular structure with a given radix, determine the order of a and b
         *
         * @param radix the size of the circular structure
         */
        static CircularDirection compare(int a, int b, int radix) {
            if (b == a) return NO_ROTATION;
            if (Math::realMod(b - a, radix) == radix / 2) return INDETERMINATE;
            if (Math::realMod(b - a, radix) < radix / 2) return CLOCKWISE;
            return COUNTERCLOCKWISE;
        }

        /*
         * Given three points a, b, c, on a circular structure, determine the flow from a to b to c.
         *   - If the three points are not within two thirds of the radix, indeterminate flow.
         *   - If all three points are on the same index, no flow.
         * @return the direction of flow, no flow, or indeterminate flow.
         */
        static CircularDirection flow(int a, int b, int c, int radix) {
            if (a == b && b == c) return NO_ROTATION;
            if (Math::realMod(b - a, radix) <= radix / 3 && Math::realMod(c - b, radix) <= radix / 3) return CLOCKWISE;
            if (Math::realMod(b - c, radix) <= radix / 3 && Math::realMod(a - b, radix) <= radix / 3) return COUNTERCLOCKWISE;
            return INDETERMINATE;
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

template <typename T>
struct Optional {
    private:
        T _value;
        bool _isNull;
    public:
        Optional() {
            _isNull = true;
        }

        Optional(T value) {
            _value = value;
            _isNull = false;
        }
        
        bool isNull() {
            return _isNull;
        }

        void toNull() {
            _isNull = true;
        }

        /*
         * Forcibly unwraps the Optional and returns the value stored.
         * Returns void if the value is null.
         */
        T _() {
            if (!_isNull) return _value;
        }

        Optional<T> operator=(T value) {
            _value = value;
            _isNull = false;
            return value;
        }

        Optional<T> operator=(Optional<T> other) {
            _value = other._value;
            _isNull = other._isNull;
            return other;
        }
};

/*
 * Wrapper around a value and a getRaw function that buffers a value.
 * Used to smooth out variations in data by waiting for bufferSize changes
 * in the raw data before registering a change in value.
 */
template <class S, typename T>
class Buffer {
    private:
        int changedCount = 0;
        int bufferSize;
        Optional<T> currentValue = Optional<T>();
        S *s;
        T (S::*getRaw)();
    public:
        /*
         * @param (S::*getRaw)() a function pointer to a member function of another object
         * @param *s a pointer to the calling instance
         */
        Buffer(int bufferSize, T (S::*getRaw)(), S *s) {
            this->bufferSize = bufferSize;
            this->s = s;
            this->getRaw = getRaw;
        }

        T value(void (*onChange)()) {
            T raw = (*s.*getRaw)();
            if (currentValue.isNull()) {
                currentValue = raw;
                return raw;
            }
            if (raw != currentValue._()) {
                changedCount++;
            } else {
                changedCount = 0;
                return currentValue._();
            }
            if (changedCount > bufferSize) {
                currentValue = raw;
                (*onChange)();
            }
            return currentValue._();
        }

        T value() {
            return value([](){});
        }

        void reset() {
            int changedCount = 0;
            currentValue.toNull();
        }
};


// MARK 1: Question 1
#define BUTTON_REPEAT_DELAY 500
class TimeForEverything {
    private:
        int x = 5;
        ButtonWrapper<TimeForEverything> buttonA = ButtonWrapper<TimeForEverything>(&uBit.buttonA, BUTTON_REPEAT_DELAY);
        ButtonWrapper<TimeForEverything> buttonB = ButtonWrapper<TimeForEverything>(&uBit.buttonB, BUTTON_REPEAT_DELAY);

        void increment() {
            if (x < 9) x++;
        }

        void decrement() {
            if (x > 1) x--;
        }

        void countdown() {
            while (x > 0) {
                uBit.sleep(1000);
                x--;
                uBit.display.print(x);
            }
        }
    public:
        void run() {
            while (1) {
                uBit.display.print(x);
                if (uBit.buttonAB.isPressed()) {
                    countdown();
                    break;
                }
                buttonA.onPress(&TimeForEverything::decrement, this);
                buttonB.onPress(&TimeForEverything::increment, this);
            }
        }
} timeForEverything;


// MARK 2: Question 2a
#define PERIMETER_LEN 18
#define INDEX_BUFFER 100
class VerticalParadox {
    private:
        Buffer<VerticalParadox, int> indexBuffer
            = Buffer<VerticalParadox, int>(INDEX_BUFFER, &VerticalParadox::getRawIndex, this);
        Optional<int> prevIndex = Optional<int>();
        Optional<int> currIndex = Optional<int>();
        Optional<int> initialIndex = Optional<int>();
        CircularDirection currUBitDir = NO_ROTATION; // direction of the uBit device, not the ring drawn

        int getRawIndex() {
            return (int) Math::getDegrees(uBit.accelerometer.getX(), uBit.accelerometer.getY()) / 20;
        }

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
            if (currUBitDir != NO_ROTATION && Math::realMod(currIndex._() + currUBitDir, PERIMETER_LEN) == initialIndex._()) return; 
            int i = initialIndex._();
            while (i != currIndex._()) {
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
            if (initialIndex._() == currIndex._()) return;

            CircularDirection flow = Circular::flow(prevIndex._(), initialIndex._(), currIndex._(), PERIMETER_LEN);
            if (flow == CLOCKWISE) {
                currUBitDir = CLOCKWISE;
            } else if (flow == COUNTERCLOCKWISE) {
                currUBitDir = COUNTERCLOCKWISE;
            }
        }

        void setInitialIndex() {
            if (initialIndex.isNull()) initialIndex = currIndex;
        }

        void setCurrentIndex() {
            currIndex = indexBuffer.value();
        }

        void setPreviousIndex() {
            if (currIndex._() == -1) {
                prevIndex = indexBuffer.value();
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
            indexBuffer.reset();
            currIndex.toNull();
            initialIndex.toNull();
            currUBitDir = NO_ROTATION;
        }
} vertParadox;


// MARK 3: Question 2b
#define TILT_SENS 250   // Defines how sensitive the dot movement is to tilting. Lower values result in more sensitive movement.
#define TILT_BUFFER 100 // How many data points before registering a change in tilt
#define BLINK_DUR 250
#define HEADING_BUFFER 100
class HorizontalParadox {
    private:
        Buffer<HorizontalParadox, Coord> posBuffer
            = Buffer<HorizontalParadox, Coord>(TILT_BUFFER, &HorizontalParadox::getRawPos, this);
        Coord pos = {0, 0};
        unsigned long lastBlink = uBit.systemTime();

        Buffer<HorizontalParadox, int> headingBuffer
            = Buffer<HorizontalParadox, int>(HEADING_BUFFER, &HorizontalParadox::getRawHeading, this);
        Optional<int> prevHeading = Optional<int>();
        Optional<int> currHeading = Optional<int>();
        Optional<int> initialHeading = Optional<int>();
        CircularDirection currUBitDir = NO_ROTATION;
        int turnCount = 0;

        Coord getRawPos() {
            Coord pos = {
                uBit.accelerometer.getX() / TILT_SENS,
                uBit.accelerometer.getY() / TILT_SENS
            };
            return pos;
        }

        int getRawHeading() {
            return uBit.compass.heading() / 20;
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

        void printComposite() {
            im.clear();
            drawRotation();
            drawTilt();
            uBit.display.print(im);
        }

        void setCurrUBitDir() {
            if (initialHeading._() == currHeading._()) return;
            CircularDirection flow = Circular::flow(prevHeading._(), initialHeading._(), currHeading._(), PERIMETER_LEN);

            if (flow == CLOCKWISE) {
                // Left or passed through initialHeading in the clockwise direction
                currUBitDir = CLOCKWISE;
            } else if (flow == COUNTERCLOCKWISE) {
                // Left or passed through initialHeading in the counter-clockwise direction
                currUBitDir = COUNTERCLOCKWISE;
            }
        }

        void checkTurns() {
            if (currUBitDir == NO_ROTATION) return;
            if (prevHeading._() == initialHeading._()) return;

            CircularDirection flow = Circular::flow(prevHeading._(), initialHeading._(), currHeading._(), PERIMETER_LEN);
            if (flow == CLOCKWISE && currUBitDir == CLOCKWISE) {
                // Touched initialHeading from the clockwise direction
                turnCount++;
                if (turnCount > 9) turnCount = 9;
            } else if (flow == COUNTERCLOCKWISE && currUBitDir == COUNTERCLOCKWISE) {
                // Touched initialHeading from the counter-clockwise direction
                turnCount--;
                if (turnCount < 0) turnCount = 0;
            }
        }

        void setInitialHeading() {
            if (initialHeading.isNull()) initialHeading = currHeading;
        }

        void setCurrentHeading() {
            currHeading = headingBuffer.value();
        }

        void setPreviousHeading() {
            if (currHeading.isNull()) {
                prevHeading = headingBuffer.value();
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
            pos = posBuffer.value();

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
            posBuffer.reset();
            pos = {0, 0};
            lastBlink = uBit.systemTime();
            headingBuffer.reset();
            prevHeading.toNull();
            currHeading.toNull();
            initialHeading.toNull();
            currUBitDir = NO_ROTATION;
            turnCount = 0;
        }
} horiParadox;


// MARK 4: Switching between 2a and 2b
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
        Buffer<OrientationManager, Orientation> orientationBuffer
            = Buffer<OrientationManager, Orientation>(ORIENTATION_BUFFER, &OrientationManager::getOrientationRaw, this);
    public:
        /*
         * Wait until we get ORIENTATION_BUFFER data points before registering a change in orientation
         *
         * @param (*onChange)() callback function when orientation changes
         */
        Orientation getOrientation(void (*onChange)()) {
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
            currOrientation = orientationBuffer.value(onChange);
            return currOrientation;
        }
} oManager;


// MARK 5: QUestion 2 runner class
class ParadoxThatDrivesUsAll {
    private:
        static void onOrientationChange() {
            vertParadox.reset();
            horiParadox.reset();
        }
    public:
        void run() {
            while (1) {
                if (oManager.getOrientation(&onOrientationChange) == VERTICAL) {
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
