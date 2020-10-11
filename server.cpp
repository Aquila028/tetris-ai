#include <utility>
#include <string>
#include <iostream>
#include <cmath>

#include "serialport.h"

weighting constant def
#define HEIGHT_WEIGHT 2	// polynomial
#define FLAT_WEIGHT 100	 // standard deviation formula
#define HOLE_WEIGHT 500 // constant
#define LINE_WEIGHT 15 //
#define DEATH_WEIGHT 10000
#define TETRIS_WEIGHT 10000
#define PIT_WEIGHT 100

using namespace std;

enum States {
	Receive, Error
};

const int tetromino[7][4] = {{5, 4, 6, 7},	// I
							{6, 5, 1, 7},	// J
							{6, 5, 7, 3},	// L
							{5, 2, 1, 6},	// O
							{6, 5, 2, 3},	// S
							{6, 5, 2, 7},	// T
							{6, 2, 1, 7}};	// Z

// weighting functions



// rotation data
int pieceNum;
int JLSTZoffset[5][4];
int Ioffset[5][4];
int offTotalX, offTotalY;
// rotation matrix data
// storage legend - x -> 0s digit, y -> 10s digit
// -2 -> 1, -1 -> 2, 0 -> 3, 1 -> 4, 2 -> 5

// game state var dec
int tiles[10][20] = {0};
int tempTiles[10][20] = {0};
int currentPiece[4][2];
int initPos[4][2];
int tempInitPos[4][2];
// tens = horizontal shift (=-); ones = rotation
int moveInstr = 0;
int highScore = -214748;
int currentRotIndex = 0;
int moveLeft = 0;
int moveRight = 0;


// moves the active piece can move in a given direction
// intput: (int) direction: the direction to move
// 1 = right, 2 = down, 3 = left
// // void return
// void activeShift(int direction) {
// 	int tempX;
// 	int tempY;
// 	// right
// 	if (direction == 1) {
// 		for (int i = 0; i < 4; i ++) {
// 			tempX = currentPiece[i][0];
// 			tempY = currentPiece[i][1];
// 			tempX++;
// 			currentPiece[i][0] = tempX;
// 		}
// 	// left
// 	} else if (direction == 3) {
// 		for (int i = 0; i < 4; i ++) {
// 			tempX = currentPiece[i][0];
// 			tempY = currentPiece[i][1];
// 			tempX--;
// 			currentPiece[i][0] = tempX;
// 		}
// 	// down
// 	} else if (direction == 2) {
// 		for (int i = 0; i < 4; i ++) {
// 			tempX = currentPiece[i][0];
// 			tempY = currentPiece[i][1];
// 			tempY--;
// 			currentPiece[i][1] = tempY;
// 		}
// 	} 
// }

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

void rotateTile(int xCoord, int yCoord, int refX, int refY, int clockRot, int tileIndex, int arrayID) {
	/*
		Rotates individual tiles.
		Parameters:
			xCoord (int): x-coordinate of tile to rotate
			yCoord (int): y-coordinate of tile to rotate
			refX (int): x-coordinate of pivot tile
			refY (int): y-coordinate of pivot tile
			clockRot (int): clockwise or not [1 for CW]
			tileIndex (int): index of tile in piece
			arrayID (int): indicates which array to update
	*/
	// variable declaration
	int relativeX = xCoord - refX;
	int relativeY = yCoord - refY;
	int newX;
	int newY;
	// check if CW or CCW
	if (clockRot == 1) {
	    newX = relativeY;
	    newY = -relativeX;
	} else {
		newX = -relativeY;
		newY = relativeX;
	}
	newX += refX;
	newY += refY;

	// store into proper array
	if (arrayID == 0) {
		currentPiece[tileIndex][0] = newX;
		currentPiece[tileIndex][1] = newY;
	} else {
		tempInitPos[tileIndex][0] = newX;
		tempInitPos[tileIndex][1] = newY;
	}
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


void attemptRotation(int clockwise, bool doOffset, int arrayID, int currRot) {
	/*
		Attempts to rotate a piece.
		Parameters:
			clockwise (int): Indicates if rotation is CW or CCW [1 for CW, -1 for CCW]
			doOffset (bool): Indicate if offset tests should be run
			arrayID (int): ID for which array to update
			currRot (int): Current rotation index
	*/
	// variable declaration
	int blockType;
	int oldRotIndex = currRot;
	int newRotIndex = currRot + clockwise;
	newRotIndex = (newRotIndex % 4 + 4) % 4;
	// perform rotation for each tile
	if (arrayID == 0) {
		for (int i = 0; i < 4; ++i) {
			rotateTile(currentPiece[i][0], currentPiece[i][1], currentPiece[0][0], currentPiece[0][1], clockwise, i, arrayID);
		}
	} else {
		for (int i = 0; i < 4; ++i) {
			rotateTile(tempInitPos[i][0], tempInitPos[i][1], tempInitPos[0][0], tempInitPos[0][1], clockwise, i, arrayID);
		}
	}
	// stop if no offset necessary
	if (!doOffset) {
		return;
	}
	// determine piece type
	if (pieceNum == 0) {	// cyan = I piece
		blockType = 1;
	} else if (pieceNum == 3) {	// yellow = O piece
		attemptRotation(-clockwise, false, arrayID, newRotIndex);
		return;
	} else {	// other
		blockType = 2;
	}
	// run offset tests
	for (int i = 0; i < 5; ++i) {
		runRotTest(blockType, oldRotIndex, newRotIndex, i);
		// check if can moves
      	if(canMove(offTotalX, offTotalY)) {
        	for (int j = 0; j < 4; ++j) {
	          	// apply offset
	          	if(arrayID == 0) {
		          	currentPiece[j][0] += offTotalX;
		          	currentPiece[j][1] += offTotalY;
	          	} else {
	          		tempInitPos[j][0] += offTotalX;
	          		tempInitPos[j][1] += offTotalY;
	          	}
          	}
        return;
      	}
	}
	// run reverse rotation if all tests fail
	attemptRotation(-clockwise, false, arrayID, newRotIndex);
}

// runs a test for doing line clears
int clearCheck() {
	bool shift = false;
	int shiftLevel = 1;
	int lowest = 20;
	int unique[4] = {-1, -1, -1, -1};
	bool clear = false;
	int temp;
	// check which lines to test
	for (int i = 0; i < 4; i ++) {
		temp = currentPiece[i][1];
		if (unique[0] != temp && unique[1] != temp &&
				unique[2] != temp && unique[3] != temp) {
			unique[i] = currentPiece[i][1];
		}
	}
	// test each line
	for (int i = 0; i < 4; i ++) {
		if (unique[i] != -1) {
			clear = true;
			for (int j = 0; j < 10; j ++) {
				if (tempTiles[j][unique[i]] == 0) {
					clear = false;
				}
			}
			// remove cleared lines
			if (clear) {
				if (lowest > unique[i]) {
					lowest = unique[i];
				}
				clear = false;
				shift = true;
				unique[i] += 20;
			}
		}
	}
	if (shift) {
		// shift down blocks
		for (int i = lowest + 1; i < 20; i ++) {
			if (unique[0] - 20 != i && unique[1] - 20 != i &&
				unique[2] - 20 != i && unique[3] - 20 != i){
				for (int j = 0; j < 10; j ++) {
					tempTiles[j][i - shiftLevel] = tempTiles[j][i];
				}
			} else {
				shiftLevel ++;
			}
		}
		//Serial.println(shiftLevel);
		for (int i = 20 - shiftLevel; i < 20; i++) {
			for (int j = 0; j < 10; j ++) {
				tempTiles[j][i] = 0; 
			} 
		}
	return shiftLevel;
	}
	return 0;
}

// // runs a test for doing line clears
// int realClearCheck() {
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
// 				}
// 			} else {
// 				shiftLevel ++;
// 			}
// 		}
// 		//Serial.println(shiftLevel);
// 		for (int i = 20 - shiftLevel; i < 20; i++) {
// 			for (int j = 0; j < 10; j ++) {
// 				tiles[j][i] = 0; 
// 			} 
// 		}
// 	return shiftLevel;
// 	}
// 	return 0;
// }

// locks current piece to the grid
// // no inputs, void return
// void lockPiece() {
// 	for (int i = 0; i < 4; i++) {
// 		// cout << currentPiece[i][0] << endl;
// 		// cout << "bufferLock" << endl;
// 		// cout << currentPiece[i][1] << endl;
// 		tempTiles[currentPiece[i][0]][currentPiece[i][1]] = 1;	
// 	}
// }

// void lockRealPiece() {
// 	for (int i = 0; i < 4; i++) {
// 		// cout << currentPiece[i][0] << endl;
// 		// cout << "bufferLock" << endl;
// 		// cout << currentPiece[i][1] << endl;
// 		tiles[currentPiece[i][0]][currentPiece[i][1]] = 1;	
// 	}
// }

// void unlockPiece() {
// 	for (int i = 0; i < 4; i++) {
// 		// cout << currentPiece[i][0] << endl;
// 		// cout << "bufferUnlock" << endl;
// 		// cout << currentPiece[i][1] << endl;
// 		tempTiles[currentPiece[i][0]][currentPiece[i][1]] = 0;	
// 	}
// }

void findFit() {
	int score = 0;
	int maxHeight = 0;
	int deviation = 0;
	int heights[10] = {0};
	int numHoles = 0;
	int numPits = 0;
	// emulate clear and get num cleared lines
	//cout << "check 1" << endl;
	int numClear = clearCheck();
	//cout << "check 2" << endl;
	// max height & bumpiness check
	for (int i = 0; i < 10; i ++) {
		for (int j = 0; j < 20; j++) {
			// store height of each column
			if (tempTiles[i][j] != 0) {
				heights[i] = j;
			}
		}
		// find max height
		if (maxHeight < heights[i]) {
			maxHeight = heights[i];
		}
	}
	//cout << "MAX: " << maxHeight << endl;
	// score height; polynomial
	
	score -= pow(maxHeight, HEIGHT_WEIGHT)*3;
	if (maxHeight > 18) {
		score -= DEATH_WEIGHT;
	}
	// do SD
	maxHeight = 0;
	for (int i = 0; i < 10; i ++) {
			maxHeight += heights[i];
	}
	if (maxHeight%10 < 5){
		maxHeight = maxHeight/10;
	} else {
		maxHeight = 1 + maxHeight/10;
	}
	for (int i = 0; i < 10; i ++) {
		deviation += abs(maxHeight - heights[i]);
	}
	// flatness score; linear
	score -= deviation*FLAT_WEIGHT;
	// line weight
	score += pow(LINE_WEIGHT, numClear);
	//score += TETRIS_WEIGHT;
	//cout << "check 5" << endl;
	for (int i = 0; i < 10; i ++) {
		for (int j = 0; j < heights[i]; j++) {
			if (tempTiles[i][j] == 0) {
				numHoles++;
			}
		}
	}
	score -= numHoles*HOLE_WEIGHT;
	//pits
	for (int i = 0; i < 10; i ++) {
		for (int j = 0; j < maxHeight; j++) {
			if (tempTiles[i][j] == 0) {
				numPits++;
			}
		}
	}
	score -= numPits*PIT_WEIGHT;
	//cout << "check 6" << endl;
	// evaluate move
	for (int i = 0; i < 8; i ++) {
		tempInitPos[i%4][i/4] = initPos[i%4][i/4];
	}
	//cout << "score" << score << endl;
	if (highScore < score) {
		highScore = score;
		for (int i = 0; i < 4; ++i) {
			tempInitPos[i][0] = initPos[i][0];
			tempInitPos[i][1] = initPos[i][1];
		}
		for (int i = 0; i < currentRotIndex; i++) {
			attemptRotation(1, true, 2, i);
		}

		//cout << "check 7" << endl;
		moveInstr = (currentPiece[0][0] - tempInitPos[0][0])*10;
		if (moveInstr == 0) {
			moveInstr = 90;
		}
		// mystery code
		if (pieceNum == 0 && (currentRotIndex == 1) && moveInstr != 90) {
			moveInstr -= 10;
		} else if (pieceNum == 0 && (currentRotIndex == 1) && moveInstr == 90) {
			moveInstr = -10;
		}
		// mystery code
		if (moveInstr > 0) {
			moveInstr += currentRotIndex;
		} else {
			moveInstr -= currentRotIndex;
		}
		cout << "position: " << currentPiece[0][0] << " " << currentPiece[0][1] << endl;
		cout << "MOOOOVVVVEEEEE" << moveInstr << endl;
		cout << "rot: " << currentRotIndex << endl;
	}
	// restore temp tiles
	for (int i = 0; i < 10; ++i) {
		for (int j = 0; j < 20; ++j) {
			tempTiles[i][j] = tiles[i][j];
		}
	}
}

void calculateMove() {
	// outer rotation loop
	int dropCounter = 0;
	highScore = -214748;
	for (int i = 0; i < 4; ++i) {
		moveRight = 0;
		moveLeft = 0;
		currentRotIndex = i;
		for (int j = 0; j < 4; ++j) {
			currentPiece[j][0] = initPos[j][0];
			currentPiece[j][1] = initPos[j][1];
		}
		// perform rotation for each tile
		for (int j = 0; j < currentRotIndex; ++j) {
			//cout << "rot loop #: " << j << endl;
			attemptRotation(1, true, 0, j);
		}
		// calc max right
		while (canMove(1, 0)) {
			moveRight++;
			activeShift(1);
		}
		// recentre
		for (int k = 0; k < moveRight; k++) {
			activeShift(3);
		}
		// calc max left
		while (canMove(-1, 0)) {
			moveLeft++;
			activeShift(3);
		}
		// test all left cases and center
		for (int j = 0; j < moveLeft + moveRight; ++j) {
			// drop
			dropCounter = 0;
			while (canMove(0, -1)) {
				activeShift(2);
				dropCounter++;
			}
			lockPiece();
			// calc weight
			findFit();
			for (int k = 0; k < 4; ++k) {
				currentPiece[k][0]++;
				currentPiece[k][1] += dropCounter;
			}
		}
		// last case
		while (canMove(0, -1)) {
			activeShift(2);
		}
		lockPiece();

		// calc weight
		findFit();
	}

}


int main() {
	// comm var dec
	SerialPort port;
	States serverState = Receive;
	string inLine;
	string temp;
	int tempX, tempY = 0;

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

	while(true) {
		while (serverState == Receive) {
			inLine = port.readline();
			// cout << inLine << endl;
			if (inLine[0] == 'I') {
				for (int i = 0; i < 200; ++i) {
					tiles[i%10][i/10] = inLine[2+i] - '0';
				}
				//debug
				for (int i = 19; i >= 0; i --) {
					for (int j = 0; j < 9; j++) {
						cout << tiles[j][i];
					}
					cout << tiles[9][i] << endl;
				}
				port.writeline("A\n");
				cout << "A" << endl;
			} else if (inLine[0] == 'C') {
				int index = 2;
				for (int i = 0; i < 4; ++i) {
					temp = "";
					while (inLine[index] != ' ') {
						temp+= inLine[index];
						index++;
					}
					index++;
					currentPiece[i][0] = stoi(temp);
					initPos[i][0] = stoi(temp);
					temp = "";
					while (inLine[index] != ' ') {
						temp+= inLine[index];
						index++;
					}
					index++;
					currentPiece[i][1] = stoi(temp);
					currentRotIndex = inLine[index] - '0';
					initPos[i][0] = stoi(temp);
				}
				// drop the first piece like a rock
				while (canMove(0, -1)) {
					activeShift(2);
				}
				lockRealPiece();
				// restore temp tiles
				for (int i = 0; i < 10; ++i) {
					for (int j = 0; j < 20; ++j) {
						tempTiles[i][j] = tiles[i][j];
					}
				}
				moveInstr = 0;
				port.writeline("A\n");
				cout << "A" << endl;
			} else if (inLine[0] == 'R') {
				port.writeline("A " + to_string(moveInstr) + "\n");
				// create piece & draw it
				temp = "";
				temp += inLine[2];
				cout << "temp " << temp << endl;
				for (int i = 0; i < 4; i ++) {
					pieceNum = stoi(temp);
					tempX = tetromino[pieceNum][i]%4 + 3;
					tempY = (tetromino[pieceNum][i] < 4) + 18;
					currentPiece[i][0] = tempX;
					currentPiece[i][1] = tempY;
					initPos[i][0] = tempX;
					initPos[i][1] = tempY;
				}
				calculateMove();
				// rotate
				// recentre piece
				for (int i = 0; i < 8; i ++) {
					currentPiece[i%4][i/4] = initPos[i%4][i/4];
				}
				// emulate move
				for (int i = 0; i < abs(moveInstr)%10; i++) {
					attemptRotation(1, true, 0, i);
				}
				// shift
				for (int i = 0; i < abs(moveInstr/10); i++) {
					if (moveInstr/10 != 9) {
						if (moveInstr > 0 && canMove(1, 0)) {
							activeShift(1);
						} else if (canMove(-1, 0)){
							activeShift(3);
						}
					}
				}
				// move down
				while (canMove(0, -1)) {
					activeShift(2);
				}
				lockRealPiece();
				// restore temp tiles
				realClearCheck();
				for (int i = 0; i < 10; ++i) {
					for (int j = 0; j < 20; ++j) {
						tempTiles[i][j] = tiles[i][j];
					}
				}
				//debug
				cout << "tiles:" << endl;
				for (int i = 19; i >= 0; i --) {
					for (int j = 0; j < 9; j++) {
						cout << tiles[j][i];
					}
					cout << tiles[9][i] << endl;
				}
			} else if (inLine[0] == 'X') {
				return 0;
			}
		}
	}
	return 0;
}
