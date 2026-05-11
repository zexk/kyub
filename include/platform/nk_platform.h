#ifndef NK_PLATFORM_H
#define NK_PLATFORM_H

struct nk_context;
typedef struct Event Event;

void nk_platform_init(struct nk_context *ctx);
void nk_platform_begin_frame(void);
void nk_platform_handle_event(const Event *event);
void nk_platform_end_frame(void);

#endif
