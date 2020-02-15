#include "MicroBit.h"

MicroBit uBit;

// MARK: Question 1
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

void countdown(int start) {
    while (start > 0) {
        uBit.sleep(1000);
        start--;
        uBit.display.print(start);
    }
}

void aTimeForEverything() {
    ExtendedButton buttonA = ExtendedButton(&uBit.buttonA, 500);
    ExtendedButton buttonB = ExtendedButton(&uBit.buttonB, 500);

    int x = 5;

    while (1) {
        uBit.display.print(x);
        if (buttonA.onPress()) {
            uBit.serial.send("A");
            if (x > 1) x--;
        }
        if (buttonB.onPress()) {
            uBit.serial.send("B");
            if (x < 9) x++;
        }
        if (uBit.buttonAB.isPressed()) {
            countdown(x);
            break;
        }
    }
}


// MARK: Question 2
/* 
 * The margin of error for what we define as vertical.
 *   0   : perfectly horizontal
 *   1000: perfectly vertical
 */
#define VERT_MARGIN 750
// Defines how many consecutive clean data points are captured before we register a change in verticality
#define VERT_SENS 20

int getSquareMagnitude(int x, int y) {
    return x * x + y * y;
}

/* 
 * We can check for verticality by checking the magnitude of the vector <x, y>
 *   0   : perfectly horizontal
 *   1000: perfectly vertical
 */
bool isVerticalRaw() {
    return getSquareMagnitude(uBit.accelerometer.getX(), uBit.accelerometer.getY()) > VERT_MARGIN * VERT_MARGIN;
}

bool isVerticalReal = false;
int changedCount = 0;
bool isVerticalBuffered() {
    if (isVerticalRaw() ^ isVerticalReal) {
        changedCount++;
    } else {
        changedCount = 0;
        return isVerticalReal;
    }
    if (changedCount > VERT_SENS) {
        isVerticalReal = !isVerticalReal;
    }
    return isVerticalReal;
}

void paradoxThatDrivesUsAll() {
    while (1) {
        // uBit.serial.send("x: " + ManagedString(uBit.accelerometer.getX()) + "|");
        // uBit.serial.send("y: " + ManagedString(uBit.accelerometer.getY()) + "|");
        // uBit.serial.send("z: " + ManagedString(uBit.accelerometer.getZ()) + "\r\n");
        uBit.serial.send(isVerticalBuffered());
        uBit.serial.send("\r\n");
        // uBit.sleep(500);
    }
}

int main() {
    uBit.init();

    aTimeForEverything();

    release_fiber();
}
