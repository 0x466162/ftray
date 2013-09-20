/*
 * $Id: ftray.c,v 1.4 2013/08/31 12:55:16 fab Exp $
 */
#include <X11/Xlib-xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>

#define SYSTEM_TRAY_REQUEST_DOCK 0

void clientmessage(xcb_client_message_event_t *);
char *get_atom_name(xcb_atom_t);

Display 		*display;
xcb_connection_t	*conn;
xcb_screen_t		*screen;
volatile sig_atomic_t    running = 1;

enum {
	_NET_SYSTEM_TRAY_OPCODE,
	_XEMBED,
	MANAGER,
	XEMBED_EMBEDDED_NOTIFY,
};

struct ewmh_hint {
	char		*name;
	xcb_atom_t	 atom;
} ewmh[10] = {
	{"_NET_SYSTEM_TRAY_OPCODE", XCB_ATOM_NONE},
	{"_XEMBED", XCB_ATOM_NONE},
	{"MANAGER", XCB_ATOM_NONE},
	{"XEMBED_EMBEDDED_NOTIFY", XCB_ATOM_NONE},
};

struct stray_icons {
	int	w;
	int	h;
	int	p;
};

struct swm_geometry {
	int	x;
	int	y;
	int	w;
	int	h;
};

struct swm_stray {
	xcb_window_t		id;
	xcb_pixmap_t		buffer;
	struct swm_geometry	g;
	struct stray_icons	i;
	unsigned int		clients;
};

struct swm_stray *stray = malloc(sizeof(struct swm_stray));

void
clientmessage(xcb_client_message_event_t *e)
{
	Window tray_icon;
	uint32_t cid, values[2];
	char *name;
	int offset;
	unsigned long xembed_info[2];
	name = get_atom_name(e->type);
	cid = e->data.data32[2];
	printf("clientmessage: window: 0x%x, atom: %s(%u), client 0x%x\n",
	    e->window, name, e->type, cid);
	free(name);

	if (! e->data.data32[1] == SYSTEM_TRAY_REQUEST_DOCK &&
	    ! ( e->type == ewmh[_NET_SYSTEM_TRAY_OPCODE].atom && 
		    e->format == 32))
		return;

	offset = (stray->g.w - stray->i.w - stray->clients) *
	    (stray->i.w + stray->i.p);

	xcb_reparent_window(conn, cid, stray->id, offset, 0);

	values[0] = stray->i.w;
	values[1] = stray->i.h;
	xcb_configure_window(conn, cid,
	    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
	    values);

	uint32_t buf[32];
	xcb_client_message_event_t		*ev =
	    (xcb_client_message_event_t*)buf;
	ev->response_type = XCB_CLIENT_MESSAGE;
	ev->window = cid;
	ev->type = ewmh[_XEMBED].atom;
	ev->format = 32;
	ev->data.data32[0] = XCB_CURRENT_TIME;
	ev->data.data32[1] = ewmh[XEMBED_EMBEDDED_NOTIFY].atom;
	ev->data.data32[2] = stray->id;
	ev->data.data32[3] = 1;
	xcb_send_event(conn, 0, cid, XCB_EVENT_MASK_NO_EVENT, (char*)e);
	xcb_change_save_set(conn, XCB_SET_MODE_INSERT, cid);
	xcb_map_window(conn, cid);
	stray->clients++;
	xcb_flush(conn);

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

int
main(void)
{
	struct swm_stray			*stray;
	char 					 atomname[strlen("_NET_SYSTEM_TRAY_S") + 11];
	char             			*title = "Hello World !";
	uint32_t				 wa[2], buf[32];
	xcb_drawable_t				 bar;
	xcb_intern_atom_cookie_t		 stc;
	xcb_intern_atom_reply_t			*bar_rpy;
	xcb_get_selection_owner_cookie_t	 gsoc;
	xcb_get_selection_owner_reply_t		*gso_rpy;
	xcb_client_message_event_t		*ev =
	    (xcb_client_message_event_t*)buf;
	xcb_generic_event_t			*evt;


	conn = xcb_connect(NULL, NULL);
	printf("generating id\n");
	stray->id = xcb_generate_id(conn);
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	printf("Setting up icon size and spacing\n");
	stray->clients = 0;
	printf("Setting up icon size and spacing\n");
	stray->i.w = 8;
	printf("Setting up icon size and spacing\n");
	stray->i.p = 2;
	printf("Setting up icon size and spacing done\n");
	
	wa[0] = screen->white_pixel;
	wa[1] = 1;
	printf("Creating the window\n");
	xcb_create_window(conn, 0, stray->id,
	    screen->root, 0, 784, 1280, 16, 0,
	    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
	    XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT, wa);

	printf("Mapping the window\n");
	xcb_map_window(conn, stray->id);

	/* System Tray Stuff */
	snprintf(atomname, strlen("_NET_SYSTEM_TRAY_S") + 11,
	    "_NET_SYSTEM_TRAY_S0");
	stc = xcb_intern_atom(conn, 0, strlen(atomname), atomname);
	if(!(bar_rpy = xcb_intern_atom_reply(conn, stc, NULL)))
		errx(1, "could not get atom %s\n", atomname);
	xcb_set_selection_owner(conn,stray->id,bar_rpy->atom, XCB_CURRENT_TIME);
	gsoc = xcb_get_selection_owner(conn, bar_rpy->atom);
	if (!(gso_rpy = xcb_get_selection_owner_reply(conn, gsoc,
			    NULL)))
		errx(1, "could not get selection owner for %s\n",
		    atomname);

	if (gso_rpy->owner != stray->id) {
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
	ev->data.data32[2]	= stray->id;
	xcb_send_event(conn, 0, screen->root, 0xFFFFFF, (char *)buf);

	xcb_flush(conn);

	while (running) {
		sleep(1);
		while ((evt = xcb_poll_for_event(conn))) {
			//printf("      Pad0 %u\n",evt->pad0);
			//printf("  Sequence %u\n",evt->sequence);
			//printf("    Pad[7] %u\n",evt->pad[7]);
			//printf(" Full Seq. %u\n",evt->full_sequence);
			if (evt->response_type == 161) {
				printf("Event type %s(%u)\n",
				    xcb_event_get_label(XCB_EVENT_RESPONSE_TYPE(evt)),
				    evt->response_type);
				clientmessage((void *)evt);
			}
			free(evt);
		}
	}
	return 0;
}
