#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifndef __MORPHOS__
#include <unistd.h>
#include <pwd.h>
#endif

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef __MORPHOS__
unsigned long _stack = 1024 * 1024 * 2;
#endif

bool fullscreen = false;

#include "game.h"

#define WND_TITLE    "OpenLara"
#define SDL_WINDOW_WIDTH           800
#define SDL_WINDOW_HEIGHT          600

typedef struct {
    SDL_Window *window;
#if defined(_GAPI_SW)    
    SDL_Surface *surface;
#else
    SDL_GLContext context;
#endif
} AppState;

AppState *as;

// Some functions
static void screenshot(const char *fileName) {
#if defined(_GAPI_SW)
//TODO
#else	
    int width  = Core::width;
	int height = Core::height;
	int size = width * height * 4;
    char *data = new char[size];
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

	char* flipped = new char[size];
	for (int y = 0; y < height; y++) {
		memcpy(
			flipped + y * width * 4,
			data + (height - 1 - y) * width * 4,
			width * 4
		);
	}

	Texture::SaveBMP(fileName, flipped, width, height);
	delete[] flipped;
	delete[] data;
#endif
}

void toggleFullscreen () {

    Uint32 flags = 0;
	int w, h;
    
	fullscreen = !fullscreen;
    SDL_SetWindowFullscreen (as->window, fullscreen);

    // Tell the engine we have changed display size!
	SDL_GetWindowSizeInPixels(as->window, &w, &h);
	Core::width  = w;
    Core::height = h;

}

// GamePad

#define MAX_JOYS 4
#define JOY_DEAD_ZONE_STICK      8192

struct sdl_input *sdl_inputs;
int sdl_numjoysticks, sdl_numgamepads;
SDL_Joystick *sdl_joysticks[MAX_JOYS];
SDL_Gamepad *sdl_gamepads[MAX_JOYS];
vec2 joyL, joyR;

bool osJoyReady(int index) {
    return index == 0; // TODO
}
void osJoyVibrate(int index, float L, float R) {
	
	if (R == 0.0 && L == 0.0)
			return;
	//if (index >= sdl_numgamepads)
    //    return;
	index++;
#ifdef __MORPHOS__
		//SDL_Log("L=%f R=%f\n", L, R);
		L = L * 2;
		R = R * 2;
		if (L > 1.0f) L = 1.0f;
		if (R > 1.0f) R = 1.0f;
#endif

    if (SDL_IsGamepad(index)) {
        SDL_RumbleGamepad(sdl_gamepads[index], L*0xFFFF, R*0xFFFF, 500);
	}
}

int joyGetIndex(SDL_JoystickID id) {
    int i;
    for (i = 0 ; i < sdl_numjoysticks; i++) {
		//SDL_Log("SDL_GetJoystickID(sdl_joysticks[i])=%d i=%d id=%d", SDL_GetJoystickID(sdl_joysticks[i]), i, id);
        if (SDL_GetJoystickID(sdl_joysticks[i]) == id-1) {
            return i;
        }
    }
    return -1;
}

bool joyIsController (Uint32 instanceID) {
    int i;
    bool ret = false;
    
    // We can't use SDL_IsGameController after we have physically disconnected a joystick, so we use this workaround.
    for (i = 0; i < sdl_numgamepads; i++) {
        if (SDL_GetJoystickID(SDL_GetGamepadJoystick(sdl_gamepads[i])) == instanceID) {
            ret = true;
            break;
        }
    }
    return ret;
}

void joyAdd(int index) {
	//SDL_Log("joyAdd index=%d", index);
    if(SDL_IsGamepad(index)) {
		//SDL_Log("joyAdd SDL_IsGamepad");
        SDL_Gamepad *gamepad = SDL_OpenGamepad(index);
		//if (gamepad)
		//	SDL_Log("joyAdd SDL_OpenGamepad");
        sdl_gamepads[index] = gamepad;
        sdl_joysticks[index] = SDL_GetGamepadJoystick(gamepad);
        sdl_numgamepads++;
    }
    else {
        sdl_joysticks[index] = SDL_OpenJoystick(index);
    }
    // Update number of joysticks
    SDL_GetJoysticks(&sdl_numjoysticks);
    sdl_numjoysticks = (sdl_numjoysticks < MAX_JOYS )? sdl_numjoysticks : MAX_JOYS;
}

void joyRemove(Uint32 instanceID) {
    int i;
	//SDL_Log("joyRemove index=%d", instanceID);
    // Closing game controller
    if (joyIsController(instanceID)) {
        for (i = 0; i < sdl_numgamepads; i++) {
            if (SDL_GetJoystickID(SDL_GetGamepadJoystick(sdl_gamepads[i])) == instanceID) {
                SDL_CloseGamepad(sdl_gamepads[i]);
                sdl_gamepads[i] = NULL;
                sdl_numgamepads--;
                sdl_numjoysticks--;
            }
        }   
    }
    // Closing joystick
    else {
        i = joyGetIndex(instanceID);    
        if (i >= 0) {
            SDL_CloseJoystick(sdl_joysticks[i]);
             sdl_numjoysticks--;
        }
    }
}
bool inputInit() {
    int index;
    joyL = joyR = vec2(0);
    SDL_GetJoysticks(&sdl_numjoysticks);
    sdl_numjoysticks = (sdl_numjoysticks < MAX_JOYS )? sdl_numjoysticks : MAX_JOYS;
    for (index = 0; index < MAX_JOYS; index++)
        sdl_joysticks[index] = NULL;

    for (index = 0; index < sdl_numjoysticks; index++)
        joyAdd(index);
    return true;
}

void inputFree() {
    int i;
    Sint32 instanceID;

    for (i = 0; i < sdl_numjoysticks; i++) {
        instanceID = SDL_GetJoystickID(sdl_joysticks[i]);
        joyRemove(instanceID);
    }
}

float joyAxisValue(int value) {
    if (value > -JOY_DEAD_ZONE_STICK && value < JOY_DEAD_ZONE_STICK)
        return 0.0f;
    return value / 32767.0f;
}

float joyTrigger(int value) {
    return min(1.0f, value / 255.0f);
}

vec2 joyDir(const vec2 &value) {
    float dist = min(1.0f, value.length());
    return value.normal() * dist;
}


JoyKey controllerCodeToJoyKey(int code) {
// joystick using the modern SDL GameController interface
    switch (code) {
        case SDL_GAMEPAD_BUTTON_SOUTH                    : return jkA;
        case SDL_GAMEPAD_BUTTON_EAST                    : return jkB;
        case SDL_GAMEPAD_BUTTON_WEST                    : return jkX;
        case SDL_GAMEPAD_BUTTON_NORTH                    : return jkY;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER         : return jkLB;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER        : return jkRB;
        case SDL_GAMEPAD_BUTTON_BACK                 : return jkSelect;
        case SDL_GAMEPAD_BUTTON_START                : return jkStart;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK            : return jkL;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK           : return jkR;
        case SDL_GAMEPAD_BUTTON_DPAD_UP              : return jkUp;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN            : return jkDown;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT            : return jkLeft;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT           : return jkRight;
    }
    return jkNone;
}

JoyKey joyCodeToJoyKey(int buttonNumber) {
// joystick using the classic SDL Joystick interface
    switch (buttonNumber) {
        case 0 : return jkY;
        case 1 : return jkB;
        case 2 : return jkA;
        case 3 : return jkX;
        case 4 : return jkL;
        case 5 : return jkR;
        case 6 : return jkLB;
        case 7 : return jkRB;
        case 8 : return jkSelect;
        case 9 : return jkStart;
    }
    return jkNone;
}

// timing
unsigned int startTime;

int osGetTimeMS() {

    timeval t;
    gettimeofday(&t, NULL);
    return int((t.tv_sec - startTime) * 1000 + t.tv_usec / 1000);
    
}

// sound
#define SND_FRAME_SIZE  4
#define SND_FRAMES      256
#define FREQ		44100

void sndFill(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {

	int count = 0; 
	if (!additional_amount) {
		return;
	}
	while (additional_amount > 0) {
		count = (additional_amount / SND_FRAME_SIZE > SND_FRAMES) ? SND_FRAMES : additional_amount / SND_FRAME_SIZE;
		Sound::fill((Sound::Frame*) userdata, count);
		if (!SDL_PutAudioStreamData(stream, userdata, count * SND_FRAME_SIZE)) {
			LOG("Couldn't flush audio stream: %s", SDL_GetError());
			return;
		}
		additional_amount -= count * SND_FRAME_SIZE;
	}
	
}

bool sndInit() {

	SDL_AudioSpec desired;
	SDL_AudioStream *stream;
	Sound::Frame        *sndData;

	// Initialize audio buffer and fill it with zeros
	sndData = new Sound::Frame[SND_FRAMES];
	memset(sndData, 0, SND_FRAMES * SND_FRAME_SIZE);

	desired.freq     = FREQ;
	desired.format   = SDL_AUDIO_S16;
	desired.channels = 2;

	stream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK , &desired, sndFill, sndData );
	
	if (!stream) {
		LOG("Couldn't create audio stream: %s\n", SDL_GetError());
		return false;
	}
	
        SDL_ResumeAudioStreamDevice(stream);
	return true;
	
};

//input 


InputKey codeToInputKey(int code) {

    switch (code) {
	// keyboard
        case SDL_SCANCODE_LEFT       : return ikLeft;
        case SDL_SCANCODE_RIGHT      : return ikRight;
        case SDL_SCANCODE_UP         : return ikUp;
        case SDL_SCANCODE_DOWN       : return ikDown;
        case SDL_SCANCODE_SPACE      : return ikSpace;
        case SDL_SCANCODE_TAB        : return ikTab;
        case SDL_SCANCODE_RETURN     : return ikEnter;
        case SDL_SCANCODE_ESCAPE     : return ikEscape;
        case SDL_SCANCODE_LSHIFT     :
        case SDL_SCANCODE_RSHIFT     : return ikShift;
        case SDL_SCANCODE_LCTRL      :
        case SDL_SCANCODE_RCTRL      : return ikCtrl;
        case SDL_SCANCODE_LALT       :
        case SDL_SCANCODE_RALT       : return ikAlt;
        case SDL_SCANCODE_0          : return ik0;
        case SDL_SCANCODE_1          : return ik1;
        case SDL_SCANCODE_2          : return ik2;
        case SDL_SCANCODE_3          : return ik3;
        case SDL_SCANCODE_4          : return ik4;
        case SDL_SCANCODE_5          : return ik5;
        case SDL_SCANCODE_6          : return ik6;
        case SDL_SCANCODE_7          : return ik7;
        case SDL_SCANCODE_8          : return ik8;
        case SDL_SCANCODE_9          : return ik9;
        case SDL_SCANCODE_A          : return ikA;
        case SDL_SCANCODE_B          : return ikB;
        case SDL_SCANCODE_C          : return ikC;
        case SDL_SCANCODE_D          : return ikD;
        case SDL_SCANCODE_E          : return ikE;
        case SDL_SCANCODE_F          : return ikF;
        case SDL_SCANCODE_G          : return ikG;
        case SDL_SCANCODE_H          : return ikH;
        case SDL_SCANCODE_I          : return ikI;
        case SDL_SCANCODE_J          : return ikJ;
        case SDL_SCANCODE_K          : return ikK;
        case SDL_SCANCODE_L          : return ikL;
        case SDL_SCANCODE_M          : return ikM;
        case SDL_SCANCODE_N          : return ikN;
        case SDL_SCANCODE_O          : return ikO;
        case SDL_SCANCODE_P          : return ikP;
        case SDL_SCANCODE_Q          : return ikQ;
        case SDL_SCANCODE_R          : return ikR;
        case SDL_SCANCODE_S          : return ikS;
        case SDL_SCANCODE_T          : return ikT;
        case SDL_SCANCODE_U          : return ikU;
        case SDL_SCANCODE_V          : return ikV;
        case SDL_SCANCODE_W          : return ikW;
        case SDL_SCANCODE_X          : return ikX;
        case SDL_SCANCODE_Y          : return ikY;
        case SDL_SCANCODE_Z          : return ikZ;
        case SDL_SCANCODE_AC_HOME    : return ikEscape;
    }
    return ikNone;
    
}

void print_help(int argc, char **argv) {
    
    printf("%s [OPTION]\nA open source re-implementation of the classic Tomb Raider engine.\n",
           argc ? argv[0] : "OpenLara");
    puts("-d [DIRECTORY]  directory where data files are");
    puts("-l [FILE]       load a specific level file");
    puts("-h              print this help");
    
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {

    cacheDir[0] = saveDir[0] = contentDir[0] = 0;
    char *lvlName = nullptr;

    int option;
    while ((option = getopt(argc, argv, "hl:d:")) != -1) {
        switch(option) {
            case 'h':
                print_help(argc, argv);
                return SDL_APP_SUCCESS;
            case 'l':
               lvlName = optarg;
               break;
            case 'd':
               strncpy(contentDir, optarg, 254);
               break;
            case ':':
                LOG("option %c needs a value\n", optopt);
                print_help(argc, argv);
                return SDL_APP_FAILURE;
            case '?':
                LOG("unknown option: %c\n", optopt);
                print_help(argc, argv);
                return SDL_APP_FAILURE;
            default:
                break;
        }
    }

    size_t contentDirLen = strlen(contentDir);

    if (contentDirLen > 0 &&
        contentDir[contentDirLen-1] != '/' && contentDir[contentDirLen-1] != '\\' &&
        contentDirLen < 254) {
        contentDir[contentDirLen] = '/';
        contentDir[contentDirLen+1] = '\0';
    }

    const char *home;
#ifdef __MORPHOS__
	home="PROGDIR:";
	strcat(cacheDir, home);
    strcat(cacheDir, ".openlara/");
#else
    if (!(home = getenv("HOME")))
        home = getpwuid(getuid())->pw_dir;
	   strcat(cacheDir, home);
    strcat(cacheDir, "/.openlara/");
#endif

    struct stat st = {0};
    if (stat(cacheDir, &st) == -1 && mkdir(cacheDir, 0777) == -1)
        cacheDir[0] = 0;
    strcpy(saveDir, cacheDir);
	
    timeval t;
    gettimeofday(&t, NULL);
    startTime = t.tv_sec;

	SDL_SetAppMetadata("OpenLara SDL3 OpenGL MorphOS", "1.0", "info.xproger.openlara");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
		LOG("Couldn't init SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
	
    as = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!as) {
        LOG("Couldn't init app state: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    *appstate = as;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  
	as->window = SDL_CreateWindow(WND_TITLE,
                              SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT,
                              SDL_WINDOW_OPENGL);
	int w, h;
	SDL_GetWindowSizeInPixels(as->window, &w, &h);
	Core::width  = w;
	Core::height = h;

	
#if defined(_GAPI_SW)
    as->surface = SDL_GetWindowSurface(as->window);
    if (as->surface == NULL) {
        LOG("Couldn't get surface: %s", SDL_GetError());   
		return SDL_APP_FAILURE;
    }
    SDL_LockSurface(as->surface);

    GAPI::swColor = (unsigned int*)as->surface->pixels;
    GAPI::resize();
#else
	as->context = SDL_GL_CreateContext(as->window);	
#endif

	SDL_HideCursor();

    if (!sndInit()) {
	//return SDL_APP_FAILURE;
    }
	
	inputInit();

    Game::init(lvlName);

    return SDL_APP_CONTINUE;
    
}

int joyIndex;

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {

    InputKey key = ikNone;

    switch (event->type) 
    {
		case SDL_EVENT_QUIT:
			Core::isQuit = true;
			return SDL_APP_SUCCESS;   
		
		case SDL_EVENT_KEY_DOWN: {
			int scancode = event->key.scancode;
			key = codeToInputKey(scancode);
			if (key != ikNone) {
				Input::setDown(key, 1);
			} 
				
			if (event->key.mod & SDL_KMOD_ALT && scancode == SDL_SCANCODE_RETURN) {
	            toggleFullscreen();
			}
	        
			if (scancode == SDL_SCANCODE_F1) {
	            screenshot("screenshot");		
	        }
			break;
		}
			
		case SDL_EVENT_KEY_UP:	
		{
			int scancode = event->key.scancode;
			key = codeToInputKey(scancode);
			if (key != ikNone) {
				Input::setDown(key, 0);
			} 
		}
		// Joystick reading using the modern SDL GameController interface
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
			joyIndex = joyGetIndex(event->gbutton.which);
			JoyKey key = controllerCodeToJoyKey(event->gbutton.button);
			Input::setJoyDown(joyIndex, key, 1);
			break;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_UP: {				
			joyIndex = joyGetIndex(event->gbutton.which);
			JoyKey key = controllerCodeToJoyKey(event->gbutton.button);
			Input::setJoyDown(joyIndex, key, 0);
			break;
		}
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			joyIndex = joyGetIndex(event->gaxis.which);
			switch (event->gaxis.axis) {
				case SDL_GAMEPAD_AXIS_LEFTX:
					joyL.x = joyAxisValue(event->gaxis.value);
					break;
				case SDL_GAMEPAD_AXIS_LEFTY:
					joyL.y = joyAxisValue(event->gaxis.value);
					break;
				case SDL_GAMEPAD_AXIS_RIGHTX:
					joyR.x = joyAxisValue(event->gaxis.value);
					break;
					
				case SDL_GAMEPAD_AXIS_RIGHTY:
					joyR.y = joyAxisValue(event->gaxis.value);
					break;
			}
			Input::setJoyPos(joyIndex, jkL, joyDir(joyL));
			Input::setJoyPos(joyIndex, jkR, joyDir(joyR));
			break;
		}
		case SDL_EVENT_GAMEPAD_ADDED: {
			 joyAdd(event->gdevice.which);
			 break;
		}
		case SDL_EVENT_GAMEPAD_REMOVED: {
			joyRemove(event->gdevice.which);
			break;
		}
    }
    return SDL_APP_CONTINUE;
    
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {

    LOG("Exiting\n");
    
    Game::deinit();
    
    if (appstate != NULL) {
        as = (AppState *)appstate;
        
#if defined(_GAPI_SW)
		SDL_DestroySurface(as->surface);
#else
		SDL_GL_DestroyContext(as->context);
#endif

		SDL_DestroyWindow(as->window);

        SDL_free(as);
    }
    
}

SDL_AppResult SDL_AppIterate(void *appstate) {

	as = (AppState *)appstate;
	
	if(Core::isQuit) {
		return SDL_APP_SUCCESS;
	}
	if (Game::update()) {
            Game::render();
			Core::waitVBlank();
#if defined(_GAPI_SW)
            SDL_UpdateWindowSurface(as->window);
#else
            SDL_GL_SwapWindow(as->window);
#endif
        }

	return SDL_APP_CONTINUE;
	
}
