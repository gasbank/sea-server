#pragma once

typedef struct _LWPTTLFULLSTATEOBJECT {
    float x0, y0;
    float x1, y1;
    float vx, vy;
    int id;
    char guid[64];
    float route_left;
} LWPTTLFULLSTATEOBJECT;

typedef struct _LWPTTLFULLSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLFULLSTATEOBJECT obj[32];
} LWPTTLFULLSTATE;

typedef struct _LWPTTLSTATICOBJECT {
    int x0;
    int y0;
    int x1;
    int y1;
} LWPTTLSTATICOBJECT;

typedef struct _LWPTTLSTATICSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSTATICOBJECT obj[128];
} LWPTTLSTATICSTATE;

typedef struct _LWPTTLSEAPORTOBJECT {
    int x0;
    int y0;
    char name[64];
} LWPTTLSEAPORTOBJECT;

typedef struct _LWPTTLSEAPORTSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSEAPORTOBJECT obj[200];
} LWPTTLSEAPORTSTATE;

typedef struct _LWPTTLTRACKCOORDS {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int id;
    float x;
    float y;
} LWPTTLTRACKCOORDS;

typedef struct _LWPTTLSEAAREA {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    char name[128];
} LWPTTLSEAAREA;
