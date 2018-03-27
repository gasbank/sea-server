#pragma once

typedef struct _LWPTTLFULLSTATEOBJECT {
    float x0, y0;
    float x1, y1;
    float vx, vy;
    int id;
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
    short x0;
    short y0;
    short x1;
    short y1;
} LWPTTLSTATICOBJECT;

typedef struct _LWPTTLSTATICSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSTATICOBJECT obj[200];
} LWPTTLSTATICSTATE;

typedef struct _LWPTTLSEAPORTOBJECT {
    short x0;
    short y0;
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

