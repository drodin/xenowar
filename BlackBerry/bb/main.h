static const char* playbookSavePath = "./data";
static const char* playbookResourcePath = "./app/native/uforesource.db";

enum eMessageType
{
        MESSAGE_TYPE_GUI_CLICK_START,
        MESSAGE_TYPE_GUI_CLICK_MOVE, //only send when button/finger is held down
        MESSAGE_TYPE_GUI_CLICK_END,
        MESSAGE_TYPE_GUI_CLICK_CANCEL,
        MESSAGE_TYPE_GUI_GESTURE_PINCH,
        MESSAGE_TYPE_GUI_GESTURE_ROTATE,
        MESSAGE_TYPE_GUI_ACCELEROMETER,
        MESSAGE_TYPE_GUI_TRACKBALL,
        MESSAGE_TYPE_GUI_CHAR, //the input box uses it on windows since we don't have a virtual keyboard
        MESSAGE_TYPE_GUI_COPY,
        MESSAGE_TYPE_GUI_PASTE,
        MESSAGE_TYPE_GUI_TOGGLE_FULLSCREEN,

        MESSAGE_TYPE_SET_ENTITY_VARIANT,
        MESSAGE_TYPE_CALL_ENTITY_FUNCTION,
        MESSAGE_TYPE_CALL_COMPONENT_FUNCTION_BY_NAME,
        MESSAGE_TYPE_PLAY_SOUND,
        MESSAGE_TYPE_VIBRATE,
        MESSAGE_TYPE_REMOVE_COMPONENT,
        MESSAGE_TYPE_ADD_COMPONENT,
        MESSAGE_TYPE_OS_CONNECTION_CHECKED, //sent by macOS, will send an eOSSTreamEvent as parm1
        MESSAGE_TYPE_PLAY_MUSIC,
        MESSAGE_TYPE_UNKNOWN,
        MESSAGE_TYPE_PRELOAD_SOUND,
        MESSAGE_TYPE_GUI_CHAR_RAW,
        MESSAGE_TYPE_SET_SOUND_ENABLED,

        MESSAGE_TYPE_TAPJOY_AD_READY,
        MESSAGE_TYPE_TAPJOY_FEATURED_APP_READY,
        MESSAGE_TYPE_TAPJOY_MOVIE_AD_READY,

        MESSAGE_TYPE_IAP_RESULT,
        MESSAGE_TYPE_IAP_ITEM_STATE,

        MESSAGE_TYPE_TAPJOY_TAP_POINTS_RETURN,
        MESSAGE_TYPE_TAPJOY_TAP_POINTS_RETURN_ERROR,
        MESSAGE_TYPE_TAPJOY_SPEND_TAP_POINTS_RETURN,
        MESSAGE_TYPE_TAPJOY_SPEND_TAP_POINTS_RETURN_ERROR,
        MESSAGE_TYPE_TAPJOY_AWARD_TAP_POINTS_RETURN,
        MESSAGE_TYPE_TAPJOY_AWARD_TAP_POINTS_RETURN_ERROR,
        MESSAGE_TYPE_TAPJOY_EARNED_TAP_POINTS,
        MESSAGE_TYPE_GUI_JOYPAD_BUTTONS, //For Jake's android gamepad input
        MESSAGE_TYPE_GUI_JOYPAD, //For Jake's android gamepad input
        MESSAGE_TYPE_GUI_JOYPAD_CONNECT, // For Jakes android gamepad input

        MESSAGE_USER = 1000, //users can add their own messages starting here

};

bool Init(int width, int height);
void Kill();
void Update();
void Draw();
void OnEnterForeground();
void OnEnterBackground();
bool IsInBackground();
bool IsBaseAppInitted();
void SendGUI( eMessageType type, float parm1, float parm2 = 0);
void SendGUIVec( eMessageType type, double parm1, double parm2, double param3);
void SendGUIEx( eMessageType type, float parm1, float parm2, int finger);
