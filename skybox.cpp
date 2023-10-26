#include "include.cpp"
struct FPSCamera {
    vec3 origin;
    double theta;
    double phi;
    double angle_of_view;
};

mat4 fps_camera_get_C(FPSCamera *human) {
    return M4_Translation(human->origin) * M4_RotationAboutYAxis(human->theta) * M4_RotationAboutXAxis(human->phi);
}

void fps_camera_move(FPSCamera *human) {
    vec3 ds = {};
    if (globals.key_held['w']) { ds += transformVector(M4_RotationAboutYAxis(human->theta), V3(0, 0, -1)); }
    if (globals.key_held['s']) { ds += transformVector(M4_RotationAboutYAxis(human->theta), V3( 0, 0, 1)); }
    if (globals.key_held['a']) { ds += transformVector(M4_RotationAboutYAxis(human->theta), V3(-1, 0, 0)); }
    if (globals.key_held['d']) { ds += transformVector(M4_RotationAboutYAxis(human->theta), V3( 1, 0, 0)); }
    double norm_ds = norm(ds);
    if (!IS_ZERO(norm_ds)) ds /= norm_ds;
    human->origin += ds;

    if (globals.mouse_left_pressed) {
        window_pointer_lock();
    } else if (globals.key_pressed[COW_KEY_ESCAPE]) {
        window_pointer_unlock();
    }

    if (window_is_pointer_locked()) {
        human->theta -= globals.mouse_change_in_position_NDC.x;
        human->phi += globals.mouse_change_in_position_NDC.y;
        human->phi = CLAMP(human->phi, RAD(-89), RAD(89));
    }
}


void hw5b() {
    FPSCamera human = { V3(0, 10, 30), 0, 0, RAD(60) };
    real time = 0;
    while (cow_begin_frame()) {
        time += .0167;
        static bool move2head;
        gui_checkbox("move2head", &move2head, COW_KEY_TAB);
        static bool draw_scene;
        gui_checkbox("draw_scene", &draw_scene, ' ');

        fps_camera_move(&human);
        mat4 C = fps_camera_get_C(&human);
        mat4 P = _window_get_P_perspective(human.angle_of_view);
        mat4 V = inverse(C);
        mat4 PV = P * V;
        { // skybox
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            {
                int3 triangle_indices[] = { { 0, 1, 2 }, { 0, 2, 3} };
                vec2 vertex_texture_coordinates[] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
                vec3 front[]  = { {  10, -10,  10 }, { -10, -10,  10 }, { -10,  10,  10 }, {  10,  10,  10 }, };
                vec3 back[]   = { { -10, -10, -10 }, {  10, -10, -10 }, {  10,  10, -10 }, { -10,  10, -10 }, };
                vec3 right[]  = { {  10, -10, -10 }, {  10, -10,  10 }, {  10,  10,  10 }, {  10,  10, -10 }, };
                vec3 left[]   = { { -10, -10,  10 }, { -10, -10, -10 }, { -10,  10, -10 }, { -10,  10,  10 }, };
                vec3 top[]    = { { -10,  10, -10 }, {  10,  10, -10 }, {  10,  10,  10 }, { -10,  10,  10 }, };
                vec3 bottom[] = { { -10, -10,  10 }, {  10, -10,  10 }, {  10, -10, -10 }, { -10, -10, -10 }, };
                auto Q = [&](vec3 *vertex_positions, char *texture_filename) {
                    mesh_draw(P, V, (!move2head) ? M4_Translation(0, 10, 0) : M4_Translation(human.origin), 2, triangle_indices, 4, vertex_positions, NULL, NULL, {}, vertex_texture_coordinates, texture_filename);
                };
                Q(front,  "codebase/skybox_front.jpg");
                Q(right,  "codebase/skybox_right.jpg");
                Q(left,   "codebase/skybox_left.jpg");
                Q(top,    "codebase/skybox_top.jpg");
                Q(bottom, "codebase/skybox_bottom.jpg");
                Q(back,   "codebase/skybox_back.jpg");
            }
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
        }
        if (draw_scene) {
            { // islands
                int N = 16;
                for (int i = 0; i < N; ++i) {
                    library.soups.tet.draw(PV * M4_RotationAboutYAxis(real(i) / N * TAU) * M4_Translation(0, 1 * sin(10 * time + i * 16.343), -100) * M4_Scaling(10, 10, 10), monokai.brown);
                }
            }
            { // ocean
                real r = 1000;
                vec3 verts[] = { { -r, 0, -r }, { -r, 0,  r }, {  r, 0,  r }, {  r, 0, -r } };
                soup_draw(PV, SOUP_QUADS, 4, verts, NULL, V4(monokai.blue, .5));
            }
        }
    }
}

int main() {
    APPS {
        APP(hw5b);
    }
}
