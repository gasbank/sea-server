#pragma once

/*
* BEGIN: should sync with packet.h in sea-server
*/
// UDP
typedef struct _LWPTTLFULLSTATEOBJECT {
    float fx0, fy0;
    float fx1, fy1;
    float fvx, fvy;
    int id;
    int type;
    char guid[64];
    float route_left;
} LWPTTLFULLSTATEOBJECT;

// UDP
typedef struct _LWPTTLFULLSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLFULLSTATEOBJECT obj[32];
} LWPTTLFULLSTATE;

// UDP
typedef struct _LWPTTLSTATICOBJECT {
    int x0;
    int y0;
    int x1;
    int y1;
} LWPTTLSTATICOBJECT;

// UDP
typedef struct _LWPTTLSTATICSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSTATICOBJECT obj[256+128];
} LWPTTLSTATICSTATE;

// UDP
typedef struct _LWPTTLSTATICOBJECT2 {
    signed char x_scaled_offset_0;
    signed char y_scaled_offset_0;
    signed char x_scaled_offset_1;
    signed char y_scaled_offset_1;
} LWPTTLSTATICOBJECT2;

// UDP
typedef struct _LWPTTLSTATICSTATE2 {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int xc0;
    int yc0;
    int view_scale;
    int count;
    LWPTTLSTATICOBJECT2 obj[256+128];
} LWPTTLSTATICSTATE2;

// UDP
typedef struct _LWPTTLSEAPORTOBJECT {
    int x0;
    int y0;
    char name[64];
} LWPTTLSEAPORTOBJECT;

// UDP
typedef struct _LWPTTLSEAPORTSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSEAPORTOBJECT obj[128];
} LWPTTLSEAPORTSTATE;

// UDP
typedef struct _LWPTTLTRACKCOORDS {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int id;
    float x;
    float y;
} LWPTTLTRACKCOORDS;

// UDP
typedef struct _LWPTTLSEAAREA {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    char name[128];
} LWPTTLSEAAREA;

// UDP
typedef struct _LWPTTLPING {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    float lng, lat, ex_lng, ex_lat; // x center, y center, extent
    int ping_seq;
    int track_object_id;
    int track_object_ship_id;
    int view_scale;
    int static_object;
} LWPTTLPING;

// UDP
typedef struct _LWPTTLREQUESTWAYPOINTS {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int ship_id;
} LWPTTLREQUESTWAYPOINTS;

// UDP
typedef struct _LWPTTLWAYPOINTS {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int ship_id;
    int count;
    struct {
        int x;
        int y;
    } waypoints[1000];
} LWPTTLWAYPOINTS;
/*
* END: should sync with packet.h in sea-server
*/
