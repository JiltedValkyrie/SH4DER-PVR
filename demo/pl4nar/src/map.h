/* The new map's loop (PL4NAR_MAP builds): a place on the loop by distance along it and offset across it, and the
 * self-driving traffic, one speed a lane. The data is tools/map.py's (build/gen/map_gen.c). Platform-free. */
#ifndef MAP_H
#define MAP_H
#include <stdint.h>

/* A place in the world: x, z, and the yaw of the travel direction (+s), turned the demo's way (draw_model: local +z
   to (sin yaw, cos yaw)). */
typedef struct map_pose { float x, z, yaw; } map_pose_t;

float      map_wrap(float s);                /* s into 0 .. MAP_LOOP_LEN */
map_pose_t map_at(float s, float lat);       /* the centre line at s, moved lat along its right (cos yaw, -sin yaw) */

/* The traffic: map_car's cars, each on its lane at the lane's speed. */
void       map_traffic_reset(void);
void       map_traffic_step(float dt);
unsigned   map_traffic_n(void);
map_pose_t map_traffic_pose(unsigned k);     /* yaw: the way the car faces (oncoming lanes turned round) */
float      map_traffic_s(unsigned k);        /* its s along the loop */
#endif
