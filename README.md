# CS106 Computer Hardware and Embedded Systems Lab 1

## Q1. A Time For Everything (12m)

1. When the micro:bit is switched on, the LEDs display a numeric value, X. (2m) 
2. Initially, X == 5. (2m)
3. Pressing A decrements X; pressing B increments X. (2m)
4. X is limited to the range 1 to 9 only; both 1 and 9 inclusive. (2m)
5. Pressing both buttons causes X to count down to zero, in approximately 1s intervals. (2m)
6. When X reaches 0, the display stays at 0. (2m)



## Q2. It’s the paradox that drives us all (3m)

1. When the micro:bit is in a vertical position, a single LED lights up (LED X), on any corner of the micro:bit display.
   1. When the micro:bit is rotated clockwise, the LEDs along the edges light up, starting from LED X, X+1, X+2, etc.
   2. When the micro:bit is rotated anti-clockwise, the lighted LEDs will be unlit, from X+2, X+1, etc. When one full circle has been made, the display clears. If the micro:bit continues to be rotated clockwise, LED X lights up again, and the pattern repeats. (1.5m)
2. When the micro:bit is in a horizontal position, a blinking LED lights up in the centre of the screen.
   1. By tilting the micro:bit, the blinking LED moves in the direction of tilt (in 4 directions only).
   2. The display initially shows a number 0.
      1. When the micro:bit is rotated clockwise, the number increments.
      2. When the micro:bit is rotated anti-clockwise, the number decrements.
      3. The number must remain in the range [0, 9].
   3. If the blinking LED hits any of the edges, the number is reset to 0. (1.5m)