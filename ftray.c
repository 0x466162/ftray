#include <X11/Xlib-xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>
#include <X11/extensions/Xrandr.h>
#include <xcb/randr.h>
#include <time.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

#define SYSTEM_TRAY_REQUEST_DOCK 0

void clientmessage(xcb_client_message_event_t *);
void event_handle(xcb_generic_event_t *);
char *get_atom_name(xcb_atom_t);

Display 		*display;
xcb_connection_t	*conn;
xcb_window_t		 bar_id;
xcb_pixmap_t		 bar_buffer;
xcb_screen_t		*screen;
uint16_t		 wpx, hpx;
int			 xrandr_eventbase;
volatile sig_atomic_t    running = 1;
unsigned int		 mapped = 0;
enum {
	_NET_SYSTEM_TRAY_OPCODE,
	_XEMBED,
	MANAGER,
	XEMBED_EMBEDDED_NOTIFY,
};

struct mapped_clients {
	uint32_t	cid;
	TAILQ_ENTRY(mapped_clients) entries;
};
TAILQ_HEAD(, mapped_clients) mapped_clients_head;

struct ewmh_hint {
	char		*name;
	xcb_atom_t	 atom;
} ewmh[10] = {
	{"_NET_SYSTEM_TRAY_OPCODE", XCB_ATOM_NONE},
	{"_XEMBED", XCB_ATOM_NONE},
	{"MANAGER", XCB_ATOM_NONE},
	{"XEMBED_EMBEDDED_NOTIFY", XCB_ATOM_NONE},
};


void
clientmessage(xcb_client_message_event_t *e)
{
	Window			 tray_icon;
	uint32_t		 values[2];
	char 			*name;
	unsigned long 		 xembed_info[2];
	struct mapped_clients	*t_client, *t_clients;
	unsigned int		 mapping;

	mapping = 0;
	t_client = malloc(sizeof(*t_client));
	if (!TAILQ_EMPTY(&mapped_clients_head)) {
		TAILQ_FOREACH(t_clients, &mapped_clients_head, entries)
		    printf("tray client: 0x%x\n",t_clients->cid);
		    mapping++;
	}

	name = get_atom_name(e->type);
	t_client->cid = e->data.data32[2];
	printf("clientmessage: window: 0x%x, atom: %s(%u), client 0x%x\n",
	    e->window, name, e->type, t_client->cid);
	free(name);

	if (! e->data.data32[1] == SYSTEM_TRAY_REQUEST_DOCK &&
	    ! ( e->type == ewmh[_NET_SYSTEM_TRAY_OPCODE].atom && 
		    e->format == 32))
		return;

	xcb_reparent_window(conn, t_client->cid, bar_id, (wpx - 16) -
	    mapping * 18, 0);

	/*
	 * following block should be an own function
	 * update_systray()
	 */
	values[0] = 16;
	values[1] = 16;
	xcb_configure_window(conn, t_client->cid,
	    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
	    values);
	uint32_t buf[32];
	xcb_client_message_event_t		*ev =
	    (xcb_client_message_event_t*)buf;
	ev->response_type = XCB_CLIENT_MESSAGE;
	ev->window = t_client->cid;
	ev->type = ewmh[_XEMBED].atom;
	ev->format = 32;
	ev->data.data32[0] = XCB_CURRENT_TIME;
	ev->data.data32[1] = ewmh[XEMBED_EMBEDDED_NOTIFY].atom;
	ev->data.data32[2] = bar_id;
	ev->data.data32[3] = 1;
	xcb_send_event(conn, 0, t_client->cid, XCB_EVENT_MASK_NO_EVENT, (char*)e);
	xcb_change_save_set(conn, XCB_SET_MODE_INSERT, t_client->cid);
	xcb_map_window(conn, t_client->cid);
	xcb_flush(conn);

	TAILQ_INSERT_TAIL(&mapped_clients_head, t_client, entries);
}

char *
get_atom_name(xcb_atom_t atom)
{
	char *name = NULL;
#if 0
        /*
         * This should be disabled during most debugging since
         * xcb_get_* causes an xcb_flush.
         */
        size_t                          len;
        xcb_get_atom_name_reply_t       *r;

        r = xcb_get_atom_name_reply(conn,
            xcb_get_atom_name(conn, atom),
            NULL);
        if (r) {
                len = xcb_get_atom_name_name_length(r);
                if (len > 0) {
                        name = malloc(len + 1);
                        if (name) {
                                memcpy(name, xcb_get_atom_name_name(r), len);
                                name[len] = '\0';
                        }
                }
                free(r);
        }
#else
        (void)atom;
#endif
        return (name);
}

void
unmapnotify(xcb_unmap_notify_event_t *e)
{
	struct mapped_clients	*t_client;
	uint32_t		 cid;
	unsigned int		 mappings;
	
	/*
	 * activate the following as soon as update_systray() works
	 *cid = e->window;
	 */
	cid = 0;
	mappings = 0;
	TAILQ_FOREACH(t_client, &mapped_clients_head, entries) {
		if (t_client->cid == cid) {
			TAILQ_REMOVE(&mapped_clients_head, t_client,
			    entries);
			continue;
		}
		mappings++;
	}

	printf("%d clients left\n",mappings);
}


void
event_handle(xcb_generic_event_t *evt)
{
	uint8_t type = XCB_EVENT_RESPONSE_TYPE(evt);

	printf("Event type %s(%u)\n", xcb_event_get_label(type), type);
	switch(type){
#define EVENT(type, callback) case type: callback((void *)evt); return
	EVENT(XCB_CLIENT_MESSAGE, clientmessage);
	EVENT(XCB_UNMAP_NOTIFY, unmapnotify);
#undef EVENT
	}
	if (type - xrandr_eventbase == XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
		printf("jackpot\n");
	}
	printf("Mapped clients %d\n", mapped);
}

int
main(void)
{
	char 					 atomname[strlen("_NET_SYSTEM_TRAY_S") + 11];
	char             			*title = "Hello World !";
	uint32_t				 wa[3], buf[32];
	xcb_drawable_t				 bar;
	xcb_intern_atom_cookie_t		 stc;
	xcb_intern_atom_reply_t			*bar_rpy;
	xcb_get_selection_owner_cookie_t	 gsoc;
	xcb_get_selection_owner_reply_t		*gso_rpy;
	xcb_client_message_event_t		*ev =
	    (xcb_client_message_event_t*)buf;
	xcb_generic_event_t			*evt;
	xcb_randr_query_version_cookie_t	 c;
	xcb_randr_query_version_reply_t		*r;
	const xcb_query_extension_reply_t	*qep;

	TAILQ_INIT(&mapped_clients_head);

	conn = xcb_connect(NULL, NULL);
	bar_id = xcb_generate_id(conn);
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	wpx = screen->width_in_pixels;
	hpx = screen->height_in_pixels;
	
	wa[0] = screen->white_pixel;
	wa[1] = 1;
	wa[2] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
	xcb_create_window(conn, 0, bar_id,
	    screen->root, 0, hpx - 16, wpx, 16, 0,
	    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
	    XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
	    XCB_CW_EVENT_MASK, wa);

	 xcb_change_property (conn, XCB_PROP_MODE_REPLACE, bar_id, XCB_ATOM_WM_NAME,
	     XCB_ATOM_STRING, 8, strlen (title), title);

	xcb_map_window(conn, bar_id);

	/* System Tray Stuff */
	snprintf(atomname, strlen("_NET_SYSTEM_TRAY_S") + 11,
	    "_NET_SYSTEM_TRAY_S0");
	stc = xcb_intern_atom(conn, 0, strlen(atomname), atomname);
	if(!(bar_rpy = xcb_intern_atom_reply(conn, stc, NULL)))
		errx(1, "could not get atom %s\n", atomname);
	xcb_set_selection_owner(conn,bar_id,bar_rpy->atom, XCB_CURRENT_TIME);
	gsoc = xcb_get_selection_owner(conn, bar_rpy->atom);
	if (!(gso_rpy = xcb_get_selection_owner_reply(conn, gsoc,
			    NULL)))
		errx(1, "could not get selection owner for %s\n",
		    atomname);

	if (gso_rpy->owner != bar_id) {
		warnx("Another system tray running?\n");
		free(gso_rpy);
		return(1);
	}

	printf("Selection owner id: %x\n",gso_rpy->owner);

	/* Let everybody know that there is a system tray running */
	ev->response_type	= XCB_CLIENT_MESSAGE;
	ev->window		= screen->root;
	ev->type		= ewmh[MANAGER].atom;
	ev->format		= 32;
	ev->data.data32[0]	= XCB_CURRENT_TIME;
	ev->data.data32[1]	= bar_rpy->atom;
	ev->data.data32[2]	= bar_id;
	xcb_send_event(conn, 0, screen->root, 0xFFFFFF, (char *)buf);

	/* initial Xrandr setup */
	qep = xcb_get_extension_data(conn, &xcb_randr_id);
	if (qep->present) {
		c = xcb_randr_query_version(conn, 1, 1);
		r = xcb_randr_query_version_reply(conn, c, NULL);
		if (r) {
			if (r->major_version >=1) {
				xrandr_eventbase = qep->first_event;
				printf("XRandR supported\n");
			}
			free(r);
		}
	}

	printf("wxh: %ux%u\n",screen->width_in_pixels,screen->height_in_pixels);

	while (running) {
		nanosleep((struct timespec[]){{0, 500000000}}, NULL);
		xcb_aux_sync(conn);
		while ((evt = xcb_wait_for_event(conn))) {
			event_handle(evt);
			free(evt);
		}
	}
	return 0;
}
