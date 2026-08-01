#ifndef SANDHYBRID_ACTOR_GLSL
#define SANDHYBRID_ACTOR_GLSL

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
