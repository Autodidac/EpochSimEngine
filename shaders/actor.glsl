#ifndef SANDHYBRID_ACTOR_GLSL
#define SANDHYBRID_ACTOR_GLSL

const int ACTOR_HALF_WIDTH = 4;
const int ACTOR_HEIGHT = 23;
const int ACTOR_TOP_OFFSET = 1 - ACTOR_HEIGHT;
const int ACTOR_HEAD_CENTER_OFFSET = -18;
const int ACTOR_TOOL_ORIGIN_OFFSET = -13;

struct ActorState {
    int x;
    int y;
    int velocityY;
    uint enabled;
    uint gold;
    uint iron;
    uint ammo;
    uint shotTimer;
    uint moveCooldown;
    uint grounded;
    uint health;
    uint oxygen;
    int hitX;
    int hitY;
    uint scene;
    uint exposureTicks;
    uint aluminum;
    uint copper;
    uint unlocks;
    uint drillLevel;
};

#endif
