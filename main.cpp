#define COW_PATCH_FRAMERATE
#include "include.cpp"
#include <functional>
#include <iostream>
#include <string>
#include <fstream>
#include <math.h> 


#define BALL_RAD 0.1
#define HOLE_RAD 0.25
#define SPEED_SCALE 0.3
#define A_CONST .009
#define FALL_FACTOR 10
#define SLOPE_CONST 0.05
#define GRAVITY 0.05



// ANCHOR struct course:
struct Hole {
    vec2 x_bound;
    vec2 y_bound;
    vec2 z_bound;
    vec3 hole_position;
    vec3 position;
    vec3 velocity;
    vec3 acceleration;
    IndexedTriangleMesh3D terrain;
    mat4 scale;
    int hole_num;
    int par;
    real angle;
    std::function<real(real, real)> terrain_f;
    std::function<real(real, real)> terrain_sz;
    std::function<real(real, real)> terrain_sx;

};

// ANCHOR make course
IndexedTriangleMesh3D makeCourse(std::function<real(real, real)> func, real x_scale, real y_scale, real z_scale) {
    StretchyBuffer<vec3> floor_verts = {};
    StretchyBuffer<vec3> colors = {};

    IndexedTriangleMesh3D course = {};

    // setting up verticies
    int x_max = 40;
    int z_max = 40;

        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < z_max; z++) {
                for (int x = 0; x < x_max; x++) {
                    real x_pb = ((2 * x_scale / (x_max-1)) * x) - x_scale;
                    real z_pb = ((2 * z_scale / (z_max-1)) * z) - z_scale;
                    real y_pb = (y * (y_scale) * (func(x_pb, z_pb) + y));
                        
                    
                    sbuff_push_back(&colors, V3(.2, .5, .2) * (MIN(y_pb, 0.8) + 0.5));
                    sbuff_push_back(&floor_verts, V3(x_pb, y_pb, z_pb));
            }
        }
    }


    StretchyBuffer<int3> floor_tris = {};
    StretchyBuffer<vec3> norms = {};

    // setting up triangles and normals of top and bottom
    course.num_vertices = floor_verts.length;
    course.vertex_positions = floor_verts.data;
    for (int y = 0; y < 2; y++) {
        int y_buff = y * x_max * z_max;
        for (int z = 0; z < z_max - 1; z++) {
            int z_buff = z * x_max;
            for (int x = 0; x < x_max - 1; x++) {
                int yz_buff = z_buff + y_buff;
                sbuff_push_back(&floor_tris, {yz_buff + x,yz_buff + x + 1,yz_buff + x + x_max});
                {        
                    vec3 a = course.vertex_positions[yz_buff + x + 1] - course.vertex_positions[yz_buff + x];
                    vec3 b = course.vertex_positions[yz_buff + x + x_max] - course.vertex_positions[yz_buff + x]; 
                    sbuff_push_back(&norms, normalized(-cross(a,b)));
                }
                sbuff_push_back(&floor_tris, {yz_buff +x + 1,yz_buff + x + x_max,yz_buff + x + 1 + x_max});
                {        
                    vec3 a = course.vertex_positions[yz_buff + x + x_max] - course.vertex_positions[yz_buff +x + 1];
                    vec3 b = course.vertex_positions[yz_buff + x + 1 + x_max] - course.vertex_positions[yz_buff +x + 1]; 
                    sbuff_push_back(&norms, normalized(cross(a,b)));
                }

            }

        }
    }
    
    // setting up triangles and normals of front face
    for (int x = 0; x < x_max -1 ; x++) {
        int x_buff = x_max * (z_max - 1);
        sbuff_push_back(&floor_tris, {x_buff + x, x_buff + x + 1, x_buff + x + x_max * z_max});
        {        
                    vec3 a = course.vertex_positions[ x_buff + x + 1] - course.vertex_positions[x_buff + x];
                    vec3 b = course.vertex_positions[x_buff + x + x_max * z_max] - course.vertex_positions[x_buff + x]; 
                    sbuff_push_back(&norms, normalized(-cross(a,b)));
        }
        sbuff_push_back(&floor_tris, {x_buff + x + 1, x_buff + x + x_max * z_max, x_buff + x + x_max * z_max + 1});
        {        
                    vec3 a = course.vertex_positions[x_buff + x + x_max * z_max] - course.vertex_positions[x_buff + x + 1];
                    vec3 b = course.vertex_positions[x_buff + x + x_max * z_max + 1] - course.vertex_positions[x_buff + x + 1]; 
                    sbuff_push_back(&norms, normalized(-cross(a,b)));
        }

    }

    course.num_triangles = floor_tris.length;
    course.vertex_colors = colors.data;
    course.triangle_indices = floor_tris.data;
    course.vertex_normals = norms.data;


    return course;
}

//ANCHOR - HOLE HELP (Sets up everything for a hole pretty much)
int hole_help(Hole hole) {

    
    // ANCHOR const

    real release_angle = 0;
    real power = 0;
    bool moving = false;
    bool live = true;
    bool in_bound = true;
    int action_ctr = 0;
    bool ball_cam = true;
    int time = 0;
    int clock2 = 0;
    int strokes = 0;
    real rotation_y = 0;
    real rotation_x = 0;
    real distance = 5;
    vec3 free_position = V3(0);
    real roll_z = 0;
    real roll_x = 0;
    bool controls = false;




    //ANCHOR - BALL SHADER

    char *vertex_shader_source = R""(
        #version 330 core
        uniform mat4 transform;
        layout (location = 0) in vec3 vertex_position;
        layout (location = 1) in vec3 vertex_normals;
  
        out vec3 norm;
        out vec3 pos;
        void main() {
            // if (vertex_normals.y > 0) {
            //     color = vec3(1.0);
            // } else {
            //     color = vec3(0);
            // }
            pos = vertex_position;
            norm = vertex_normals;

            gl_Position = transform * vec4(vertex_position, 1.0);
        }
    )"";

    char *fragment_shader_source = _load_file_into_char_array("golf-ball.frag");

    Shader shader = shader_create(vertex_shader_source, 2, fragment_shader_source);

    //ANCHOR Cam
    
    Camera3D camera = { 12.0, RAD(60), 0, -0.25};
    IndexedTriangleMesh3D mesh = makeCourse(hole.terrain_f, hole.x_bound[1], hole.y_bound[1], hole.z_bound[1]);
    IndexedTriangleMesh3D ball = library.meshes.sphere;


    while (cow_begin_frame()) {

        // ANCHOR GUI
        gui_readout("Hole:", &hole.hole_num);
        gui_readout("Par:", &hole.par);
        gui_readout("Strokes:", &strokes);
        gui_checkbox("ball_cam / free_cam", &ball_cam, 'b');
        gui_checkbox("controls", &controls, 'c');
        
        // ANCHOR Control Draw
        if (controls) {
            text_draw(globals.Identity, "CONTROLS:", V3(-0.35, 0.8, 0), monokai.white, 100, V2(0), true);
            text_draw(globals.Identity, "<w>: Increase power", V3(-0.8, 0.3, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<a>: rotate release left", V3(-0.8, 0.1, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<s>: Decrease power", V3(-0.8, -0.1, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<d>: rotate release right", V3(-0.8, -0.3, 0), monokai.white, 40, V2(0), true);

            text_draw(globals.Identity, "<i>: Move cam up / move forward", V3(0, 0.3, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<j>: Rotate cam left / move left", V3(0, 0.1, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<k>: Move cam down / move back", V3(0, -0.1, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<l>: Rotate cam right / move right", V3(0, -0.3, 0), monokai.white, 40, V2(0), true);

            text_draw(globals.Identity, "hold <shift>: Fine aim", V3(-0.40, -0.5, 0), monokai.white, 40, V2(0), true);
            text_draw(globals.Identity, "<space>: Hit Ball", V3(-0.35, -0.7, 0), monokai.white, 40, V2(0), true);


            

        }
        else {
        mat4 P = camera_get_P(&camera);
        mat4 V = inverse(M4_Translation(hole.position + free_position) * M4_RotationAboutYAxis(rotation_y) * M4_RotationAboutXAxis(50) * M4_RotationAboutXAxis(rotation_x) * M4_Translation(V3(0, 1.5, distance) ));
        // V = camera_get_V(&camera);
        real fine_aim = 1.0;

        

        
        camera_move(&camera);
        mat4 M = globals.Identity;

        {   
            //STUB - HIT
            if (strokes >= 12) {
                // forfit
                if (live) {
                    sound_play_sound("sounds/background.wav");
                }
                live = false;
                hole.velocity = V3(0);
                hole.acceleration = V3(0);

            } else if (globals.key_pressed[' '] && !moving && live && power > 0) {
                strokes++;
                sound_play_sound("sounds/ballHit.wav");
                hole.velocity = V3(sin(release_angle), .5, -cos(release_angle)) * power/100;
                hole.acceleration = -A_CONST * V3(sin(release_angle), 0, -cos(release_angle)); // should be 0.9 need to implement rolling

            

                moving = true;

            }
        }

        // END GUI //

        // ANCHOR keystrokes
        {   
            if (!moving) {


                if (ball_cam) {
                    free_position = V3(0);
                    if (globals.key_shift_held) {fine_aim = 0.1;}
                    if (globals.key_held['a']) {release_angle = release_angle - 0.025 * fine_aim;}
                    if (globals.key_held['d']) {release_angle = release_angle + 0.025 * fine_aim;}
                    if (globals.key_held['s']) {power = CLAMP(power - 1, 0, 100);}
                    if (globals.key_held['w']) {power = CLAMP(power + 1, 0, 100);}
                    if (globals.key_held['j']) {rotation_y += 0.025;}
                    if (globals.key_held['l']) {rotation_y -= 0.025;}
                    if (globals.key_held['i']) {rotation_x = MAX(rotation_x - 0.025, -1.1);}
                    if (globals.key_held['k']) {rotation_x = MIN(rotation_x + 0.025, -1.1 + PI/2);}
                    // if (globals.key_held['p']) {distance = CLAMP(distance + 0.1, 1.0, 7.0);}
                    // if (globals.key_held['o']) {distance = CLAMP(distance - 0.1, 1.0, 7.0);}

                } else {
                    rotation_x = 0;
                    rotation_y = 0;

                    if (globals.key_held['j']) {free_position.x = MAX(free_position.x - 0.1, -14);}
                    if (globals.key_held['l']) {free_position.x = MIN(free_position.x + 0.1, 14);}
                    if (globals.key_held['i']) {free_position.z = MAX(free_position.z - 0.1, -20);}
                    if (globals.key_held['k']) {free_position.z = MIN(free_position.z + 0.1, 0);}
                }
            }

        }
        // ANCHOR physics

        // if (hole.position.y > hole.y_bound[1] + BALL_RAD) { //hole.terrain_f(hole.position.x, hole.position.z)) {
        //     hole.acceleration.y = -9.81;
        // } else {
        //     hole.acceleration.y = 0.0;
        // }
        //STUB - Y Physics
        real ground_level = 0.5 * (1 + hole.terrain_f(hole.position.x, hole.position.z)) + BALL_RAD;
        // real slope = hole.terrain_s(hole.position.x, hole.position.z);


        if (in_bound && hole.position.y + TINY_VAL > ground_level) {
            hole.position.y = MAX(ground_level, hole.position.y - GRAVITY);
            hole.velocity.y -= GRAVITY / 3;

        } else {
            if (in_bound) {hole.position.y = ground_level;}
             
        }

        real slope_z = hole.terrain_sz(hole.position.x, hole.position.z);
        real slope_x = hole.terrain_sx(hole.position.x, hole.position.z);
        
        if (hole.position.y <= ground_level + TINY_VAL && moving) {
            if (hole.position.z > hole.z_bound[0] + 0.5 && abs(slope_z) > SLOPE_CONST && hole.position.z < hole.z_bound[1] - 0.2) {
                
                hole.velocity.z -= 0.01 * slope_z * 100/(time + 100);

            }
        }

        if (hole.position.y <= ground_level + TINY_VAL && moving && hole.position.x > hole.x_bound[0] + 0.2 && hole.position.x < hole.x_bound[1] - 0.2) {
            if (abs(slope_x) > SLOPE_CONST) {

                hole.velocity.x -= 0.01 * slope_x * 100/(time + 100);

            }
        }

        if (abs(hole.velocity.z) < 0.05 && abs(hole.velocity.x) < 0.05) {
            clock2++;
            if (clock2 > 60) {
                hole.velocity = V3(0);
                moving = false;
                time = 0;  
            }      
        } else {
            clock2 = 0;
        }

        if (moving) {
            hole.position += hole.velocity * SPEED_SCALE;
            // hole.velocity += hole.acceleration * SPEED_SCALE;
            hole.velocity += normalized(hole.velocity) * -A_CONST* SPEED_SCALE;

            time++;

            // printf("velocity: ");
            // pprint(hole.velocity);
            // printf("acceleration: ");
            
            // pprint(hole.acceleration);
        }

        //STUB - in hole
        if (norm(V3(hole.hole_position.x, ground_level, hole.hole_position.z) - hole.position) < BALL_RAD + HOLE_RAD) {

            if (norm(hole.velocity) * FALL_FACTOR < (2 * HOLE_RAD - BALL_RAD) * sqrt(9.81 / (2 * BALL_RAD))) {

                if (live) {
                    sound_play_sound("sounds/inHole.wav");
                    sound_play_sound("sounds/background.wav");
                }
                live = false;
                hole.velocity = V3(0);
                hole.acceleration = V3(0);

                
            }
        }

        for (int i = 0; i < 3; i++) {
            if (abs(hole.velocity[i]) <= abs(hole.acceleration[i])) {
                hole.velocity[i] = 0.0;
                hole.acceleration[i] = 0.0;
            }
        }
        
        // (norm(hole.velocity) < TINY_VAL && hole.position.z + BALL_RAD *2 + TINY_VAL < hole.z_bound[0]))
        if (abs(hole.velocity.x) <= abs(hole.acceleration.x) &&
            abs(hole.velocity.z) <= abs(hole.acceleration.z) &&
            abs(slope_z) < TINY_VAL && abs(slope_x) < TINY_VAL) { //sin(hole.velocity.x) + cos(hole.velocity.z)
            moving = false;
            hole.velocity = V3(0);
        }

        


    // end physics
    // ANCHOR collisions

    {   
        // x check (right)
        if (in_bound) {
            
            //STUB - RIGHT
            if (hole.position.x + BALL_RAD + hole.velocity.x*SPEED_SCALE >= hole.x_bound[1]) {
                //bounce
                sound_play_sound("sounds/ballHit.wav");
                vec3 a = normalized(-hole.velocity + hole.acceleration);
                vec3 b = normalized(V3(hole.position.x, hole.position.y, hole.z_bound[1]) - hole.position);
                real in_v_norm = norm(hole.velocity);
                real in_theta = RAD(trunc(DEG(dot(a, b)))); // this isnt hacky i swear
                real out_theta = -in_theta;
                // if (in_theta >= 0) {
                hole.velocity = V3(-cos(out_theta), 0, sin(out_theta)) * 0.7 * in_v_norm;
                hole.acceleration = -A_CONST * V3(-cos(out_theta), 0, sin(out_theta));
                // } else {
                //     hole.velocity = V3(cos(out_theta), 0, -sin(out_theta)) * 0.7 * in_v_norm;
                //     hole.acceleration = -A_CONST * V3(cos(out_theta), 0, -sin(out_theta));
                // }
            

            }
            //STUB - LEFT
            else if (hole.position.x - BALL_RAD + hole.velocity.x*SPEED_SCALE <= hole.x_bound[0]) {
                //bounce
                sound_play_sound("sounds/ballHit.wav");
                vec3 a = normalized(-hole.velocity + hole.acceleration);
                vec3 b = normalized(V3(hole.position.x, hole.position.y, hole.z_bound[1]) - hole.position);
                real in_v_norm = norm(hole.velocity);
                real in_theta = RAD(trunc(DEG(dot(a, b)))); // this isnt hacky i swear
                real out_theta = -in_theta;
                // if (in_theta >= 0) {
                hole.velocity = V3(cos(out_theta), 0, sin(out_theta)) * 0.7 * in_v_norm;
                hole.acceleration = -A_CONST * V3(cos(out_theta), 0, sin(out_theta));
            }
            //STUB - TOP
            else if (hole.position.z - BALL_RAD + hole.velocity.z*SPEED_SCALE <= hole.z_bound[0]) {
                //bounce
                sound_play_sound("sounds/ballHit.wav");
                vec3 a = normalized(-hole.velocity + hole.acceleration);
                vec3 b = normalized(V3(hole.x_bound[1], hole.position.y, hole.position.z) - hole.position);
                real in_v_norm = norm(hole.velocity);
                real in_theta = RAD(trunc(DEG(dot(a, b)))); // this isnt hacky i swear

                real out_theta = -in_theta;
                hole.velocity = V3(sin(out_theta), 0, cos(out_theta)) * 0.7 * in_v_norm;
                hole.acceleration = -A_CONST * V3(sin(out_theta), 0, cos(out_theta));
            } else if (norm(hole.velocity) < TINY_VAL){
                hole.velocity = V3(0);
                hole.acceleration = V3(0);

                moving = false;
            }
        }

        // bottom
        if (hole.position.z + BALL_RAD > hole.z_bound[1]) {
            in_bound = false;
            hole.velocity.y = -1;
            hole.velocity.z = 0.1;
            hole.acceleration = V3(0.0);
            // reset
            action_ctr++;


            if (action_ctr > 100) {
                hole.position = V3(0.0, ground_level + hole.y_bound[1] + BALL_RAD, hole.z_bound[1] - 1);
                hole.velocity = V3(0.0);
                hole.acceleration = V3(0.0);
                in_bound = true;
                action_ctr = 0;
                time = 0;
                strokes++;
                moving = false;

            }
            
        }

//

        
    }
    // end collisions
    // ANCHOR main draw

    // STUB skybox

            { // skybox
            // glDisable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            {
                int3 triangle_indices[] = { { 0, 1, 2 }, { 0, 2, 3} };
                vec2 vertex_texture_coordinates[] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
                vec3 front[]  = { {  15, -15,  15 }, { -15, -15,  15 }, { -15,  15,  15 }, {  15,  15,  15 }, };
                vec3 back[]   = { { -15, -15, -15 }, {  15, -15, -15 }, {  15,  15, -15 }, { -15,  15, -15 }, };
                vec3 right[]  = { {  15, -15, -15 }, {  15, -15,  15 }, {  15,  15,  15 }, {  15,  15, -15 }, };
                vec3 left[]   = { { -15, -15,  15 }, { -15, -15, -15 }, { -15,  15, -15 }, { -15,  15,  15 }, };
                vec3 top[]    = { { -15,  15, -15 }, {  15,  15, -15 }, {  15,  15,  15 }, { -15,  15,  15 }, };
                vec3 bottom[] = { { -15, -15,  15 }, {  15, -15,  15 }, {  15, -15, -15 }, { -15, -15, -15 }, };
                auto Q = [&](vec3 *vertex_positions, char *texture_filename) {
                    mesh_draw(P, V, M, 2, triangle_indices, 4, vertex_positions, NULL, NULL, {}, vertex_texture_coordinates, texture_filename);
                };
                Q(front,  "codebase/skybox_front.jpg");
                Q(right,  "codebase/skybox_right.jpg");
                Q(left,   "codebase/skybox_left.jpg");
                Q(top,    "codebase/skybox_top.jpg");
                Q(bottom, "codebase/skybox_bottom.jpg");
                Q(back,   "codebase/skybox_back.jpg");
            }
            glDisable(GL_CULL_FACE);
            // glEnable(GL_DEPTH_TEST);
        }

    // drawing stuff
    {   
        //STUB - Rotation of ball
        roll_z += hole.velocity.z / 1.5;
        roll_x += hole.velocity.x / 1.5;

        mat4 ball_scale = M4_Translation(hole.position) * M4_RotationAboutZAxis(-roll_x) * M4_RotationAboutXAxis(roll_z) * M4_Scaling(BALL_RAD, BALL_RAD, BALL_RAD); //  M4_RotationAboutXAxis(time)
        
        //ANCHOR FLOOR DRAW

        mesh.draw(P, V, M); //floor_scale

        // shader_set_uniform(&shader_grass, "transform", P * V * M);
        // vec3* vertex_positions = mesh.vertex_positions;

        // shader_pass_vertex_attribute(&shader_grass, mesh.num_vertices, vertex_positions);
        // shader_set_uniform(&shader_grass, "iResolution", V3(window_get_size(), 0.0));

        
        // shader_draw(&shader_grass, mesh.num_triangles, mesh.triangle_indices);




        library.meshes.cylinder.draw(P, V, M4_Translation(hole.hole_position) * M4_Scaling(HOLE_RAD, 0.51, HOLE_RAD), monokai.white);
        if (norm(V3(hole.hole_position.x, 0, hole.hole_position.z) - V3(hole.position.x, 0, hole.position.z)) > 2) {

            library.meshes.cylinder.draw(P, V, M4_Translation(hole.hole_position) * M4_Scaling(.05, 5, .05), monokai.white);
            eso_begin(P*V, SOUP_TRIANGLES);
            eso_color(V3(1.0, 0.2, 0.0));
            eso_vertex(hole.hole_position + V3(0, 5, 0));
            eso_vertex(hole.hole_position + V3(0, 3.5, 0));
            eso_vertex(hole.hole_position + V3(1.5, 4.25, 0));
            eso_end();
        }


        if (live) {
            // ball.draw(P, V, ball_scale, monokai.white, "ball.png");
            // STUB - BALL DRAW

            shader_set_uniform(&shader, "transform", P * V * ball_scale);
            vec3* vertex_positions = ball.vertex_positions;
            vec3* vertex_normals = ball.vertex_normals;

            shader_pass_vertex_attribute(&shader, ball.num_vertices, vertex_positions);
            shader_pass_vertex_attribute(&shader, ball.num_vertices, vertex_normals);
            shader_set_uniform(&shader, "iResolution", V3(window_get_size(), 0.0));

            
            shader_draw(&shader, ball.num_triangles, ball.triangle_indices);

            

        } else {
            std::string message;
            std::string scores[6] {"   Eagle   ", "   Birdy   ", "    Par    ", "   Bogey   ", "Double Bogey", "Triple Bogey"};
            int score = strokes - hole.par + 2;

            if (strokes == 12) {
                message = "Forfit: " + std::to_string(12);
            }
            else if (score > 5) {
                message = "    +" + std::to_string(score - 2) + "      ";
            }
            else {
                message = scores[score];

            }
            char* temp = const_cast<char*>(message.c_str());
            text_draw(globals.Identity, temp, V3(-.375, 0, 0.0), monokai.white, 100, V2(0), true);
            text_draw(globals.Identity, "Press <space> to continue", V3(-.17, -.3, 0.0), monokai.white, 20, V2(0), true);
            if (globals.key_pressed[' ']) {
                sound_stop_all();
                return strokes;

                
            }


            // eso_begin(globals.Identity, SOUP_LINES);
            // eso_color(monokai.white);
            // eso_vertex(0, 1, 0);
            // eso_vertex(0, -1, 0);
            // eso_end();

        }

        //STUB - walls
        {
            // 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1))
            real y_height = MAX(hole.terrain_f(hole.x_bound[1], 0) / 2, MAX(((hole.terrain_f(0, 0)) + 2) / 4, MAX(hole.y_bound[1], MAX((hole.terrain_f(0, hole.z_bound[0]) + 2) / 4, (hole.terrain_f(0, hole.z_bound[1]) + 2) / 4))));
            library.meshes.box.draw(P, V, M4_Translation(0, y_height, hole.z_bound[0] - 0.251) * M4_Scaling(hole.x_bound[1] + 0.5, y_height, 0.25), monokai.brown);
            library.meshes.box.draw(P, V, M4_Translation(hole.x_bound[0] - 0.251, y_height, 0) * M4_Scaling(0.25, y_height, hole.z_bound[0]), monokai.brown);
            library.meshes.box.draw(P, V, M4_Translation(hole.x_bound[1] + 0.251, y_height, 0) * M4_Scaling(0.25, y_height, hole.z_bound[0]), monokai.brown);
            


        }

        // STUB ARROW
        if (!moving && live) {
            // vec3 arrow = hole.position + V3(sin(release_angle) * (power/10), 0 , -cos(release_angle) * (power/10));
            eso_begin(P * V, SOUP_LINES);
                eso_color(LINEAR_REMAP(power, 0, 100, monokai.green, monokai.red));
                eso_vertex(hole.position);
                // eso_vertex(arrow);
                int i_max = 50;
                for (int i = 0; i < i_max; i++) {
                    // x = hole.position.x + (release_angle) * (power/100) * i
                    
                    real x = sin(release_angle) * (power/(10 * i_max)) * i;
                    real z = -cos(release_angle) * (power/(10 * i_max)) * i;
                    real y = 0.5 * (hole.terrain_f(hole.position.x + x, hole.position.z + z) - hole.terrain_f(hole.position.x, hole.position.z));
                    for (int j = 0; j < 2; j++) {   
                        if (hole.position.x + x < hole.x_bound[1] && hole.position.z + z < hole.z_bound[1] &&
                            hole.position.x + x > hole.x_bound[0] && hole.position.z + z > hole.z_bound[0]) {
                            eso_vertex(hole.position + V3(x, y, z));
                        }
                        
                    }
                }
            eso_end();

        }

    }
    // end drawing stuff
    }
    }
    return 12;
}

//ANCHOR - HOLES

int hole1() {

    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.hole_num = 1;
    hole.par = 2;
    hole.terrain_f = [](real x, real z) {return 0;};
    hole.terrain_sz = [](real x, real z) {return 0;};
    hole.terrain_sx = [](real x, real z) {return 0;};
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] - 1);
    real hole_x = random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD);
    hole.hole_position = V3(hole_x, 0.5 * (hole.terrain_f(hole_x, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);

    return hole_help(hole);
}

int hole2() {

    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.par = 2;
    hole.hole_num = 2;
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.terrain_f = [](real x, real z) {return -0.1 * z;};
    hole.terrain_sz = [](real x, real z) {return -0.1;};
    hole.terrain_sx = [](real x, real z) {return 0;};

    hole.hole_position = V3(random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD), 0.5 * (hole.terrain_f(0.0, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] -1);

    return hole_help(hole);
}

int hole3() { // change
    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8, 8);
    hole.par = 2; // change
    hole.hole_num = 3; // change
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.terrain_f = [](real x, real z) {return -abs(x) + 2;};
    hole.terrain_sz = [](real x, real z) {return 0;};
    hole.terrain_sx = [](real x, real z) {return (x > 0) ? (-1) : (x == 0) ? (0): 1;};
    real hole_x = random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD);
    hole.hole_position = V3(hole_x, 0.5 * (hole.terrain_f(hole_x, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] -1);
    return hole_help(hole);
}

int hole4() {

    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.par = 3;
    hole.hole_num = 4;
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.terrain_f = [](real x, real z) {return 0.1 * 4* sin(z);};
    hole.terrain_sz = [](real x, real z) {return 0.1 * 4* cos(z);};
    hole.terrain_sx = [](real x, real z) {return 0;};

    hole.hole_position = V3(random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD), 0.5 * (hole.terrain_f(0.0, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] -1);

    return hole_help(hole);
}

int hole5() {

    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.par = 3;
    hole.hole_num = 5;

    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.terrain_f = [](real x, real z) {return .7 * atan(z) + PI/2;};
    hole.terrain_sz = [](real x, real z) {return .7 / (z*z + 1);};
    hole.terrain_sx = [](real x, real z) {return 0;};

    hole.hole_position = V3(random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD), 0.5 * (hole.terrain_f(0.0, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] -1);
 



    return hole_help(hole);
}

int hole6() { // change
    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.hole_num = 6;
    hole.par = 2;
    hole.terrain_f = [](real x, real z) {return -1 * (cos(x*PI/2)) + 1;};
    hole.terrain_sz = [](real x, real z) {return 0;};
    hole.terrain_sx = [](real x, real z) {return (1/2) * PI * sin(PI * x / 2);};
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] - 1);
    real hole_x = random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD);
    hole.hole_position = V3(hole_x, 0.5 * (hole.terrain_f(hole_x, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);

    return hole_help(hole);
}

int hole7() {

    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.hole_num = 7;
    hole.par = 2;
    hole.terrain_f = [](real x, real z) {return .4 * sin(x) * sin(z);};
    hole.terrain_sz = [](real x, real z) {return .4 * sin(x)* cos(z);};
    hole.terrain_sx = [](real x, real z) {return .4 * sin(z) * cos(x);};
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] - 1);
    real hole_x = random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD);
    hole.hole_position = V3(hole_x, 0.5 * (hole.terrain_f(hole_x, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);

    return hole_help(hole);
}

int hole8() { // change
    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8.0, 8.0);
    hole.par = 3; // change
    hole.hole_num = 8; // change
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.terrain_f = [](real x, real z) {return -4 * cos(z / 5.1) + 4;}; // change
    hole.terrain_sz = [](real x, real z) {return 4.0/5.1 * sin(z/ 5.1);}; // change
    hole.terrain_sx = [](real x, real z) {return 0;};

    hole.hole_position = V3(random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD), 0.5 * (hole.terrain_f(0.0, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] -1);
    return hole_help(hole);
}


int hole9() { // change
    Hole hole = {};
    hole.x_bound = V2(-2, 2);
    hole.y_bound = V2(-0.5, 0.5);
    hole.z_bound = V2(-8, 8);
    hole.par = 4; // change
    hole.hole_num = 9; // change
    hole.velocity = V3(0);
    hole.acceleration = V3(0);
    hole.terrain_f = [](real x, real z) {return (cos(z)*cos(z) - sin(z))*(cos(z)*cos(z) - sin(z));}; // change
    hole.terrain_sz = [](real x, real z) {return 2*(cos(z)*cos(z) - sin(z))*(-sin(2*z) - cos(z)) ;}; // change
    hole.terrain_sx = [](real x, real z) {return 0;};

    hole.hole_position = V3(random_real(hole.x_bound[0] + 2 * HOLE_RAD, hole.x_bound[1] - 2 * HOLE_RAD), 0.5 * (hole.terrain_f(0.0, hole.z_bound[0] + 1)), hole.z_bound[0] + 1);
    hole.position = V3(0.0, 0.5 * (1 + hole.terrain_f(0.0, hole.z_bound[1] -1)) + BALL_RAD, hole.z_bound[1] -1);
    return hole_help(hole);
}



void course() { 
    sound_play_sound("sounds/background.wav");
    int state = 0;
    int score = 0;
    bool control = true;
    StretchyBuffer<int> scores = {};
    std::string line;
    std::ifstream rFile ("leaderboard.txt");
    if (rFile.is_open()) {
        while ( getline (rFile,line) )
        {
        sbuff_push_back(&scores, atoi(line.c_str()));



        }
        rFile.close();
    }
    int highscore = 109;
    int range = scores.length;
    for (int i = 0; i < range; i++) {
        highscore = MIN(scores.data[i], highscore);
    }

    while (cow_begin_frame()) {

        if (state == 0) {
            text_draw(globals.Identity, "WELCOME TO \nMINI GOLF!", V3(-0.35, 0.2, 0), monokai.white, 100, V2(0), true);
            text_draw(globals.Identity, "Press <space> to continue", V3(-.17, -.3, 0.0), monokai.white, 20, V2(0), true);

            if (globals.key_pressed[' ']) {
                sound_stop_all();
                state++;
            }
        }
        if (state == 1) {
            score = hole1() + hole2() + hole3() + hole4() + hole5() + hole6() + hole7() + hole8() + hole9();
            control = true;
            state++;
        }
        
        if (state == 2) {
            std::string msg = "SCORE: " + std::to_string(score);
            std::string msg2 = "HIGH SCORE: " + std::to_string(highscore);

            char* output = const_cast<char*>(msg.c_str());
            char* output2 = const_cast<char*>(msg2.c_str());
            if (score < highscore) {
                text_draw(globals.Identity, "NEW HIGH SCORE!", V3(-0.5, 0.6, 0), monokai.white, 100);

            }
            text_draw(globals.Identity, output2, V3(-0.42, 0.2, 0), monokai.white, 100);
            text_draw(globals.Identity, output, V3(-0.3, -0.2, 0), monokai.white, 100);
            text_draw(globals.Identity, "Press <p> to play again", V3(-.2, -.45, 0.0), monokai.white, 20, V2(0), true);


            if (control) {    
                std::ofstream myfile;
                myfile.open ("leaderboard.txt", std::ios_base::app);
                myfile << std::to_string(score) + "\n";
                myfile.close();
                control = false;
            }
            if (globals.key_pressed['p']) {
                break;
            }

        }
    }       
}

int main() {
    APPS {

        APP(course);

    }
}
