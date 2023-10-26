void ***() {
    Camera2D camera = { 3.0 };


    while (cow_begin_frame()) {

        camera_move(&camera);
        mat4 PV = camera_get_PV(&camera);

    }
}
