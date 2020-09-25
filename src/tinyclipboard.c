/* tinyclipboard - a cross-platform C library for accessing the clipboard.
 *
 * Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
 *
 * All rights reserved. See the README and LICENSE files for the
 * licensing conditions.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#if defined(__unix__)
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <langinfo.h>
#include <iconv.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>

/* Helper variables */
static pid_t s_cb_pid = 0;
static Window s_clipowner_window = None;

/* Helper functions */
static void finish_subprocess_on_exit(void);
static void child_handle_sigint(int);
static void own_x11_clipboard(int filedes);
static void handle_x11_selectionrequest(Display* p_display, XEvent evt, const char* cliptext, int textlen);
static bool write_to_clipboard_manager(const char* cliptext, int len);
static void get_clipboard_text(int filedes, char** p_str, int* p_len);

#elif defined(_WIN32)
#define WINVER 0x0600 /* >= Windows Vista */
#include <windows.h>

/* Helper functions */
LRESULT Win32MessageHandler(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
#else
#error Dont know how to access the clipboard on this OS!
#endif

#include "../include/tinyclipboard.h"

/*
 * Resources:
 * - https://stackoverflow.com/questions/10570315/clipboard-selection-transfer-does-not-work
 * - https://web.archive.org/web/20120923013354/http://michael.toren.net/mirrors/doc/X-copy+paste.txt
 * - https://github.com/Quintus/imitator/blob/master/imitator_x/ext/clipboard.c
 * - https://wiki.archlinux.org/index.php/Clipboard
 * - https://wiki.freedesktop.org/www/ClipboardManager/
 * - http://www.x.org/docs/ICCCM/icccm.pdf
 * - xclip(1) sources
 *
 * - https://msdn.microsoft.com/en-us/library/windows/desktop/aa383745%28v=vs.85%29.aspx
 * - https://msdn.microsoft.com/en-us/library/windows/desktop/dd374131%28v=vs.85%29.aspx
 * - https://msdn.microsoft.com/en-us/library/windows/desktop/dd374130%28v=vs.85%29.aspx
 * - https://msdn.microsoft.com/en-us/library/windows/desktop/ms648709%28v=vs.85%29.aspx
 */

/****************************************
 * Public API
 ***************************************/

char* tiny_clipread(int* len)
{
#if defined(__unix__)
  Display* p_display = NULL;
  Window window = None;
  XEvent evt;
  Atom clipboard;
  Atom utf8;
  Atom store_prop;

  p_display = XOpenDisplay(NULL);
  if (!p_display) {
    errno = ECONNREFUSED;
    return NULL;
  }

  /* Allocate atoms */
  clipboard = XInternAtom(p_display, "CLIPBOARD", False);       /* CLIPBOARD is the atom for the win32-like clipboard */
  utf8 = XInternAtom(p_display, "UTF8_STRING", True);           /* Resource for UTF-8 text */
  store_prop = XInternAtom(p_display, "TINYCLIP_STORE", False); /* Our custom window property for storage */

  /* Check if there is a clipboard owner that can answer me */
  if (XGetSelectionOwner(p_display, clipboard) == None) {
    XCloseDisplay(p_display);
    errno = EAGAIN;
    return NULL;
  }

  /* Request selection content */
  window = XCreateSimpleWindow(p_display, XDefaultRootWindow(p_display), 0, 0, 1, 1, 0, 0, 0);
  XConvertSelection(p_display, clipboard, utf8, store_prop, window, CurrentTime);

  /* X11 will send us a SelectionNotify event when the result
   * is available from the owner. */
  for(;;) {
    XNextEvent(p_display, &evt);
    if (evt.type == SelectionNotify) {
      break;
    }
  }

  /* property is None here if the owner is unable to convert the
   * selection to the requested format (UTF-8 text). */
  if (evt.xselection.property == None) {
    XDestroyWindow(p_display, window);
    XCloseDisplay(p_display);
    errno = ENOTSUP;
    return NULL;
  }
  else {
    Atom actual_type;
    int actual_format = 0;
    unsigned long nitems = 0;
    unsigned long bytes_left = 0;
    int bytes = 0;
    unsigned char* property = NULL;
    char* outbuf = NULL;

    /* Get the length of the data that was stored in our property. */
    if (XGetWindowProperty(p_display, window, store_prop,
			   0, 0, False,
			   AnyPropertyType, &actual_type, &actual_format,
			   &nitems, &bytes_left, &property) != Success) {
      /* In theory, we should never get here */
      XDestroyWindow(p_display, window);
      XCloseDisplay(p_display);
      errno = ECANCELED;
      return NULL;
    }

    /* I need to constrain to int as the largest common type */
    if (bytes_left + 1 > INT_MAX) {
      XDestroyWindow(p_display, window);
      XCloseDisplay(p_display);
      errno = EOVERFLOW;
      return NULL;
    }

    /* Prepare return buffer (with terminating NUL) */
    outbuf = (char*) calloc(bytes_left + 1, sizeof(char));
    bytes = (int) bytes_left; /* INT_MAX checked above */

    /* Retrieval requested with enough space */
    if (XGetWindowProperty(p_display, window, store_prop,
			   0, bytes_left /* / 8 ?? */, False,
			   AnyPropertyType, &actual_type, &actual_format,
			   &nitems, &bytes_left, &property) != Success) {
      /* In theory, we should never get here */
      XDestroyWindow(p_display, window);
      XCloseDisplay(p_display);
      errno = ECANCELED;
      return NULL;
    }

    /* Copy the data (note `bytes_left' is 0 here, hence I use `bytes',
     * which was set above). */
    strncpy(outbuf, (char*)property, bytes);
    if (len)
      *len = bytes;

    /* Cleanup */
    XFree(property);
    XDestroyWindow(p_display, window);
    XCloseDisplay(p_display);

    /* Finish */
    return outbuf;
  }
#elif defined(_WIN32)
  HGLOBAL global_handle = NULL;
  LPWSTR cliptext = NULL;
  int bufsize = 0;
  char* outbuf = NULL;

  if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    /* Unsupported data format */
    errno = ENOTSUP;
    return NULL;
  }
  if (!OpenClipboard(NULL)) {
    /* Another application has the clipboard open */
    errno = EAGAIN;
    return NULL;
  }

  global_handle = GetClipboardData(CF_UNICODETEXT);
  if (!global_handle) {
    /* Clipboard owner lied before and has no unicode data */
    CloseClipboard();
    errno = ENOTSUP;
    return NULL;
  }

  cliptext = GlobalLock(global_handle);
  bufsize = WideCharToMultiByte(CP_UTF8, 0, cliptext, -1, NULL, 0, NULL, NULL);
  if (!bufsize) {
    /* There was invalid UTF-16 on the clipboard */
    GlobalUnlock(global_handle);
    CloseClipboard();
    errno = EILSEQ;
    return NULL;
  }

  outbuf = calloc(bufsize, 1);
  bufsize = WideCharToMultiByte(CP_UTF8, 0, cliptext, -1, outbuf, bufsize, NULL, NULL);
  if (!bufsize) {
    /* This should not happen in theory */
    free(outbuf);
    GlobalUnlock(global_handle);
    CloseClipboard();
    errno = ECANCELED;
    return NULL;
  }

  /* Cleanup */
  GlobalUnlock(global_handle);
  CloseClipboard();

  if (len)
    *len = bufsize;

  return outbuf;
#else
#error Dont know how to read the clipboard on this platform!
#endif
}

int tiny_clipnwrite(const char* text, int len)
{
#if defined(__unix__)
  static unsigned short tries = 0;
  static int pipefds[2];
  static int has_registered_exit_handler = 0;
  Display* p_display = XOpenDisplay(NULL);

  if (!p_display) { /* No X11 server running */
    errno = ECONNREFUSED;
    return -1;
  }
  else {
    XCloseDisplay(p_display);
  }

  /* If a clipboard manager can take over, we do not do all this
   * hard fork() work and simply have it serve the content. */
  if (write_to_clipboard_manager(text, len))
    return 0;

  if (!s_cb_pid) { /* No clipboard handler process has been spawned yet. Do it now. */
    if (tries++ > 3) {
      /* 3 times in a row failed while recursing,
       * fail finally to prevent endless recursion. */
      tries = 0;
      errno = ECHILD;
      return -1;
    }

    /* Create child communication pipe */
    if (pipe(pipefds) < 0) {
      errno = EPIPE;
      return -1;
    }

    /* I don't want this pipe to block; see get_clipboard_text(). */
    fcntl(pipefds[0], F_SETFL, O_NONBLOCK);

    switch(s_cb_pid = fork()) { /* single = intended */
    case -1:
      /* fork failed, clean things up. */
      s_cb_pid = 0;
      close(pipefds[0]);
      close(pipefds[1]);
      errno = ECHILD;
      return -1;
    case 0: /* child */
      /* Close pipe ending we do not use */
      close(pipefds[1]);

      /* Loop */
      signal(SIGINT, child_handle_sigint);
      own_x11_clipboard(pipefds[0]);

      /* Cleanup and exit */
      close(pipefds[0]);
      exit(0);
      return 0; /* not reached */
    default: /* parent */
      /* Close pipe ending we do not use */
      close(pipefds[0]);

      /* Register our friendly process killer exactly once. */
      if (!has_registered_exit_handler) {
	atexit(finish_subprocess_on_exit);
	has_registered_exit_handler = 1;
      }

      /* Recurse so we reach the other if branch */
      return tiny_clipnwrite(text, len);
    }
  }
  else { /* Existing clipboard handler process */
    if (waitpid(s_cb_pid, NULL, WNOHANG) != 0) {
      /* Child process died, recreate it */
      s_cb_pid = 0;
      close(pipefds[1]);

      return tiny_clipnwrite(text, len);
    }
    else { /* Process is still alive */
      /* Write length and text into the child process */
      write(pipefds[1], &len, sizeof(int)); /* yes, raw byte value! */
      write(pipefds[1], text, len);

      tries = 0; /* Reset process death counter */
      return 0;
    }
  }
#elif defined(_WIN32)
  HWND window = NULL;
  HGLOBAL global_handle;
  LPWSTR cliptext_utf16;
  int charcount;
  MSG message;
  BOOL result;
  static bool class_registered = false;

  /* Register our window class only on the first call */
  if (!class_registered) {
    WNDCLASSEXW windowclass;
    memset(&windowclass, '\0', sizeof(WNDCLASSEX));

    windowclass.cbSize = sizeof(WNDCLASSEX);
    windowclass.lpfnWndProc = (WNDPROC) Win32MessageHandler;
    windowclass.lpszClassName = L"TinyClipboardWindowClass";

    if (RegisterClassExW(&windowclass)) {
      class_registered = true;
    }
    else {
      errno = ENOTSUP;
      return -1;
    }
  }

  window = CreateWindowW(L"TinyClipboardWindowClass", L"TinyClipboard window",
			0, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, NULL, NULL,
			NULL, NULL);
  if (!window) {
    errno = ENOTSUP;
    return -1;
  }

  UpdateWindow(window);

  /* Acquire clipboard ownership. */
  if (!OpenClipboard(window)) {
    DestroyWindow(window);
    errno = EAGAIN;
    return -1;
  }
  EmptyClipboard();

  /* Determine size of this in UTF-16 */
  charcount = MultiByteToWideChar(CP_UTF8, 0, text, (int) len, NULL, 0);
  if (!charcount) {
    /* User passed invalid UTF-8 */
    CloseClipboard();
    DestroyWindow(window);
    errno = EINVAL;
    return -1;
  }

  /* Allocate system-global memory chunk. */
  global_handle = GlobalAlloc(GMEM_MOVEABLE, charcount * sizeof(WCHAR) + 2); /* Terminating UTF-16 NUL-NUL */
  cliptext_utf16 = GlobalLock(global_handle);

  /* Write string as UTF-16 into the clipboard. */
  memset(cliptext_utf16, '\0', charcount * sizeof(WCHAR) + 2);
  charcount = MultiByteToWideChar(CP_UTF8, 0, text, (int) len, cliptext_utf16, charcount);
  if (!charcount) {
    GlobalFree(global_handle); /* probably includes GlobalUnlock() */
    CloseClipboard();
    DestroyWindow(window);
    errno = EINVAL;
    return -1;
  }

  /* Hand the pointer over to the OS, which will take care
   * of freeing it and detaching it from this process so
   * that is stays around even after the application has
   * closed. */
  if(!SetClipboardData(CF_UNICODETEXT, global_handle)) {
    /* This should not happen. */
    GlobalFree(global_handle);
    CloseClipboard();
    DestroyWindow(window);
    errno = ECANCELED;
    return -1;
  }

  /* Cleanup */
  GlobalUnlock(global_handle);
  CloseClipboard();

  /* Tell the window to properly exit */
  PostMessageW(window, WM_CLOSE, 0, 0);

  /* Main loop */
  while ((result = GetMessageW(&message, NULL, 0, 0)) != 0) {
    if (result == -1) {
      /* Very interesting. GetMessage() docs indicate that a BOOL
       * type can actually be one of three values: -1, 0, nonzero.
       * What was exactly the purpose of a BOOL then? */
      DestroyWindow(window);
      errno = ECANCELED;
      return -1;
    }

    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return 0;
#else
#error Dont know how to access the clipboard on this system!
#endif
}

int tiny_clipwrite(const char* text)
{
  return tiny_clipnwrite(text, strlen(text));
}

const char* tiny_clipversion()
{
  static char buf[512];
  int year = TINYCLIPBOARD_VERSION / 10000L;
  int month = (TINYCLIPBOARD_VERSION - year * 10000L) / 100L;
  int day = (TINYCLIPBOARD_VERSION - year * 10000L) - month * 100L;

  if (day) {
    sprintf(buf,
	    "tinyclipboard %d.%02d.%d%s, copyright © %d Marvin Gülker. This is free software distributed under the terms of the GNU GPLv3 license.",
	    year % 100,
	    month,
	    day,
	    TINYCLIPBOARD_VERSION_POSTFIX,
	    year);
  }
  else {
    sprintf(buf,
	    "tinyclipboard %d.%02d%s, copyright © %d Marvin Gülker. This is free software distributed under the terms of the GNU GPLv3 license.",
	    year % 100,
	    month,
	    TINYCLIPBOARD_VERSION_POSTFIX,
	    year);
  }

  return buf;
}


/****************************************
 * Private helpers for X11
 ***************************************/

#ifdef __unix__
void finish_subprocess_on_exit(void)
{
  if (s_cb_pid) {
    /* Initiate civilised shutdown in clipboard owner process. */
    kill(s_cb_pid, SIGINT);
    waitpid(s_cb_pid, NULL, 0);
  }

  /* FIXME: Does not close parent process' pipe ending. This
   * should be done here. But hey, we are exiting anyway, and
   * this way I don't need an extra file-static variable... */
}

/* Initiates cililised shutdown by closing the clipboard owner window
 * (which generates a DestroyNotify event; see XDestroyWindowEvent(3)).
 * If that fails for whatever reason, calls exit() directly.
 */
void child_handle_sigint(int signum)
{
  Display* p_display = NULL;

  /* Exit immediately if there is no window to clean up
   * (which should never be the case). */
  if (s_clipowner_window == None)
    exit(2);

  if ((p_display = XOpenDisplay(NULL))) { /* Single = intended */
    XDestroyWindow(p_display, s_clipowner_window);
    XCloseDisplay(p_display);
    /* Main code will exit() instead of us */
  }
  else {
    /* No X11 connection, but we are on shutdown. Emergency shutdown. */
    exit(2);
  }
}

void get_clipboard_text(int filedes, char** p_str, int* p_len)
{
  ssize_t ret = 0;

  /* Attempt to read one ulong from the pipe. Note it has O_NONBLOCK set! */
  ret = read(filedes, p_len, sizeof(int)); /* raw byte read */
  if (ret < 0 && errno == EAGAIN)
    return; /* Reading would block, i.e. no new clipboard data available */
  else if (ret < 0 || (ret != sizeof(int)))
    goto fail; /* Transfer protocol violated */

  /* Allocate space for the number of bytes advertised. */
  *p_str = realloc(*p_str, *p_len + 1);
  memset(*p_str, '\0', *p_len + 1); /* For extra safety */

  /* Read advertised bytes from the pipe. */
  ret = read(filedes, *p_str, *p_len);
  if (ret <= 0 || (ret != *p_len))
    goto fail; /* Transfer protocol violated */

  return;

  fail:
    fprintf(stderr, "**tinyclipboard: Parent process violated transfer protocol, discarding. This is likely a bug.\n");
    free(*p_str);
    *p_str = NULL;
    *p_len = 0;
}

void own_x11_clipboard(int filedes)
{
  Display* p_display = NULL;
  char* cliptext = NULL;
  int textlen = 0;
  int terminate = 0;
  Atom clipboard;

  p_display = XOpenDisplay(NULL);
  if (!p_display) {
    fprintf(stderr, "**tinyclipboard: Failed to open X11 display connection.\n");
    exit(1);
    return;
  }

  s_clipowner_window = XCreateSimpleWindow(p_display, XDefaultRootWindow(p_display), 0, 0, 1, 1, 0, 0, 0);

  /* Tell X.org we want to receive the DestroyNotify event; see
   * - https://tronche.com/gui/x/xlib/events/window-state-change/destroy.html
   * - http://www.lemoda.net/c/xlib-resize/ */
  XSelectInput(p_display, s_clipowner_window, StructureNotifyMask);

  /* Own CLIPBOARD (= win32-like clipboard) */
  clipboard = XInternAtom(p_display, "CLIPBOARD", False);
  XSetSelectionOwner(p_display, clipboard, s_clipowner_window, CurrentTime);

  if (XGetSelectionOwner(p_display, clipboard) != s_clipowner_window) {
    fprintf(stderr, "**tinyclipboard: Failed to obtain ownership of X11 CLIPBOARD clipboard.\n");
    exit(1);
  }

  /* Main loop */
  while (!terminate) {
    XEvent evt;
    XNextEvent(p_display, &evt); /* blocks if no events available */

    switch(evt.type) {
    case SelectionRequest:
      get_clipboard_text(filedes, &cliptext, &textlen);
      handle_x11_selectionrequest(p_display, evt, cliptext, textlen);
      break;
    case SelectionClear: /* We are no longer CLIPBOARD owner */
      XDestroyWindow(p_display, s_clipowner_window);
      break;
    case DestroyNotify: /* X11 killed the window */
      s_clipowner_window = None;
      terminate = 1;
      break;
    default:
      break; /* Ignore unsupported event */
    }
  }

  free(cliptext);
}

void handle_x11_selectionrequest(Display* p_display, XEvent evt, const char* cliptext, int textlen)
{
  Atom utf8 = XInternAtom(p_display, "UTF8_STRING", True); /* Resource for UTF-8 text */
  Atom targets = XInternAtom(p_display, "TARGETS", True);  /* Query for available types */
  Atom save_targets = XInternAtom(p_display, "SAVE_TARGETS", False); /* No-op marker atom for clipmanagers */
  XEvent response;

  response.xselection.type	= SelectionNotify;
  response.xselection.display	= evt.xselectionrequest.display;
  response.xselection.requestor = evt.xselectionrequest.requestor;
  response.xselection.selection = evt.xselectionrequest.selection;
  response.xselection.target	= evt.xselectionrequest.target;
  response.xselection.time	= evt.xselectionrequest.time;

  if (textlen > 0 && (evt.xselectionrequest.target == targets)) { /* Request for supported clipboard targets (we only supported text) */
    Atom supported_targets[] = {utf8, XA_STRING, save_targets};
    response.xselection.property = evt.xselectionrequest.property;
    XChangeProperty(p_display,
		    evt.xselectionrequest.requestor,
		    evt.xselectionrequest.property,
		    evt.xselectionrequest.target, /* 'targets' atom */
		    8,
		    PropModeReplace,
		    (unsigned char*)(&supported_targets),
		    sizeof(supported_targets));
  }
  else if (textlen > 0 && evt.xselectionrequest.target == save_targets) {
    /* This is a No-op target as per freedesktop.org spec. */
    response.xselection.property = None;
  }
  else if (textlen > 0 && evt.xselectionrequest.target == utf8) { /* Request for real text content, UTF-8 requested */
    response.xselection.property = evt.xselectionrequest.property;
    XChangeProperty(p_display,
		    evt.xselectionrequest.requestor,
		    evt.xselectionrequest.property,
		    evt.xselectionrequest.target, /* 'utf8' atom */
		    8,
		    PropModeReplace,
		    (unsigned char*)cliptext,
		    textlen);
  }
  else if (textlen > 0 && evt.xselectionrequest.target == XA_STRING) { /* Request for locale-dependant encoded text -- UNTESTED with non-utf8-locales*/
    const char* locale_encoding = nl_langinfo(CODESET);
    iconv_t converter = iconv_open(locale_encoding, "UTF-8");
    char* source_string = (char*) cliptext; /* remove 'const' -- we do not change it, but the function prototype is broken */
    size_t bytes_allocated = 32;
    char* target_string = (char*) calloc(bytes_allocated, 1);
    char* outbuf = target_string;
    size_t inbytesleft = textlen;
    size_t outbytesleft = bytes_allocated;

    /* Convert from UTF-8 to locale's encoding. */
    while (inbytesleft > 0) {
      if (iconv(converter, &source_string, &inbytesleft, &outbuf, &bytes_allocated) == ((size_t)-1)) {
	if (errno == E2BIG) {
	  target_string = (char*) realloc(target_string, bytes_allocated + 32);
	  memset(target_string + bytes_allocated, '\0', 32);

	  outbuf = target_string + bytes_allocated - outbytesleft;

	  bytes_allocated += 32;
	  outbytesleft += 32;
	}
	else {
	  perror("**tinyclipboard: Failed to convert string into locale encoding");
	  response.xselection.property = None;
	  XSendEvent(p_display, evt.xselectionrequest.requestor, 0, 0, &response);
	  iconv_close(converter);
	  free(target_string);
	  return;
	}
      }
    }

    /* The following is the same as with utf8 above, just with another charset. */
    response.xselection.property = evt.xselectionrequest.property;
    XChangeProperty(p_display,
		    evt.xselectionrequest.requestor,
		    evt.xselectionrequest.property,
		    evt.xselectionrequest.target, /* XA_STRING atom */
		    8,
		    PropModeReplace,
		    (unsigned char*)target_string,
		    bytes_allocated - outbytesleft);

    iconv_close(converter);
    free(target_string);
  }
  else { /* Unsupported target requested or textlen <= 0 (i.e. empty clipboard) */
    response.xselection.property = None;
  }

  XSendEvent(p_display, evt.xselectionrequest.requestor, 0, 0, &response);
}

bool write_to_clipboard_manager(const char* cliptext, int len)
{
  Display* p_display = NULL;
  Window window = None;
  Atom clipboard_manager;
  Atom clipboard;
  Atom save_targets;
  bool terminate = false;
  bool result = false;

  p_display = XOpenDisplay(NULL);
  if (!p_display)
    return false;

  clipboard = XInternAtom(p_display, "CLIPBOARD", False);
  clipboard_manager = XInternAtom(p_display, "CLIPBOARD_MANAGER", False);
  save_targets = XInternAtom(p_display, "SAVE_TARGETS", False);

  /* Check if a clipboard manager is available. If not, we cannot write
   * to it. */
  if (XGetSelectionOwner(p_display, clipboard_manager) == None) {
    XCloseDisplay(p_display);
    return false;
  }

  /* Own CLIPBOARD */
  window = XCreateSimpleWindow(p_display, XDefaultRootWindow(p_display), 0, 0, 1, 1, 0, 0, 0);
  XSetSelectionOwner(p_display, clipboard, window, CurrentTime);

  /* Notify CLIPBOARD_MANAGER we want it to take over. */
  XConvertSelection(p_display, clipboard_manager, save_targets, None, window, CurrentTime);

  /* Main loop */
  while (!terminate) {
    XEvent evt;
    XNextEvent(p_display, &evt); /* blocks if no events available */

    switch(evt.type) {
    case SelectionRequest:
      /* Take advantage of existing handler function. */
      handle_x11_selectionrequest(p_display, evt, cliptext, len);
      break;
    case SelectionClear: /* We are no longer owner; a 3rd party took over. */
      terminate = true;
      result = true; /* 3rd party superseded us; do not write to clipboard anymore */
      break;
    case SelectionNotify:
      /* If Clipboard manager is done. */
      if (evt.xselection.target == save_targets) {
	terminate = true;
	result = evt.xselection.property == None; /* Check wheather clipboard managers failed. */
      }
      break;
    default:
      break; /* Ignore unknown events */
    }
  }

  XDestroyWindow(p_display, window);
  XCloseDisplay(p_display);
  return result;
}

#endif

/****************************************
 * Private helpers for Win32
 ***************************************/

#ifdef _WIN32
LRESULT Win32MessageHandler(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch(message) {
  case WM_NCCREATE:
    return TRUE;
  case WM_CREATE:
    return 0;
  case WM_CLOSE:
    DestroyWindow(window);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
  default:
    /* Do not process event by default */
    return -1;
  }
}
#endif
