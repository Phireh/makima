#include "makima.h"

int main()
{
    // TODO: command-line arg parsing

    /* X11 Window creation */
    if (!(x11_display = XOpenDisplay(NULL)))
    {
        log_err("Could not open X11 display\n");
        return 0;
    }

    x11_screen = DefaultScreenOfDisplay(x11_display);
    x11_screen_id = DefaultScreen(x11_display);
    
    /* GLX initialization */
    GLint glx_attr[] = {
        GLX_X_RENDERABLE    , True,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , True,
        None
    };

    int fb_candidate_configs_count = 0;
    GLXFBConfig *candidate_fbs = glXChooseFBConfig(x11_display, x11_screen_id, glx_attr, &fb_candidate_configs_count);
    if (!fb_candidate_configs_count)
    {
        log_err("Unable to find framebuffer candidates for desired config");
        return 1;
    }  
    log_info("Found %d framebuffer candidates", fb_candidate_configs_count);

    int best_fb_index = -1;
    int best_sample_count = 0;

    for (int i = 0; i < fb_candidate_configs_count; ++i)
    {
        XVisualInfo *candidate_info = glXGetVisualFromFBConfig(x11_display, candidate_fbs[i]);
        if (candidate_info)
        {
            int sample_buffers;
            int samples;
            glXGetFBConfigAttrib(x11_display, candidate_fbs[i], GLX_SAMPLE_BUFFERS, &sample_buffers);
            glXGetFBConfigAttrib(x11_display, candidate_fbs[i], GLX_SAMPLES       , &samples);

            if (best_fb_index < 0 || (sample_buffers && samples > best_sample_count))
            {
                best_fb_index = i;
                best_sample_count = samples;
            }          
        }
        XFree(candidate_info);
    }
    log_debug("Best visual info index is %d", best_fb_index);

    GLXFBConfig best_fb_candidate = candidate_fbs[best_fb_index];
    XFree(candidate_fbs);

    XVisualInfo *glx_visual = glXGetVisualFromFBConfig(x11_display, best_fb_candidate);
    if (!glx_visual)
    {
        log_err("Unable to create GLX virtual");
        return 1;
    }

    if (x11_screen_id != glx_visual->screen)
    {
        log_err("Screen ID %d does not match visual->screen %d", x11_screen_id, glx_visual->screen);
        return 1;
    }

    GLint major_glx_version, minor_glx_version;
    glXQueryVersion(x11_display, &major_glx_version, &minor_glx_version);
    log_info("GLX version %d.%d", major_glx_version, minor_glx_version);
  

    XSetWindowAttributes x11_window_set_attr;
    x11_window_set_attr.border_pixel = BlackPixel(x11_display, x11_screen_id);
    x11_window_set_attr.background_pixel = WhitePixel(x11_display, x11_screen_id);
    x11_window_set_attr.override_redirect = True;
    x11_window_set_attr.colormap = XCreateColormap(x11_display, RootWindow(x11_display, x11_screen_id), glx_visual->visual, AllocNone);
    // NOTE: Unneeded for the moment, this is for input handling
    x11_window_set_attr.event_mask = KeyPressMask | KeyReleaseMask | KeymapStateMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | PointerMotionMask;

    // NOTE: We can't use XCreateSimpleWindow if we need GLX/OpenGL
    x11_window = XCreateWindow(x11_display,
                               RootWindowOfScreen(x11_screen),
                               x11_window_x_coord,
                               x11_window_y_coord,
                               x11_window_width,
                               x11_window_height,
                               x11_window_border_size,
                               glx_visual->depth,
                               InputOutput,
                               glx_visual->visual,
                               CWBackPixel | CWColormap | CWBorderPixel | CWEventMask,
                               &x11_window_set_attr);

    // Register WM_DELETE_WINDOW so we can handle the window manager's close request in our message dispatch                           
    Atom wmDeleteMessage = XInternAtom(x11_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x11_display, x11_window, &wmDeleteMessage, 1);

    /* Create GLX OpenGL context */

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

    int context_attr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 2,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    if (check_for_glx_extension("GLX_ARB_create_context", x11_display, x11_screen_id))
    {
        log_info("glXCreateContextAttribsARB is supported");
        glx_context = glXCreateContextAttribsARB(x11_display, best_fb_candidate, 0, true, context_attr);
    }
    else
    {
        glx_context = glXCreateNewContext(x11_display, best_fb_candidate, GLX_RGBA_TYPE, 0, True);
        log_info("glXCreateContextAttribsARB is not supported");
    }
    XSync(x11_display, False);  
    glXMakeCurrent(x11_display, x11_window, glx_context);

    /* Get OpenGL function addresses and link manually since we do not use GLEW or similar libs
       Failing to do this will make us SIGSEGV on GL calls */
    link_gl_functions();
     
  
    /* Verify if context is direct */
    if (glXIsDirect(x11_display, glx_context))
    {
        log_info("GLX context is direct");
    }
    else
    {
        log_info("GLX context is indirect");
    }
  
    log_info("Starting OpenGL version %s", glGetString(GL_VERSION));

    // Set title name for our open window
    XStoreName(x11_display, x11_window, "Makima");
  
    XClearWindow(x11_display, x11_window);

    // Actually show the window
    XMapRaised(x11_display, x11_window);

    // Tell X11 to send autorepeat keys as a secuence of presses, instead of press+release
    Bool detectable_autorepeat_sup;
    XkbSetDetectableAutoRepeat(x11_display, true, &detectable_autorepeat_sup);  

    // How large is the window
    XWindowAttributes x11_window_attr;
    XGetWindowAttributes(x11_display, x11_window, &x11_window_attr);

    /* Window message loop */

    XEvent x11_event;
    KeySym keysym = 0;
    main_loop_state_t main_loop_state = RUNNING;
    while (main_loop_state == RUNNING)
    {

        /* NOTE: The basic input handling loop is as follows:
           We mantain 2 different structures: the last frame, and current frame inputs.
         
           Each frame we copy the last recorded input, then use the function
           consume_first_presses to make the sure keys are only in JUST_RELEASED/PRESSED
           states for a single frame.                  
        */

        double input_phase_stamp = get_current_time();

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            last_frame_input[i] = curr_frame_input[i];
        }
      
        player_input_t *player1_input = &curr_frame_input[PLAYER_1];
        player_input_t *player1_last_input = &last_frame_input[PLAYER_1];

        /* HACK: We are just starting a frame, so there can be no JUST_PRESSED
           buttons just yet. Clean all of them for all active players */
        consume_first_presses(player1_input);
      
        /* Event handling */      
        // TODO: Sleep so we don't burn the CPU

        while (XPending(x11_display))
        {
            XNextEvent(x11_display, &x11_event);
            switch (x11_event.type)
            {          
            case Expose:
                /* The window has been resized */
                XGetWindowAttributes(x11_display, x11_window, &x11_window_attr);
                x11_window_width = x11_window_attr.width;
                x11_window_height = x11_window_attr.height;
                glViewport(0, 0, x11_window_width, x11_window_height);
                log_debug("Window resized; Window width %d; Window height %d", x11_window_attr.width, x11_window_attr.height);
                break;

            case ClientMessage:
                /* Petition to close window by the window manager. See Xlib's ICCCM docs */
                if (x11_event.xclient.data.l[0] == (long int)wmDeleteMessage)
                {
                    main_loop_state = FINISHED;
                    log_debug("Closing window by window manager's request");
                }
                break;

            case ButtonPress:
            {
                unsigned int button_pressed = ((XButtonEvent*)&x11_event)->button;
                switch (button_pressed) {
                case LEFT_MOUSE_BUTTON:
                    set_input_state(&player1_input->mouse_left_click,
                                    &player1_last_input->mouse_left_click,
                                    PRESSED,
                                    input_phase_stamp);
                    break;
                case WHEEL_PRESS_MOUSE_BUTTON: 
                    break;
                case RIGHT_MOUSE_BUTTON:
                    set_input_state(&player1_input->mouse_right_click,
                                    &player1_last_input->mouse_right_click,
                                    PRESSED,
                                    input_phase_stamp);                  
                    break;
                case WHEEL_UP_MOUSE_BUTTON: 
                    break;
                case WHEEL_DOWN_MOUSE_BUTTON:
                    break;
                default:
                    /* TODO: Despite not being defined in X11/X.h,
                       we can capture button presses from additional mouse buttons (8, 9...)
                    */
                    break;
                }
            } break;
            case ButtonRelease:
            {
                unsigned int button_pressed = ((XButtonEvent*)&x11_event)->button;
                /* int x = ((XButtonEvent*)&x11_event)->x; */
                /* int y = ((XButtonEvent*)&x11_event)->y; */
                switch (button_pressed)
                {
                case LEFT_MOUSE_BUTTON:
                    set_input_state(&player1_input->mouse_left_click,
                                    &player1_last_input->mouse_left_click,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case WHEEL_PRESS_MOUSE_BUTTON: 
                    break;
                case RIGHT_MOUSE_BUTTON:
                    set_input_state(&player1_input->mouse_right_click,
                                    &player1_last_input->mouse_right_click,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case WHEEL_UP_MOUSE_BUTTON: 
                    break;
                case WHEEL_DOWN_MOUSE_BUTTON:
                    break;
                default:
                        /* TODO: Despite not being defined in X11/X.h,
                           we can capture button presses from additional mouse buttons (8, 9...)
                        */
                    break;                  
                }
            } break;

            case MotionNotify:
            {
                int x = ((XPointerMovedEvent*)&x11_event)->x;
                int y = ((XPointerMovedEvent*)&x11_event)->y;
                /* Save old cursor coords */
                player1_last_input->cursor_x = player1_input->cursor_x;
                player1_last_input->cursor_y = player1_input->cursor_y;
              
                /* Store the new ones */
                player1_input->cursor_x = x;
                player1_input->cursor_y = y;
            } break;

            case KeyPress:
            {
                keysym = XLookupKeysym(&x11_event.xkey, 0);

                /* Look for modifier keys */
                unsigned int mod_keys = ((XKeyEvent*)&x11_event)->state;
                dispatch_mod_keys(mod_keys);
          
                switch (keysym)
                {
                case XK_Escape:
                    main_loop_state = FINISHED;
                    break;

                case XK_Shift_L:
                case XK_Shift_R:
                    set_shift_key();
                    break;
                case XK_Control_L:
                case XK_Control_R:
                    set_ctrl_key();
                    break;
                case XK_Alt_L:
                case XK_Alt_R:
                    set_alt_key();
                    break;

                case XK_W:
                case XK_w:
                    set_input_state(&player1_input->forward,
                                    &player1_last_input->forward,
                                    PRESSED,
                                    input_phase_stamp);
                    break;
                case XK_S:
                case XK_s:
                    set_input_state(&player1_input->back,
                                    &player1_last_input->back,
                                    PRESSED,
                                    input_phase_stamp);
                    break;
                case XK_A:
                case XK_a:
                    set_input_state(&player1_input->left,
                                    &player1_last_input->left,
                                    PRESSED,
                                    input_phase_stamp);
                    break;
                case XK_D:
                case XK_d: 
                    set_input_state(&player1_input->right,
                                    &player1_last_input->right,
                                    PRESSED,
                                    input_phase_stamp);                 
                    break;
                case XK_R:
                case XK_r:
                    set_input_state(&player1_input->up,
                                    &player1_last_input->up,
                                    PRESSED,
                                    input_phase_stamp);                 
                    break;
                case XK_F:
                case XK_f:
                    set_input_state(&player1_input->down,
                                    &player1_last_input->down,
                                    PRESSED,
                                    input_phase_stamp);
                } break;
            } break;

            case KeyRelease:
            {
                keysym = XLookupKeysym(&x11_event.xkey, 0);
                switch (keysym)
                {
                case XK_Shift_L:
                case XK_Shift_R:
                    unset_shift_key();
                    break;
                case XK_Control_L:
                case XK_Control_R:
                    unset_ctrl_key();
                    break;
                case XK_Alt_L:
                case XK_Alt_R:
                    unset_alt_key();
                    break;
                  
                case XK_W:
                case XK_w:
                    set_input_state(&player1_input->forward,
                                    &player1_last_input->forward,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case XK_S:
                case XK_s:
                    set_input_state(&player1_input->back,
                                    &player1_last_input->back,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case XK_A:
                case XK_a:
                    set_input_state(&player1_input->left,
                                    &player1_last_input->left,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case XK_D:
                case XK_d:
                    set_input_state(&player1_input->right,
                                    &player1_last_input->right,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case XK_R:
                case XK_r:
                    set_input_state(&player1_input->up,
                                    &player1_last_input->up,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                case XK_F:
                case XK_f:
                    set_input_state(&player1_input->down,
                                    &player1_last_input->down,
                                    UNPRESSED,
                                    input_phase_stamp);
                    break;
                }
            } break;
            
            default:
                break;
          }
        } // event handling end


        /* Copy mod_key information to player input struct */
        player1_input->alt = alt_key_is_set();
        player1_input->ctrl = ctrl_key_is_set();
        player1_input->shift = shift_key_is_set();

        /* Auxiliar struct members for comfier input handling */
        player1_input->cursor_x_delta = (int)player1_input->cursor_x - (int)player1_last_input->cursor_x;
        player1_input->cursor_y_delta = (int)player1_input->cursor_y - (int)player1_last_input->cursor_y;

        /* TODO: Logic here */
       
        

        /* Rendering */
        glClearColor(1.0f, 0.6f, 1.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //draw_test_triangle();
        draw_pyramid_test();
        glXSwapBuffers(x11_display, x11_window);
    }
    /* Cleanup */  
    XDestroyWindow(x11_display, x11_window);
    XCloseDisplay(x11_display);
    return 1;  
}

int check_for_glx_extension(char *extension, Display *display, int screen_id)
{
    const GLubyte *extensions = NULL;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(extension, ' ');
    if (where || *extension == '\0')
        return 0;
    extensions = (const GLubyte *) glXQueryExtensionsString(display, screen_id);
    // log_debug("Extensions found: %s", extensions);
    /* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
    start = extensions;
    for (;;) {
        where = (GLubyte *) strstr((const char *) start, extension);
        if (!where)
            break;
        terminator = where + strlen(extension);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return 1;
        start = terminator;
    }
    return 0;
}

unsigned int make_gl_program(unsigned int vertex_shader, unsigned int fragment_shader)
{
    char infoLog[512];        

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // print linking errors if any
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)         // TODO: Maybe better error handling?
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("Error: shader linking failed: %s\n", infoLog);
    }
    else
    {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }    
    return program;
}

unsigned int build_shader(char *source, int type)
{
    char infoLog[512];
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar * const *)(&source), NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("Error: shader compilation failed: %s\n", infoLog);        
    }
    // TODO: Maybe better error handling?
    return shader;
}

unsigned int build_shader_from_file(char *pathname, int type)
{
    FILE *shader_file = fopen(pathname, "r");
    fseek(shader_file, 0, SEEK_END);
    long file_size = ftell(shader_file);
    fseek(shader_file, 0, SEEK_SET);  /* same as rewind(f); */

    char *file_contents = calloc(sizeof(char), file_size + 1);
    fread(file_contents, 1, file_size, shader_file);
    fclose (shader_file);
    unsigned int shader = build_shader(file_contents, type);
    free(file_contents);
    return shader;
}

void draw_test_triangle(void)
{
    float vertices[] = {
       -0.5f,  -0.5f,
        0.5f,  -0.5f,
        0.0f, 0.5f        
    };
    
    static unsigned int program = 0;
    if (!program)
        program = make_gl_program(build_shader_from_file(vertex_shader_path, GL_VERTEX_SHADER), build_shader_from_file(fragment_shader_path, GL_FRAGMENT_SHADER));
    glUseProgram(program);
    
    static unsigned int vao;
    if (!vao)
        glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    static unsigned int vbo = 0;
    if (!vbo)
        glGenBuffers(1, &vbo);

    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);


    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void draw_pyramid_test(void)
{
    /* We are being disgusting human beings here and incrementing a counter as a
       poor man's time variable for simplicity */
    static float current_rot = 0.0f;
    current_rot += 2 * 3.141596f * get_delta_time(); // 1 full rotation per second
    
    float vertices[] = {
        0.0f,   0.43f,   0.0f,
       -0.5f,  -0.43f,  -0.5f,
        0.5f,  -0.43f,  -0.5f,
        0.0f,  -0.43f,   0.5f      
    };

    float colours[] = {
       1.0f,   1.0f,   1.0f,
       1.0f,   0.0f,   0.0f,
       0.0f,   1.0f,   0.0f,
       0.0f,   0.0f,   1.0f     
    };
    uint num_indices = 12;
    uint indices[] =
    {
        3,1,2, 0,1,2, 0,3,2, 0,2,3
    };
    
    static unsigned int program = 0;
    if (!program)
        program = make_gl_program(build_shader_from_file(vertex_shader_path, GL_VERTEX_SHADER), build_shader_from_file(fragment_shader_path, GL_FRAGMENT_SHADER));
    glUseProgram(program);

    static int uniform_location = -1;
    if (uniform_location == -1)
        uniform_location = glGetUniformLocation(program, "rot");
    
    glUniform1f(uniform_location, current_rot);
    
    static unsigned int vao = 0;
    if (!vao)
        glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    static unsigned int position_bo = 0;
    if (!position_bo)
        glGenBuffers(1, &position_bo);
    glBindBuffer(GL_ARRAY_BUFFER, position_bo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    static unsigned int colour_bo = 0;
    if (!colour_bo)
        glGenBuffers(1, &colour_bo);
    glBindBuffer(GL_ARRAY_BUFFER, colour_bo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colours), colours, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(1);

    static unsigned int element_bo = 0;
    if (!element_bo)
        glGenBuffers(1, &element_bo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_bo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);

    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, (void*)0);
}

int link_gl_functions(void)
{
    // TODO: Error handling
    glUseProgram              = (PFNGLUSEPROGRAMPROC)             glXGetProcAddress((const GLubyte *)"glUseProgram");
    glGetShaderiv             = (PFNGLGETSHADERIVPROC)            glXGetProcAddress((const GLubyte *)"glGetShaderiv");
    glShaderSource            = (PFNGLSHADERSOURCEPROC)           glXGetProcAddress((const GLubyte *)"glShaderSource");
    glCompileShader           = (PFNGLCOMPILESHADERPROC)          glXGetProcAddress((const GLubyte *)"glCompileShader");
    glGetShaderInfoLog        = (PFNGLGETSHADERINFOLOGPROC)       glXGetProcAddress((const GLubyte *)"glGetShaderInfoLog");
    glCreateShader            = (PFNGLCREATESHADERPROC)           glXGetProcAddress((const GLubyte *)"glCreateShader");
    glCreateProgram           = (PFNGLCREATEPROGRAMPROC)          glXGetProcAddress((const GLubyte *)"glCreateProgram");
    glDeleteShader            = (PFNGLDELETESHADERPROC)           glXGetProcAddress((const GLubyte *)"glDeleteShader");
    glGetProgramiv            = (PFNGLGETPROGRAMIVPROC)           glXGetProcAddress((const GLubyte *)"glGetProgramiv");
    glGetProgramInfoLog       = (PFNGLGETPROGRAMINFOLOGPROC)      glXGetProcAddress((const GLubyte *)"glGetProgramInfoLog");
    glAttachShader            = (PFNGLATTACHSHADERPROC)           glXGetProcAddress((const GLubyte *)"glAttachShader");
    glLinkProgram             = (PFNGLLINKPROGRAMPROC)            glXGetProcAddress((const GLubyte *)"glLinkProgram");
    glGenVertexArrays         = (PFNGLGENVERTEXARRAYSPROC)        glXGetProcAddress((const GLubyte *)"glGenVertexArrays");
    glGenBuffers              = (PFNGLGENBUFFERSPROC)             glXGetProcAddress((const GLubyte *)"glGenBuffers");
    glBindVertexArray         = (PFNGLBINDVERTEXARRAYPROC)        glXGetProcAddress((const GLubyte *)"glBindVertexArray");
    glBindBuffer              = (PFNGLBINDBUFFERPROC)             glXGetProcAddress((const GLubyte *)"glBindBuffer");    
    glBufferData              = (PFNGLBUFFERDATAPROC)             glXGetProcAddress((const GLubyte *)"glBufferData");
    glVertexAttribPointer     = (PFNGLVERTEXATTRIBPOINTERPROC)    glXGetProcAddress((const GLubyte *)"glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glXGetProcAddress((const GLubyte *)"glEnableVertexAttribArray");
    glGetUniformLocation      = (PFNGLGETUNIFORMLOCATIONPROC)     glXGetProcAddress((const GLubyte *)"glGetUniformLocation");
    glUniform1f               = (PFNGLUNIFORM1FPROC)              glXGetProcAddress((const GLubyte *)"glUniform1f");
        
    
    return 1;
}

float get_delta_time()
{
    static double time_lf = 0;
    if (!time_lf)
        time_lf = get_current_time();

    double time_cf = get_current_time();
    float delta = time_cf - time_lf;

    time_lf = time_cf;
    return delta;
}

double get_current_time()
{
    long            ns; // Nanoseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec;
    ns = spec.tv_nsec;

    double currentSeconds = s + ns / 1.0e9;

    return currentSeconds;
}
