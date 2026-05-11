#include "platform/nk_platform.h"
#include "nuklear.h"
#include "platform/platform.h"

static struct nk_context *g_nk_ctx;

void nk_platform_init(struct nk_context *ctx) {
    g_nk_ctx = ctx;
}

void nk_platform_begin_frame(void) {
    if (g_nk_ctx) nk_input_begin(g_nk_ctx);
}

void nk_platform_end_frame(void) {
    if (g_nk_ctx) nk_input_end(g_nk_ctx);
}

/* Local keysym constants (standard X11 values) - no X11 header dependency */
#define KS_Shift_L   0xFFE1
#define KS_Shift_R   0xFFE2
#define KS_Control_L 0xFFE3
#define KS_Control_R 0xFFE4
#define KS_Alt_L     0xFFE9
#define KS_Alt_R     0xFFEA
#define KS_Delete    0xFFFF
#define KS_Return    0xFF0D
#define KS_KP_Enter  0xFF8D
#define KS_Tab       0xFF09
#define KS_BackSpace 0xFF08
#define KS_Up        0xFF52
#define KS_Down      0xFF54
#define KS_Left      0xFF51
#define KS_Right     0xFF53
#define KS_Home      0xFF50
#define KS_End       0xFF57
#define KS_Page_Up   0xFF55
#define KS_Page_Down 0xFF56
#define KS_F1        0xFFBE
#define KS_F2        0xFFBF
#define KS_F3        0xFFC0
#define KS_F4        0xFFC1
#define KS_F5        0xFFC2
#define KS_F6        0xFFC3
#define KS_F7        0xFFC4
#define KS_F8        0xFFC5
#define KS_F9        0xFFC6
#define KS_F10       0xFFC7
#define KS_F11       0xFFC8
#define KS_F12       0xFFC9

static enum nk_keys keysym_to_nk(unsigned long keysym) {
    switch (keysym) {
        case KS_Shift_L: case KS_Shift_R: return NK_KEY_SHIFT;
        case KS_Control_L: case KS_Control_R: return NK_KEY_CTRL;
        case KS_Alt_L: case KS_Alt_R: return NK_KEY_ALT;
        case KS_Delete: return NK_KEY_DEL;
        case KS_Return: case KS_KP_Enter: return NK_KEY_ENTER;
        case KS_Tab: return NK_KEY_TAB;
        case KS_BackSpace: return NK_KEY_BACKSPACE;
        case KS_Up: return NK_KEY_UP;
        case KS_Down: return NK_KEY_DOWN;
        case KS_Left: return NK_KEY_LEFT;
        case KS_Right: return NK_KEY_RIGHT;
        case KS_Home: return NK_KEY_TEXT_LINE_START;
        case KS_End: return NK_KEY_TEXT_LINE_END;
        case KS_Page_Up: return NK_KEY_SCROLL_UP;
        case KS_Page_Down: return NK_KEY_SCROLL_DOWN;
        case KS_F1: return NK_KEY_F1;
        case KS_F2: return NK_KEY_F2;
        case KS_F3: return NK_KEY_F3;
        case KS_F4: return NK_KEY_F4;
        case KS_F5: return NK_KEY_F5;
        case KS_F6: return NK_KEY_F6;
        case KS_F7: return NK_KEY_F7;
        case KS_F8: return NK_KEY_F8;
        case KS_F9: return NK_KEY_F9;
        case KS_F10: return NK_KEY_F10;
        case KS_F11: return NK_KEY_F11;
        case KS_F12: return NK_KEY_F12;
        default: return NK_KEY_NONE;
    }
}

void nk_platform_handle_event(const Event *event) {
    if (!g_nk_ctx) return;
    switch (event->type) {
        case EVENT_KEY_DOWN:
        case EVENT_KEY_UP: {
            enum nk_keys nk = keysym_to_nk(event->key.keysym);
            if (nk != NK_KEY_NONE) {
                nk_input_key(g_nk_ctx, nk,
                    event->type == EVENT_KEY_DOWN ? nk_true : nk_false);
            }
            break;
        }
        case EVENT_MOUSE_BUTTON: {
            enum nk_buttons btn = NK_BUTTON_LEFT;
            if (event->mouse_button.button == MOUSE_BUTTON_RIGHT) btn = NK_BUTTON_RIGHT;
            else if (event->mouse_button.button == MOUSE_BUTTON_MIDDLE) btn = NK_BUTTON_MIDDLE;
            nk_input_button(g_nk_ctx, btn, event->mouse_button.x, event->mouse_button.y,
                event->mouse_button.down ? nk_true : nk_false);
            break;
        }
        case EVENT_MOUSE_MOTION:
            nk_input_motion(g_nk_ctx, event->mouse_motion.x, event->mouse_motion.y);
            break;
        case EVENT_SCROLL:
            nk_input_scroll(g_nk_ctx, nk_vec2((float)event->scroll.dx, (float)event->scroll.dy));
            break;
        case EVENT_TEXT:
            nk_input_char(g_nk_ctx, event->text.c);
            break;
        default: break;
    }
}
