
#include "gba.h"
#include "charsprites.h"
#include "tile.h"
#include "end.h"

#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>


ObjectAttributes oam_object_backbuffer[128];

extern int Divide(int, int);

short colmap[400] = { //collision map
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define min(x,y) (x > y ? y : x)
#define max(x,y) (x < y ? y : x)

const int FLOOR_Y = 160-16;
const int GRAVITY = 1;
const int WALK_SPEED = 2;
const int JUMP_VI = -6;

unsigned int gridx, gridy;

extern void Addition(int* final, int left, int right);
// need "extern" because the function body is in an external .s file

enum {
	INGAME,
	PAUSED,
	MENU,
	LOAD,
	FINISHED
} GameStates;

typedef struct {
	ObjectAttributes* attributes;

	int facing;
	int firstAnimFrame;
	int animFrame;
	int posX;
	int posY;
	int velX;
	int velY;
	int timeInAir;
} PlayerSprite;

typedef struct {
	ObjectAttributes* attributes;

	int posX;
	int posY;
} ObjectSprite;

void createPlayer(PlayerSprite* sprite, ObjectAttributes* attributes) {
	sprite->attributes = attributes;
	sprite->facing = 1;
	sprite->firstAnimFrame = 0;
	sprite->animFrame = 0;
	sprite->posX = 0;
	sprite->posY = FLOOR_Y;
	sprite->velX = 0;
	sprite->velY = 0;
	sprite->timeInAir = 0;
}

void createObject(ObjectSprite* sprite, ObjectAttributes* attributes) {
	sprite->attributes = attributes;

	sprite->posX = 100;
	sprite->posY = 130;
}

long gridpos(PlayerSprite* sprite, int addx, int addy) {   //returns the current grid value relative to the player + (addx, addy)
    gridx = (sprite->posX/12) + addx;
    gridy = (sprite->posY/8) + addy;
    return colmap[((20*gridy) + gridx)];
}

// Allow objects to move and have collision update
void setCollision(ObjectSprite* sprite, int previousX, int previousY) {
	int oldX = previousX/12;
	int oldY = previousY/8;

	gridx = (sprite->posX/12);
	gridy = (sprite->posY/8);

	if (oldX == gridx && oldY == gridy) return;

	colmap[((20*oldY) + oldX)] = 0;
	colmap[((20*gridy) + gridx)] = 1;
}

void tickPlayerAnimation(PlayerSprite* sprite) {
	ObjectAttributes* spriteAtributes = sprite->attributes;

	//TODO change this to reflect the closest point to the ground
	int inAir = sprite->posY != FLOOR_Y && gridpos(sprite, 0, 1) != 1;

	if (inAir) {
		sprite->firstAnimFrame = 56;
		sprite->animFrame = sprite->velY > 0 ? 1 : 0;
	} else {
		if (sprite->velX != 0) {
            sprite->firstAnimFrame = 32;
            sprite->animFrame = (++sprite->animFrame) % 3;
        } else {
            sprite->firstAnimFrame = 0;
            sprite->animFrame = (++sprite->animFrame) % 4;
        }
	}

	spriteAtributes->attr2 = sprite->firstAnimFrame + (sprite->animFrame * 8);
}

void tickObjectAnimation(ObjectSprite* sprite, int length) {
	
}

void doMovement(PlayerSprite* sprite, uint16 keyState) {
	if (keyState & KEY_LEFT) {
		sprite->facing = 0;
		if (gridpos(sprite,-1,0) != 1)
        	sprite->velX = -WALK_SPEED;
	} else if (keyState & KEY_RIGHT) {
		sprite->facing = 1;
		if (gridpos(sprite,1,0) != 1)
        	sprite->velX = WALK_SPEED;
	} else sprite->velX = 0;

	int inAir = sprite->posY != FLOOR_Y && gridpos(sprite, 0, 1) != 1;

	if (keyState & KEY_A) {
        if (!inAir) {
            sprite->velY = JUMP_VI;
            sprite->timeInAir = 0;
        }
    }

	if(inAir) {
		sprite->velY = gridpos(sprite, 0, 0) == 1 ? JUMP_VI + (5 * sprite->timeInAir) : JUMP_VI + (GRAVITY * sprite->timeInAir);
		sprite->velY = min(5, sprite->velY);
		sprite->timeInAir++;
	}

	if (gridpos(sprite,1,0) == 1 || gridpos(sprite,-1,0) == 1) sprite->velX = 0;

	if (gridpos(sprite, 0, 1) == 1) {
		if (sprite->velY >= 0)
			sprite->velY = 0;
	}

	sprite->posX += sprite->velX;
	sprite->posX = min(240-16, sprite->posX);
    sprite->posX = max(0, sprite->posX);

	// Addition(sprite->posY, sprite->posY, sprite->velY);
	sprite->posY += sprite->velY;
	sprite->posY = min(sprite->posY, FLOOR_Y);

	sprite->attributes->attr0 = 0x2000 + sprite->posY;
    sprite->attributes->attr1 = (sprite->facing ? 0x4000 : 0x5000) + sprite->posX;
}

int main(void) {
    memcpy(&MEM_TILE[4][0], charspritesTiles, charspritesTilesLen);
    memcpy(MEM_PALETTE, charspritesPal, charspritesPalLen);

	memcpy(&MEM_TILE[4][40], tileTiles, tileTilesLen);
	memcpy(&MEM_TILE[4][45], endTiles, endTilesLen);

	irqInit();
	irqEnable(IRQ_VBLANK);

	REG_DISPLAYCONTROL =  VIDEOMODE_0 | BACKGROUND_0 | ENABLE_OBJECTS | MAPPINGMODE_1D;

	PlayerSprite sprite;
	createPlayer(&sprite, &MEM_OAM[0]);

	ObjectSprite* object;
	int oamIndex = 1;

	int state = MENU;

	// main loop
	while (1) {
		scanKeys();
		uint16 keyState = keysHeld();

		switch (state) {
			case INGAME: {
				if (gridpos(&sprite, 0, 0) == 2) {
					state = FINISHED;
				}
				
				doMovement(&sprite, keyState);
				tickPlayerAnimation(&sprite);
				tickObjectAnimation(object, oamIndex - 1);

				if (keysDown() & KEY_START) state = PAUSED;
				if (keysDown() & KEY_SELECT) {
					sprite.posX = 0;
					sprite.posY = FLOOR_Y;
				}
			} break;
			case PAUSED: if (keysDown() & KEY_START) state = INGAME; break;
			case MENU: if (keysDown() & KEY_START) state = LOAD; break;
			case LOAD: {
				for (int i = 0; i < 360; i++) {
					if (colmap[i] == 1) {
						createObject(&object[oamIndex - 1], &MEM_OAM[i]);
						MEM_OAM[i].attr0 = 0x2000 + ((i / 20) * 8) + 8;
						MEM_OAM[i].attr1 = 0x4000 + ((i * 12) % 240) + 12;
						MEM_OAM[i].attr2 = 80;

						oamIndex++;
					} else if (colmap[i] == 2) {
					 	createObject(&object[oamIndex - 1], &MEM_OAM[i]);
						MEM_OAM[i].attr0 = 0x2000 + ((i / 20) * 8) + 8;
						MEM_OAM[i].attr1 = 0x4000 + ((i * 12) % 240) + 12;
					 	MEM_OAM[i].attr2 = 90;

						oamIndex++;
					}
				}
				// TODO set map

				state = INGAME;
			} break;
			case FINISHED: {
				for (int i = 0; i >= oamIndex; i++) {
					delete(&object[i]);
				}

				object = NULL;
			}
		}

		VBlankIntrWait();
	}
}
