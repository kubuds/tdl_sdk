#ifndef _CVIAI_APP_CAPTURE_TYPE_H_
#define _CVIAI_APP_CAPTURE_TYPE_H_

typedef enum { IDLE = 0, ALIVE, MISS } tracker_state_e;

typedef enum { AUTO = 0, FAST, CYCLE } capture_mode_e;

typedef enum { AREA_RATIO = 0, EYES_DISTANCE, LAPLACIAN } quality_assessment_e;

#endif