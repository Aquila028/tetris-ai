#include <Arduino.h>
// core graphics library (written by Adafruit)
#include <Adafruit_GFX.h>

// Hardware-specific graphics library for MCU Friend 3.5" TFT LCD shield
#include <MCUFRIEND_kbv.h>
// LCD and SD card will communicate using the Serial Peripheral Interface (SPI)
// e.g., SPI is used to display images stored on the SD card
#include <SPI.h>

// needed for reading/writing to SD card
#include <SD.h>

#include <string.h>

#include <TouchScreen.h>
using namespace std;

#define JOY_CENTER	 512
#define JOY_DEADZONE 64

// many, many constant declarations
#define SD_CS 10
#define JOY_VERT	A9	// should connect A9 to pin VRx
#define JOY_HORIZ A8	// should connect A8 to pin VRy
#define JOY_SEL	 53
#define CLOCKWISE_BUTTON 47
#define COUNTER_BUTTON 45

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH	480
#define DISPLAY_HEIGHT 320

// thresholds to determine if there was a touch
#define MINPRESSURE	 10
#define MAXPRESSURE 1000

MCUFRIEND_kbv tft;

enum States {
	InitialSend, WaitingForAck, SendingPiece, Error, ProcessingPiece
};

// Global game board array
short tiles[10][20] = {0};

// colour array: black, cyan, blue, orange, yellow, green, purple, red, white.
const int32_t colors[9] = {0x0000, 0x07FF, 0x001F, 0xFA60, 0xFFE0,
													 0x07E0, 0xF81F, 0xF800, 0xFFFF};

// score vars
int score = 0;
int combo = 0;
int level = 0;
int linesCleared = 0;
unsigned long speedUp = 800;

// offset vars
int offTotalX, offTotalY;

// rotation data
short JLSTZoffset[5][4];
short Ioffset[5][4];

// block setup: block coordinates represent relative position
// of each block in the tetromino, wrt the previous position.
// base block is the tile at [4][20] on the play field
// 0-3: locations on row 20; 4-7: locations on row 19
// Blocks: I, J, L, O, S, T, Z
const int tetromino[7][4] = {{5, 4, 6, 7},	// I
														 {6, 5, 1, 7},	// J
														 {6, 5, 7, 3},	// L
														 {5, 2, 1, 6},	// O
														 {6, 5, 2, 3},	// S
														 {6, 5, 2, 7},	// T
														 {6, 2, 1, 7}};	// Z
// current tile
int currentPiece[4][2];
int beforeRot[4][2];
int currentColour;
int currentRotIndex = 0;
bool activePiece = false;
int randomNums[7] = {3, 3, 3, 3, 3, 3, 3};
// timer locks
int aiLock = 0;
int shiftLock = 0; 
int rotLock = 0;
unsigned long fallTimer = millis();


// setup
void setup() {
	init();
	// seed rng
	randomSeed(analogRead(15));
	// burn first number (required)
	random();
	Serial.begin(9600);
	pinMode(JOY_SEL, INPUT_PULLUP);

	// initialize button pins
	pinMode(CLOCKWISE_BUTTON, INPUT_PULLUP);
	pinMode(COUNTER_BUTTON, INPUT_PULLUP);

	// rotation matrix data
	// storage legend - x -> 0s digit, y -> 10s digit
	// -2 -> 1, -1 -> 2, 0 -> 3, 1 -> 4, 2 -> 5
	JLSTZoffset[0][0] = 33;
	JLSTZoffset[0][1] = 33;
	JLSTZoffset[0][2] = 33;
	JLSTZoffset[0][3] = 33;

	JLSTZoffset[1][0] = 33;
	JLSTZoffset[1][1] = 43;
	JLSTZoffset[1][2] = 33;
	JLSTZoffset[1][3] = 23;

	JLSTZoffset[2][0] = 33;
	JLSTZoffset[2][1] = 42;
	JLSTZoffset[2][2] = 33;
	JLSTZoffset[2][3] = 22;

	JLSTZoffset[3][0] = 33;
	JLSTZoffset[3][1] = 35;
	JLSTZoffset[3][2] = 33;
	JLSTZoffset[3][3] = 35;

	JLSTZoffset[4][0] = 33;
	JLSTZoffset[4][1] = 45;
	JLSTZoffset[4][2] = 33;
	JLSTZoffset[4][3] = 25;

	// I piece data
	Ioffset[0][0] = 33;
	Ioffset[0][1] = 23;
	Ioffset[0][2] = 24;
	Ioffset[0][3] = 34;

	Ioffset[1][0] = 23;
	Ioffset[1][1] = 33;
	Ioffset[1][2] = 44;
	Ioffset[1][3] = 34;

	Ioffset[2][0] = 53;
	Ioffset[2][1] = 33;
	Ioffset[2][2] = 14;
	Ioffset[2][3] = 34;

	Ioffset[3][0] = 23;
	Ioffset[3][1] = 34;
	Ioffset[3][2] = 43;
	Ioffset[3][3] = 32;

	Ioffset[4][0] = 53;
	Ioffset[4][1] = 31;
	Ioffset[4][2] = 13;
	Ioffset[4][3] = 35;
	
	// tft display initialization
	uint16_t ID = tft.readID();
	tft.begin(ID);
	tft.setRotation(0);
	tft.fillScreen(TFT_BLACK);
	tft.drawLine(241, 0, 241, 480, colors[8]);
}

// generate next piece ID; NOT random
// weighted to give spacing between identical pieces
// leaves 3 gap minimum between identical pieces
// void input, int return
// int getNext() {
// 	int temp;
// 	bool done = false;
// 	while (!done) {
// 		temp = random(7);
// 		if (randomNums[temp] == 3) {
// 			done = true;
// 		}
// 	}
// 	for (int i = 0; i < 7; i ++) {
// 		if (randomNums[i] < 3) {
// 			randomNums[i] += 1;
// 		}
// 	}
// 	randomNums[temp] = 0;
// 	return temp;
// }

// draws block at a given index
// xy coord int input, void return
// void drawblock(int x, int y) {
// 	tft.fillRect(x*24, 456 - y*24, 24, 24, colors[tiles[x][y]]);
// 	tft.drawRect(x*24, 456 - y*24, 24, 24, colors[0]);
// }

// // returns true if the joystick is attempted to be moved
// // outside the deadzone, false otherwise.
// // takes in x and y values of the joystick, boolean return.
// bool moveAttempt(int X, int Y) {
// 	if (abs(X-JOY_CENTER) > JOY_DEADZONE || abs(Y-JOY_CENTER) > JOY_DEADZONE) {
// 		return true;
// 	}
// 	return false;
// }

// checks if the active piece can move in a given direction
// intput: (int) direction: the direction to try and move
// 1 = right, -1 = left, 1 = up, -1 = down, 0 = stay still
// output: boolean return
bool canMove(int directionX, int directionY) {
	int tempX;
	int tempY;
	for (int i = 0; i < 4; i ++) {
		tempX = currentPiece[i][0];
		tempY = currentPiece[i][1];
		if (tempX+directionX < 0 || tempX+directionX > 9 || tempY+directionY < 0 || tempY+directionY > 19 || tiles[tempX+directionX][tempY+directionY] != 0) {
			return 0;
		}
	}
	return 1;
}

// moves the active piece can move in a given direction
// intput: (int) direction: the direction to move
// 1 = right, 2 = down, 3 = left, 4 = up
// void return
// void activeShift(int direction) {
// 	int tempX;
// 	int tempY;
// 	shiftLock = 2500;
// 	// right
// 	if (direction == 1) {
// 		for (int i = 0; i < 4; i ++) {
// 			tempX = currentPiece[i][0];
// 			tempY = currentPiece[i][1];
// 			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
// 			tempX++;
// 			currentPiece[i][0] = tempX;
// 		}
// 	// left
// 	} else if (direction == 3) {
// 		for (int i = 0; i < 4; i ++) {
// 			tempX = currentPiece[i][0];
// 			tempY = currentPiece[i][1];
// 			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
// 			tempX--;
// 			currentPiece[i][0] = tempX;
// 		}
// 	// down
// 	} else if (direction == 2) {
// 		shiftLock = 0;
// 		for (int i = 0; i < 4; i ++) {
// 			tempX = currentPiece[i][0];
// 			tempY = currentPiece[i][1];
// 			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
// 			tempY--;
// 			currentPiece[i][1] = tempY;
// 		}
// 	} 
// 	// draw piece in new location
// 	for (int i = 0; i < 4; i++) {
// 		tempX = currentPiece[i][0];
// 		tempY = currentPiece[i][1];
// 		tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
// 		tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
// 	}
// }

// void updateScore() {
// 	tft.fillRect(250, 130, 60, 80, colors[0]);
// 	// tft.println(combo);
// 	// tft.setCursor(270,130);
// 	// tft.println(level);
// 	// tft.setCursor(270,150);
// 	// tft.println(speedUp);
// 	// tft.setCursor(270,170);
// 	// tft.println(score);
// 	if ((combo % 4) != 0) {
// 		score += (combo % 4)*100;
// 		linesCleared += (combo % 4);
// 		combo = 0;
// 	}
// 	else if (combo == 4){
// 		score += 800;
// 		linesCleared += 4;
// 	}
// 	else{
// 		score += 1200;
// 		linesCleared += 4;
// 	}
// 	tft.setCursor(255,140);
// 	tft.println(score);

// 	level = linesCleared/10;

// 	// speed Aaron you have to look at the code for this one cause i deleted some of it
// 	if (level < 9) {
// 		speedUp = (48 - level*5)*100/6; 
// 	} else if (level < 27) {
// 		speedUp = (9 - level/3)*100/6;
// 	} else {
// 		speedUp = 100/6;
// 	}
// }

// // runs a test for doing line clears
// void clearCheck() {
// 	bool shift = false;
// 	int shiftLevel = 1;
// 	int lowest = 20;
// 	int unique[4] = {-1, -1, -1, -1};
// 	bool clear = false;
// 	int temp;
// 	// check which lines to test
// 	for (int i = 0; i < 4; i ++) {
// 		temp = currentPiece[i][1];
// 		if (unique[0] != temp && unique[1] != temp &&
// 				unique[2] != temp && unique[3] != temp) {
// 			unique[i] = currentPiece[i][1];
// 		}
// 	}
// 	// test each line
// 	for (int i = 0; i < 4; i ++) {
// 		if (unique[i] != -1) {
// 			clear = true;
// 			for (int j = 0; j < 10; j ++) {
// 				if (tiles[j][unique[i]] == 0) {
// 					clear = false;
// 				}
// 			}
// 			// remove cleared lines
// 			if (clear) {
// 				if (lowest > unique[i]) {
// 					lowest = unique[i];
// 				}
// 				clear = false;
// 				shift = true;
// 				for (int j = 0; j < 10; j ++) {
// 					tft.fillRect(j*24, 456 - unique[i]*24, 24, 24, colors[0]);
// 				}
// 				unique[i] += 20;
// 			}
// 		}
// 	}
// 	if (shift) {
// 		// shift down blocks
// 		for (int i = lowest + 1; i < 20; i ++) {
// 			if (unique[0] - 20 != i && unique[1] - 20 != i &&
// 				unique[2] - 20 != i && unique[3] - 20 != i){
// 				for (int j = 0; j < 10; j ++) {
// 					tiles[j][i - shiftLevel] = tiles[j][i];
// 					drawblock(j, i - shiftLevel);
// 				}
// 			} else {
// 				shiftLevel ++;
// 			}
// 		}
// 		combo += shiftLevel;
// 		updateScore();
// 		//Serial.println(shiftLevel);
// 		for (int i = 20 - shiftLevel; i < 20; i++) {
// 			for (int j = 0; j < 10; j ++) {
// 				tiles[j][i] = 0; 
// 				tft.fillRect(j*24, 456 - i*24, 24, 24, colors[0]); 
// 			} 
// 		}
// 	}
// }

// locks current piece to the grid
// no inputs, void return
// void lockPiece() {
// 	for (int i = 0; i < 4; i++) {
// 		tiles[currentPiece[i][0]][currentPiece[i][1]] = currentColour;
// 	}
// 	activePiece = false;
// 	clearCheck();
// }


int convertCoord (short num, bool isX) {
	/*
		Converts coordinates into usable rotation offsets
		Parameters:
			num (short): number to convert
			isX (bool): number is an x-coordinate
	*/
	if (isX) {
		return (num / 10) - 3;
	} else {
		return (num % 10) - 3;
	}
}


void rotateTile(int xCoord, int yCoord, int refX, int refY, int clockRot, int tileIndex) {
	/*
		Rotates individual tiles.
		Parameters:
			xCoord (int): x-coordinate of tile to rotate
			yCoord (int): y-coordinate of tile to rotate
			refX (int): x-coordinate of pivot tile
			refY (int): y-coordinate of pivot tile
			clockRot (int): clockwise or not [1 for CW]
			tileIndex (int): index of tile in piece
	*/
	// variable declaration
	int relativeX = xCoord - refX;
	int relativeY = yCoord - refY;
	int newX;
	int newY;
	// check for CW or CCW
	if (clockRot == 1) {
		newX = relativeY;
		newY = -relativeX;
	} else {
		newX = -relativeY;
		newY = relativeX;
	}
	newX += refX;
	newY += refY;
	// store into globals
	int tempX = currentPiece[tileIndex][0];
	int tempY = currentPiece[tileIndex][1];

	beforeRot[tileIndex][0] = tempX;
	beforeRot[tileIndex][1] = tempY;
	currentPiece[tileIndex][0] = newX;
	currentPiece[tileIndex][1] = newY;
}


void runRotTest (int blockType, int oldRotIndex, int newRotIndex, int testNum) {
	/*
		Runs rotation offset tests.
		Parameters:
			blockType (int): Indicates the type of piece
			oldRotIndex (int): Rotation index before rotation performed
			newRotIndex (int): Rotation index after rotation performed
			testNum (int): Which test is being used (5 tests total)
	*/
	// I block test
	if (blockType == 1) {
		offTotalX = convertCoord(Ioffset[testNum][oldRotIndex], true) - convertCoord(Ioffset[testNum][newRotIndex], true);
		offTotalY = convertCoord(Ioffset[testNum][oldRotIndex], false) - convertCoord(Ioffset[testNum][newRotIndex], false);
	// everything else test
	} else {
		offTotalX = convertCoord(JLSTZoffset[testNum][oldRotIndex], true) - convertCoord(JLSTZoffset[testNum][newRotIndex], true);
		offTotalY = convertCoord(JLSTZoffset[testNum][oldRotIndex], false) - convertCoord(JLSTZoffset[testNum][newRotIndex], false);
	}
}


void attemptRotation(int clockwise, bool doOffset) {
	/*
		Attempts to rotate a piece.
		Parameters:
			clockwise (int): Indicates if rotation is CW or CCW [1 for CW, -1 for CCW]
			doOffset (bool): Indicate if offset tests should be run
	*/
	// variable declaration
	int blockType;
	int oldRotIndex = currentRotIndex;
	int tempX;
	int tempY;
	int befX;
	int befY;
	String inLine;
	currentRotIndex += clockwise;
	currentRotIndex = (currentRotIndex % 4 + 4) % 4;	// 4 is amount of possibl rot. indices

	// perform rotation for each tile
	for (int i = 0; i < 4; ++i) {
		rotateTile(currentPiece[i][0], currentPiece[i][1], currentPiece[0][0], currentPiece[0][1], clockwise, i);
	}
	// stop if no offset necessary
	if (!doOffset) {
		for (int i = 0; i < 4; ++i) {
			int tempX = currentPiece[i][0];
			int tempY = currentPiece[i][1];
			// draw on board
			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
			tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
		}
		return;
	}
	// determine piece type
	if (colors[currentColour] == 0x07FF) {	// cyan = I piece
		blockType = 1;
	} else if (colors[currentColour] == 0xFFE0) {	// yellow = O piece
		attemptRotation(-clockwise, false);
		return;
	} else {	// other
		blockType = 2;
	}
	// run offset tests
	for (int i = 0; i < 5; ++i) {
		runRotTest(blockType, oldRotIndex, currentRotIndex, i);
		// check if can move
      	if(canMove(offTotalX, offTotalY)) {
        	for (int j = 0; j < 4; ++j) {
          	// apply offset
          	currentPiece[j][0] += offTotalX;
          	currentPiece[j][1] += offTotalY;
          	// erase previous position
          	befX = beforeRot[j][0];
          	befY = beforeRot[j][1];
          	tft.fillRect(befX*24, 456 - befY*24, 24, 24, colors[0]);
        	}
        	for (int j = 0; j < 4; ++j) {
          	// draw new position
          	tempX = currentPiece[j][0];
          	tempY = currentPiece[j][1];
          	tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
          	tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
        }
        return;
      }
	}
	// rotate backwards if all offset tests fail
	attemptRotation(-clockwise, false);
}
	

void processJoystick() {
	// joystick inputs
	int xVal = analogRead(JOY_HORIZ);
	int yVal = analogRead(JOY_VERT);
	int buttonVal = digitalRead(JOY_SEL);

	// check move
	if (moveAttempt(xVal, yVal)) {
		// check direction, move if able
		if (yVal < JOY_CENTER - JOY_DEADZONE) {
		} else if ((yVal > JOY_CENTER + JOY_DEADZONE)) {
			if (canMove(0, -1)) {
				activeShift(2);
			} else {
				// hit the bottom
				lockPiece();
			}
		} else if ((xVal > JOY_CENTER + JOY_DEADZONE) && canMove(-1, 0)) {
			activeShift(3);
		} else if ((xVal < JOY_CENTER - JOY_DEADZONE) && canMove(1, 0)) {
			activeShift(1);		 
		}
	}
}

// main loop
void tetris() {
	tft.fillRect(250, 250, 60, 80, colors[0]);
	tft.setCursor(255,260);
	tft.println("debug");
	// var dec
	bool gameActive = true;
	int tempX;
	int tempY;
	int next;
	int nextX;
	int nextY;
	int temp;
	int joyVal = digitalRead(JOY_SEL);
	bool aiActive = false;
	String strTemp;
	States clientState = InitialSend;

	// AI stuff
	int moveInstr;
	String strInstr;
	int remActions = 0;
	bool moveLeft = false;
	int rots = 0;

	activePiece = true;
	temp = getNext();
	currentRotIndex = 0;
	currentColour = temp + 1;
	// create piece & draw it
	for (int i = 0; i < 4; i ++) {
		tempX = tetromino[temp][i]%4 + 3;
		tempY = (tetromino[temp][i] < 4) + 18;
		currentPiece[i][0] = tempX;
		currentPiece[i][1] = tempY;
		tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
		tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
	}

	next = getNext();
	tft.setCursor(255, 10);
	tft.setTextSize(2);
	tft.print("NEXT:");
	tft.setCursor(253, 110);
	tft.print("SCORE");
	tft.setCursor(255, 140);
	tft.println(score);
	for (int i = 0; i < 4; i ++) {
		nextX = tetromino[next][i]%4 + 3;
		nextY = (tetromino[next][i] < 4) + 18;
		tft.fillRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[next + 1]);
		tft.drawRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[0]);
	}

	while (gameActive) {
		// gen new piece
		joyVal = digitalRead(JOY_SEL);
		if (!activePiece) {
			activePiece = true;
			currentRotIndex = 0;
			currentColour = next + 1;
			// create piece & draw it
			for (int i = 0; i < 4; i ++) {
				tempX = tetromino[next][i]%4 + 3;
				tempY = (tetromino[next][i] < 4) + 18;
				currentPiece[i][0] = tempX;
				currentPiece[i][1] = tempY;
				tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
				tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
			}
			next = getNext();
			tft.fillRect(245, 30, 60, 60, colors[0]);
			for (int i = 0; i < 4; i ++) {
				nextX = tetromino[next][i]%4 + 3;
				nextY = (tetromino[next][i] < 4) + 18;
				tft.fillRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[next + 1]);
				tft.drawRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[0]);
			}
		}

		// cooldown between moves
		if (aiLock == 0) {
			if (joyVal == 0 && !aiActive) {
				aiActive = true;
				strTemp = "I ";
				// concatenate tile data
				for (int i = 0; i < 200; ++i) {
					strTemp += String(tiles[i%10][i/10]);
				}
				Serial.println(strTemp);
				clientState = WaitingForAck;
				// wait for acknowledgement from server
				while (clientState == WaitingForAck) {
					Serial.setTimeout(50);
					strTemp = Serial.readString();
					if (strTemp[0] == 'A') {
						clientState = SendingPiece;
					}
				}
				// sending current piece
				strTemp = "C ";
				for (int i = 0; i < 4; ++i) {
					strTemp += String(currentPiece[i][0]);
					strTemp += " ";
					strTemp += String(currentPiece[i][1]);
					strTemp += " ";
				}
				strTemp += currentRotIndex;
				Serial.println(strTemp);
				clientState = WaitingForAck;
				// waiting for acknowledgement from server
				while (clientState == WaitingForAck) {
					Serial.setTimeout(50);
					strTemp = Serial.readString();
					if (strTemp[0] == 'A') {
						clientState = SendingPiece;
					}
				}
			} else if (joyVal == 0) {
				aiActive = false;
			}
			aiLock = 1000;
		} else {
			aiLock--;
		}
		// ai inactive
		if (!aiActive) {
			if (shiftLock == 0) {
				processJoystick();
			} else {
				shiftLock--;
			}
			if (activePiece) {
				// rotation check
				if(rotLock == 0) {
					if (digitalRead(CLOCKWISE_BUTTON) == LOW) {
						// 1 is for clockwise
						attemptRotation(1, true);
						rotLock = 500;
					} else if (digitalRead(COUNTER_BUTTON) == LOW) {
						// -1 is for counterclockwise
						attemptRotation(-1, true);
						rotLock = 500;
					}
				} else {
					rotLock--;
				}

				if (fallTimer + speedUp <= millis()) {
					fallTimer = millis();
					if (canMove(0, -1)) {
						activeShift(2);
					} else {
						lockPiece();
					}
				}
			}
		// ai active
		} else {
			if (clientState == SendingPiece) {
				strTemp = "R " + String(next);
				Serial.println(strTemp);
				clientState = WaitingForAck;
			} else if (clientState == WaitingForAck) {
				Serial.setTimeout(50);
				strTemp = Serial.readString();
				if (strTemp[0] == 'A') {
					strInstr = "";
					clientState = ProcessingPiece;
					debug(strTemp);
					if (strTemp[2] == '-') {
						moveLeft = true;
						remActions = strTemp[3] - 48;
						rots = strTemp[4] - 48;
						for (int i = 0; i < 3; ++i) {
							strInstr += strTemp[2+i];
						}
					} else {
						moveLeft = false;
						if (strTemp[2] != '9') {
							remActions = strTemp[2] - 48;
						} else {
							remActions = 0;
						}
						rots = strTemp[3] - 48;
						for (int i = 0; i < 2; ++i) {
							strInstr += strTemp[2+i];
						}
					}
					debug2(strInstr);
					moveInstr = strInstr.toInt();
					remActions += rots;
				}
			} else if (clientState == ProcessingPiece) {
				// emulate move
				for (int i = 0; i < abs(moveInstr)%10; i++) {
					attemptRotation(1, true);
				}
				// shift
				for (int i = 0; i < abs(moveInstr/10); i++) {
					if (moveInstr/10 != 9) {
						if (moveInstr > 0 && canMove(1, 0)) {
							activeShift(1);
						} else if (canMove(-1, 0)) {
							activeShift(3);
						}
					}
				}
				// move down
				while (canMove(0, -1)) {
					activeShift(2);
				}
				lockPiece();
				clientState = SendingPiece;
			}

			// end ai
			if (activePiece) {
				if (fallTimer + speedUp <= millis()) {
					fallTimer = millis();
					if (canMove(0, -1)) {
						activeShift(2);
					} else {
						lockPiece();
					}
				}	
			}
		}
		// end game
		for (int i = 0; i < 10; i++) {
			if (tiles[i][19] != 0) {
				gameActive = false;
				if (aiActive) {
					Serial.println("X\n");
				}
			}
		}
	}
}

// main
int main() {
	setup();
	// main loop
	while (true) {
		tetris();
		while (digitalRead(JOY_SEL) == 1);
		score = 0;
		combo = 0;
		level = 0;
		linesCleared = 0;
		speedUp = 800;
		shiftLock = 0; 
		rotLock = 0;
		fallTimer = millis();
		for (int i = 0; i < 7; i ++) {
			randomNums[i] = 3;
		}
		tft.fillScreen(TFT_BLACK);
		tft.drawLine(241, 0, 241, 480, colors[8]);
		for (int i = 0; i < 200; i ++) {
			tiles[i%10][i/10] = 0;
		}
	}
	Serial.flush();
	Serial.end();
	return 0;
}
