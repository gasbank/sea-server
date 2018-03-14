#pragma once

namespace ss {
    struct sea_object_public {
        int id;
        int type;
        float x, y;
        float w, h;
    };
    class sea_object {
    public:
        sea_object(int id, int type, float x, float y, float w, float h) : id(id), type(type), x(x), y(y), w(w), h(h)
        {
        }
        void fill_sop(sea_object_public& sop) {
            sop.x = x;
            sop.y = y;
            sop.w = w;
            sop.h = h;
            sop.id = id;
            sop.type = type;
        }
    private:
        explicit sea_object() {}
        int id;
        int type;
        float x, y;
        float w, h;
    };
}
