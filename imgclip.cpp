#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <string>


constexpr size_t chunk_size = 256*1024;
struct chunk_t {
    char data[chunk_size];
};
struct image_t {
    chunk_t chunk;              // TODO: Handle more chunks.
    size_t size;
};


void send_no(Display *dpy, XSelectionRequestEvent *sev)
{
    XSelectionEvent ssev;

    /* All of these should match the values of the request. */
    ssev.type = SelectionNotify;
    ssev.requestor = sev->requestor;
    ssev.selection = sev->selection;
    ssev.target = sev->target;
    ssev.property = None;  /* signifies "nope" */
    ssev.time = sev->time;

    XSendEvent(dpy, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
}

void send_targets(Display *dpy, XSelectionRequestEvent *sev, size_t count, Atom *target_list)
{
    XSelectionEvent ssev;

    XChangeProperty(dpy, sev->requestor, sev->property, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)target_list, (int)count);

    ssev.type = SelectionNotify;
    ssev.requestor = sev->requestor;
    ssev.selection = sev->selection;
    ssev.target = sev->target;
    ssev.property = sev->property;
    ssev.time = sev->time;

    XSendEvent(dpy, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
}

void send_utf8(Display *dpy, XSelectionRequestEvent *sev, Atom utf8, const char* path)
{
    XSelectionEvent ssev;

    XChangeProperty(dpy, sev->requestor, sev->property, utf8, 8, PropModeReplace,
                    (unsigned char *)path, strlen(path));

    ssev.type = SelectionNotify;
    ssev.requestor = sev->requestor;
    ssev.selection = sev->selection;
    ssev.target = sev->target;
    ssev.property = sev->property;
    ssev.time = sev->time;

    XSendEvent(dpy, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
}

void send_png(Display *dpy, XSelectionRequestEvent *sev, Atom png, const image_t image)
{
    XSelectionEvent ssev;

    XChangeProperty(dpy, sev->requestor, sev->property, png, 8, PropModeReplace,
                    (unsigned char *)image.chunk.data, image.size);

    ssev.type = SelectionNotify;
    ssev.requestor = sev->requestor;
    ssev.selection = sev->selection;
    ssev.target = sev->target;
    ssev.property = sev->property;
    ssev.time = sev->time;

    XSendEvent(dpy, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
}

image_t load_image(const char* image_path)
{
    image_t image;
    std::ifstream file{image_path, std::ios::binary};

    file.read(image.chunk.data, chunk_size);
    return image;
}

std::string get_url(const char* image_path)
{
    using namespace std::string_literals;
    const size_t path_size = strlen(image_path);
    char* path_buf = (char*)malloc(sizeof(char) * (path_size + 1));
    strncpy(path_buf, image_path, path_size + 1);
    const std::string url = std::string(BASE_URL) + basename(path_buf);
    free(path_buf);
    return url;
}

int main(int argc, char *argv[])
{
    Display *dpy;
    Window owner, root;
    int screen;
    Atom sel, utf8, png, targets;
    XEvent ev;
    XSelectionRequestEvent *sev;

    const char* image_path;
    if (argc != 2) {
        fprintf(stderr, "Argument required\n");
        return 2;
    } else {
        image_path = argv[1];
    }

    const std::string url = get_url(image_path);
    const image_t image = load_image(image_path);

    dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, "Could not open X display\n");
        return 1;
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    /* We need a window to receive messages from other clients. */
    owner = XCreateSimpleWindow(dpy, root, -10, -10, 1, 1, 0, 0, 0);

    sel = XInternAtom(dpy, "CLIPBOARD", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    png = XInternAtom(dpy, "image/png", False);
    targets = XInternAtom(dpy, "TARGETS", False);

    /* Claim ownership of the clipboard. */
    XSetSelectionOwner(dpy, sel, owner, CurrentTime);

    for (;;)
    {
        XNextEvent(dpy, &ev);
        switch (ev.type)
        {
            case SelectionClear:
                return 0;
                break;
            case SelectionRequest:
                sev = &ev.xselectionrequest;

                auto an = XGetAtomName(dpy, sev->target);
                fprintf(stderr, "Requested target: %s\n", an);
                if (an) {
                    XFree(an);
                }

                /* Property is set to None by "obsolete" clients. */
                if (sev->property == None) {
                    send_no(dpy, sev);
                } else if (sev->target == utf8) {
                    send_utf8(dpy, sev, utf8, url.c_str());
                } else if (sev->target == png) {
                    send_png(dpy, sev, png, image);
                } else if (sev->target == targets) {
                    Atom target_list[] = {targets, png, utf8};
                    send_targets(dpy, sev, sizeof(target_list) / sizeof(Atom), target_list);
                } else {
                    send_no(dpy, sev);
                }
                break;
        }
    }
}
