

#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <abCircle.h>
#include <shape.h>
#include <p2switches.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buzzer.h"
#include "buzzer.c"

#define GREEN_LED BIT6

//temp coloring background
u_int bgColor = COLOR_BLACK; 

/////////////////////////////////////////////////////////////////////
//************** IGNORE THIS PART IT DIDNT WORK *****************
///////////////////////////////////////////////////////////////////

/*
//making maze, creating it as an abstract shape
typedef struct dangoMaze{
  void (* getBounds)(const struct dangoMaze* maze, const Vec2* center, Region* bounds);
  int(*check)(const struct dangoMaze* maze, const Vec2* center, const Vec2* pixel);
  const int size;
} Maze;

//actual making of the maze
// 1 turns on & fills the pixel
int mazeCheck(const Maze* maze, const Vec2* center, const Vec2* pixel){
  Region bounds;
  mazeBounds(maze, center, &bounds);
  if((bounds.topLeft.axes[0] == pixel->axes[0]) || (bounds.botRight.axes[0] == pixel->axes[0])){
    return 1;
  }else{
    return 0;

  }
}

//required by abshape, computes bounding box of object
//refer to the arrow part for this
void mazeBounds(const struct dangoMaze* maze, const Vec2* center, Region* bounds){
  int size = maze->size, halfSize = size/2;
  vec2Sub(&bounds->topLeft, center, &maze);
  vecAdd(&bounds->botRight, center, &maze);
}
*/


/*
//maze layer
Layer layer2 = {
  (AbShape*) &dangoMaze,
  {(screenWidth), (screenHeight)},
  {0,0}, {0,0},
  COLOR_GREEN,
  0
};
*/

///////////////////////////////////////////////////////////////////////

//first width, 2nd height
AbRect rect10 = {abRectGetBounds, abRectCheck, 50,.5};
int redrawScreen = 1; //to redraw screen
Region fieldFence; //fence around playing field
Region playerRegion;

AbRectOutline fieldOutline = {	
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
};


////////////////////////////////////
//********** LAYERS **********
///////////////////////////////////

//making circle
Layer player = {(AbShape *)&circle7,{106, (screenHeight-22)},{0,0}, {0,0},COLOR_VIOLET,0};

//field w/ circle player
Layer field = {(AbShape *) &fieldOutline,{(screenWidth/2),(screenHeight/2)},{0,0},{0,0},COLOR_GREEN,&player};

//adding obstacles, 2nd height, 1st width
Layer layer3 = {(AbShape *)&rect10,{(40),(42)},{0,0},{0,0},COLOR_GREEN, &field};

Layer layer2= {(AbShape *)&rect10,{(screenWidth-40),(68)},{0,0},{0,0},COLOR_GREEN,&layer3};

Layer layer1 = {(AbShape *)&rect10,{(40), (95)},{0,0},{0,0},COLOR_GREEN,&layer2};

Layer layer0 = {(AbShape *)&rect10,{(screenWidth-36),(120)},{0,0},{0,0},COLOR_GREEN,&layer1};


/////////////////////////////////////////////////
//************* MOVING LAYERS *****************
////////////////////////////////////////////////

typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer mlplayer = {&player, {0,0}, 0}; 
 
MovLayer ml3 = {&layer3, {0,0}, &mlplayer}; 
MovLayer ml2 = {&layer2, {0,0}, &ml3}; 
MovLayer ml1 = {&layer1, {0,0}, &ml2}; 
MovLayer ml0 = {&layer0, {0,0}, &ml1}; 

MovLayer left = {&player, {-5,0}, &mlplayer}; 
MovLayer right = {&player, {5,0}, &mlplayer}; 
MovLayer up = {&player, {0,-5}, &mlplayer}; 
MovLayer down = {&player, {0,5}, &mlplayer}; 

//from shape motion demo
void movLayerDraw(MovLayer *movLayers, Layer *layers){
  int row, col;
  MovLayer *movLayer;
  and_sr(~8); //GIE off
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8); //GIE on
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
    bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
          Vec2 pixelPos = {col, row};
          u_int color = bgColor;
          Layer *probeLayer;
    for (probeLayer = layers; probeLayer; probeLayer = probeLayer->next) { /* probe all layers, in order */
        if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
            color = probeLayer->color;
            break; 
        } /* if probe check */
    } // for checking all layers at col, row
    lcd_writeColor(color);           
    } // for col
    } // for row
  } // for moving layer being updated
}  

void mlAdvance(MovLayer *ml, Region *fence, MovLayer *mlplayer){
  MovLayer *mlCopy = ml;
  Vec2 newPos, playerPos;
  u_char axis;
  Region player, shapeBoundary;
  vec2Add(&playerPos, &mlplayer->layer->posNext, &mlplayer->velocity);
  abShapeGetBounds(mlplayer->layer->abShape, &playerPos, &player);
  for (; ml -> next -> next; ml = ml->next) { //to skip player
      vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
      abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
      for (axis = 0; axis < 2; axis ++) {
          if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) || 
              (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
              int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
              newPos.axes[axis] += (2*velocity);
            }
            //collisions
            if ((shapeBoundary.botRight.axes[0] > player.topLeft.axes[0]) && 
                (player.botRight.axes[0] > shapeBoundary.topLeft.axes[0]) && 
                (shapeBoundary.botRight.axes[1] > player.topLeft.axes[1]) && 
                (player.botRight.axes[1] > shapeBoundary.topLeft.axes[1])) {
                drawString5x7(30,screenHeight/2-30, "You Died...", COLOR_VIOLET, COLOR_BLACK);
            WDTCTL = 0;
        }
    } /**< for axis */
    ml->layer->posNext= newPos;
  } /**< for ml */
  vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
  abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
  if ((shapeBoundary.topLeft.axes[1] < fence->topLeft.axes[0]) || 
      (shapeBoundary.botRight.axes[1] > fence->botRight.axes[1]) || 
      (shapeBoundary.botRight.axes[0] > fence->botRight.axes[1])) {
      newPos.axes[0] = screenWidth - 10;
      newPos.axes[1] = screenHeight/2;
    } /**< if outside of fence */
    ml->layer->posNext = newPos;
}

///////////////////////////////////////////////
//************* BUTTONS ********************
/////////////////////////////////////////////

//from shape motion demo combined with switch cases
void wdt_c_handler(){
char state = 15 - p2sw_read();
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    switch (state) { //buttons pressed
    case 1: //most left button
      buzzer_set_period(2000);
      mlAdvance(&up, &fieldFence, &mlplayer);
      break;
    case 2:
      buzzer_set_period(2000);
      mlAdvance(&down, &fieldFence, &mlplayer);
      break;
    case 4:
      buzzer_set_period(2000);
      mlAdvance(&left, &fieldFence, &mlplayer);
      break;
    case 8: //most right button
      buzzer_set_period(2000);
      mlAdvance(&right, &fieldFence, &mlplayer);
      break;
    }
    mlAdvance(&ml0, &fieldFence, &mlplayer);
    if (p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}


///////////////////////////////////////////////
//***************** MAIN ********************
///////////////////////////////////////////////

void main(){
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  buzzer_init();
  p2sw_init(15);
  shapeInit();
  
  layerInit(&layer0);
  layerDraw(&layer0);
  layerGetBounds(&field, &fieldFence);
  layerGetBounds(&player, &playerRegion);

  //from shape motion demo
  enableWDTInterrupts(); 
  or_sr(0x8);
  for(;;) {
    buzzer_set_period(0);
     while (!redrawScreen) {
      P1OUT &= ~GREEN_LED;
      or_sr(0x10);	      
    }
    P1OUT |= GREEN_LED;  
    redrawScreen = 0;    
    movLayerDraw(&ml0, &layer0);
  }
}

