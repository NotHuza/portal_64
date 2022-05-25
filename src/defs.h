
#ifndef __DEFS_H__
#define __DEFS_H__

#define	STACKSIZEBYTES	0x2000

#ifdef _LANGUAGE_C

#define MAX_FRAME_BUFFER_MESGS	8

#define INIT_PRIORITY		10
#define GAME_PRIORITY		10
#define AUDIO_PRIORITY		12
#define SCHEDULER_PRIORITY	13
#define NUM_FIELDS      1 

#define DMA_QUEUE_SIZE  200

#define SCENE_SCALE 256

#define MAX_DYNAMIC_OBJECTS     16

#define MAX_RENDER_COUNT        128

#define SIMPLE_CONTROLLER_MSG	    (5)

#define PRINTF(a) 

#endif /* _LANGUAGE_C */

#endif
