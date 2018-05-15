#pragma once
namespace ss {
    struct sea_object_pod {
        int id;
        int type;
        float fx, fy;
        float fw, fh;
        float fvx, fvy;
        char guid[64];
    };
}
