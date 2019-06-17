/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Implements communication through a socket or any file handle.
 */

#include "vim.h"

#if defined(FEAT_JOB_CHANNEL) || defined(PROTO)

/* TRUE when netbeans is running with a GUI. */
#ifdef FEAT_GUI
# define CH_HAS_GUI (gui.in_use || gui.starting)
#endif

/* Note: when making changes here also adjust configure.ac. */
#ifdef MSWIN
/* WinSock API is separated from C API, thus we can't use read(), write(),
 * errno... */
# define SOCK_ERRNO errno = WSAGetLastError()
# undef ECONNREFUSED
# define ECONNREFUSED WSAECONNREFUSED
# undef EWOULDBLOCK
# define EWOULDBLOCK WSAEWOULDBLOCK
# undef EINPROGRESS
# define EINPROGRESS WSAEINPROGRESS
# ifdef EINTR
#  undef EINTR
# endif
# define EINTR WSAEINTR
# define sock_write(sd, buf, len) send((SOCKET)sd, buf, len, 0)
# define sock_read(sd, buf, len) recv((SOCKET)sd, buf, len, 0)
# define sock_close(sd) closesocket((SOCKET)sd)
#else
# include <netdb.h>
# include <netinet/in.h>

# include <sys/socket.h>
# ifdef HAVE_LIBGEN_H
#  include <libgen.h>
# endif
# define SOCK_ERRNO
# define sock_write(sd, buf, len) write(sd, buf, len)
# define sock_read(sd, buf, len) read(sd, buf, len)
# define sock_close(sd) close(sd)
# define fd_read(fd, buf, len) read(fd, buf, len)
# define fd_write(sd, buf, len) write(sd, buf, len)
# define fd_close(sd) close(sd)
#endif

static void channel_read(channel_T *channel, ch_part_T part, char *func);

/* Whether a redraw is needed for appending a line to a buffer. */
static int channel_need_redraw = FALSE;

/* Whether we are inside channel_parse_messages() or another situation where it
 * is safe to invoke callbacks. */
static int safe_to_invoke_callback = 0;

static char *part_names[] = {"sock", "out", "err", "in"};

#ifdef MSWIN
    static int
fd_read(sock_T fd, char *buf, size_t len)
{
    HANDLE h = (HANDLE)fd;
    DWORD nread;

    if (!ReadFile(h, buf, (DWORD)len, &nread, NULL))
	return -1;
    return (int)nread;
}

    static int
fd_write(sock_T fd, char *buf, size_t len)
{
    size_t	todo = len;
    HANDLE	h = (HANDLE)fd;
    DWORD	nwrite, size, done = 0;
    OVERLAPPED	ov;

    while (todo > 0)
    {
	if (todo > MAX_NAMED_PIPE_SIZE)
	    size = MAX_NAMED_PIPE_SIZE;
	else
	    size = (DWORD)todo;
	// If the pipe overflows while the job does not read the data,
	// WriteFile() will block forever. This abandons the write.
	memset(&ov, 0, sizeof(ov));
	nwrite = 0;
	if (!WriteFile(h, buf + done, size, &nwrite, &ov))
	{
	    DWORD err = GetLastError();

	    if (err != ERROR_IO_PENDING)
		return -1;
	    if (!GetOverlappedResult(h, &ov, &nwrite, FALSE))
		return -1;
	    FlushFileBuffers(h);
	}
	else if (nwrite == 0)
	    // WriteFile() returns TRUE but did not write anything. This causes
	    // a hang, so bail out.
	    break;
	todo -= nwrite;
	done += nwrite;
    }
    return (int)done;
}

    static void
fd_close(sock_T fd)
{
    HANDLE h = (HANDLE)fd;

    CloseHandle(h);
}
#endif

/* Log file opened with ch_logfile(). */
static FILE *log_fd = NULL;
#ifdef FEAT_RELTIME
static proftime_T log_start;
#endif

    void
ch_logfile(char_u *fname, char_u *opt)
{
    FILE   *file = NULL;

    if (log_fd != NULL)
	fclose(log_fd);

    if (*fname != NUL)
    {
	file = fopen((char *)fname, *opt == 'w' ? "w" : "a");
	if (file == NULL)
	{
	    semsg(_(e_notopen), fname);
	    return;
	}
    }
    log_fd = file;

    if (log_fd != NULL)
    {
	fprintf(log_fd, "==== start log session ====\n");
#ifdef FEAT_RELTIME
	profile_start(&log_start);
#endif
    }
}

    int
ch_log_active(void)
{
    return log_fd != NULL;
}

    static void
ch_log_lead(const char *what, channel_T *ch, ch_part_T part)
{
    if (log_fd != NULL)
    {
#ifdef FEAT_RELTIME
	proftime_T log_now;

	profile_start(&log_now);
	profile_sub(&log_now, &log_start);
	fprintf(log_fd, "%s ", profile_msg(&log_now));
#endif
	if (ch != NULL)
	{
	    if (part < PART_COUNT)
		fprintf(log_fd, "%son %d(%s): ",
					   what, ch->ch_id, part_names[part]);
	    else
		fprintf(log_fd, "%son %d: ", what, ch->ch_id);
	}
	else
	    fprintf(log_fd, "%s: ", what);
    }
}

static int did_log_msg = TRUE;

#ifndef PROTO  // prototype is in proto.h
    void
ch_log(channel_T *ch, const char *fmt, ...)
{
    if (log_fd != NULL)
    {
	va_list ap;

	ch_log_lead("", ch, PART_COUNT);
	va_start(ap, fmt);
	vfprintf(log_fd, fmt, ap);
	va_end(ap);
	fputc('\n', log_fd);
	fflush(log_fd);
	did_log_msg = TRUE;
    }
}
#endif

    static void
ch_error(channel_T *ch, const char *fmt, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 2, 3)))
#endif
    ;

    static void
ch_error(channel_T *ch, const char *fmt, ...)
{
    if (log_fd != NULL)
    {
	va_list ap;

	ch_log_lead("ERR ", ch, PART_COUNT);
	va_start(ap, fmt);
	vfprintf(log_fd, fmt, ap);
	va_end(ap);
	fputc('\n', log_fd);
	fflush(log_fd);
	did_log_msg = TRUE;
    }
}

#ifdef MSWIN
# undef PERROR
# define PERROR(msg) (void)semsg("%s: %s", msg, strerror_win32(errno))

    static char *
strerror_win32(int eno)
{
    static LPVOID msgbuf = NULL;
    char_u *ptr;

    if (msgbuf)
    {
	LocalFree(msgbuf);
	msgbuf = NULL;
    }
    FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM |
	FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL,
	eno,
	MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
	(LPTSTR) &msgbuf,
	0,
	NULL);
    if (msgbuf != NULL)
	/* chomp \r or \n */
	for (ptr = (char_u *)msgbuf; *ptr; ptr++)
	    switch (*ptr)
	    {
		case '\r':
		    STRMOVE(ptr, ptr + 1);
		    ptr--;
		    break;
		case '\n':
		    if (*(ptr + 1) == '\0')
			*ptr = '\0';
		    else
			*ptr = ' ';
		    break;
	    }
    return msgbuf;
}
#endif

/*
 * The list of all allocated channels.
 */
static channel_T *first_channel = NULL;
static int next_ch_id = 0;

/*
 * Allocate a new channel.  The refcount is set to 1.
 * The channel isn't actually used until it is opened.
 * Returns NULL if out of memory.
 */
    channel_T *
add_channel(void)
{
    ch_part_T	part;
    channel_T	*channel = ALLOC_CLEAR_ONE(channel_T);

    if (channel == NULL)
	return NULL;

    channel->ch_id = next_ch_id++;
    ch_log(channel, "Created channel");

    for (part = PART_SOCK; part < PART_COUNT; ++part)
    {
	channel->ch_part[part].ch_fd = INVALID_FD;
#ifdef FEAT_GUI_X11
	channel->ch_part[part].ch_inputHandler = (XtInputId)NULL;
#endif
#ifdef FEAT_GUI_GTK
	channel->ch_part[part].ch_inputHandler = 0;
#endif
	channel->ch_part[part].ch_timeout = 2000;
    }

    if (first_channel != NULL)
    {
	first_channel->ch_prev = channel;
	channel->ch_next = first_channel;
    }
    first_channel = channel;

    channel->ch_refcount = 1;
    return channel;
}

    int
has_any_channel(void)
{
    return first_channel != NULL;
}

/*
 * Called when the refcount of a channel is zero.
 * Return TRUE if "channel" has a callback and the associated job wasn't
 * killed.
 */
    static int
channel_still_useful(channel_T *channel)
{
    int has_sock_msg;
    int	has_out_msg;
    int	has_err_msg;

    /* If the job was killed the channel is not expected to work anymore. */
    if (channel->ch_job_killed && channel->ch_job == NULL)
	return FALSE;

    /* If there is a close callback it may still need to be invoked. */
    if (channel->ch_close_cb.cb_name != NULL)
	return TRUE;

    /* If reading from or a buffer it's still useful. */
    if (channel->ch_part[PART_IN].ch_bufref.br_buf != NULL)
	return TRUE;

    /* If there is no callback then nobody can get readahead.  If the fd is
     * closed and there is no readahead then the callback won't be called. */
    has_sock_msg = channel->ch_part[PART_SOCK].ch_fd != INVALID_FD
		|| channel->ch_part[PART_SOCK].ch_head.rq_next != NULL
		|| channel->ch_part[PART_SOCK].ch_json_head.jq_next != NULL;
    has_out_msg = channel->ch_part[PART_OUT].ch_fd != INVALID_FD
		  || channel->ch_part[PART_OUT].ch_head.rq_next != NULL
		  || channel->ch_part[PART_OUT].ch_json_head.jq_next != NULL;
    has_err_msg = channel->ch_part[PART_ERR].ch_fd != INVALID_FD
		  || channel->ch_part[PART_ERR].ch_head.rq_next != NULL
		  || channel->ch_part[PART_ERR].ch_json_head.jq_next != NULL;
    return (channel->ch_callback.cb_name != NULL && (has_sock_msg
		|| has_out_msg || has_err_msg))
	    || ((channel->ch_part[PART_OUT].ch_callback.cb_name != NULL
		       || channel->ch_part[PART_OUT].ch_bufref.br_buf != NULL)
		    && has_out_msg)
	    || ((channel->ch_part[PART_ERR].ch_callback.cb_name != NULL
		       || channel->ch_part[PART_ERR].ch_bufref.br_buf != NULL)
		    && has_err_msg);
}

/*
 * Return TRUE if "channel" is closeable (i.e. all readable fds are closed).
 */
    static int
channel_can_close(channel_T *channel)
{
    return channel->ch_to_be_closed == 0;
}

/*
 * Close a channel and free all its resources.
 */
    static void
channel_free_contents(channel_T *channel)
{
    channel_close(channel, TRUE);
    channel_clear(channel);
    ch_log(channel, "Freeing channel");
}

    static void
channel_free_channel(channel_T *channel)
{
    if (channel->ch_next != NULL)
	channel->ch_next->ch_prev = channel->ch_prev;
    if (channel->ch_prev == NULL)
	first_channel = channel->ch_next;
    else
	channel->ch_prev->ch_next = channel->ch_next;
    vim_free(channel);
}

    static void
channel_free(channel_T *channel)
{
    if (!in_free_unref_items)
    {
	if (safe_to_invoke_callback == 0)
	    channel->ch_to_be_freed = TRUE;
	else
	{
	    channel_free_contents(channel);
	    channel_free_channel(channel);
	}
    }
}

/*
 * Close a channel and free all its resources if there is no further action
 * possible, there is no callback to be invoked or the associated job was
 * killed.
 * Return TRUE if the channel was freed.
 */
    static int
channel_may_free(channel_T *channel)
{
    if (!channel_still_useful(channel))
    {
	channel_free(channel);
	return TRUE;
    }
    return FALSE;
}

/*
 * Decrement the reference count on "channel" and maybe free it when it goes
 * down to zero.  Don't free it if there is a pending action.
 * Returns TRUE when the channel is no longer referenced.
 */
    int
channel_unref(channel_T *channel)
{
    if (channel != NULL && --channel->ch_refcount <= 0)
	return channel_may_free(channel);
    return FALSE;
}

    int
free_unused_channels_contents(int copyID, int mask)
{
    int		did_free = FALSE;
    channel_T	*ch;

    /* This is invoked from the garbage collector, which only runs at a safe
     * point. */
    ++safe_to_invoke_callback;

    for (ch = first_channel; ch != NULL; ch = ch->ch_next)
	if (!channel_still_useful(ch)
				 && (ch->ch_copyID & mask) != (copyID & mask))
	{
	    /* Free the channel and ordinary items it contains, but don't
	     * recurse into Lists, Dictionaries etc. */
	    channel_free_contents(ch);
	    did_free = TRUE;
	}

    --safe_to_invoke_callback;
    return did_free;
}

    void
free_unused_channels(int copyID, int mask)
{
    channel_T	*ch;
    channel_T	*ch_next;

    for (ch = first_channel; ch != NULL; ch = ch_next)
    {
	ch_next = ch->ch_next;
	if (!channel_still_useful(ch)
				 && (ch->ch_copyID & mask) != (copyID & mask))
	{
	    /* Free the channel struct itself. */
	    channel_free_channel(ch);
	}
    }
}

#if defined(FEAT_GUI) || defined(PROTO)

#if defined(FEAT_GUI_X11) || defined(FEAT_GUI_GTK)
    static void
channel_read_fd(int fd)
{
    channel_T	*channel;
    ch_part_T	part;

    channel = channel_fd2channel(fd, &part);
    if (channel == NULL)
	ch_error(NULL, "Channel for fd %d not found", fd);
    else
	channel_read(channel, part, "channel_read_fd");
}
#endif

/*
 * Read a command from netbeans.
 */
#ifdef FEAT_GUI_X11
    static void
messageFromServerX11(XtPointer clientData,
		  int *unused1 UNUSED,
		  XtInputId *unused2 UNUSED)
{
    channel_read_fd((int)(long)clientData);
}
#endif

#ifdef FEAT_GUI_GTK
# if GTK_CHECK_VERSION(3,0,0)
    static gboolean
messageFromServerGtk3(GIOChannel *unused1 UNUSED,
		  GIOCondition unused2 UNUSED,
		  gpointer clientData)
{
    channel_read_fd(GPOINTER_TO_INT(clientData));
    return TRUE; /* Return FALSE instead in case the event source is to
		  * be removed after this function returns. */
}
# else
    static void
messageFromServerGtk2(gpointer clientData,
		  gint unused1 UNUSED,
		  GdkInputCondition unused2 UNUSED)
{
    channel_read_fd((int)(long)clientData);
}
# endif
#endif

    static void
channel_gui_register_one(channel_T *channel, ch_part_T part)
{
    if (!CH_HAS_GUI)
	return;

    /* gets stuck in handling events for a not connected channel */
    if (channel->ch_keep_open)
	return;

# ifdef FEAT_GUI_X11
    /* Tell notifier we are interested in being called when there is input on
     * the editor connection socket. */
    if (channel->ch_part[part].ch_inputHandler == (XtInputId)NULL)
    {
	ch_log(channel, "Registering part %s with fd %d",
		part_names[part], channel->ch_part[part].ch_fd);

	channel->ch_part[part].ch_inputHandler = XtAppAddInput(
		(XtAppContext)app_context,
		channel->ch_part[part].ch_fd,
		(XtPointer)(XtInputReadMask + XtInputExceptMask),
		messageFromServerX11,
		(XtPointer)(long)channel->ch_part[part].ch_fd);
    }
# else
#  ifdef FEAT_GUI_GTK
    /* Tell gdk we are interested in being called when there is input on the
     * editor connection socket. */
    if (channel->ch_part[part].ch_inputHandler == 0)
    {
	ch_log(channel, "Registering part %s with fd %d",
		part_names[part], channel->ch_part[part].ch_fd);
#   if GTK_CHECK_VERSION(3,0,0)
	GIOChannel *chnnl = g_io_channel_unix_new(
		(gint)channel->ch_part[part].ch_fd);

	channel->ch_part[part].ch_inputHandler = g_io_add_watch(
		chnnl,
		G_IO_IN|G_IO_HUP|G_IO_ERR|G_IO_PRI,
		messageFromServerGtk3,
		GINT_TO_POINTER(channel->ch_part[part].ch_fd));

	g_io_channel_unref(chnnl);
#   else
	channel->ch_part[part].ch_inputHandler = gdk_input_add(
		(gint)channel->ch_part[part].ch_fd,
		(GdkInputCondition)
			     ((int)GDK_INPUT_READ + (int)GDK_INPUT_EXCEPTION),
		messageFromServerGtk2,
		(gpointer)(long)channel->ch_part[part].ch_fd);
#   endif
    }
#  endif
# endif
}

    static void
channel_gui_register(channel_T *channel)
{
    if (channel->CH_SOCK_FD != INVALID_FD)
	channel_gui_register_one(channel, PART_SOCK);
    if (channel->CH_OUT_FD != INVALID_FD
	    && channel->CH_OUT_FD != channel->CH_SOCK_FD)
	channel_gui_register_one(channel, PART_OUT);
    if (channel->CH_ERR_FD != INVALID_FD
	    && channel->CH_ERR_FD != channel->CH_SOCK_FD
	    && channel->CH_ERR_FD != channel->CH_OUT_FD)
	channel_gui_register_one(channel, PART_ERR);
}

/*
 * Register any of our file descriptors with the GUI event handling system.
 * Called when the GUI has started.
 */
    void
channel_gui_register_all(void)
{
    channel_T *channel;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	channel_gui_register(channel);
}

    static void
channel_gui_unregister_one(channel_T *channel, ch_part_T part)
{
# ifdef FEAT_GUI_X11
    if (channel->ch_part[part].ch_inputHandler != (XtInputId)NULL)
    {
	ch_log(channel, "Unregistering part %s", part_names[part]);
	XtRemoveInput(channel->ch_part[part].ch_inputHandler);
	channel->ch_part[part].ch_inputHandler = (XtInputId)NULL;
    }
# else
#  ifdef FEAT_GUI_GTK
    if (channel->ch_part[part].ch_inputHandler != 0)
    {
	ch_log(channel, "Unregistering part %s", part_names[part]);
#   if GTK_CHECK_VERSION(3,0,0)
	g_source_remove(channel->ch_part[part].ch_inputHandler);
#   else
	gdk_input_remove(channel->ch_part[part].ch_inputHandler);
#   endif
	channel->ch_part[part].ch_inputHandler = 0;
    }
#  endif
# endif
}

    static void
channel_gui_unregister(channel_T *channel)
{
    ch_part_T	part;

    for (part = PART_SOCK; part < PART_IN; ++part)
	channel_gui_unregister_one(channel, part);
}

#endif

static char *e_cannot_connect = N_("E902: Cannot connect to port");

/*
 * Open a socket channel to "hostname":"port".
 * "waittime" is the time in msec to wait for the connection.
 * When negative wait forever.
 * Returns the channel for success.
 * Returns NULL for failure.
 */
    channel_T *
channel_open(
	char *hostname,
	int port_in,
	int waittime,
	void (*nb_close_cb)(void))
{
    int			sd = -1;
    struct sockaddr_in	server;
    struct hostent	*host;
#ifdef MSWIN
    u_short		port = port_in;
    u_long		val = 1;
#else
    int			port = port_in;
#endif
    channel_T		*channel;
    int			ret;

#ifdef MSWIN
    channel_init_winsock();
#endif

    channel = add_channel();
    if (channel == NULL)
    {
	ch_error(NULL, "Cannot allocate channel.");
	return NULL;
    }

    /* Get the server internet address and put into addr structure */
    /* fill in the socket address structure and connect to server */
    vim_memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if ((host = gethostbyname(hostname)) == NULL)
    {
	ch_error(channel, "in gethostbyname() in channel_open()");
	PERROR(_("E901: gethostbyname() in channel_open()"));
	channel_free(channel);
	return NULL;
    }
    {
	char		*p;

	/* When using host->h_addr_list[0] directly ubsan warns for it to not
	 * be aligned.  First copy the pointer to avoid that. */
	memcpy(&p, &host->h_addr_list[0], sizeof(p));
	memcpy((char *)&server.sin_addr, p, host->h_length);
    }

    /* On Mac and Solaris a zero timeout almost never works.  At least wait
     * one millisecond. Let's do it for all systems, because we don't know why
     * this is needed. */
    if (waittime == 0)
	waittime = 1;

    /*
     * For Unix we need to call connect() again after connect() failed.
     * On Win32 one time is sufficient.
     */
    while (TRUE)
    {
	long	elapsed_msec = 0;
	int	waitnow;

	if (sd >= 0)
	    sock_close(sd);
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
	{
	    ch_error(channel, "in socket() in channel_open().");
	    PERROR(_("E898: socket() in channel_open()"));
	    channel_free(channel);
	    return NULL;
	}

	if (waittime >= 0)
	{
	    /* Make connect() non-blocking. */
	    if (
#ifdef MSWIN
		ioctlsocket(sd, FIONBIO, &val) < 0
#else
		fcntl(sd, F_SETFL, O_NONBLOCK) < 0
#endif
	       )
	    {
		SOCK_ERRNO;
		ch_error(channel,
			 "channel_open: Connect failed with errno %d", errno);
		sock_close(sd);
		channel_free(channel);
		return NULL;
	    }
	}

	/* Try connecting to the server. */
	ch_log(channel, "Connecting to %s port %d", hostname, port);
	ret = connect(sd, (struct sockaddr *)&server, sizeof(server));

	if (ret == 0)
	    /* The connection could be established. */
	    break;

	SOCK_ERRNO;
	if (waittime < 0 || (errno != EWOULDBLOCK
		&& errno != ECONNREFUSED
#ifdef EINPROGRESS
		&& errno != EINPROGRESS
#endif
		))
	{
	    ch_error(channel,
			 "channel_open: Connect failed with errno %d", errno);
	    PERROR(_(e_cannot_connect));
	    sock_close(sd);
	    channel_free(channel);
	    return NULL;
	}

	/* Limit the waittime to 50 msec.  If it doesn't work within this
	 * time we close the socket and try creating it again. */
	waitnow = waittime > 50 ? 50 : waittime;

	/* If connect() didn't finish then try using select() to wait for the
	 * connection to be made. For Win32 always use select() to wait. */
#ifndef MSWIN
	if (errno != ECONNREFUSED)
#endif
	{
	    struct timeval	tv;
	    fd_set		rfds;
	    fd_set		wfds;
#ifndef MSWIN
	    int			so_error = 0;
	    socklen_t		so_error_len = sizeof(so_error);
	    struct timeval	start_tv;
	    struct timeval	end_tv;
#endif
	    FD_ZERO(&rfds);
	    FD_SET(sd, &rfds);
	    FD_ZERO(&wfds);
	    FD_SET(sd, &wfds);

	    tv.tv_sec = waitnow / 1000;
	    tv.tv_usec = (waitnow % 1000) * 1000;
#ifndef MSWIN
	    gettimeofday(&start_tv, NULL);
#endif
	    ch_log(channel,
		    "Waiting for connection (waiting %d msec)...", waitnow);
	    ret = select((int)sd + 1, &rfds, &wfds, NULL, &tv);

	    if (ret < 0)
	    {
		SOCK_ERRNO;
		ch_error(channel,
			"channel_open: Connect failed with errno %d", errno);
		PERROR(_(e_cannot_connect));
		sock_close(sd);
		channel_free(channel);
		return NULL;
	    }

#ifdef MSWIN
	    /* On Win32: select() is expected to work and wait for up to
	     * "waitnow" msec for the socket to be open. */
	    if (FD_ISSET(sd, &wfds))
		break;
	    elapsed_msec = waitnow;
	    if (waittime > 1 && elapsed_msec < waittime)
	    {
		waittime -= elapsed_msec;
		continue;
	    }
#else
	    /* On Linux-like systems: See socket(7) for the behavior
	     * After putting the socket in non-blocking mode, connect() will
	     * return EINPROGRESS, select() will not wait (as if writing is
	     * possible), need to use getsockopt() to check if the socket is
	     * actually able to connect.
	     * We detect a failure to connect when either read and write fds
	     * are set.  Use getsockopt() to find out what kind of failure. */
	    if (FD_ISSET(sd, &rfds) || FD_ISSET(sd, &wfds))
	    {
		ret = getsockopt(sd,
			      SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);
		if (ret < 0 || (so_error != 0
			&& so_error != EWOULDBLOCK
			&& so_error != ECONNREFUSED
# ifdef EINPROGRESS
			&& so_error != EINPROGRESS
# endif
			))
		{
		    ch_error(channel,
			    "channel_open: Connect failed with errno %d",
			    so_error);
		    PERROR(_(e_cannot_connect));
		    sock_close(sd);
		    channel_free(channel);
		    return NULL;
		}
	    }

	    if (FD_ISSET(sd, &wfds) && so_error == 0)
		/* Did not detect an error, connection is established. */
		break;

	    gettimeofday(&end_tv, NULL);
	    elapsed_msec = (end_tv.tv_sec - start_tv.tv_sec) * 1000
			     + (end_tv.tv_usec - start_tv.tv_usec) / 1000;
#endif
	}

#ifndef MSWIN
	if (waittime > 1 && elapsed_msec < waittime)
	{
	    /* The port isn't ready but we also didn't get an error.
	     * This happens when the server didn't open the socket
	     * yet.  Select() may return early, wait until the remaining
	     * "waitnow"  and try again. */
	    waitnow -= elapsed_msec;
	    waittime -= elapsed_msec;
	    if (waitnow > 0)
	    {
		mch_delay((long)waitnow, TRUE);
		ui_breakcheck();
		waittime -= waitnow;
	    }
	    if (!got_int)
	    {
		if (waittime <= 0)
		    /* give it one more try */
		    waittime = 1;
		continue;
	    }
	    /* we were interrupted, behave as if timed out */
	}
#endif

	/* We timed out. */
	ch_error(channel, "Connection timed out");
	sock_close(sd);
	channel_free(channel);
	return NULL;
    }

    ch_log(channel, "Connection made");

    if (waittime >= 0)
    {
#ifdef MSWIN
	val = 0;
	ioctlsocket(sd, FIONBIO, &val);
#else
	(void)fcntl(sd, F_SETFL, 0);
#endif
    }

    channel->CH_SOCK_FD = (sock_T)sd;
    channel->ch_nb_close_cb = nb_close_cb;
    channel->ch_hostname = (char *)vim_strsave((char_u *)hostname);
    channel->ch_port = port_in;
    channel->ch_to_be_closed |= (1U << PART_SOCK);

#ifdef FEAT_GUI
    channel_gui_register_one(channel, PART_SOCK);
#endif

    return channel;
}

/*
 * Implements ch_open().
 */
    channel_T *
channel_open_func(typval_T *argvars)
{
    char_u	*address;
    char_u	*p;
    char	*rest;
    int		port;
    jobopt_T    opt;
    channel_T	*channel = NULL;

    address = tv_get_string(&argvars[0]);
    if (argvars[1].v_type != VAR_UNKNOWN
	 && (argvars[1].v_type != VAR_DICT || argvars[1].vval.v_dict == NULL))
    {
	emsg(_(e_invarg));
	return NULL;
    }

    /* parse address */
    p = vim_strchr(address, ':');
    if (p == NULL)
    {
	semsg(_(e_invarg2), address);
	return NULL;
    }
    *p++ = NUL;
    port = strtol((char *)p, &rest, 10);
    if (*address == NUL || port <= 0 || *rest != NUL)
    {
	p[-1] = ':';
	semsg(_(e_invarg2), address);
	return NULL;
    }

    /* parse options */
    clear_job_options(&opt);
    opt.jo_mode = MODE_JSON;
    opt.jo_timeout = 2000;
    if (get_job_options(&argvars[1], &opt,
	    JO_MODE_ALL + JO_CB_ALL + JO_WAITTIME + JO_TIMEOUT_ALL, 0) == FAIL)
	goto theend;
    if (opt.jo_timeout < 0)
    {
	emsg(_(e_invarg));
	goto theend;
    }

    channel = channel_open((char *)address, port, opt.jo_waittime, NULL);
    if (channel != NULL)
    {
	opt.jo_set = JO_ALL;
	channel_set_options(channel, &opt);
    }
theend:
    free_job_options(&opt);
    return channel;
}

    static void
ch_close_part(channel_T *channel, ch_part_T part)
{
    sock_T *fd = &channel->ch_part[part].ch_fd;

    if (*fd != INVALID_FD)
    {
	if (part == PART_SOCK)
	    sock_close(*fd);
	else
	{
	    /* When using a pty the same FD is set on multiple parts, only
	     * close it when the last reference is closed. */
	    if ((part == PART_IN || channel->CH_IN_FD != *fd)
		    && (part == PART_OUT || channel->CH_OUT_FD != *fd)
		    && (part == PART_ERR || channel->CH_ERR_FD != *fd))
	    {
#ifdef MSWIN
		if (channel->ch_named_pipe)
		    DisconnectNamedPipe((HANDLE)fd);
#endif
		fd_close(*fd);
	    }
	}
	*fd = INVALID_FD;

	/* channel is closed, may want to end the job if it was the last */
	channel->ch_to_be_closed &= ~(1U << part);
    }
}

    void
channel_set_pipes(channel_T *channel, sock_T in, sock_T out, sock_T err)
{
    if (in != INVALID_FD)
    {
	ch_close_part(channel, PART_IN);
	channel->CH_IN_FD = in;
# if defined(UNIX)
	/* Do not end the job when all output channels are closed, wait until
	 * the job ended. */
	if (mch_isatty(in))
	    channel->ch_to_be_closed |= (1U << PART_IN);
# endif
    }
    if (out != INVALID_FD)
    {
# if defined(FEAT_GUI)
	channel_gui_unregister_one(channel, PART_OUT);
# endif
	ch_close_part(channel, PART_OUT);
	channel->CH_OUT_FD = out;
	channel->ch_to_be_closed |= (1U << PART_OUT);
# if defined(FEAT_GUI)
	channel_gui_register_one(channel, PART_OUT);
# endif
    }
    if (err != INVALID_FD)
    {
# if defined(FEAT_GUI)
	channel_gui_unregister_one(channel, PART_ERR);
# endif
	ch_close_part(channel, PART_ERR);
	channel->CH_ERR_FD = err;
	channel->ch_to_be_closed |= (1U << PART_ERR);
# if defined(FEAT_GUI)
	channel_gui_register_one(channel, PART_ERR);
# endif
    }
}

/*
 * Sets the job the channel is associated with and associated options.
 * This does not keep a refcount, when the job is freed ch_job is cleared.
 */
    void
channel_set_job(channel_T *channel, job_T *job, jobopt_T *options)
{
    channel->ch_job = job;

    channel_set_options(channel, options);

    if (job->jv_in_buf != NULL)
    {
	chanpart_T *in_part = &channel->ch_part[PART_IN];

	set_bufref(&in_part->ch_bufref, job->jv_in_buf);
	ch_log(channel, "reading from buffer '%s'",
				 (char *)in_part->ch_bufref.br_buf->b_ffname);
	if (options->jo_set & JO_IN_TOP)
	{
	    if (options->jo_in_top == 0 && !(options->jo_set & JO_IN_BOT))
	    {
		/* Special mode: send last-but-one line when appending a line
		 * to the buffer. */
		in_part->ch_bufref.br_buf->b_write_to_channel = TRUE;
		in_part->ch_buf_append = TRUE;
		in_part->ch_buf_top =
			    in_part->ch_bufref.br_buf->b_ml.ml_line_count + 1;
	    }
	    else
		in_part->ch_buf_top = options->jo_in_top;
	}
	else
	    in_part->ch_buf_top = 1;
	if (options->jo_set & JO_IN_BOT)
	    in_part->ch_buf_bot = options->jo_in_bot;
	else
	    in_part->ch_buf_bot = in_part->ch_bufref.br_buf->b_ml.ml_line_count;
    }
}

/*
 * Prepare buffer "buf" for writing channel output to.
 */
	static void
prepare_buffer(buf_T *buf)
{
    buf_T *save_curbuf = curbuf;

    buf_copy_options(buf, BCO_ENTER);
    curbuf = buf;
#ifdef FEAT_QUICKFIX
    set_option_value((char_u *)"bt", 0L, (char_u *)"nofile", OPT_LOCAL);
    set_option_value((char_u *)"bh", 0L, (char_u *)"hide", OPT_LOCAL);
#endif
    if (curbuf->b_ml.ml_mfp == NULL)
	ml_open(curbuf);
    curbuf = save_curbuf;
}

/*
 * Find a buffer matching "name" or create a new one.
 * Returns NULL if there is something very wrong (error already reported).
 */
    static buf_T *
find_buffer(char_u *name, int err, int msg)
{
    buf_T *buf = NULL;
    buf_T *save_curbuf = curbuf;

    if (name != NULL && *name != NUL)
    {
	buf = buflist_findname(name);
	if (buf == NULL)
	    buf = buflist_findname_exp(name);
    }
    if (buf == NULL)
    {
	buf = buflist_new(name == NULL || *name == NUL ? NULL : name,
				     NULL, (linenr_T)0, BLN_LISTED | BLN_NEW);
	if (buf == NULL)
	    return NULL;
	prepare_buffer(buf);

	curbuf = buf;
	if (msg)
	    ml_replace(1, (char_u *)(err ? "Reading from channel error..."
				   : "Reading from channel output..."), TRUE);
	changed_bytes(1, 0);
	curbuf = save_curbuf;
    }

    return buf;
}

/*
 * Copy callback from "src" to "dest", incrementing the refcounts.
 */
    static void
copy_callback(callback_T *dest, callback_T *src)
{
    dest->cb_partial = src->cb_partial;
    if (dest->cb_partial != NULL)
    {
	dest->cb_name = src->cb_name;
	dest->cb_free_name = FALSE;
	++dest->cb_partial->pt_refcount;
    }
    else
    {
	dest->cb_name = vim_strsave(src->cb_name);
	dest->cb_free_name = TRUE;
	func_ref(src->cb_name);
    }
}

    static void
free_set_callback(callback_T *cbp, callback_T *callback)
{
    free_callback(cbp);

    if (callback->cb_name != NULL && *callback->cb_name != NUL)
	copy_callback(cbp, callback);
    else
	cbp->cb_name = NULL;
}

/*
 * Set various properties from an "opt" argument.
 */
    void
channel_set_options(channel_T *channel, jobopt_T *opt)
{
    ch_part_T	part;

    if (opt->jo_set & JO_MODE)
	for (part = PART_SOCK; part < PART_COUNT; ++part)
	    channel->ch_part[part].ch_mode = opt->jo_mode;
    if (opt->jo_set & JO_IN_MODE)
	channel->ch_part[PART_IN].ch_mode = opt->jo_in_mode;
    if (opt->jo_set & JO_OUT_MODE)
	channel->ch_part[PART_OUT].ch_mode = opt->jo_out_mode;
    if (opt->jo_set & JO_ERR_MODE)
	channel->ch_part[PART_ERR].ch_mode = opt->jo_err_mode;
    channel->ch_nonblock = opt->jo_noblock;

    if (opt->jo_set & JO_TIMEOUT)
	for (part = PART_SOCK; part < PART_COUNT; ++part)
	    channel->ch_part[part].ch_timeout = opt->jo_timeout;
    if (opt->jo_set & JO_OUT_TIMEOUT)
	channel->ch_part[PART_OUT].ch_timeout = opt->jo_out_timeout;
    if (opt->jo_set & JO_ERR_TIMEOUT)
	channel->ch_part[PART_ERR].ch_timeout = opt->jo_err_timeout;
    if (opt->jo_set & JO_BLOCK_WRITE)
	channel->ch_part[PART_IN].ch_block_write = 1;

    if (opt->jo_set & JO_CALLBACK)
	free_set_callback(&channel->ch_callback, &opt->jo_callback);
    if (opt->jo_set & JO_OUT_CALLBACK)
	free_set_callback(&channel->ch_part[PART_OUT].ch_callback,
							      &opt->jo_out_cb);
    if (opt->jo_set & JO_ERR_CALLBACK)
	free_set_callback(&channel->ch_part[PART_ERR].ch_callback,
							      &opt->jo_err_cb);
    if (opt->jo_set & JO_CLOSE_CALLBACK)
	free_set_callback(&channel->ch_close_cb, &opt->jo_close_cb);
    channel->ch_drop_never = opt->jo_drop_never;

    if ((opt->jo_set & JO_OUT_IO) && opt->jo_io[PART_OUT] == JIO_BUFFER)
    {
	buf_T *buf;

	/* writing output to a buffer. Default mode is NL. */
	if (!(opt->jo_set & JO_OUT_MODE))
	    channel->ch_part[PART_OUT].ch_mode = MODE_NL;
	if (opt->jo_set & JO_OUT_BUF)
	{
	    buf = buflist_findnr(opt->jo_io_buf[PART_OUT]);
	    if (buf == NULL)
		semsg(_(e_nobufnr), (long)opt->jo_io_buf[PART_OUT]);
	}
	else
	{
	    int msg = TRUE;

	    if (opt->jo_set2 & JO2_OUT_MSG)
		msg = opt->jo_message[PART_OUT];
	    buf = find_buffer(opt->jo_io_name[PART_OUT], FALSE, msg);
	}
	if (buf != NULL)
	{
	    if (opt->jo_set & JO_OUT_MODIFIABLE)
		channel->ch_part[PART_OUT].ch_nomodifiable =
						!opt->jo_modifiable[PART_OUT];

	    if (!buf->b_p_ma && !channel->ch_part[PART_OUT].ch_nomodifiable)
	    {
		emsg(_(e_modifiable));
	    }
	    else
	    {
		ch_log(channel, "writing out to buffer '%s'",
						       (char *)buf->b_ffname);
		set_bufref(&channel->ch_part[PART_OUT].ch_bufref, buf);
		// if the buffer was deleted or unloaded resurrect it
		if (buf->b_ml.ml_mfp == NULL)
		    prepare_buffer(buf);
	    }
	}
    }

    if ((opt->jo_set & JO_ERR_IO) && (opt->jo_io[PART_ERR] == JIO_BUFFER
	 || (opt->jo_io[PART_ERR] == JIO_OUT && (opt->jo_set & JO_OUT_IO)
				       && opt->jo_io[PART_OUT] == JIO_BUFFER)))
    {
	buf_T *buf;

	/* writing err to a buffer. Default mode is NL. */
	if (!(opt->jo_set & JO_ERR_MODE))
	    channel->ch_part[PART_ERR].ch_mode = MODE_NL;
	if (opt->jo_io[PART_ERR] == JIO_OUT)
	    buf = channel->ch_part[PART_OUT].ch_bufref.br_buf;
	else if (opt->jo_set & JO_ERR_BUF)
	{
	    buf = buflist_findnr(opt->jo_io_buf[PART_ERR]);
	    if (buf == NULL)
		semsg(_(e_nobufnr), (long)opt->jo_io_buf[PART_ERR]);
	}
	else
	{
	    int msg = TRUE;

	    if (opt->jo_set2 & JO2_ERR_MSG)
		msg = opt->jo_message[PART_ERR];
	    buf = find_buffer(opt->jo_io_name[PART_ERR], TRUE, msg);
	}
	if (buf != NULL)
	{
	    if (opt->jo_set & JO_ERR_MODIFIABLE)
		channel->ch_part[PART_ERR].ch_nomodifiable =
						!opt->jo_modifiable[PART_ERR];
	    if (!buf->b_p_ma && !channel->ch_part[PART_ERR].ch_nomodifiable)
	    {
		emsg(_(e_modifiable));
	    }
	    else
	    {
		ch_log(channel, "writing err to buffer '%s'",
						       (char *)buf->b_ffname);
		set_bufref(&channel->ch_part[PART_ERR].ch_bufref, buf);
		// if the buffer was deleted or unloaded resurrect it
		if (buf->b_ml.ml_mfp == NULL)
		    prepare_buffer(buf);
	    }
	}
    }

    channel->ch_part[PART_OUT].ch_io = opt->jo_io[PART_OUT];
    channel->ch_part[PART_ERR].ch_io = opt->jo_io[PART_ERR];
    channel->ch_part[PART_IN].ch_io = opt->jo_io[PART_IN];
}

/*
 * Set the callback for "channel"/"part" for the response with "id".
 */
    void
channel_set_req_callback(
	channel_T   *channel,
	ch_part_T   part,
	callback_T  *callback,
	int	    id)
{
    cbq_T *head = &channel->ch_part[part].ch_cb_head;
    cbq_T *item = ALLOC_ONE(cbq_T);

    if (item != NULL)
    {
	copy_callback(&item->cq_callback, callback);
	item->cq_seq_nr = id;
	item->cq_prev = head->cq_prev;
	head->cq_prev = item;
	item->cq_next = NULL;
	if (item->cq_prev == NULL)
	    head->cq_next = item;
	else
	    item->cq_prev->cq_next = item;
    }
}

    static void
write_buf_line(buf_T *buf, linenr_T lnum, channel_T *channel)
{
    char_u  *line = ml_get_buf(buf, lnum, FALSE);
    int	    len = (int)STRLEN(line);
    char_u  *p;
    int	    i;

    /* Need to make a copy to be able to append a NL. */
    if ((p = alloc(len + 2)) == NULL)
	return;
    memcpy((char *)p, (char *)line, len);

    if (channel->ch_write_text_mode)
	p[len] = CAR;
    else
    {
	for (i = 0; i < len; ++i)
	    if (p[i] == NL)
		p[i] = NUL;

	p[len] = NL;
    }
    p[len + 1] = NUL;
    channel_send(channel, PART_IN, p, len + 1, "write_buf_line");
    vim_free(p);
}

/*
 * Return TRUE if "channel" can be written to.
 * Returns FALSE if the input is closed or the write would block.
 */
    static int
can_write_buf_line(channel_T *channel)
{
    chanpart_T *in_part = &channel->ch_part[PART_IN];

    if (in_part->ch_fd == INVALID_FD)
	return FALSE;  /* pipe was closed */

    /* for testing: block every other attempt to write */
    if (in_part->ch_block_write == 1)
	in_part->ch_block_write = -1;
    else if (in_part->ch_block_write == -1)
	in_part->ch_block_write = 1;

    /* TODO: Win32 implementation, probably using WaitForMultipleObjects() */
#ifndef MSWIN
    {
# if defined(HAVE_SELECT)
	struct timeval	tval;
	fd_set		wfds;
	int		ret;

	FD_ZERO(&wfds);
	FD_SET((int)in_part->ch_fd, &wfds);
	tval.tv_sec = 0;
	tval.tv_usec = 0;
	for (;;)
	{
	    ret = select((int)in_part->ch_fd + 1, NULL, &wfds, NULL, &tval);
#  ifdef EINTR
	    SOCK_ERRNO;
	    if (ret == -1 && errno == EINTR)
		continue;
#  endif
	    if (ret <= 0 || in_part->ch_block_write == 1)
	    {
		if (ret > 0)
		    ch_log(channel, "FAKED Input not ready for writing");
		else
		    ch_log(channel, "Input not ready for writing");
		return FALSE;
	    }
	    break;
	}
# else
	struct pollfd	fds;

	fds.fd = in_part->ch_fd;
	fds.events = POLLOUT;
	if (poll(&fds, 1, 0) <= 0)
	{
	    ch_log(channel, "Input not ready for writing");
	    return FALSE;
	}
	if (in_part->ch_block_write == 1)
	{
	    ch_log(channel, "FAKED Input not ready for writing");
	    return FALSE;
	}
# endif
    }
#endif
    return TRUE;
}

/*
 * Write any buffer lines to the input channel.
 */
    static void
channel_write_in(channel_T *channel)
{
    chanpart_T *in_part = &channel->ch_part[PART_IN];
    linenr_T    lnum;
    buf_T	*buf = in_part->ch_bufref.br_buf;
    int		written = 0;

    if (buf == NULL || in_part->ch_buf_append)
	return;  /* no buffer or using appending */
    if (!bufref_valid(&in_part->ch_bufref) || buf->b_ml.ml_mfp == NULL)
    {
	/* buffer was wiped out or unloaded */
	ch_log(channel, "input buffer has been wiped out");
	in_part->ch_bufref.br_buf = NULL;
	return;
    }

    for (lnum = in_part->ch_buf_top; lnum <= in_part->ch_buf_bot
				   && lnum <= buf->b_ml.ml_line_count; ++lnum)
    {
	if (!can_write_buf_line(channel))
	    break;
	write_buf_line(buf, lnum, channel);
	++written;
    }

    if (written == 1)
	ch_log(channel, "written line %d to channel", (int)lnum - 1);
    else if (written > 1)
	ch_log(channel, "written %d lines to channel", written);

    in_part->ch_buf_top = lnum;
    if (lnum > buf->b_ml.ml_line_count || lnum > in_part->ch_buf_bot)
    {
#if defined(FEAT_TERMINAL)
	/* Send CTRL-D or "eof_chars" to close stdin on MS-Windows. */
	if (channel->ch_job != NULL)
	    term_send_eof(channel);
#endif

	/* Writing is done, no longer need the buffer. */
	in_part->ch_bufref.br_buf = NULL;
	ch_log(channel, "Finished writing all lines to channel");

	/* Close the pipe/socket, so that the other side gets EOF. */
	ch_close_part(channel, PART_IN);
    }
    else
	ch_log(channel, "Still %ld more lines to write",
				   (long)(buf->b_ml.ml_line_count - lnum + 1));
}

/*
 * Handle buffer "buf" being freed, remove it from any channels.
 */
    void
channel_buffer_free(buf_T *buf)
{
    channel_T	*channel;
    ch_part_T	part;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	for (part = PART_SOCK; part < PART_COUNT; ++part)
	{
	    chanpart_T  *ch_part = &channel->ch_part[part];

	    if (ch_part->ch_bufref.br_buf == buf)
	    {
		ch_log(channel, "%s buffer has been wiped out",
							    part_names[part]);
		ch_part->ch_bufref.br_buf = NULL;
	    }
	}
}

/*
 * Write any lines waiting to be written to "channel".
 */
    static void
channel_write_input(channel_T *channel)
{
    chanpart_T	*in_part = &channel->ch_part[PART_IN];

    if (in_part->ch_writeque.wq_next != NULL)
	channel_send(channel, PART_IN, (char_u *)"", 0, "channel_write_input");
    else if (in_part->ch_bufref.br_buf != NULL)
    {
	if (in_part->ch_buf_append)
	    channel_write_new_lines(in_part->ch_bufref.br_buf);
	else
	    channel_write_in(channel);
    }
}

/*
 * Write any lines waiting to be written to a channel.
 */
    void
channel_write_any_lines(void)
{
    channel_T	*channel;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	channel_write_input(channel);
}

/*
 * Write appended lines above the last one in "buf" to the channel.
 */
    void
channel_write_new_lines(buf_T *buf)
{
    channel_T	*channel;
    int		found_one = FALSE;

    /* There could be more than one channel for the buffer, loop over all of
     * them. */
    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	chanpart_T  *in_part = &channel->ch_part[PART_IN];
	linenr_T    lnum;
	int	    written = 0;

	if (in_part->ch_bufref.br_buf == buf && in_part->ch_buf_append)
	{
	    if (in_part->ch_fd == INVALID_FD)
		continue;  /* pipe was closed */
	    found_one = TRUE;
	    for (lnum = in_part->ch_buf_bot; lnum < buf->b_ml.ml_line_count;
								       ++lnum)
	    {
		if (!can_write_buf_line(channel))
		    break;
		write_buf_line(buf, lnum, channel);
		++written;
	    }

	    if (written == 1)
		ch_log(channel, "written line %d to channel", (int)lnum - 1);
	    else if (written > 1)
		ch_log(channel, "written %d lines to channel", written);
	    if (lnum < buf->b_ml.ml_line_count)
		ch_log(channel, "Still %ld more lines to write",
				       (long)(buf->b_ml.ml_line_count - lnum));

	    in_part->ch_buf_bot = lnum;
	}
    }
    if (!found_one)
	buf->b_write_to_channel = FALSE;
}

/*
 * Invoke the "callback" on channel "channel".
 * This does not redraw but sets channel_need_redraw;
 */
    static void
invoke_callback(channel_T *channel, callback_T *callback, typval_T *argv)
{
    typval_T	rettv;
    int		dummy;

    if (safe_to_invoke_callback == 0)
	iemsg("INTERNAL: Invoking callback when it is not safe");

    argv[0].v_type = VAR_CHANNEL;
    argv[0].vval.v_channel = channel;

    call_callback(callback, -1, &rettv, 2, argv, NULL,
						   0L, 0L, &dummy, TRUE, NULL);
    clear_tv(&rettv);
    channel_need_redraw = TRUE;
}

/*
 * Return the first node from "channel"/"part" without removing it.
 * Returns NULL if there is nothing.
 */
    readq_T *
channel_peek(channel_T *channel, ch_part_T part)
{
    readq_T *head = &channel->ch_part[part].ch_head;

    return head->rq_next;
}

/*
 * Return a pointer to the first NL in "node".
 * Skips over NUL characters.
 * Returns NULL if there is no NL.
 */
    char_u *
channel_first_nl(readq_T *node)
{
    char_u  *buffer = node->rq_buffer;
    long_u  i;

    for (i = 0; i < node->rq_buflen; ++i)
	if (buffer[i] == NL)
	    return buffer + i;
    return NULL;
}

/*
 * Return the first buffer from channel "channel"/"part" and remove it.
 * The caller must free it.
 * Returns NULL if there is nothing.
 */
    char_u *
channel_get(channel_T *channel, ch_part_T part, int *outlen)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    char_u *p;

    if (node == NULL)
	return NULL;
    if (outlen != NULL)
	*outlen += node->rq_buflen;
    /* dispose of the node but keep the buffer */
    p = node->rq_buffer;
    head->rq_next = node->rq_next;
    if (node->rq_next == NULL)
	head->rq_prev = NULL;
    else
	node->rq_next->rq_prev = NULL;
    vim_free(node);
    return p;
}

/*
 * Returns the whole buffer contents concatenated for "channel"/"part".
 * Replaces NUL bytes with NL.
 */
    static char_u *
channel_get_all(channel_T *channel, ch_part_T part, int *outlen)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node;
    long_u  len = 0;
    char_u  *res;
    char_u  *p;

    // Concatenate everything into one buffer.
    for (node = head->rq_next; node != NULL; node = node->rq_next)
	len += node->rq_buflen;
    res = alloc(len + 1);
    if (res == NULL)
	return NULL;
    p = res;
    for (node = head->rq_next; node != NULL; node = node->rq_next)
    {
	mch_memmove(p, node->rq_buffer, node->rq_buflen);
	p += node->rq_buflen;
    }
    *p = NUL;

    // Free all buffers
    do
    {
	p = channel_get(channel, part, NULL);
	vim_free(p);
    } while (p != NULL);

    if (outlen != NULL)
    {
	// Returning the length, keep NUL characters.
	*outlen += len;
	return res;
    }

    // Turn all NUL into NL, so that the result can be used as a string.
    p = res;
    while (p < res + len)
    {
	if (*p == NUL)
	    *p = NL;
#ifdef MSWIN
	else if (*p == 0x1b)
	{
	    // crush the escape sequence OSC 0/1/2: ESC ]0;
	    if (p + 3 < res + len
		    && p[1] == ']'
		    && (p[2] == '0' || p[2] == '1' || p[2] == '2')
		    && p[3] == ';')
	    {
		// '\a' becomes a NL
	        while (p < res + (len - 1) && *p != '\a')
		    ++p;
		// BEL is zero width characters, suppress display mistake
		// ConPTY (after 10.0.18317) requires advance checking
		if (p[-1] == NUL)
		    p[-1] = 0x07;
	    }
	}
#endif
	++p;
    }

    return res;
}

/*
 * Consume "len" bytes from the head of "node".
 * Caller must check these bytes are available.
 */
    void
channel_consume(channel_T *channel, ch_part_T part, int len)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    char_u *buf = node->rq_buffer;

    mch_memmove(buf, buf + len, node->rq_buflen - len);
    node->rq_buflen -= len;
    node->rq_buffer[node->rq_buflen] = NUL;
}

/*
 * Collapses the first and second buffer for "channel"/"part".
 * Returns FAIL if that is not possible.
 * When "want_nl" is TRUE collapse more buffers until a NL is found.
 */
    int
channel_collapse(channel_T *channel, ch_part_T part, int want_nl)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    readq_T *last_node;
    readq_T *n;
    char_u  *newbuf;
    char_u  *p;
    long_u len;

    if (node == NULL || node->rq_next == NULL)
	return FAIL;

    last_node = node->rq_next;
    len = node->rq_buflen + last_node->rq_buflen;
    if (want_nl)
	while (last_node->rq_next != NULL
		&& channel_first_nl(last_node) == NULL)
	{
	    last_node = last_node->rq_next;
	    len += last_node->rq_buflen;
	}

    p = newbuf = alloc(len + 1);
    if (newbuf == NULL)
	return FAIL;	    /* out of memory */
    mch_memmove(p, node->rq_buffer, node->rq_buflen);
    p += node->rq_buflen;
    vim_free(node->rq_buffer);
    node->rq_buffer = newbuf;
    for (n = node; n != last_node; )
    {
	n = n->rq_next;
	mch_memmove(p, n->rq_buffer, n->rq_buflen);
	p += n->rq_buflen;
	vim_free(n->rq_buffer);
    }
    *p = NUL;
    node->rq_buflen = (long_u)(p - newbuf);

    /* dispose of the collapsed nodes and their buffers */
    for (n = node->rq_next; n != last_node; )
    {
	n = n->rq_next;
	vim_free(n->rq_prev);
    }
    node->rq_next = last_node->rq_next;
    if (last_node->rq_next == NULL)
	head->rq_prev = node;
    else
	last_node->rq_next->rq_prev = node;
    vim_free(last_node);
    return OK;
}

/*
 * Store "buf[len]" on "channel"/"part".
 * When "prepend" is TRUE put in front, otherwise append at the end.
 * Returns OK or FAIL.
 */
    static int
channel_save(channel_T *channel, ch_part_T part, char_u *buf, int len,
						      int prepend, char *lead)
{
    readq_T *node;
    readq_T *head = &channel->ch_part[part].ch_head;
    char_u  *p;
    int	    i;

    node = ALLOC_ONE(readq_T);
    if (node == NULL)
	return FAIL;	    /* out of memory */
    /* A NUL is added at the end, because netbeans code expects that.
     * Otherwise a NUL may appear inside the text. */
    node->rq_buffer = alloc(len + 1);
    if (node->rq_buffer == NULL)
    {
	vim_free(node);
	return FAIL;	    /* out of memory */
    }

    if (channel->ch_part[part].ch_mode == MODE_NL)
    {
	/* Drop any CR before a NL. */
	p = node->rq_buffer;
	for (i = 0; i < len; ++i)
	    if (buf[i] != CAR || i + 1 >= len || buf[i + 1] != NL)
		*p++ = buf[i];
	*p = NUL;
	node->rq_buflen = (long_u)(p - node->rq_buffer);
    }
    else
    {
	mch_memmove(node->rq_buffer, buf, len);
	node->rq_buffer[len] = NUL;
	node->rq_buflen = (long_u)len;
    }

    if (prepend)
    {
	// prepend node to the head of the queue
	node->rq_next = head->rq_next;
	node->rq_prev = NULL;
	if (head->rq_next == NULL)
	    head->rq_prev = node;
	else
	    head->rq_next->rq_prev = node;
	head->rq_next = node;
    }
    else
    {
	// append node to the tail of the queue
	node->rq_next = NULL;
	node->rq_prev = head->rq_prev;
	if (head->rq_prev == NULL)
	    head->rq_next = node;
	else
	    head->rq_prev->rq_next = node;
	head->rq_prev = node;
    }

    if (ch_log_active() && lead != NULL)
    {
	ch_log_lead(lead, channel, part);
	fprintf(log_fd, "'");
	vim_ignored = (int)fwrite(buf, len, 1, log_fd);
	fprintf(log_fd, "'\n");
    }
    return OK;
}

/*
 * Try to fill the buffer of "reader".
 * Returns FALSE when nothing was added.
 */
    static int
channel_fill(js_read_T *reader)
{
    channel_T	*channel = (channel_T *)reader->js_cookie;
    ch_part_T	part = reader->js_cookie_arg;
    char_u	*next = channel_get(channel, part, NULL);
    int		keeplen;
    int		addlen;
    char_u	*p;

    if (next == NULL)
	return FALSE;

    keeplen = reader->js_end - reader->js_buf;
    if (keeplen > 0)
    {
	/* Prepend unused text. */
	addlen = (int)STRLEN(next);
	p = alloc(keeplen + addlen + 1);
	if (p == NULL)
	{
	    vim_free(next);
	    return FALSE;
	}
	mch_memmove(p, reader->js_buf, keeplen);
	mch_memmove(p + keeplen, next, addlen + 1);
	vim_free(next);
	next = p;
    }

    vim_free(reader->js_buf);
    reader->js_buf = next;
    return TRUE;
}

/*
 * Use the read buffer of "channel"/"part" and parse a JSON message that is
 * complete.  The messages are added to the queue.
 * Return TRUE if there is more to read.
 */
    static int
channel_parse_json(channel_T *channel, ch_part_T part)
{
    js_read_T	reader;
    typval_T	listtv;
    jsonq_T	*item;
    chanpart_T	*chanpart = &channel->ch_part[part];
    jsonq_T	*head = &chanpart->ch_json_head;
    int		status;
    int		ret;

    if (channel_peek(channel, part) == NULL)
	return FALSE;

    reader.js_buf = channel_get(channel, part, NULL);
    reader.js_used = 0;
    reader.js_fill = channel_fill;
    reader.js_cookie = channel;
    reader.js_cookie_arg = part;

    /* When a message is incomplete we wait for a short while for more to
     * arrive.  After the delay drop the input, otherwise a truncated string
     * or list will make us hang.
     * Do not generate error messages, they will be written in a channel log. */
    ++emsg_silent;
    status = json_decode(&reader, &listtv,
				  chanpart->ch_mode == MODE_JS ? JSON_JS : 0);
    --emsg_silent;
    if (status == OK)
    {
	/* Only accept the response when it is a list with at least two
	 * items. */
	if (listtv.v_type != VAR_LIST || listtv.vval.v_list->lv_len < 2)
	{
	    if (listtv.v_type != VAR_LIST)
		ch_error(channel, "Did not receive a list, discarding");
	    else
		ch_error(channel, "Expected list with two items, got %d",
						  listtv.vval.v_list->lv_len);
	    clear_tv(&listtv);
	}
	else
	{
	    item = ALLOC_ONE(jsonq_T);
	    if (item == NULL)
		clear_tv(&listtv);
	    else
	    {
		item->jq_no_callback = FALSE;
		item->jq_value = alloc_tv();
		if (item->jq_value == NULL)
		{
		    vim_free(item);
		    clear_tv(&listtv);
		}
		else
		{
		    *item->jq_value = listtv;
		    item->jq_prev = head->jq_prev;
		    head->jq_prev = item;
		    item->jq_next = NULL;
		    if (item->jq_prev == NULL)
			head->jq_next = item;
		    else
			item->jq_prev->jq_next = item;
		}
	    }
	}
    }

    if (status == OK)
	chanpart->ch_wait_len = 0;
    else if (status == MAYBE)
    {
	size_t buflen = STRLEN(reader.js_buf);

	if (chanpart->ch_wait_len < buflen)
	{
	    /* First time encountering incomplete message or after receiving
	     * more (but still incomplete): set a deadline of 100 msec. */
	    ch_log(channel,
		    "Incomplete message (%d bytes) - wait 100 msec for more",
		    (int)buflen);
	    reader.js_used = 0;
	    chanpart->ch_wait_len = buflen;
#ifdef MSWIN
	    chanpart->ch_deadline = GetTickCount() + 100L;
#else
	    gettimeofday(&chanpart->ch_deadline, NULL);
	    chanpart->ch_deadline.tv_usec += 100 * 1000;
	    if (chanpart->ch_deadline.tv_usec > 1000 * 1000)
	    {
		chanpart->ch_deadline.tv_usec -= 1000 * 1000;
		++chanpart->ch_deadline.tv_sec;
	    }
#endif
	}
	else
	{
	    int timeout;
#ifdef MSWIN
	    timeout = GetTickCount() > chanpart->ch_deadline;
#else
	    {
		struct timeval now_tv;

		gettimeofday(&now_tv, NULL);
		timeout = now_tv.tv_sec > chanpart->ch_deadline.tv_sec
		      || (now_tv.tv_sec == chanpart->ch_deadline.tv_sec
			   && now_tv.tv_usec > chanpart->ch_deadline.tv_usec);
	    }
#endif
	    if (timeout)
	    {
		status = FAIL;
		chanpart->ch_wait_len = 0;
		ch_log(channel, "timed out");
	    }
	    else
	    {
		reader.js_used = 0;
		ch_log(channel, "still waiting on incomplete message");
	    }
	}
    }

    if (status == FAIL)
    {
	ch_error(channel, "Decoding failed - discarding input");
	ret = FALSE;
	chanpart->ch_wait_len = 0;
    }
    else if (reader.js_buf[reader.js_used] != NUL)
    {
	/* Put the unread part back into the channel. */
	channel_save(channel, part, reader.js_buf + reader.js_used,
			(int)(reader.js_end - reader.js_buf) - reader.js_used,
								  TRUE, NULL);
	ret = status == MAYBE ? FALSE: TRUE;
    }
    else
	ret = FALSE;

    vim_free(reader.js_buf);
    return ret;
}

/*
 * Remove "node" from the queue that it is in.  Does not free it.
 */
    static void
remove_cb_node(cbq_T *head, cbq_T *node)
{
    if (node->cq_prev == NULL)
	head->cq_next = node->cq_next;
    else
	node->cq_prev->cq_next = node->cq_next;
    if (node->cq_next == NULL)
	head->cq_prev = node->cq_prev;
    else
	node->cq_next->cq_prev = node->cq_prev;
}

/*
 * Remove "node" from the queue that it is in and free it.
 * Caller should have freed or used node->jq_value.
 */
    static void
remove_json_node(jsonq_T *head, jsonq_T *node)
{
    if (node->jq_prev == NULL)
	head->jq_next = node->jq_next;
    else
	node->jq_prev->jq_next = node->jq_next;
    if (node->jq_next == NULL)
	head->jq_prev = node->jq_prev;
    else
	node->jq_next->jq_prev = node->jq_prev;
    vim_free(node);
}

/*
 * Get a message from the JSON queue for channel "channel".
 * When "id" is positive it must match the first number in the list.
 * When "id" is zero or negative jut get the first message.  But not the one
 * with id ch_block_id.
 * When "without_callback" is TRUE also get messages that were pushed back.
 * Return OK when found and return the value in "rettv".
 * Return FAIL otherwise.
 */
    static int
channel_get_json(
	channel_T   *channel,
	ch_part_T   part,
	int	    id,
	int	    without_callback,
	typval_T    **rettv)
{
    jsonq_T   *head = &channel->ch_part[part].ch_json_head;
    jsonq_T   *item = head->jq_next;

    while (item != NULL)
    {
	list_T	    *l = item->jq_value->vval.v_list;
	typval_T    *tv = &l->lv_first->li_tv;

	if ((without_callback || !item->jq_no_callback)
	    && ((id > 0 && tv->v_type == VAR_NUMBER && tv->vval.v_number == id)
	      || (id <= 0 && (tv->v_type != VAR_NUMBER
		 || tv->vval.v_number == 0
		 || tv->vval.v_number != channel->ch_part[part].ch_block_id))))
	{
	    *rettv = item->jq_value;
	    if (tv->v_type == VAR_NUMBER)
		ch_log(channel, "Getting JSON message %ld",
						      (long)tv->vval.v_number);
	    remove_json_node(head, item);
	    return OK;
	}
	item = item->jq_next;
    }
    return FAIL;
}

/*
 * Put back "rettv" into the JSON queue, there was no callback for it.
 * Takes over the values in "rettv".
 */
    static void
channel_push_json(channel_T *channel, ch_part_T part, typval_T *rettv)
{
    jsonq_T   *head = &channel->ch_part[part].ch_json_head;
    jsonq_T   *item = head->jq_next;
    jsonq_T   *newitem;

    if (head->jq_prev != NULL && head->jq_prev->jq_no_callback)
	/* last item was pushed back, append to the end */
	item = NULL;
    else while (item != NULL && item->jq_no_callback)
	/* append after the last item that was pushed back */
	item = item->jq_next;

    newitem = ALLOC_ONE(jsonq_T);
    if (newitem == NULL)
	clear_tv(rettv);
    else
    {
	newitem->jq_value = alloc_tv();
	if (newitem->jq_value == NULL)
	{
	    vim_free(newitem);
	    clear_tv(rettv);
	}
	else
	{
	    newitem->jq_no_callback = FALSE;
	    *newitem->jq_value = *rettv;
	    if (item == NULL)
	    {
		/* append to the end */
		newitem->jq_prev = head->jq_prev;
		head->jq_prev = newitem;
		newitem->jq_next = NULL;
		if (newitem->jq_prev == NULL)
		    head->jq_next = newitem;
		else
		    newitem->jq_prev->jq_next = newitem;
	    }
	    else
	    {
		/* append after "item" */
		newitem->jq_prev = item;
		newitem->jq_next = item->jq_next;
		item->jq_next = newitem;
		if (newitem->jq_next == NULL)
		    head->jq_prev = newitem;
		else
		    newitem->jq_next->jq_prev = newitem;
	    }
	}
    }
}

#define CH_JSON_MAX_ARGS 4

/*
 * Execute a command received over "channel"/"part"
 * "argv[0]" is the command string.
 * "argv[1]" etc. have further arguments, type is VAR_UNKNOWN if missing.
 */
    static void
channel_exe_cmd(channel_T *channel, ch_part_T part, typval_T *argv)
{
    char_u  *cmd = argv[0].vval.v_string;
    char_u  *arg;
    int	    options = channel->ch_part[part].ch_mode == MODE_JS ? JSON_JS : 0;

    if (argv[1].v_type != VAR_STRING)
    {
	ch_error(channel, "received command with non-string argument");
	if (p_verbose > 2)
	    emsg(_("E903: received command with non-string argument"));
	return;
    }
    arg = argv[1].vval.v_string;
    if (arg == NULL)
	arg = (char_u *)"";

    if (STRCMP(cmd, "ex") == 0)
    {
	int save_called_emsg = called_emsg;

	called_emsg = FALSE;
	ch_log(channel, "Executing ex command '%s'", (char *)arg);
	++emsg_silent;
	do_cmdline_cmd(arg);
	--emsg_silent;
	if (called_emsg)
	    ch_log(channel, "Ex command error: '%s'",
					  (char *)get_vim_var_str(VV_ERRMSG));
	called_emsg = save_called_emsg;
    }
    else if (STRCMP(cmd, "normal") == 0)
    {
	exarg_T ea;

	ch_log(channel, "Executing normal command '%s'", (char *)arg);
	vim_memset(&ea, 0, sizeof(ea));
	ea.arg = arg;
	ea.addr_count = 0;
	ea.forceit = TRUE; /* no mapping */
	ex_normal(&ea);
    }
    else if (STRCMP(cmd, "redraw") == 0)
    {
	exarg_T ea;

	ch_log(channel, "redraw");
	vim_memset(&ea, 0, sizeof(ea));
	ea.forceit = *arg != NUL;
	ex_redraw(&ea);
	showruler(FALSE);
	setcursor();
	out_flush_cursor(TRUE, FALSE);
    }
    else if (STRCMP(cmd, "expr") == 0 || STRCMP(cmd, "call") == 0)
    {
	int is_call = cmd[0] == 'c';
	int id_idx = is_call ? 3 : 2;

	if (argv[id_idx].v_type != VAR_UNKNOWN
					 && argv[id_idx].v_type != VAR_NUMBER)
	{
	    ch_error(channel, "last argument for expr/call must be a number");
	    if (p_verbose > 2)
		emsg(_("E904: last argument for expr/call must be a number"));
	}
	else if (is_call && argv[2].v_type != VAR_LIST)
	{
	    ch_error(channel, "third argument for call must be a list");
	    if (p_verbose > 2)
		emsg(_("E904: third argument for call must be a list"));
	}
	else
	{
	    typval_T	*tv = NULL;
	    typval_T	res_tv;
	    typval_T	err_tv;
	    char_u	*json = NULL;

	    /* Don't pollute the display with errors. */
	    ++emsg_skip;
	    if (!is_call)
	    {
		ch_log(channel, "Evaluating expression '%s'", (char *)arg);
		tv = eval_expr(arg, NULL);
	    }
	    else
	    {
		ch_log(channel, "Calling '%s'", (char *)arg);
		if (func_call(arg, &argv[2], NULL, NULL, &res_tv) == OK)
		    tv = &res_tv;
	    }

	    if (argv[id_idx].v_type == VAR_NUMBER)
	    {
		int id = argv[id_idx].vval.v_number;

		if (tv != NULL)
		    json = json_encode_nr_expr(id, tv, options | JSON_NL);
		if (tv == NULL || (json != NULL && *json == NUL))
		{
		    /* If evaluation failed or the result can't be encoded
		     * then return the string "ERROR". */
		    vim_free(json);
		    err_tv.v_type = VAR_STRING;
		    err_tv.vval.v_string = (char_u *)"ERROR";
		    json = json_encode_nr_expr(id, &err_tv, options | JSON_NL);
		}
		if (json != NULL)
		{
		    channel_send(channel,
				 part == PART_SOCK ? PART_SOCK : PART_IN,
				 json, (int)STRLEN(json), (char *)cmd);
		    vim_free(json);
		}
	    }
	    --emsg_skip;
	    if (tv == &res_tv)
		clear_tv(tv);
	    else
		free_tv(tv);
	}
    }
    else if (p_verbose > 2)
    {
	ch_error(channel, "Received unknown command: %s", (char *)cmd);
	semsg(_("E905: received unknown command: %s"), cmd);
    }
}

/*
 * Invoke the callback at "cbhead".
 * Does not redraw but sets channel_need_redraw.
 */
    static void
invoke_one_time_callback(
	channel_T   *channel,
	cbq_T	    *cbhead,
	cbq_T	    *item,
	typval_T    *argv)
{
    ch_log(channel, "Invoking one-time callback %s",
					    (char *)item->cq_callback.cb_name);
    /* Remove the item from the list first, if the callback
     * invokes ch_close() the list will be cleared. */
    remove_cb_node(cbhead, item);
    invoke_callback(channel, &item->cq_callback, argv);
    free_callback(&item->cq_callback);
    vim_free(item);
}

    static void
append_to_buffer(buf_T *buffer, char_u *msg, channel_T *channel, ch_part_T part)
{
    bufref_T	save_curbuf = {NULL, 0, 0};
    win_T	*save_curwin = NULL;
    tabpage_T	*save_curtab = NULL;
    linenr_T    lnum = buffer->b_ml.ml_line_count;
    int		save_write_to = buffer->b_write_to_channel;
    chanpart_T  *ch_part = &channel->ch_part[part];
    int		save_p_ma = buffer->b_p_ma;
    int		empty = (buffer->b_ml.ml_flags & ML_EMPTY) ? 1 : 0;

    if (!buffer->b_p_ma && !ch_part->ch_nomodifiable)
    {
	if (!ch_part->ch_nomod_error)
	{
	    ch_error(channel, "Buffer is not modifiable, cannot append");
	    ch_part->ch_nomod_error = TRUE;
	}
	return;
    }

    /* If the buffer is also used as input insert above the last
     * line. Don't write these lines. */
    if (save_write_to)
    {
	--lnum;
	buffer->b_write_to_channel = FALSE;
    }

    /* Append to the buffer */
    ch_log(channel, "appending line %d to buffer", (int)lnum + 1 - empty);

    buffer->b_p_ma = TRUE;

    /* Save curbuf/curwin/curtab and make "buffer" the current buffer. */
    switch_to_win_for_buf(buffer, &save_curwin, &save_curtab, &save_curbuf);

    u_sync(TRUE);
    /* ignore undo failure, undo is not very useful here */
    vim_ignored = u_save(lnum - empty, lnum + 1);

    if (empty)
    {
	/* The buffer is empty, replace the first (dummy) line. */
	ml_replace(lnum, msg, TRUE);
	lnum = 0;
    }
    else
	ml_append(lnum, msg, 0, FALSE);
    appended_lines_mark(lnum, 1L);

    /* Restore curbuf/curwin/curtab */
    restore_win_for_buf(save_curwin, save_curtab, &save_curbuf);

    if (ch_part->ch_nomodifiable)
	buffer->b_p_ma = FALSE;
    else
	buffer->b_p_ma = save_p_ma;

    if (buffer->b_nwindows > 0)
    {
	win_T	*wp;

	FOR_ALL_WINDOWS(wp)
	{
	    if (wp->w_buffer == buffer
		    && (save_write_to
			? wp->w_cursor.lnum == lnum + 1
			: (wp->w_cursor.lnum == lnum
			    && wp->w_cursor.col == 0)))
	    {
		++wp->w_cursor.lnum;
		save_curwin = curwin;
		curwin = wp;
		curbuf = curwin->w_buffer;
		scroll_cursor_bot(0, FALSE);
		curwin = save_curwin;
		curbuf = curwin->w_buffer;
	    }
	}
	redraw_buf_and_status_later(buffer, VALID);
	channel_need_redraw = TRUE;
    }

    if (save_write_to)
    {
	channel_T *ch;

	/* Find channels reading from this buffer and adjust their
	 * next-to-read line number. */
	buffer->b_write_to_channel = TRUE;
	for (ch = first_channel; ch != NULL; ch = ch->ch_next)
	{
	    chanpart_T  *in_part = &ch->ch_part[PART_IN];

	    if (in_part->ch_bufref.br_buf == buffer)
		in_part->ch_buf_bot = buffer->b_ml.ml_line_count;
	}
    }
}

    static void
drop_messages(channel_T *channel, ch_part_T part)
{
    char_u *msg;

    while ((msg = channel_get(channel, part, NULL)) != NULL)
    {
	ch_log(channel, "Dropping message '%s'", (char *)msg);
	vim_free(msg);
    }
}

/*
 * Invoke a callback for "channel"/"part" if needed.
 * This does not redraw but sets channel_need_redraw when redraw is needed.
 * Return TRUE when a message was handled, there might be another one.
 */
    static int
may_invoke_callback(channel_T *channel, ch_part_T part)
{
    char_u	*msg = NULL;
    typval_T	*listtv = NULL;
    typval_T	argv[CH_JSON_MAX_ARGS];
    int		seq_nr = -1;
    chanpart_T	*ch_part = &channel->ch_part[part];
    ch_mode_T	ch_mode = ch_part->ch_mode;
    cbq_T	*cbhead = &ch_part->ch_cb_head;
    cbq_T	*cbitem;
    callback_T	*callback = NULL;
    buf_T	*buffer = NULL;
    char_u	*p;

    if (channel->ch_nb_close_cb != NULL)
	/* this channel is handled elsewhere (netbeans) */
	return FALSE;

    /* Use a message-specific callback, part callback or channel callback */
    for (cbitem = cbhead->cq_next; cbitem != NULL; cbitem = cbitem->cq_next)
	if (cbitem->cq_seq_nr == 0)
	    break;
    if (cbitem != NULL)
	callback = &cbitem->cq_callback;
    else if (ch_part->ch_callback.cb_name != NULL)
	callback = &ch_part->ch_callback;
    else if (channel->ch_callback.cb_name != NULL)
	callback = &channel->ch_callback;

    buffer = ch_part->ch_bufref.br_buf;
    if (buffer != NULL && (!bufref_valid(&ch_part->ch_bufref)
					       || buffer->b_ml.ml_mfp == NULL))
    {
	/* buffer was wiped out or unloaded */
	ch_log(channel, "%s buffer has been wiped out", part_names[part]);
	ch_part->ch_bufref.br_buf = NULL;
	buffer = NULL;
    }

    if (ch_mode == MODE_JSON || ch_mode == MODE_JS)
    {
	listitem_T	*item;
	int		argc = 0;

	/* Get any json message in the queue. */
	if (channel_get_json(channel, part, -1, FALSE, &listtv) == FAIL)
	{
	    /* Parse readahead, return when there is still no message. */
	    channel_parse_json(channel, part);
	    if (channel_get_json(channel, part, -1, FALSE, &listtv) == FAIL)
		return FALSE;
	}

	for (item = listtv->vval.v_list->lv_first;
			    item != NULL && argc < CH_JSON_MAX_ARGS;
						    item = item->li_next)
	    argv[argc++] = item->li_tv;
	while (argc < CH_JSON_MAX_ARGS)
	    argv[argc++].v_type = VAR_UNKNOWN;

	if (argv[0].v_type == VAR_STRING)
	{
	    /* ["cmd", arg] or ["cmd", arg, arg] or ["cmd", arg, arg, arg] */
	    channel_exe_cmd(channel, part, argv);
	    free_tv(listtv);
	    return TRUE;
	}

	if (argv[0].v_type != VAR_NUMBER)
	{
	    ch_error(channel,
		      "Dropping message with invalid sequence number type");
	    free_tv(listtv);
	    return FALSE;
	}
	seq_nr = argv[0].vval.v_number;
    }
    else if (channel_peek(channel, part) == NULL)
    {
	/* nothing to read on RAW or NL channel */
	return FALSE;
    }
    else
    {
	/* If there is no callback or buffer drop the message. */
	if (callback == NULL && buffer == NULL)
	{
	    /* If there is a close callback it may use ch_read() to get the
	     * messages. */
	    if (channel->ch_close_cb.cb_name == NULL && !channel->ch_drop_never)
		drop_messages(channel, part);
	    return FALSE;
	}

	if (ch_mode == MODE_NL)
	{
	    char_u  *nl = NULL;
	    char_u  *buf;
	    readq_T *node;

	    /* See if we have a message ending in NL in the first buffer.  If
	     * not try to concatenate the first and the second buffer. */
	    while (TRUE)
	    {
		node = channel_peek(channel, part);
		nl = channel_first_nl(node);
		if (nl != NULL)
		    break;
		if (channel_collapse(channel, part, TRUE) == FAIL)
		{
		    if (ch_part->ch_fd == INVALID_FD && node->rq_buflen > 0)
			break;
		    return FALSE; /* incomplete message */
		}
	    }
	    buf = node->rq_buffer;

	    // Convert NUL to NL, the internal representation.
	    for (p = buf; (nl == NULL || p < nl)
					    && p < buf + node->rq_buflen; ++p)
		if (*p == NUL)
		    *p = NL;

	    if (nl == NULL)
	    {
		// get the whole buffer, drop the NL
		msg = channel_get(channel, part, NULL);
	    }
	    else if (nl + 1 == buf + node->rq_buflen)
	    {
		// get the whole buffer
		msg = channel_get(channel, part, NULL);
		*nl = NUL;
	    }
	    else
	    {
		/* Copy the message into allocated memory (excluding the NL)
		 * and remove it from the buffer (including the NL). */
		msg = vim_strnsave(buf, (int)(nl - buf));
		channel_consume(channel, part, (int)(nl - buf) + 1);
	    }
	}
	else
	{
	    /* For a raw channel we don't know where the message ends, just
	     * get everything we have.
	     * Convert NUL to NL, the internal representation. */
	    msg = channel_get_all(channel, part, NULL);
	}

	if (msg == NULL)
	    return FALSE; /* out of memory (and avoids Coverity warning) */

	argv[1].v_type = VAR_STRING;
	argv[1].vval.v_string = msg;
    }

    if (seq_nr > 0)
    {
	int	done = FALSE;

	/* JSON or JS mode: invoke the one-time callback with the matching nr */
	for (cbitem = cbhead->cq_next; cbitem != NULL; cbitem = cbitem->cq_next)
	    if (cbitem->cq_seq_nr == seq_nr)
	    {
		invoke_one_time_callback(channel, cbhead, cbitem, argv);
		done = TRUE;
		break;
	    }
	if (!done)
	{
	    if (channel->ch_drop_never)
	    {
		/* message must be read with ch_read() */
		channel_push_json(channel, part, listtv);
		listtv = NULL;
	    }
	    else
		ch_log(channel, "Dropping message %d without callback",
								       seq_nr);
	}
    }
    else if (callback != NULL || buffer != NULL)
    {
	if (buffer != NULL)
	{
	    if (msg == NULL)
		/* JSON or JS mode: re-encode the message. */
		msg = json_encode(listtv, ch_mode);
	    if (msg != NULL)
	    {
#ifdef FEAT_TERMINAL
		if (buffer->b_term != NULL)
		    write_to_term(buffer, msg, channel);
		else
#endif
		    append_to_buffer(buffer, msg, channel, part);
	    }
	}

	if (callback != NULL)
	{
	    if (cbitem != NULL)
		invoke_one_time_callback(channel, cbhead, cbitem, argv);
	    else
	    {
		/* invoke the channel callback */
		ch_log(channel, "Invoking channel callback %s",
						    (char *)callback->cb_name);
		invoke_callback(channel, callback, argv);
	    }
	}
    }
    else
	ch_log(channel, "Dropping message %d", seq_nr);

    if (listtv != NULL)
	free_tv(listtv);
    vim_free(msg);

    return TRUE;
}

/*
 * Return TRUE when channel "channel" is open for reading or writing.
 * Also returns FALSE for invalid "channel".
 */
    int
channel_is_open(channel_T *channel)
{
    return channel != NULL && (channel->CH_SOCK_FD != INVALID_FD
			  || channel->CH_IN_FD != INVALID_FD
			  || channel->CH_OUT_FD != INVALID_FD
			  || channel->CH_ERR_FD != INVALID_FD);
}

/*
 * Return TRUE if "channel" has JSON or other typeahead.
 */
    int
channel_has_readahead(channel_T *channel, ch_part_T part)
{
    ch_mode_T	ch_mode = channel->ch_part[part].ch_mode;

    if (ch_mode == MODE_JSON || ch_mode == MODE_JS)
    {
	jsonq_T   *head = &channel->ch_part[part].ch_json_head;
	jsonq_T   *item = head->jq_next;

	return item != NULL;
    }
    return channel_peek(channel, part) != NULL;
}

/*
 * Return a string indicating the status of the channel.
 * If "req_part" is not negative check that part.
 */
    char *
channel_status(channel_T *channel, int req_part)
{
    ch_part_T part;
    int has_readahead = FALSE;

    if (channel == NULL)
	 return "fail";
    if (req_part == PART_OUT)
    {
	if (channel->CH_OUT_FD != INVALID_FD)
	    return "open";
	if (channel_has_readahead(channel, PART_OUT))
	    has_readahead = TRUE;
    }
    else if (req_part == PART_ERR)
    {
	if (channel->CH_ERR_FD != INVALID_FD)
	    return "open";
	if (channel_has_readahead(channel, PART_ERR))
	    has_readahead = TRUE;
    }
    else
    {
	if (channel_is_open(channel))
	    return "open";
	for (part = PART_SOCK; part < PART_IN; ++part)
	    if (channel_has_readahead(channel, part))
	    {
		has_readahead = TRUE;
		break;
	    }
    }

    if (has_readahead)
	return "buffered";
    return "closed";
}

    static void
channel_part_info(channel_T *channel, dict_T *dict, char *name, ch_part_T part)
{
    chanpart_T *chanpart = &channel->ch_part[part];
    char	namebuf[20];  /* longest is "sock_timeout" */
    size_t	tail;
    char	*status;
    char	*s = "";

    vim_strncpy((char_u *)namebuf, (char_u *)name, 4);
    STRCAT(namebuf, "_");
    tail = STRLEN(namebuf);

    STRCPY(namebuf + tail, "status");
    if (chanpart->ch_fd != INVALID_FD)
	status = "open";
    else if (channel_has_readahead(channel, part))
	status = "buffered";
    else
	status = "closed";
    dict_add_string(dict, namebuf, (char_u *)status);

    STRCPY(namebuf + tail, "mode");
    switch (chanpart->ch_mode)
    {
	case MODE_NL: s = "NL"; break;
	case MODE_RAW: s = "RAW"; break;
	case MODE_JSON: s = "JSON"; break;
	case MODE_JS: s = "JS"; break;
    }
    dict_add_string(dict, namebuf, (char_u *)s);

    STRCPY(namebuf + tail, "io");
    if (part == PART_SOCK)
	s = "socket";
    else switch (chanpart->ch_io)
    {
	case JIO_NULL: s = "null"; break;
	case JIO_PIPE: s = "pipe"; break;
	case JIO_FILE: s = "file"; break;
	case JIO_BUFFER: s = "buffer"; break;
	case JIO_OUT: s = "out"; break;
    }
    dict_add_string(dict, namebuf, (char_u *)s);

    STRCPY(namebuf + tail, "timeout");
    dict_add_number(dict, namebuf, chanpart->ch_timeout);
}

    void
channel_info(channel_T *channel, dict_T *dict)
{
    dict_add_number(dict, "id", channel->ch_id);
    dict_add_string(dict, "status", (char_u *)channel_status(channel, -1));

    if (channel->ch_hostname != NULL)
    {
	dict_add_string(dict, "hostname", (char_u *)channel->ch_hostname);
	dict_add_number(dict, "port", channel->ch_port);
	channel_part_info(channel, dict, "sock", PART_SOCK);
    }
    else
    {
	channel_part_info(channel, dict, "out", PART_OUT);
	channel_part_info(channel, dict, "err", PART_ERR);
	channel_part_info(channel, dict, "in", PART_IN);
    }
}

/*
 * Close channel "channel".
 * Trigger the close callback if "invoke_close_cb" is TRUE.
 * Does not clear the buffers.
 */
    void
channel_close(channel_T *channel, int invoke_close_cb)
{
    ch_log(channel, "Closing channel");

#ifdef FEAT_GUI
    channel_gui_unregister(channel);
#endif

    ch_close_part(channel, PART_SOCK);
    ch_close_part(channel, PART_IN);
    ch_close_part(channel, PART_OUT);
    ch_close_part(channel, PART_ERR);

    if (invoke_close_cb)
    {
	ch_part_T	part;

	/* Invoke callbacks and flush buffers before the close callback. */
	if (channel->ch_close_cb.cb_name != NULL)
	    ch_log(channel,
		     "Invoking callbacks and flushing buffers before closing");
	for (part = PART_SOCK; part < PART_IN; ++part)
	{
	    if (channel->ch_close_cb.cb_name != NULL
			    || channel->ch_part[part].ch_bufref.br_buf != NULL)
	    {
		/* Increment the refcount to avoid the channel being freed
		 * halfway. */
		++channel->ch_refcount;
		if (channel->ch_close_cb.cb_name == NULL)
		    ch_log(channel, "flushing %s buffers before closing",
							     part_names[part]);
		while (may_invoke_callback(channel, part))
		    ;
		--channel->ch_refcount;
	    }
	}

	if (channel->ch_close_cb.cb_name != NULL)
	{
	      typval_T	argv[1];
	      typval_T	rettv;
	      int		dummy;

	      /* Increment the refcount to avoid the channel being freed
	       * halfway. */
	      ++channel->ch_refcount;
	      ch_log(channel, "Invoking close callback %s",
					 (char *)channel->ch_close_cb.cb_name);
	      argv[0].v_type = VAR_CHANNEL;
	      argv[0].vval.v_channel = channel;
	      call_callback(&channel->ch_close_cb, -1,
			   &rettv, 1, argv, NULL, 0L, 0L, &dummy, TRUE, NULL);
	      clear_tv(&rettv);
	      channel_need_redraw = TRUE;

	      /* the callback is only called once */
	      free_callback(&channel->ch_close_cb);

	      if (channel_need_redraw)
	      {
		  channel_need_redraw = FALSE;
		  redraw_after_callback(TRUE);
	      }

	      if (!channel->ch_drop_never)
		  /* any remaining messages are useless now */
		  for (part = PART_SOCK; part < PART_IN; ++part)
		      drop_messages(channel, part);

	      --channel->ch_refcount;
	}
    }

    channel->ch_nb_close_cb = NULL;

#ifdef FEAT_TERMINAL
    term_channel_closed(channel);
#endif
}

/*
 * Close the "in" part channel "channel".
 */
    void
channel_close_in(channel_T *channel)
{
    ch_close_part(channel, PART_IN);
}

    static void
remove_from_writeque(writeq_T *wq, writeq_T *entry)
{
    ga_clear(&entry->wq_ga);
    wq->wq_next = entry->wq_next;
    if (wq->wq_next == NULL)
	wq->wq_prev = NULL;
    else
	wq->wq_next->wq_prev = NULL;
    vim_free(entry);
}

/*
 * Clear the read buffer on "channel"/"part".
 */
    static void
channel_clear_one(channel_T *channel, ch_part_T part)
{
    chanpart_T *ch_part = &channel->ch_part[part];
    jsonq_T *json_head = &ch_part->ch_json_head;
    cbq_T   *cb_head = &ch_part->ch_cb_head;

    while (channel_peek(channel, part) != NULL)
	vim_free(channel_get(channel, part, NULL));

    while (cb_head->cq_next != NULL)
    {
	cbq_T *node = cb_head->cq_next;

	remove_cb_node(cb_head, node);
	free_callback(&node->cq_callback);
	vim_free(node);
    }

    while (json_head->jq_next != NULL)
    {
	free_tv(json_head->jq_next->jq_value);
	remove_json_node(json_head, json_head->jq_next);
    }

    free_callback(&ch_part->ch_callback);

    while (ch_part->ch_writeque.wq_next != NULL)
	remove_from_writeque(&ch_part->ch_writeque,
						 ch_part->ch_writeque.wq_next);
}

/*
 * Clear all the read buffers on "channel".
 */
    void
channel_clear(channel_T *channel)
{
    ch_log(channel, "Clearing channel");
    VIM_CLEAR(channel->ch_hostname);
    channel_clear_one(channel, PART_SOCK);
    channel_clear_one(channel, PART_OUT);
    channel_clear_one(channel, PART_ERR);
    channel_clear_one(channel, PART_IN);
    free_callback(&channel->ch_callback);
    free_callback(&channel->ch_close_cb);
}

#if defined(EXITFREE) || defined(PROTO)
    void
channel_free_all(void)
{
    channel_T *channel;

    ch_log(NULL, "channel_free_all()");
    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	channel_clear(channel);
}
#endif


/* Sent when the netbeans channel is found closed when reading. */
#define DETACH_MSG_RAW "DETACH\n"

/* Buffer size for reading incoming messages. */
#define MAXMSGSIZE 4096

#if defined(HAVE_SELECT)
/*
 * Add write fds where we are waiting for writing to be possible.
 */
    static int
channel_fill_wfds(int maxfd_arg, fd_set *wfds)
{
    int		maxfd = maxfd_arg;
    channel_T	*ch;

    for (ch = first_channel; ch != NULL; ch = ch->ch_next)
    {
	chanpart_T  *in_part = &ch->ch_part[PART_IN];

	if (in_part->ch_fd != INVALID_FD
		&& (in_part->ch_bufref.br_buf != NULL
		    || in_part->ch_writeque.wq_next != NULL))
	{
	    FD_SET((int)in_part->ch_fd, wfds);
	    if ((int)in_part->ch_fd >= maxfd)
		maxfd = (int)in_part->ch_fd + 1;
	}
    }
    return maxfd;
}
#else
/*
 * Add write fds where we are waiting for writing to be possible.
 */
    static int
channel_fill_poll_write(int nfd_in, struct pollfd *fds)
{
    int		nfd = nfd_in;
    channel_T	*ch;

    for (ch = first_channel; ch != NULL; ch = ch->ch_next)
    {
	chanpart_T  *in_part = &ch->ch_part[PART_IN];

	if (in_part->ch_fd != INVALID_FD
		&& (in_part->ch_bufref.br_buf != NULL
		    || in_part->ch_writeque.wq_next != NULL))
	{
	    in_part->ch_poll_idx = nfd;
	    fds[nfd].fd = in_part->ch_fd;
	    fds[nfd].events = POLLOUT;
	    ++nfd;
	}
	else
	    in_part->ch_poll_idx = -1;
    }
    return nfd;
}
#endif

typedef enum {
    CW_READY,
    CW_NOT_READY,
    CW_ERROR
} channel_wait_result;

/*
 * Check for reading from "fd" with "timeout" msec.
 * Return CW_READY when there is something to read.
 * Return CW_NOT_READY when there is nothing to read.
 * Return CW_ERROR when there is an error.
 */
    static channel_wait_result
channel_wait(channel_T *channel, sock_T fd, int timeout)
{
    if (timeout > 0)
	ch_log(channel, "Waiting for up to %d msec", timeout);

# ifdef MSWIN
    if (fd != channel->CH_SOCK_FD)
    {
	DWORD	nread;
	int	sleep_time;
	DWORD	deadline = GetTickCount() + timeout;
	int	delay = 1;

	/* reading from a pipe, not a socket */
	while (TRUE)
	{
	    int r = PeekNamedPipe((HANDLE)fd, NULL, 0, NULL, &nread, NULL);

	    if (r && nread > 0)
		return CW_READY;

	    if (channel->ch_named_pipe)
	    {
		DisconnectNamedPipe((HANDLE)fd);
		ConnectNamedPipe((HANDLE)fd, NULL);
	    }
	    else if (r == 0)
		return CW_ERROR;

	    /* perhaps write some buffer lines */
	    channel_write_any_lines();

	    sleep_time = deadline - GetTickCount();
	    if (sleep_time <= 0)
		break;
	    /* Wait for a little while.  Very short at first, up to 10 msec
	     * after looping a few times. */
	    if (sleep_time > delay)
		sleep_time = delay;
	    Sleep(sleep_time);
	    delay = delay * 2;
	    if (delay > 10)
		delay = 10;
	}
    }
    else
#endif
    {
#if defined(HAVE_SELECT)
	struct timeval	tval;
	fd_set		rfds;
	fd_set		wfds;
	int		ret;
	int		maxfd;

	tval.tv_sec = timeout / 1000;
	tval.tv_usec = (timeout % 1000) * 1000;
	for (;;)
	{
	    FD_ZERO(&rfds);
	    FD_SET((int)fd, &rfds);

	    /* Write lines to a pipe when a pipe can be written to.  Need to
	     * set this every time, some buffers may be done. */
	    maxfd = (int)fd + 1;
	    FD_ZERO(&wfds);
	    maxfd = channel_fill_wfds(maxfd, &wfds);

	    ret = select(maxfd, &rfds, &wfds, NULL, &tval);
# ifdef EINTR
	    SOCK_ERRNO;
	    if (ret == -1 && errno == EINTR)
		continue;
# endif
	    if (ret > 0)
	    {
		if (FD_ISSET(fd, &rfds))
		    return CW_READY;
		channel_write_any_lines();
		continue;
	    }
	    break;
	}
#else
	for (;;)
	{
	    struct pollfd   fds[MAX_OPEN_CHANNELS + 1];
	    int		    nfd = 1;

	    fds[0].fd = fd;
	    fds[0].events = POLLIN;
	    nfd = channel_fill_poll_write(nfd, fds);
	    if (poll(fds, nfd, timeout) > 0)
	    {
		if (fds[0].revents & POLLIN)
		    return CW_READY;
		channel_write_any_lines();
		continue;
	    }
	    break;
	}
#endif
    }
    return CW_NOT_READY;
}

    static void
ch_close_part_on_error(
	channel_T *channel, ch_part_T part, int is_err, char *func)
{
    char	msg[] = "%s(): Read %s from ch_part[%d], closing";

    if (is_err)
	/* Do not call emsg(), most likely the other end just exited. */
	ch_error(channel, msg, func, "error", part);
    else
	ch_log(channel, msg, func, "EOF", part);

    /* Queue a "DETACH" netbeans message in the command queue in order to
     * terminate the netbeans session later. Do not end the session here
     * directly as we may be running in the context of a call to
     * netbeans_parse_messages():
     *	netbeans_parse_messages
     *	    -> autocmd triggered while processing the netbeans cmd
     *		-> ui_breakcheck
     *		    -> gui event loop or select loop
     *			-> channel_read()
     * Only send "DETACH" for a netbeans channel.
     */
    if (channel->ch_nb_close_cb != NULL)
	channel_save(channel, PART_SOCK, (char_u *)DETACH_MSG_RAW,
			      (int)STRLEN(DETACH_MSG_RAW), FALSE, "PUT ");

    /* When reading is not possible close this part of the channel.  Don't
     * close the channel yet, there may be something to read on another part.
     * When stdout and stderr use the same FD we get the error only on one of
     * them, also close the other. */
    if (part == PART_OUT || part == PART_ERR)
    {
	ch_part_T other = part == PART_OUT ? PART_ERR : PART_OUT;

	if (channel->ch_part[part].ch_fd == channel->ch_part[other].ch_fd)
	    ch_close_part(channel, other);
    }
    ch_close_part(channel, part);

#ifdef FEAT_GUI
    /* Stop listening to GUI events right away. */
    channel_gui_unregister_one(channel, part);
#endif
}

    static void
channel_close_now(channel_T *channel)
{
    ch_log(channel, "Closing channel because all readable fds are closed");
    if (channel->ch_nb_close_cb != NULL)
	(*channel->ch_nb_close_cb)();
    channel_close(channel, TRUE);
}

/*
 * Read from channel "channel" for as long as there is something to read.
 * "part" is PART_SOCK, PART_OUT or PART_ERR.
 * The data is put in the read queue.  No callbacks are invoked here.
 */
    static void
channel_read(channel_T *channel, ch_part_T part, char *func)
{
    static char_u	*buf = NULL;
    int			len = 0;
    int			readlen = 0;
    sock_T		fd;
    int			use_socket = FALSE;

    fd = channel->ch_part[part].ch_fd;
    if (fd == INVALID_FD)
    {
	ch_error(channel, "channel_read() called while %s part is closed",
							    part_names[part]);
	return;
    }
    use_socket = fd == channel->CH_SOCK_FD;

    /* Allocate a buffer to read into. */
    if (buf == NULL)
    {
	buf = alloc(MAXMSGSIZE);
	if (buf == NULL)
	    return;	/* out of memory! */
    }

    /* Keep on reading for as long as there is something to read.
     * Use select() or poll() to avoid blocking on a message that is exactly
     * MAXMSGSIZE long. */
    for (;;)
    {
	if (channel_wait(channel, fd, 0) != CW_READY)
	    break;
	if (use_socket)
	    len = sock_read(fd, (char *)buf, MAXMSGSIZE);
	else
	    len = fd_read(fd, (char *)buf, MAXMSGSIZE);
	if (len <= 0)
	    break;	/* error or nothing more to read */

	/* Store the read message in the queue. */
	channel_save(channel, part, buf, len, FALSE, "RECV ");
	readlen += len;
	if (len < MAXMSGSIZE)
	    break;	/* did read everything that's available */
    }

    /* Reading a disconnection (readlen == 0), or an error. */
    if (readlen <= 0)
    {
	if (!channel->ch_keep_open)
	    ch_close_part_on_error(channel, part, (len < 0), func);
    }
#if defined(CH_HAS_GUI) && defined(FEAT_GUI_GTK)
    else if (CH_HAS_GUI && gtk_main_level() > 0)
	/* signal the main loop that there is something to read */
	gtk_main_quit();
#endif
}

/*
 * Read from RAW or NL "channel"/"part".  Blocks until there is something to
 * read or the timeout expires.
 * When "raw" is TRUE don't block waiting on a NL.
 * Returns what was read in allocated memory.
 * Returns NULL in case of error or timeout.
 */
    static char_u *
channel_read_block(
	channel_T *channel, ch_part_T part, int timeout, int raw, int *outlen)
{
    char_u	*buf;
    char_u	*msg;
    ch_mode_T	mode = channel->ch_part[part].ch_mode;
    sock_T	fd = channel->ch_part[part].ch_fd;
    char_u	*nl;
    readq_T	*node;

    ch_log(channel, "Blocking %s read, timeout: %d msec",
				     mode == MODE_RAW ? "RAW" : "NL", timeout);

    while (TRUE)
    {
	node = channel_peek(channel, part);
	if (node != NULL)
	{
	    if (mode == MODE_RAW || (mode == MODE_NL
					   && channel_first_nl(node) != NULL))
		/* got a complete message */
		break;
	    if (channel_collapse(channel, part, mode == MODE_NL) == OK)
		continue;
	    /* If not blocking or nothing more is coming then return what we
	     * have. */
	    if (raw || fd == INVALID_FD)
		break;
	}

	/* Wait for up to the channel timeout. */
	if (fd == INVALID_FD)
	    return NULL;
	if (channel_wait(channel, fd, timeout) != CW_READY)
	{
	    ch_log(channel, "Timed out");
	    return NULL;
	}
	channel_read(channel, part, "channel_read_block");
    }

    /* We have a complete message now. */
    if (mode == MODE_RAW || outlen != NULL)
    {
	msg = channel_get_all(channel, part, outlen);
    }
    else
    {
	char_u *p;

	buf = node->rq_buffer;
	nl = channel_first_nl(node);

	/* Convert NUL to NL, the internal representation. */
	for (p = buf; (nl == NULL || p < nl) && p < buf + node->rq_buflen; ++p)
	    if (*p == NUL)
		*p = NL;

	if (nl == NULL)
	{
	    /* must be a closed channel with missing NL */
	    msg = channel_get(channel, part, NULL);
	}
	else if (nl + 1 == buf + node->rq_buflen)
	{
	    /* get the whole buffer */
	    msg = channel_get(channel, part, NULL);
	    *nl = NUL;
	}
	else
	{
	    /* Copy the message into allocated memory and remove it from the
	     * buffer. */
	    msg = vim_strnsave(buf, (int)(nl - buf));
	    channel_consume(channel, part, (int)(nl - buf) + 1);
	}
    }
    if (ch_log_active())
	ch_log(channel, "Returning %d bytes", (int)STRLEN(msg));
    return msg;
}

/*
 * Read one JSON message with ID "id" from "channel"/"part" and store the
 * result in "rettv".
 * When "id" is -1 accept any message;
 * Blocks until the message is received or the timeout is reached.
 */
    static int
channel_read_json_block(
	channel_T   *channel,
	ch_part_T   part,
	int	    timeout_arg,
	int	    id,
	typval_T    **rettv)
{
    int		more;
    sock_T	fd;
    int		timeout;
    chanpart_T	*chanpart = &channel->ch_part[part];

    ch_log(channel, "Reading JSON");
    if (id != -1)
	chanpart->ch_block_id = id;
    for (;;)
    {
	more = channel_parse_json(channel, part);

	/* search for message "id" */
	if (channel_get_json(channel, part, id, TRUE, rettv) == OK)
	{
	    chanpart->ch_block_id = 0;
	    return OK;
	}

	if (!more)
	{
	    /* Handle any other messages in the queue.  If done some more
	     * messages may have arrived. */
	    if (channel_parse_messages())
		continue;

	    /* Wait for up to the timeout.  If there was an incomplete message
	     * use the deadline for that. */
	    timeout = timeout_arg;
	    if (chanpart->ch_wait_len > 0)
	    {
#ifdef MSWIN
		timeout = chanpart->ch_deadline - GetTickCount() + 1;
#else
		{
		    struct timeval now_tv;

		    gettimeofday(&now_tv, NULL);
		    timeout = (chanpart->ch_deadline.tv_sec
						       - now_tv.tv_sec) * 1000
			+ (chanpart->ch_deadline.tv_usec
						     - now_tv.tv_usec) / 1000
			+ 1;
		}
#endif
		if (timeout < 0)
		{
		    /* Something went wrong, channel_parse_json() didn't
		     * discard message.  Cancel waiting. */
		    chanpart->ch_wait_len = 0;
		    timeout = timeout_arg;
		}
		else if (timeout > timeout_arg)
		    timeout = timeout_arg;
	    }
	    fd = chanpart->ch_fd;
	    if (fd == INVALID_FD
			    || channel_wait(channel, fd, timeout) != CW_READY)
	    {
		if (timeout == timeout_arg)
		{
		    if (fd != INVALID_FD)
			ch_log(channel, "Timed out");
		    break;
		}
	    }
	    else
		channel_read(channel, part, "channel_read_json_block");
	}
    }
    chanpart->ch_block_id = 0;
    return FAIL;
}

/*
 * Common for ch_read() and ch_readraw().
 */
    void
common_channel_read(typval_T *argvars, typval_T *rettv, int raw, int blob)
{
    channel_T	*channel;
    ch_part_T	part = PART_COUNT;
    jobopt_T	opt;
    int		mode;
    int		timeout;
    int		id = -1;
    typval_T	*listtv = NULL;

    /* return an empty string by default */
    rettv->v_type = VAR_STRING;
    rettv->vval.v_string = NULL;

    clear_job_options(&opt);
    if (get_job_options(&argvars[1], &opt, JO_TIMEOUT + JO_PART + JO_ID, 0)
								      == FAIL)
	goto theend;

    if (opt.jo_set & JO_PART)
	part = opt.jo_part;
    channel = get_channel_arg(&argvars[0], TRUE, TRUE, part);
    if (channel != NULL)
    {
	if (part == PART_COUNT)
	    part = channel_part_read(channel);
	mode = channel_get_mode(channel, part);
	timeout = channel_get_timeout(channel, part);
	if (opt.jo_set & JO_TIMEOUT)
	    timeout = opt.jo_timeout;

	if (blob)
	{
	    int	    outlen = 0;
	    char_u  *p = channel_read_block(channel, part,
						       timeout, TRUE, &outlen);
	    if (p != NULL)
	    {
		blob_T	*b = blob_alloc();

		if (b != NULL)
		{
		    b->bv_ga.ga_len = outlen;
		    if (ga_grow(&b->bv_ga, outlen) == FAIL)
			blob_free(b);
		    else
		    {
			memcpy(b->bv_ga.ga_data, p, outlen);
			rettv_blob_set(rettv, b);
		    }
		}
		vim_free(p);
	    }
	}
	else if (raw || mode == MODE_RAW || mode == MODE_NL)
	    rettv->vval.v_string = channel_read_block(channel, part,
							 timeout, raw, NULL);
	else
	{
	    if (opt.jo_set & JO_ID)
		id = opt.jo_id;
	    channel_read_json_block(channel, part, timeout, id, &listtv);
	    if (listtv != NULL)
	    {
		*rettv = *listtv;
		vim_free(listtv);
	    }
	    else
	    {
		rettv->v_type = VAR_SPECIAL;
		rettv->vval.v_number = VVAL_NONE;
	    }
	}
    }

theend:
    free_job_options(&opt);
}

# if defined(MSWIN) || defined(FEAT_GUI_X11) || defined(FEAT_GUI_GTK) \
	|| defined(PROTO)
/*
 * Lookup the channel from the socket.  Set "partp" to the fd index.
 * Returns NULL when the socket isn't found.
 */
    channel_T *
channel_fd2channel(sock_T fd, ch_part_T *partp)
{
    channel_T	*channel;
    ch_part_T	part;

    if (fd != INVALID_FD)
	for (channel = first_channel; channel != NULL;
						   channel = channel->ch_next)
	{
	    for (part = PART_SOCK; part < PART_IN; ++part)
		if (channel->ch_part[part].ch_fd == fd)
		{
		    *partp = part;
		    return channel;
		}
	}
    return NULL;
}
# endif

# if defined(MSWIN) || defined(FEAT_GUI) || defined(PROTO)
/*
 * Check the channels for anything that is ready to be read.
 * The data is put in the read queue.
 * if "only_keep_open" is TRUE only check channels where ch_keep_open is set.
 */
    void
channel_handle_events(int only_keep_open)
{
    channel_T	*channel;
    ch_part_T	part;
    sock_T	fd;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	if (only_keep_open && !channel->ch_keep_open)
	    continue;

	/* check the socket and pipes */
	for (part = PART_SOCK; part < PART_IN; ++part)
	{
	    fd = channel->ch_part[part].ch_fd;
	    if (fd != INVALID_FD)
	    {
		int r = channel_wait(channel, fd, 0);

		if (r == CW_READY)
		    channel_read(channel, part, "channel_handle_events");
		else if (r == CW_ERROR)
		    ch_close_part_on_error(channel, part, TRUE,
						     "channel_handle_events");
	    }
	}
    }
}
# endif

# if defined(FEAT_GUI) || defined(PROTO)
/*
 * Return TRUE when there is any channel with a keep_open flag.
 */
    int
channel_any_keep_open()
{
    channel_T	*channel;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	if (channel->ch_keep_open)
	    return TRUE;
    return FALSE;
}
# endif

/*
 * Set "channel"/"part" to non-blocking.
 * Only works for sockets and pipes.
 */
    void
channel_set_nonblock(channel_T *channel, ch_part_T part)
{
    chanpart_T *ch_part = &channel->ch_part[part];
    int		fd = ch_part->ch_fd;

    if (fd != INVALID_FD)
    {
#ifdef MSWIN
	u_long	val = 1;

	ioctlsocket(fd, FIONBIO, &val);
#else
	(void)fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
	ch_part->ch_nonblocking = TRUE;
    }
}

/*
 * Write "buf" (NUL terminated string) to "channel"/"part".
 * When "fun" is not NULL an error message might be given.
 * Return FAIL or OK.
 */
    int
channel_send(
	channel_T *channel,
	ch_part_T part,
	char_u	  *buf_arg,
	int	  len_arg,
	char	  *fun)
{
    int		res;
    sock_T	fd;
    chanpart_T	*ch_part = &channel->ch_part[part];
    int		did_use_queue = FALSE;

    fd = ch_part->ch_fd;
    if (fd == INVALID_FD)
    {
	if (!channel->ch_error && fun != NULL)
	{
	    ch_error(channel, "%s(): write while not connected", fun);
	    semsg(_("E630: %s(): write while not connected"), fun);
	}
	channel->ch_error = TRUE;
	return FAIL;
    }

    if (channel->ch_nonblock && !ch_part->ch_nonblocking)
	channel_set_nonblock(channel, part);

    if (ch_log_active())
    {
	ch_log_lead("SEND ", channel, part);
	fprintf(log_fd, "'");
	vim_ignored = (int)fwrite(buf_arg, len_arg, 1, log_fd);
	fprintf(log_fd, "'\n");
	fflush(log_fd);
	did_log_msg = TRUE;
    }

    for (;;)
    {
	writeq_T    *wq = &ch_part->ch_writeque;
	char_u	    *buf;
	int	    len;

	if (wq->wq_next != NULL)
	{
	    /* first write what was queued */
	    buf = wq->wq_next->wq_ga.ga_data;
	    len = wq->wq_next->wq_ga.ga_len;
	    did_use_queue = TRUE;
	}
	else
	{
	    if (len_arg == 0)
		/* nothing to write, called from channel_select_check() */
		return OK;
	    buf = buf_arg;
	    len = len_arg;
	}

	if (part == PART_SOCK)
	    res = sock_write(fd, (char *)buf, len);
	else
	{
	    res = fd_write(fd, (char *)buf, len);
#ifdef MSWIN
	    if (channel->ch_named_pipe && res < 0)
	    {
		DisconnectNamedPipe((HANDLE)fd);
		ConnectNamedPipe((HANDLE)fd, NULL);
	    }
#endif
	}
	if (res < 0 && (errno == EWOULDBLOCK
#ifdef EAGAIN
			|| errno == EAGAIN
#endif
		    ))
	    res = 0; /* nothing got written */

	if (res >= 0 && ch_part->ch_nonblocking)
	{
	    writeq_T *entry = wq->wq_next;

	    if (did_use_queue)
		ch_log(channel, "Sent %d bytes now", res);
	    if (res == len)
	    {
		/* Wrote all the buf[len] bytes. */
		if (entry != NULL)
		{
		    /* Remove the entry from the write queue. */
		    remove_from_writeque(wq, entry);
		    continue;
		}
		if (did_use_queue)
		    ch_log(channel, "Write queue empty");
	    }
	    else
	    {
		/* Wrote only buf[res] bytes, can't write more now. */
		if (entry != NULL)
		{
		    if (res > 0)
		    {
			/* Remove the bytes that were written. */
			mch_memmove(entry->wq_ga.ga_data,
				    (char *)entry->wq_ga.ga_data + res,
				    len - res);
			entry->wq_ga.ga_len -= res;
		    }
		    buf = buf_arg;
		    len = len_arg;
		}
		else
		{
		    buf += res;
		    len -= res;
		}
		ch_log(channel, "Adding %d bytes to the write queue", len);

		/* Append the not written bytes of the argument to the write
		 * buffer.  Limit entries to 4000 bytes. */
		if (wq->wq_prev != NULL
			&& wq->wq_prev->wq_ga.ga_len + len < 4000)
		{
		    writeq_T *last = wq->wq_prev;

		    /* append to the last entry */
		    if (ga_grow(&last->wq_ga, len) == OK)
		    {
			mch_memmove((char *)last->wq_ga.ga_data
							  + last->wq_ga.ga_len,
				    buf, len);
			last->wq_ga.ga_len += len;
		    }
		}
		else
		{
		    writeq_T *last = ALLOC_ONE(writeq_T);

		    if (last != NULL)
		    {
			last->wq_prev = wq->wq_prev;
			last->wq_next = NULL;
			if (wq->wq_prev == NULL)
			    wq->wq_next = last;
			else
			    wq->wq_prev->wq_next = last;
			wq->wq_prev = last;
			ga_init2(&last->wq_ga, 1, 1000);
			if (ga_grow(&last->wq_ga, len) == OK)
			{
			    mch_memmove(last->wq_ga.ga_data, buf, len);
			    last->wq_ga.ga_len = len;
			}
		    }
		}
	    }
	}
	else if (res != len)
	{
	    if (!channel->ch_error && fun != NULL)
	    {
		ch_error(channel, "%s(): write failed", fun);
		semsg(_("E631: %s(): write failed"), fun);
	    }
	    channel->ch_error = TRUE;
	    return FAIL;
	}

	channel->ch_error = FALSE;
	return OK;
    }
}

/*
 * Common for "ch_sendexpr()" and "ch_sendraw()".
 * Returns the channel if the caller should read the response.
 * Sets "part_read" to the read fd.
 * Otherwise returns NULL.
 */
    static channel_T *
send_common(
	typval_T    *argvars,
	char_u	    *text,
	int	    len,
	int	    id,
	int	    eval,
	jobopt_T    *opt,
	char	    *fun,
	ch_part_T   *part_read)
{
    channel_T	*channel;
    ch_part_T	part_send;

    clear_job_options(opt);
    channel = get_channel_arg(&argvars[0], TRUE, FALSE, 0);
    if (channel == NULL)
	return NULL;
    part_send = channel_part_send(channel);
    *part_read = channel_part_read(channel);

    if (get_job_options(&argvars[2], opt, JO_CALLBACK + JO_TIMEOUT, 0) == FAIL)
	return NULL;

    /* Set the callback. An empty callback means no callback and not reading
     * the response. With "ch_evalexpr()" and "ch_evalraw()" a callback is not
     * allowed. */
    if (opt->jo_callback.cb_name != NULL && *opt->jo_callback.cb_name != NUL)
    {
	if (eval)
	{
	    semsg(_("E917: Cannot use a callback with %s()"), fun);
	    return NULL;
	}
	channel_set_req_callback(channel, *part_read, &opt->jo_callback, id);
    }

    if (channel_send(channel, part_send, text, len, fun) == OK
					   && opt->jo_callback.cb_name == NULL)
	return channel;
    return NULL;
}

/*
 * common for "ch_evalexpr()" and "ch_sendexpr()"
 */
    void
ch_expr_common(typval_T *argvars, typval_T *rettv, int eval)
{
    char_u	*text;
    typval_T	*listtv;
    channel_T	*channel;
    int		id;
    ch_mode_T	ch_mode;
    ch_part_T	part_send;
    ch_part_T	part_read;
    jobopt_T    opt;
    int		timeout;

    /* return an empty string by default */
    rettv->v_type = VAR_STRING;
    rettv->vval.v_string = NULL;

    channel = get_channel_arg(&argvars[0], TRUE, FALSE, 0);
    if (channel == NULL)
	return;
    part_send = channel_part_send(channel);

    ch_mode = channel_get_mode(channel, part_send);
    if (ch_mode == MODE_RAW || ch_mode == MODE_NL)
    {
	emsg(_("E912: cannot use ch_evalexpr()/ch_sendexpr() with a raw or nl channel"));
	return;
    }

    id = ++channel->ch_last_msg_id;
    text = json_encode_nr_expr(id, &argvars[1],
				 (ch_mode == MODE_JS ? JSON_JS : 0) | JSON_NL);
    if (text == NULL)
	return;

    channel = send_common(argvars, text, (int)STRLEN(text), id, eval, &opt,
			    eval ? "ch_evalexpr" : "ch_sendexpr", &part_read);
    vim_free(text);
    if (channel != NULL && eval)
    {
	if (opt.jo_set & JO_TIMEOUT)
	    timeout = opt.jo_timeout;
	else
	    timeout = channel_get_timeout(channel, part_read);
	if (channel_read_json_block(channel, part_read, timeout, id, &listtv)
									== OK)
	{
	    list_T *list = listtv->vval.v_list;

	    /* Move the item from the list and then change the type to
	     * avoid the value being freed. */
	    *rettv = list->lv_last->li_tv;
	    list->lv_last->li_tv.v_type = VAR_NUMBER;
	    free_tv(listtv);
	}
    }
    free_job_options(&opt);
}

/*
 * common for "ch_evalraw()" and "ch_sendraw()"
 */
    void
ch_raw_common(typval_T *argvars, typval_T *rettv, int eval)
{
    char_u	buf[NUMBUFLEN];
    char_u	*text;
    int		len;
    channel_T	*channel;
    ch_part_T	part_read;
    jobopt_T    opt;
    int		timeout;

    /* return an empty string by default */
    rettv->v_type = VAR_STRING;
    rettv->vval.v_string = NULL;

    if (argvars[1].v_type == VAR_BLOB)
    {
	text = argvars[1].vval.v_blob->bv_ga.ga_data;
	len = argvars[1].vval.v_blob->bv_ga.ga_len;
    }
    else
    {
	text = tv_get_string_buf(&argvars[1], buf);
	len = (int)STRLEN(text);
    }
    channel = send_common(argvars, text, len, 0, eval, &opt,
			      eval ? "ch_evalraw" : "ch_sendraw", &part_read);
    if (channel != NULL && eval)
    {
	if (opt.jo_set & JO_TIMEOUT)
	    timeout = opt.jo_timeout;
	else
	    timeout = channel_get_timeout(channel, part_read);
	rettv->vval.v_string = channel_read_block(channel, part_read,
							timeout, TRUE, NULL);
    }
    free_job_options(&opt);
}

# define KEEP_OPEN_TIME 20  /* msec */

# if (defined(UNIX) && !defined(HAVE_SELECT)) || defined(PROTO)
/*
 * Add open channels to the poll struct.
 * Return the adjusted struct index.
 * The type of "fds" is hidden to avoid problems with the function proto.
 */
    int
channel_poll_setup(int nfd_in, void *fds_in, int *towait)
{
    int		nfd = nfd_in;
    channel_T	*channel;
    struct	pollfd *fds = fds_in;
    ch_part_T	part;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	for (part = PART_SOCK; part < PART_IN; ++part)
	{
	    chanpart_T	*ch_part = &channel->ch_part[part];

	    if (ch_part->ch_fd != INVALID_FD)
	    {
		if (channel->ch_keep_open)
		{
		    /* For unknown reason poll() returns immediately for a
		     * keep-open channel.  Instead of adding it to the fds add
		     * a short timeout and check, like polling. */
		    if (*towait < 0 || *towait > KEEP_OPEN_TIME)
			*towait = KEEP_OPEN_TIME;
		}
		else
		{
		    ch_part->ch_poll_idx = nfd;
		    fds[nfd].fd = ch_part->ch_fd;
		    fds[nfd].events = POLLIN;
		    nfd++;
		}
	    }
	    else
		channel->ch_part[part].ch_poll_idx = -1;
	}
    }

    nfd = channel_fill_poll_write(nfd, fds);

    return nfd;
}

/*
 * The type of "fds" is hidden to avoid problems with the function proto.
 */
    int
channel_poll_check(int ret_in, void *fds_in)
{
    int		ret = ret_in;
    channel_T	*channel;
    struct	pollfd *fds = fds_in;
    ch_part_T	part;
    int		idx;
    chanpart_T	*in_part;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	for (part = PART_SOCK; part < PART_IN; ++part)
	{
	    idx = channel->ch_part[part].ch_poll_idx;

	    if (ret > 0 && idx != -1 && (fds[idx].revents & POLLIN))
	    {
		channel_read(channel, part, "channel_poll_check");
		--ret;
	    }
	    else if (channel->ch_part[part].ch_fd != INVALID_FD
						      && channel->ch_keep_open)
	    {
		/* polling a keep-open channel */
		channel_read(channel, part, "channel_poll_check_keep_open");
	    }
	}

	in_part = &channel->ch_part[PART_IN];
	idx = in_part->ch_poll_idx;
	if (ret > 0 && idx != -1 && (fds[idx].revents & POLLOUT))
	{
	    channel_write_input(channel);
	    --ret;
	}
    }

    return ret;
}
# endif /* UNIX && !HAVE_SELECT */

# if (!defined(MSWIN) && defined(HAVE_SELECT)) || defined(PROTO)

/*
 * The "fd_set" type is hidden to avoid problems with the function proto.
 */
    int
channel_select_setup(
	int maxfd_in,
	void *rfds_in,
	void *wfds_in,
	struct timeval *tv,
	struct timeval **tvp)
{
    int		maxfd = maxfd_in;
    channel_T	*channel;
    fd_set	*rfds = rfds_in;
    fd_set	*wfds = wfds_in;
    ch_part_T	part;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	for (part = PART_SOCK; part < PART_IN; ++part)
	{
	    sock_T fd = channel->ch_part[part].ch_fd;

	    if (fd != INVALID_FD)
	    {
		if (channel->ch_keep_open)
		{
		    /* For unknown reason select() returns immediately for a
		     * keep-open channel.  Instead of adding it to the rfds add
		     * a short timeout and check, like polling. */
		    if (*tvp == NULL || tv->tv_sec > 0
					|| tv->tv_usec > KEEP_OPEN_TIME * 1000)
		    {
			*tvp = tv;
			tv->tv_sec = 0;
			tv->tv_usec = KEEP_OPEN_TIME * 1000;
		    }
		}
		else
		{
		    FD_SET((int)fd, rfds);
		    if (maxfd < (int)fd)
			maxfd = (int)fd;
		}
	    }
	}
    }

    maxfd = channel_fill_wfds(maxfd, wfds);

    return maxfd;
}

/*
 * The "fd_set" type is hidden to avoid problems with the function proto.
 */
    int
channel_select_check(int ret_in, void *rfds_in, void *wfds_in)
{
    int		ret = ret_in;
    channel_T	*channel;
    fd_set	*rfds = rfds_in;
    fd_set	*wfds = wfds_in;
    ch_part_T	part;
    chanpart_T	*in_part;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	for (part = PART_SOCK; part < PART_IN; ++part)
	{
	    sock_T fd = channel->ch_part[part].ch_fd;

	    if (ret > 0 && fd != INVALID_FD && FD_ISSET(fd, rfds))
	    {
		channel_read(channel, part, "channel_select_check");
		FD_CLR(fd, rfds);
		--ret;
	    }
	    else if (fd != INVALID_FD && channel->ch_keep_open)
	    {
		/* polling a keep-open channel */
		channel_read(channel, part, "channel_select_check_keep_open");
	    }
	}

	in_part = &channel->ch_part[PART_IN];
	if (ret > 0 && in_part->ch_fd != INVALID_FD
					    && FD_ISSET(in_part->ch_fd, wfds))
	{
	    /* Clear the flag first, ch_fd may change in channel_write_input(). */
	    FD_CLR(in_part->ch_fd, wfds);
	    channel_write_input(channel);
	    --ret;
	}
    }

    return ret;
}
# endif /* !MSWIN && HAVE_SELECT */

/*
 * Execute queued up commands.
 * Invoked from the main loop when it's safe to execute received commands.
 * Return TRUE when something was done.
 */
    int
channel_parse_messages(void)
{
    channel_T	*channel = first_channel;
    int		ret = FALSE;
    int		r;
    ch_part_T	part = PART_SOCK;
#ifdef ELAPSED_FUNC
    elapsed_T	start_tv;

    ELAPSED_INIT(start_tv);
#endif

    ++safe_to_invoke_callback;

    /* Only do this message when another message was given, otherwise we get
     * lots of them. */
    if (did_log_msg)
    {
	ch_log(NULL, "looking for messages on channels");
	did_log_msg = FALSE;
    }
    while (channel != NULL)
    {
	if (channel_can_close(channel))
	{
	    channel->ch_to_be_closed = (1U << PART_COUNT);
	    channel_close_now(channel);
	    // channel may have been freed, start over
	    channel = first_channel;
	    continue;
	}
	if (channel->ch_to_be_freed || channel->ch_killing)
	{
	    if (channel->ch_killing)
	    {
		channel_free_contents(channel);
		channel->ch_job->jv_channel = NULL;
	    }
	    channel_free(channel);
	    // channel has been freed, start over
	    channel = first_channel;
	    continue;
	}
	if (channel->ch_refcount == 0 && !channel_still_useful(channel))
	{
	    // channel is no longer useful, free it
	    channel_free(channel);
	    channel = first_channel;
	    part = PART_SOCK;
	    continue;
	}
	if (channel->ch_part[part].ch_fd != INVALID_FD
				      || channel_has_readahead(channel, part))
	{
	    /* Increase the refcount, in case the handler causes the channel
	     * to be unreferenced or closed. */
	    ++channel->ch_refcount;
	    r = may_invoke_callback(channel, part);
	    if (r == OK)
		ret = TRUE;
	    if (channel_unref(channel) || (r == OK
#ifdef ELAPSED_FUNC
			/* Limit the time we loop here to 100 msec, otherwise
			 * Vim becomes unresponsive when the callback takes
			 * more than a bit of time. */
			&& ELAPSED_FUNC(start_tv) < 100L
#endif
			))
	    {
		/* channel was freed or something was done, start over */
		channel = first_channel;
		part = PART_SOCK;
		continue;
	    }
	}
	if (part < PART_ERR)
	    ++part;
	else
	{
	    channel = channel->ch_next;
	    part = PART_SOCK;
	}
    }

    if (channel_need_redraw)
    {
	channel_need_redraw = FALSE;
	redraw_after_callback(TRUE);
    }

    --safe_to_invoke_callback;

    return ret;
}

/*
 * Return TRUE if any channel has readahead.  That means we should not block on
 * waiting for input.
 */
    int
channel_any_readahead(void)
{
    channel_T	*channel = first_channel;
    ch_part_T	part = PART_SOCK;

    while (channel != NULL)
    {
	if (channel_has_readahead(channel, part))
	    return TRUE;
	if (part < PART_ERR)
	    ++part;
	else
	{
	    channel = channel->ch_next;
	    part = PART_SOCK;
	}
    }
    return FALSE;
}

/*
 * Mark references to lists used in channels.
 */
    int
set_ref_in_channel(int copyID)
{
    int		abort = FALSE;
    channel_T	*channel;
    typval_T	tv;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	if (channel_still_useful(channel))
	{
	    tv.v_type = VAR_CHANNEL;
	    tv.vval.v_channel = channel;
	    abort = abort || set_ref_in_item(&tv, copyID, NULL, NULL);
	}
    return abort;
}

/*
 * Return the "part" to write to for "channel".
 */
    ch_part_T
channel_part_send(channel_T *channel)
{
    if (channel->CH_SOCK_FD == INVALID_FD)
	return PART_IN;
    return PART_SOCK;
}

/*
 * Return the default "part" to read from for "channel".
 */
    ch_part_T
channel_part_read(channel_T *channel)
{
    if (channel->CH_SOCK_FD == INVALID_FD)
	return PART_OUT;
    return PART_SOCK;
}

/*
 * Return the mode of "channel"/"part"
 * If "channel" is invalid returns MODE_JSON.
 */
    ch_mode_T
channel_get_mode(channel_T *channel, ch_part_T part)
{
    if (channel == NULL)
	return MODE_JSON;
    return channel->ch_part[part].ch_mode;
}

/*
 * Return the timeout of "channel"/"part"
 */
    int
channel_get_timeout(channel_T *channel, ch_part_T part)
{
    return channel->ch_part[part].ch_timeout;
}

    static int
handle_mode(typval_T *item, jobopt_T *opt, ch_mode_T *modep, int jo)
{
    char_u	*val = tv_get_string(item);

    opt->jo_set |= jo;
    if (STRCMP(val, "nl") == 0)
	*modep = MODE_NL;
    else if (STRCMP(val, "raw") == 0)
	*modep = MODE_RAW;
    else if (STRCMP(val, "js") == 0)
	*modep = MODE_JS;
    else if (STRCMP(val, "json") == 0)
	*modep = MODE_JSON;
    else
    {
	semsg(_(e_invarg2), val);
	return FAIL;
    }
    return OK;
}

    static int
handle_io(typval_T *item, ch_part_T part, jobopt_T *opt)
{
    char_u	*val = tv_get_string(item);

    opt->jo_set |= JO_OUT_IO << (part - PART_OUT);
    if (STRCMP(val, "null") == 0)
	opt->jo_io[part] = JIO_NULL;
    else if (STRCMP(val, "pipe") == 0)
	opt->jo_io[part] = JIO_PIPE;
    else if (STRCMP(val, "file") == 0)
	opt->jo_io[part] = JIO_FILE;
    else if (STRCMP(val, "buffer") == 0)
	opt->jo_io[part] = JIO_BUFFER;
    else if (STRCMP(val, "out") == 0 && part == PART_ERR)
	opt->jo_io[part] = JIO_OUT;
    else
    {
	semsg(_(e_invarg2), val);
	return FAIL;
    }
    return OK;
}

/*
 * Clear a jobopt_T before using it.
 */
    void
clear_job_options(jobopt_T *opt)
{
    vim_memset(opt, 0, sizeof(jobopt_T));
}

/*
 * Free any members of a jobopt_T.
 */
    void
free_job_options(jobopt_T *opt)
{
    if (opt->jo_callback.cb_partial != NULL)
	partial_unref(opt->jo_callback.cb_partial);
    else if (opt->jo_callback.cb_name != NULL)
	func_unref(opt->jo_callback.cb_name);
    if (opt->jo_out_cb.cb_partial != NULL)
	partial_unref(opt->jo_out_cb.cb_partial);
    else if (opt->jo_out_cb.cb_name != NULL)
	func_unref(opt->jo_out_cb.cb_name);
    if (opt->jo_err_cb.cb_partial != NULL)
	partial_unref(opt->jo_err_cb.cb_partial);
    else if (opt->jo_err_cb.cb_name != NULL)
	func_unref(opt->jo_err_cb.cb_name);
    if (opt->jo_close_cb.cb_partial != NULL)
	partial_unref(opt->jo_close_cb.cb_partial);
    else if (opt->jo_close_cb.cb_name != NULL)
	func_unref(opt->jo_close_cb.cb_name);
    if (opt->jo_exit_cb.cb_partial != NULL)
	partial_unref(opt->jo_exit_cb.cb_partial);
    else if (opt->jo_exit_cb.cb_name != NULL)
	func_unref(opt->jo_exit_cb.cb_name);
    if (opt->jo_env != NULL)
	dict_unref(opt->jo_env);
}

/*
 * Get the PART_ number from the first character of an option name.
 */
    static int
part_from_char(int c)
{
    return c == 'i' ? PART_IN : c == 'o' ? PART_OUT: PART_ERR;
}

/*
 * Get the option entries from the dict in "tv", parse them and put the result
 * in "opt".
 * Only accept JO_ options in "supported" and JO2_ options in "supported2".
 * If an option value is invalid return FAIL.
 */
    int
get_job_options(typval_T *tv, jobopt_T *opt, int supported, int supported2)
{
    typval_T	*item;
    char_u	*val;
    dict_T	*dict;
    int		todo;
    hashitem_T	*hi;
    ch_part_T	part;

    if (tv->v_type == VAR_UNKNOWN)
	return OK;
    if (tv->v_type != VAR_DICT)
    {
	emsg(_(e_dictreq));
	return FAIL;
    }
    dict = tv->vval.v_dict;
    if (dict == NULL)
	return OK;

    todo = (int)dict->dv_hashtab.ht_used;
    for (hi = dict->dv_hashtab.ht_array; todo > 0; ++hi)
	if (!HASHITEM_EMPTY(hi))
	{
	    item = &dict_lookup(hi)->di_tv;

	    if (STRCMP(hi->hi_key, "mode") == 0)
	    {
		if (!(supported & JO_MODE))
		    break;
		if (handle_mode(item, opt, &opt->jo_mode, JO_MODE) == FAIL)
		    return FAIL;
	    }
	    else if (STRCMP(hi->hi_key, "in_mode") == 0)
	    {
		if (!(supported & JO_IN_MODE))
		    break;
		if (handle_mode(item, opt, &opt->jo_in_mode, JO_IN_MODE)
								      == FAIL)
		    return FAIL;
	    }
	    else if (STRCMP(hi->hi_key, "out_mode") == 0)
	    {
		if (!(supported & JO_OUT_MODE))
		    break;
		if (handle_mode(item, opt, &opt->jo_out_mode, JO_OUT_MODE)
								      == FAIL)
		    return FAIL;
	    }
	    else if (STRCMP(hi->hi_key, "err_mode") == 0)
	    {
		if (!(supported & JO_ERR_MODE))
		    break;
		if (handle_mode(item, opt, &opt->jo_err_mode, JO_ERR_MODE)
								      == FAIL)
		    return FAIL;
	    }
	    else if (STRCMP(hi->hi_key, "noblock") == 0)
	    {
		if (!(supported & JO_MODE))
		    break;
		opt->jo_noblock = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "in_io") == 0
		    || STRCMP(hi->hi_key, "out_io") == 0
		    || STRCMP(hi->hi_key, "err_io") == 0)
	    {
		if (!(supported & JO_OUT_IO))
		    break;
		if (handle_io(item, part_from_char(*hi->hi_key), opt) == FAIL)
		    return FAIL;
	    }
	    else if (STRCMP(hi->hi_key, "in_name") == 0
		    || STRCMP(hi->hi_key, "out_name") == 0
		    || STRCMP(hi->hi_key, "err_name") == 0)
	    {
		part = part_from_char(*hi->hi_key);

		if (!(supported & JO_OUT_IO))
		    break;
		opt->jo_set |= JO_OUT_NAME << (part - PART_OUT);
		opt->jo_io_name[part] =
		       tv_get_string_buf_chk(item, opt->jo_io_name_buf[part]);
	    }
	    else if (STRCMP(hi->hi_key, "pty") == 0)
	    {
		if (!(supported & JO_MODE))
		    break;
		opt->jo_pty = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "in_buf") == 0
		    || STRCMP(hi->hi_key, "out_buf") == 0
		    || STRCMP(hi->hi_key, "err_buf") == 0)
	    {
		part = part_from_char(*hi->hi_key);

		if (!(supported & JO_OUT_IO))
		    break;
		opt->jo_set |= JO_OUT_BUF << (part - PART_OUT);
		opt->jo_io_buf[part] = tv_get_number(item);
		if (opt->jo_io_buf[part] <= 0)
		{
		    semsg(_(e_invargNval), hi->hi_key, tv_get_string(item));
		    return FAIL;
		}
		if (buflist_findnr(opt->jo_io_buf[part]) == NULL)
		{
		    semsg(_(e_nobufnr), (long)opt->jo_io_buf[part]);
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "out_modifiable") == 0
		    || STRCMP(hi->hi_key, "err_modifiable") == 0)
	    {
		part = part_from_char(*hi->hi_key);

		if (!(supported & JO_OUT_IO))
		    break;
		opt->jo_set |= JO_OUT_MODIFIABLE << (part - PART_OUT);
		opt->jo_modifiable[part] = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "out_msg") == 0
		    || STRCMP(hi->hi_key, "err_msg") == 0)
	    {
		part = part_from_char(*hi->hi_key);

		if (!(supported & JO_OUT_IO))
		    break;
		opt->jo_set2 |= JO2_OUT_MSG << (part - PART_OUT);
		opt->jo_message[part] = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "in_top") == 0
		    || STRCMP(hi->hi_key, "in_bot") == 0)
	    {
		linenr_T *lp;

		if (!(supported & JO_OUT_IO))
		    break;
		if (hi->hi_key[3] == 't')
		{
		    lp = &opt->jo_in_top;
		    opt->jo_set |= JO_IN_TOP;
		}
		else
		{
		    lp = &opt->jo_in_bot;
		    opt->jo_set |= JO_IN_BOT;
		}
		*lp = tv_get_number(item);
		if (*lp < 0)
		{
		    semsg(_(e_invargNval), hi->hi_key, tv_get_string(item));
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "channel") == 0)
	    {
		if (!(supported & JO_OUT_IO))
		    break;
		opt->jo_set |= JO_CHANNEL;
		if (item->v_type != VAR_CHANNEL)
		{
		    semsg(_(e_invargval), "channel");
		    return FAIL;
		}
		opt->jo_channel = item->vval.v_channel;
	    }
	    else if (STRCMP(hi->hi_key, "callback") == 0)
	    {
		if (!(supported & JO_CALLBACK))
		    break;
		opt->jo_set |= JO_CALLBACK;
		opt->jo_callback = get_callback(item);
		if (opt->jo_callback.cb_name == NULL)
		{
		    semsg(_(e_invargval), "callback");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "out_cb") == 0)
	    {
		if (!(supported & JO_OUT_CALLBACK))
		    break;
		opt->jo_set |= JO_OUT_CALLBACK;
		opt->jo_out_cb = get_callback(item);
		if (opt->jo_out_cb.cb_name == NULL)
		{
		    semsg(_(e_invargval), "out_cb");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "err_cb") == 0)
	    {
		if (!(supported & JO_ERR_CALLBACK))
		    break;
		opt->jo_set |= JO_ERR_CALLBACK;
		opt->jo_err_cb = get_callback(item);
		if (opt->jo_err_cb.cb_name == NULL)
		{
		    semsg(_(e_invargval), "err_cb");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "close_cb") == 0)
	    {
		if (!(supported & JO_CLOSE_CALLBACK))
		    break;
		opt->jo_set |= JO_CLOSE_CALLBACK;
		opt->jo_close_cb = get_callback(item);
		if (opt->jo_close_cb.cb_name == NULL)
		{
		    semsg(_(e_invargval), "close_cb");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "drop") == 0)
	    {
		int never = FALSE;
		val = tv_get_string(item);

		if (STRCMP(val, "never") == 0)
		    never = TRUE;
		else if (STRCMP(val, "auto") != 0)
		{
		    semsg(_(e_invargNval), "drop", val);
		    return FAIL;
		}
		opt->jo_drop_never = never;
	    }
	    else if (STRCMP(hi->hi_key, "exit_cb") == 0)
	    {
		if (!(supported & JO_EXIT_CB))
		    break;
		opt->jo_set |= JO_EXIT_CB;
		opt->jo_exit_cb = get_callback(item);
		if (opt->jo_exit_cb.cb_name == NULL)
		{
		    semsg(_(e_invargval), "exit_cb");
		    return FAIL;
		}
	    }
#ifdef FEAT_TERMINAL
	    else if (STRCMP(hi->hi_key, "term_name") == 0)
	    {
		if (!(supported2 & JO2_TERM_NAME))
		    break;
		opt->jo_set2 |= JO2_TERM_NAME;
		opt->jo_term_name = tv_get_string_chk(item);
		if (opt->jo_term_name == NULL)
		{
		    semsg(_(e_invargval), "term_name");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "term_finish") == 0)
	    {
		if (!(supported2 & JO2_TERM_FINISH))
		    break;
		val = tv_get_string(item);
		if (STRCMP(val, "open") != 0 && STRCMP(val, "close") != 0)
		{
		    semsg(_(e_invargNval), "term_finish", val);
		    return FAIL;
		}
		opt->jo_set2 |= JO2_TERM_FINISH;
		opt->jo_term_finish = *val;
	    }
	    else if (STRCMP(hi->hi_key, "term_opencmd") == 0)
	    {
		char_u *p;

		if (!(supported2 & JO2_TERM_OPENCMD))
		    break;
		opt->jo_set2 |= JO2_TERM_OPENCMD;
		p = opt->jo_term_opencmd = tv_get_string_chk(item);
		if (p != NULL)
		{
		    /* Must have %d and no other %. */
		    p = vim_strchr(p, '%');
		    if (p != NULL && (p[1] != 'd'
					    || vim_strchr(p + 2, '%') != NULL))
			p = NULL;
		}
		if (p == NULL)
		{
		    semsg(_(e_invargval), "term_opencmd");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "eof_chars") == 0)
	    {
		char_u *p;

		if (!(supported2 & JO2_EOF_CHARS))
		    break;
		opt->jo_set2 |= JO2_EOF_CHARS;
		p = opt->jo_eof_chars = tv_get_string_chk(item);
		if (p == NULL)
		{
		    semsg(_(e_invargval), "eof_chars");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "term_rows") == 0)
	    {
		if (!(supported2 & JO2_TERM_ROWS))
		    break;
		opt->jo_set2 |= JO2_TERM_ROWS;
		opt->jo_term_rows = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "term_cols") == 0)
	    {
		if (!(supported2 & JO2_TERM_COLS))
		    break;
		opt->jo_set2 |= JO2_TERM_COLS;
		opt->jo_term_cols = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "vertical") == 0)
	    {
		if (!(supported2 & JO2_VERTICAL))
		    break;
		opt->jo_set2 |= JO2_VERTICAL;
		opt->jo_vertical = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "curwin") == 0)
	    {
		if (!(supported2 & JO2_CURWIN))
		    break;
		opt->jo_set2 |= JO2_CURWIN;
		opt->jo_curwin = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "hidden") == 0)
	    {
		if (!(supported2 & JO2_HIDDEN))
		    break;
		opt->jo_set2 |= JO2_HIDDEN;
		opt->jo_hidden = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "norestore") == 0)
	    {
		if (!(supported2 & JO2_NORESTORE))
		    break;
		opt->jo_set2 |= JO2_NORESTORE;
		opt->jo_term_norestore = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "term_kill") == 0)
	    {
		if (!(supported2 & JO2_TERM_KILL))
		    break;
		opt->jo_set2 |= JO2_TERM_KILL;
		opt->jo_term_kill = tv_get_string_chk(item);
	    }
	    else if (STRCMP(hi->hi_key, "tty_type") == 0)
	    {
		char_u *p;

		if (!(supported2 & JO2_TTY_TYPE))
		    break;
		opt->jo_set2 |= JO2_TTY_TYPE;
		p = tv_get_string_chk(item);
		if (p == NULL)
		{
		    semsg(_(e_invargval), "tty_type");
		    return FAIL;
		}
		// Allow empty string, "winpty", "conpty".
		if (!(*p == NUL || STRCMP(p, "winpty") == 0
					          || STRCMP(p, "conpty") == 0))
		{
		    semsg(_(e_invargval), "tty_type");
		    return FAIL;
		}
		opt->jo_tty_type = p[0];
	    }
# if defined(FEAT_GUI) || defined(FEAT_TERMGUICOLORS)
	    else if (STRCMP(hi->hi_key, "ansi_colors") == 0)
	    {
		int		n = 0;
		listitem_T	*li;
		long_u		rgb[16];

		if (!(supported2 & JO2_ANSI_COLORS))
		    break;

		if (item == NULL || item->v_type != VAR_LIST
			|| item->vval.v_list == NULL)
		{
		    semsg(_(e_invargval), "ansi_colors");
		    return FAIL;
		}

		li = item->vval.v_list->lv_first;
		for (; li != NULL && n < 16; li = li->li_next, n++)
		{
		    char_u	*color_name;
		    guicolor_T	guicolor;

		    color_name = tv_get_string_chk(&li->li_tv);
		    if (color_name == NULL)
			return FAIL;

		    guicolor = GUI_GET_COLOR(color_name);
		    if (guicolor == INVALCOLOR)
			return FAIL;

		    rgb[n] = GUI_MCH_GET_RGB(guicolor);
		}

		if (n != 16 || li != NULL)
		{
		    semsg(_(e_invargval), "ansi_colors");
		    return FAIL;
		}

		opt->jo_set2 |= JO2_ANSI_COLORS;
		memcpy(opt->jo_ansi_colors, rgb, sizeof(rgb));
	    }
# endif
#endif
	    else if (STRCMP(hi->hi_key, "env") == 0)
	    {
		if (!(supported2 & JO2_ENV))
		    break;
		if (item->v_type != VAR_DICT)
		{
		    semsg(_(e_invargval), "env");
		    return FAIL;
		}
		opt->jo_set2 |= JO2_ENV;
		opt->jo_env = item->vval.v_dict;
		if (opt->jo_env != NULL)
		    ++opt->jo_env->dv_refcount;
	    }
	    else if (STRCMP(hi->hi_key, "cwd") == 0)
	    {
		if (!(supported2 & JO2_CWD))
		    break;
		opt->jo_cwd = tv_get_string_buf_chk(item, opt->jo_cwd_buf);
		if (opt->jo_cwd == NULL || !mch_isdir(opt->jo_cwd)
#ifndef MSWIN  // Win32 directories don't have the concept of "executable"
				|| mch_access((char *)opt->jo_cwd, X_OK) != 0
#endif
				)
		{
		    semsg(_(e_invargval), "cwd");
		    return FAIL;
		}
		opt->jo_set2 |= JO2_CWD;
	    }
	    else if (STRCMP(hi->hi_key, "waittime") == 0)
	    {
		if (!(supported & JO_WAITTIME))
		    break;
		opt->jo_set |= JO_WAITTIME;
		opt->jo_waittime = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "timeout") == 0)
	    {
		if (!(supported & JO_TIMEOUT))
		    break;
		opt->jo_set |= JO_TIMEOUT;
		opt->jo_timeout = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "out_timeout") == 0)
	    {
		if (!(supported & JO_OUT_TIMEOUT))
		    break;
		opt->jo_set |= JO_OUT_TIMEOUT;
		opt->jo_out_timeout = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "err_timeout") == 0)
	    {
		if (!(supported & JO_ERR_TIMEOUT))
		    break;
		opt->jo_set |= JO_ERR_TIMEOUT;
		opt->jo_err_timeout = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "part") == 0)
	    {
		if (!(supported & JO_PART))
		    break;
		opt->jo_set |= JO_PART;
		val = tv_get_string(item);
		if (STRCMP(val, "err") == 0)
		    opt->jo_part = PART_ERR;
		else if (STRCMP(val, "out") == 0)
		    opt->jo_part = PART_OUT;
		else
		{
		    semsg(_(e_invargNval), "part", val);
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "id") == 0)
	    {
		if (!(supported & JO_ID))
		    break;
		opt->jo_set |= JO_ID;
		opt->jo_id = tv_get_number(item);
	    }
	    else if (STRCMP(hi->hi_key, "stoponexit") == 0)
	    {
		if (!(supported & JO_STOPONEXIT))
		    break;
		opt->jo_set |= JO_STOPONEXIT;
		opt->jo_stoponexit = tv_get_string_buf_chk(item,
							     opt->jo_soe_buf);
		if (opt->jo_stoponexit == NULL)
		{
		    semsg(_(e_invargval), "stoponexit");
		    return FAIL;
		}
	    }
	    else if (STRCMP(hi->hi_key, "block_write") == 0)
	    {
		if (!(supported & JO_BLOCK_WRITE))
		    break;
		opt->jo_set |= JO_BLOCK_WRITE;
		opt->jo_block_write = tv_get_number(item);
	    }
	    else
		break;
	    --todo;
	}
    if (todo > 0)
    {
	semsg(_(e_invarg2), hi->hi_key);
	return FAIL;
    }

    return OK;
}

/*
 * Get the channel from the argument.
 * Returns NULL if the handle is invalid.
 * When "check_open" is TRUE check that the channel can be used.
 * When "reading" is TRUE "check_open" considers typeahead useful.
 * "part" is used to check typeahead, when PART_COUNT use the default part.
 */
    channel_T *
get_channel_arg(typval_T *tv, int check_open, int reading, ch_part_T part)
{
    channel_T	*channel = NULL;
    int		has_readahead = FALSE;

    if (tv->v_type == VAR_JOB)
    {
	if (tv->vval.v_job != NULL)
	    channel = tv->vval.v_job->jv_channel;
    }
    else if (tv->v_type == VAR_CHANNEL)
    {
	channel = tv->vval.v_channel;
    }
    else
    {
	semsg(_(e_invarg2), tv_get_string(tv));
	return NULL;
    }
    if (channel != NULL && reading)
	has_readahead = channel_has_readahead(channel,
		       part != PART_COUNT ? part : channel_part_read(channel));

    if (check_open && (channel == NULL || (!channel_is_open(channel)
					     && !(reading && has_readahead))))
    {
	emsg(_("E906: not an open channel"));
	return NULL;
    }
    return channel;
}

static job_T *first_job = NULL;

    static void
job_free_contents(job_T *job)
{
    int		i;

    ch_log(job->jv_channel, "Freeing job");
    if (job->jv_channel != NULL)
    {
	/* The link from the channel to the job doesn't count as a reference,
	 * thus don't decrement the refcount of the job.  The reference from
	 * the job to the channel does count the reference, decrement it and
	 * NULL the reference.  We don't set ch_job_killed, unreferencing the
	 * job doesn't mean it stops running. */
	job->jv_channel->ch_job = NULL;
	channel_unref(job->jv_channel);
    }
    mch_clear_job(job);

    vim_free(job->jv_tty_in);
    vim_free(job->jv_tty_out);
    vim_free(job->jv_stoponexit);
#ifdef UNIX
    vim_free(job->jv_termsig);
#endif
#ifdef MSWIN
    vim_free(job->jv_tty_type);
#endif
    free_callback(&job->jv_exit_cb);
    if (job->jv_argv != NULL)
    {
	for (i = 0; job->jv_argv[i] != NULL; i++)
	    vim_free(job->jv_argv[i]);
	vim_free(job->jv_argv);
    }
}

/*
 * Remove "job" from the list of jobs.
 */
    static void
job_unlink(job_T *job)
{
    if (job->jv_next != NULL)
	job->jv_next->jv_prev = job->jv_prev;
    if (job->jv_prev == NULL)
	first_job = job->jv_next;
    else
	job->jv_prev->jv_next = job->jv_next;
}

    static void
job_free_job(job_T *job)
{
    job_unlink(job);
    vim_free(job);
}

    static void
job_free(job_T *job)
{
    if (!in_free_unref_items)
    {
	job_free_contents(job);
	job_free_job(job);
    }
}

job_T *jobs_to_free = NULL;

/*
 * Put "job" in a list to be freed later, when it's no longer referenced.
 */
    static void
job_free_later(job_T *job)
{
    job_unlink(job);
    job->jv_next = jobs_to_free;
    jobs_to_free = job;
}

    static void
free_jobs_to_free_later(void)
{
    job_T *job;

    while (jobs_to_free != NULL)
    {
	job = jobs_to_free;
	jobs_to_free = job->jv_next;
	job_free_contents(job);
	vim_free(job);
    }
}

#if defined(EXITFREE) || defined(PROTO)
    void
job_free_all(void)
{
    while (first_job != NULL)
	job_free(first_job);
    free_jobs_to_free_later();

# ifdef FEAT_TERMINAL
    free_unused_terminals();
# endif
}
#endif

/*
 * Return TRUE if we need to check if the process of "job" has ended.
 */
    static int
job_need_end_check(job_T *job)
{
    return job->jv_status == JOB_STARTED
	    && (job->jv_stoponexit != NULL || job->jv_exit_cb.cb_name != NULL);
}

/*
 * Return TRUE if the channel of "job" is still useful.
 */
    static int
job_channel_still_useful(job_T *job)
{
    return job->jv_channel != NULL && channel_still_useful(job->jv_channel);
}

/*
 * Return TRUE if the channel of "job" is closeable.
 */
    static int
job_channel_can_close(job_T *job)
{
    return job->jv_channel != NULL && channel_can_close(job->jv_channel);
}

/*
 * Return TRUE if the job should not be freed yet.  Do not free the job when
 * it has not ended yet and there is a "stoponexit" flag, an exit callback
 * or when the associated channel will do something with the job output.
 */
    static int
job_still_useful(job_T *job)
{
    return job_need_end_check(job) || job_channel_still_useful(job);
}

#if defined(GUI_MAY_FORK) || defined(GUI_MAY_SPAWN) || defined(PROTO)
/*
 * Return TRUE when there is any running job that we care about.
 */
    int
job_any_running()
{
    job_T	*job;

    for (job = first_job; job != NULL; job = job->jv_next)
	if (job_still_useful(job))
	{
	    ch_log(NULL, "GUI not forking because a job is running");
	    return TRUE;
	}
    return FALSE;
}
#endif

#if !defined(USE_ARGV) || defined(PROTO)
/*
 * Escape one argument for an external command.
 * Returns the escaped string in allocated memory.  NULL when out of memory.
 */
    static char_u *
win32_escape_arg(char_u *arg)
{
    int		slen, dlen;
    int		escaping = 0;
    int		i;
    char_u	*s, *d;
    char_u	*escaped_arg;
    int		has_spaces = FALSE;

    /* First count the number of extra bytes required. */
    slen = (int)STRLEN(arg);
    dlen = slen;
    for (s = arg; *s != NUL; MB_PTR_ADV(s))
    {
	if (*s == '"' || *s == '\\')
	    ++dlen;
	if (*s == ' ' || *s == '\t')
	    has_spaces = TRUE;
    }

    if (has_spaces)
	dlen += 2;

    if (dlen == slen)
	return vim_strsave(arg);

    /* Allocate memory for the result and fill it. */
    escaped_arg = alloc(dlen + 1);
    if (escaped_arg == NULL)
	return NULL;
    memset(escaped_arg, 0, dlen+1);

    d = escaped_arg;

    if (has_spaces)
	*d++ = '"';

    for (s = arg; *s != NUL;)
    {
	switch (*s)
	{
	    case '"':
		for (i = 0; i < escaping; i++)
		    *d++ = '\\';
		escaping = 0;
		*d++ = '\\';
		*d++ = *s++;
		break;
	    case '\\':
		escaping++;
		*d++ = *s++;
		break;
	    default:
		escaping = 0;
		MB_COPY_CHAR(s, d);
		break;
	}
    }

    /* add terminating quote and finish with a NUL */
    if (has_spaces)
    {
	for (i = 0; i < escaping; i++)
	    *d++ = '\\';
	*d++ = '"';
    }
    *d = NUL;

    return escaped_arg;
}

/*
 * Build a command line from a list, taking care of escaping.
 * The result is put in gap->ga_data.
 * Returns FAIL when out of memory.
 */
    int
win32_build_cmd(list_T *l, garray_T *gap)
{
    listitem_T  *li;
    char_u	*s;

    for (li = l->lv_first; li != NULL; li = li->li_next)
    {
	s = tv_get_string_chk(&li->li_tv);
	if (s == NULL)
	    return FAIL;
	s = win32_escape_arg(s);
	if (s == NULL)
	    return FAIL;
	ga_concat(gap, s);
	vim_free(s);
	if (li->li_next != NULL)
	    ga_append(gap, ' ');
    }
    return OK;
}
#endif

/*
 * NOTE: Must call job_cleanup() only once right after the status of "job"
 * changed to JOB_ENDED (i.e. after job_status() returned "dead" first or
 * mch_detect_ended_job() returned non-NULL).
 * If the job is no longer used it will be removed from the list of jobs, and
 * deleted a bit later.
 */
    void
job_cleanup(job_T *job)
{
    if (job->jv_status != JOB_ENDED)
	return;

    /* Ready to cleanup the job. */
    job->jv_status = JOB_FINISHED;

    /* When only channel-in is kept open, close explicitly. */
    if (job->jv_channel != NULL)
	ch_close_part(job->jv_channel, PART_IN);

    if (job->jv_exit_cb.cb_name != NULL)
    {
	typval_T	argv[3];
	typval_T	rettv;
	int		dummy;

	/* Invoke the exit callback. Make sure the refcount is > 0. */
	ch_log(job->jv_channel, "Invoking exit callback %s",
						      job->jv_exit_cb.cb_name);
	++job->jv_refcount;
	argv[0].v_type = VAR_JOB;
	argv[0].vval.v_job = job;
	argv[1].v_type = VAR_NUMBER;
	argv[1].vval.v_number = job->jv_exitval;
	call_callback(&job->jv_exit_cb, -1,
			    &rettv, 2, argv, NULL, 0L, 0L, &dummy, TRUE, NULL);
	clear_tv(&rettv);
	--job->jv_refcount;
	channel_need_redraw = TRUE;
    }

    if (job->jv_channel != NULL && job->jv_channel->ch_anonymous_pipe)
	job->jv_channel->ch_killing = TRUE;

    // Do not free the job in case the close callback of the associated channel
    // isn't invoked yet and may get information by job_info().
    if (job->jv_refcount == 0 && !job_channel_still_useful(job))
	// The job was already unreferenced and the associated channel was
	// detached, now that it ended it can be freed. However, a caller might
	// still use it, thus free it a bit later.
	job_free_later(job);
}

/*
 * Mark references in jobs that are still useful.
 */
    int
set_ref_in_job(int copyID)
{
    int		abort = FALSE;
    job_T	*job;
    typval_T	tv;

    for (job = first_job; job != NULL; job = job->jv_next)
	if (job_still_useful(job))
	{
	    tv.v_type = VAR_JOB;
	    tv.vval.v_job = job;
	    abort = abort || set_ref_in_item(&tv, copyID, NULL, NULL);
	}
    return abort;
}

/*
 * Dereference "job".  Note that after this "job" may have been freed.
 */
    void
job_unref(job_T *job)
{
    if (job != NULL && --job->jv_refcount <= 0)
    {
	/* Do not free the job if there is a channel where the close callback
	 * may get the job info. */
	if (!job_channel_still_useful(job))
	{
	    /* Do not free the job when it has not ended yet and there is a
	     * "stoponexit" flag or an exit callback. */
	    if (!job_need_end_check(job))
	    {
		job_free(job);
	    }
	    else if (job->jv_channel != NULL)
	    {
		/* Do remove the link to the channel, otherwise it hangs
		 * around until Vim exits. See job_free() for refcount. */
		ch_log(job->jv_channel, "detaching channel from job");
		job->jv_channel->ch_job = NULL;
		channel_unref(job->jv_channel);
		job->jv_channel = NULL;
	    }
	}
    }
}

    int
free_unused_jobs_contents(int copyID, int mask)
{
    int		did_free = FALSE;
    job_T	*job;

    for (job = first_job; job != NULL; job = job->jv_next)
	if ((job->jv_copyID & mask) != (copyID & mask)
						    && !job_still_useful(job))
	{
	    /* Free the channel and ordinary items it contains, but don't
	     * recurse into Lists, Dictionaries etc. */
	    job_free_contents(job);
	    did_free = TRUE;
	}
    return did_free;
}

    void
free_unused_jobs(int copyID, int mask)
{
    job_T	*job;
    job_T	*job_next;

    for (job = first_job; job != NULL; job = job_next)
    {
	job_next = job->jv_next;
	if ((job->jv_copyID & mask) != (copyID & mask)
						    && !job_still_useful(job))
	{
	    /* Free the job struct itself. */
	    job_free_job(job);
	}
    }
}

/*
 * Allocate a job.  Sets the refcount to one and sets options default.
 */
    job_T *
job_alloc(void)
{
    job_T *job;

    job = ALLOC_CLEAR_ONE(job_T);
    if (job != NULL)
    {
	job->jv_refcount = 1;
	job->jv_stoponexit = vim_strsave((char_u *)"term");

	if (first_job != NULL)
	{
	    first_job->jv_prev = job;
	    job->jv_next = first_job;
	}
	first_job = job;
    }
    return job;
}

    void
job_set_options(job_T *job, jobopt_T *opt)
{
    if (opt->jo_set & JO_STOPONEXIT)
    {
	vim_free(job->jv_stoponexit);
	if (opt->jo_stoponexit == NULL || *opt->jo_stoponexit == NUL)
	    job->jv_stoponexit = NULL;
	else
	    job->jv_stoponexit = vim_strsave(opt->jo_stoponexit);
    }
    if (opt->jo_set & JO_EXIT_CB)
    {
	free_callback(&job->jv_exit_cb);
	if (opt->jo_exit_cb.cb_name == NULL || *opt->jo_exit_cb.cb_name == NUL)
	{
	    job->jv_exit_cb.cb_name = NULL;
	    job->jv_exit_cb.cb_partial = NULL;
	}
	else
	    copy_callback(&job->jv_exit_cb, &opt->jo_exit_cb);
    }
}

/*
 * Called when Vim is exiting: kill all jobs that have the "stoponexit" flag.
 */
    void
job_stop_on_exit(void)
{
    job_T	*job;

    for (job = first_job; job != NULL; job = job->jv_next)
	if (job->jv_status == JOB_STARTED && job->jv_stoponexit != NULL)
	    mch_signal_job(job, job->jv_stoponexit);
}

/*
 * Return TRUE when there is any job that has an exit callback and might exit,
 * which means job_check_ended() should be called more often.
 */
    int
has_pending_job(void)
{
    job_T	    *job;

    for (job = first_job; job != NULL; job = job->jv_next)
	/* Only should check if the channel has been closed, if the channel is
	 * open the job won't exit. */
	if ((job->jv_status == JOB_STARTED && !job_channel_still_useful(job))
		    || (job->jv_status == JOB_FINISHED
					      && job_channel_can_close(job)))
	    return TRUE;
    return FALSE;
}

#define MAX_CHECK_ENDED 8

/*
 * Called once in a while: check if any jobs that seem useful have ended.
 * Returns TRUE if a job did end.
 */
    int
job_check_ended(void)
{
    int		i;
    int		did_end = FALSE;

    // be quick if there are no jobs to check
    if (first_job == NULL)
	return did_end;

    for (i = 0; i < MAX_CHECK_ENDED; ++i)
    {
	// NOTE: mch_detect_ended_job() must only return a job of which the
	// status was just set to JOB_ENDED.
	job_T	*job = mch_detect_ended_job(first_job);

	if (job == NULL)
	    break;
	did_end = TRUE;
	job_cleanup(job); // may add "job" to jobs_to_free
    }

    // Actually free jobs that were cleaned up.
    free_jobs_to_free_later();

    if (channel_need_redraw)
    {
	channel_need_redraw = FALSE;
	redraw_after_callback(TRUE);
    }
    return did_end;
}

/*
 * Create a job and return it.  Implements job_start().
 * "argv_arg" is only for Unix.
 * When "argv_arg" is NULL then "argvars" is used.
 * The returned job has a refcount of one.
 * Returns NULL when out of memory.
 */
    job_T *
job_start(
	typval_T    *argvars,
	char	    **argv_arg,
	jobopt_T    *opt_arg,
	int	    is_terminal UNUSED)
{
    job_T	*job;
    char_u	*cmd = NULL;
    char	**argv = NULL;
    int		argc = 0;
#if defined(UNIX)
# define USE_ARGV
    int		i;
#else
    garray_T	ga;
#endif
    jobopt_T	opt;
    ch_part_T	part;

    job = job_alloc();
    if (job == NULL)
	return NULL;

    job->jv_status = JOB_FAILED;
#ifndef USE_ARGV
    ga_init2(&ga, (int)sizeof(char*), 20);
#endif

    if (opt_arg != NULL)
	opt = *opt_arg;
    else
    {
	/* Default mode is NL. */
	clear_job_options(&opt);
	opt.jo_mode = MODE_NL;
	if (get_job_options(&argvars[1], &opt,
		    JO_MODE_ALL + JO_CB_ALL + JO_TIMEOUT_ALL + JO_STOPONEXIT
			 + JO_EXIT_CB + JO_OUT_IO + JO_BLOCK_WRITE,
		     JO2_ENV + JO2_CWD) == FAIL)
	    goto theend;
    }

    /* Check that when io is "file" that there is a file name. */
    for (part = PART_OUT; part < PART_COUNT; ++part)
	if ((opt.jo_set & (JO_OUT_IO << (part - PART_OUT)))
		&& opt.jo_io[part] == JIO_FILE
		&& (!(opt.jo_set & (JO_OUT_NAME << (part - PART_OUT)))
		    || *opt.jo_io_name[part] == NUL))
	{
	    emsg(_("E920: _io file requires _name to be set"));
	    goto theend;
	}

    if ((opt.jo_set & JO_IN_IO) && opt.jo_io[PART_IN] == JIO_BUFFER)
    {
	buf_T *buf = NULL;

	/* check that we can find the buffer before starting the job */
	if (opt.jo_set & JO_IN_BUF)
	{
	    buf = buflist_findnr(opt.jo_io_buf[PART_IN]);
	    if (buf == NULL)
		semsg(_(e_nobufnr), (long)opt.jo_io_buf[PART_IN]);
	}
	else if (!(opt.jo_set & JO_IN_NAME))
	{
	    emsg(_("E915: in_io buffer requires in_buf or in_name to be set"));
	}
	else
	    buf = buflist_find_by_name(opt.jo_io_name[PART_IN], FALSE);
	if (buf == NULL)
	    goto theend;
	if (buf->b_ml.ml_mfp == NULL)
	{
	    char_u	numbuf[NUMBUFLEN];
	    char_u	*s;

	    if (opt.jo_set & JO_IN_BUF)
	    {
		sprintf((char *)numbuf, "%d", opt.jo_io_buf[PART_IN]);
		s = numbuf;
	    }
	    else
		s = opt.jo_io_name[PART_IN];
	    semsg(_("E918: buffer must be loaded: %s"), s);
	    goto theend;
	}
	job->jv_in_buf = buf;
    }

    job_set_options(job, &opt);

#ifdef USE_ARGV
    if (argv_arg != NULL)
    {
	/* Make a copy of argv_arg for job->jv_argv. */
	for (i = 0; argv_arg[i] != NULL; i++)
	    argc++;
	argv = ALLOC_MULT(char *, argc + 1);
	if (argv == NULL)
	    goto theend;
	for (i = 0; i < argc; i++)
	    argv[i] = (char *)vim_strsave((char_u *)argv_arg[i]);
	argv[argc] = NULL;
    }
    else
#endif
    if (argvars[0].v_type == VAR_STRING)
    {
	/* Command is a string. */
	cmd = argvars[0].vval.v_string;
	if (cmd == NULL || *cmd == NUL)
	{
	    emsg(_(e_invarg));
	    goto theend;
	}

	if (build_argv_from_string(cmd, &argv, &argc) == FAIL)
	    goto theend;
    }
    else if (argvars[0].v_type != VAR_LIST
	    || argvars[0].vval.v_list == NULL
	    || argvars[0].vval.v_list->lv_len < 1)
    {
	emsg(_(e_invarg));
	goto theend;
    }
    else
    {
	list_T *l = argvars[0].vval.v_list;

	if (build_argv_from_list(l, &argv, &argc) == FAIL)
	    goto theend;
#ifndef USE_ARGV
	if (win32_build_cmd(l, &ga) == FAIL)
	    goto theend;
	cmd = ga.ga_data;
#endif
    }

    /* Save the command used to start the job. */
    job->jv_argv = argv;

#ifdef USE_ARGV
    if (ch_log_active())
    {
	garray_T    ga;

	ga_init2(&ga, (int)sizeof(char), 200);
	for (i = 0; i < argc; ++i)
	{
	    if (i > 0)
		ga_concat(&ga, (char_u *)"  ");
	    ga_concat(&ga, (char_u *)argv[i]);
	}
	ga_append(&ga, NUL);
	ch_log(NULL, "Starting job: %s", (char *)ga.ga_data);
	ga_clear(&ga);
    }
    mch_job_start(argv, job, &opt, is_terminal);
#else
    ch_log(NULL, "Starting job: %s", (char *)cmd);
    mch_job_start((char *)cmd, job, &opt);
#endif

    /* If the channel is reading from a buffer, write lines now. */
    if (job->jv_channel != NULL)
	channel_write_in(job->jv_channel);

theend:
#ifndef USE_ARGV
    vim_free(ga.ga_data);
#endif
    if (argv != job->jv_argv)
	vim_free(argv);
    free_job_options(&opt);
    return job;
}

/*
 * Get the status of "job" and invoke the exit callback when needed.
 * The returned string is not allocated.
 */
    char *
job_status(job_T *job)
{
    char	*result;

    if (job->jv_status >= JOB_ENDED)
	/* No need to check, dead is dead. */
	result = "dead";
    else if (job->jv_status == JOB_FAILED)
	result = "fail";
    else
    {
	result = mch_job_status(job);
	if (job->jv_status == JOB_ENDED)
	    job_cleanup(job);
    }
    return result;
}

/*
 * Implementation of job_info().
 */
    void
job_info(job_T *job, dict_T *dict)
{
    dictitem_T	*item;
    varnumber_T	nr;
    list_T	*l;
    int		i;

    dict_add_string(dict, "status", (char_u *)job_status(job));

    item = dictitem_alloc((char_u *)"channel");
    if (item == NULL)
	return;
    item->di_tv.v_type = VAR_CHANNEL;
    item->di_tv.vval.v_channel = job->jv_channel;
    if (job->jv_channel != NULL)
	++job->jv_channel->ch_refcount;
    if (dict_add(dict, item) == FAIL)
	dictitem_free(item);

#ifdef UNIX
    nr = job->jv_pid;
#else
    nr = job->jv_proc_info.dwProcessId;
#endif
    dict_add_number(dict, "process", nr);
    dict_add_string(dict, "tty_in", job->jv_tty_in);
    dict_add_string(dict, "tty_out", job->jv_tty_out);

    dict_add_number(dict, "exitval", job->jv_exitval);
    dict_add_string(dict, "exit_cb", job->jv_exit_cb.cb_name);
    dict_add_string(dict, "stoponexit", job->jv_stoponexit);
#ifdef UNIX
    dict_add_string(dict, "termsig", job->jv_termsig);
#endif
#ifdef MSWIN
    dict_add_string(dict, "tty_type", job->jv_tty_type);
#endif

    l = list_alloc();
    if (l != NULL)
    {
	dict_add_list(dict, "cmd", l);
	if (job->jv_argv != NULL)
	    for (i = 0; job->jv_argv[i] != NULL; i++)
		list_append_string(l, (char_u *)job->jv_argv[i], -1);
    }
}

/*
 * Implementation of job_info() to return info for all jobs.
 */
    void
job_info_all(list_T *l)
{
    job_T	*job;
    typval_T	tv;

    for (job = first_job; job != NULL; job = job->jv_next)
    {
	tv.v_type = VAR_JOB;
	tv.vval.v_job = job;

	if (list_append_tv(l, &tv) != OK)
	    return;
    }
}

/*
 * Send a signal to "job".  Implements job_stop().
 * When "type" is not NULL use this for the type.
 * Otherwise use argvars[1] for the type.
 */
    int
job_stop(job_T *job, typval_T *argvars, char *type)
{
    char_u *arg;

    if (type != NULL)
	arg = (char_u *)type;
    else if (argvars[1].v_type == VAR_UNKNOWN)
	arg = (char_u *)"";
    else
    {
	arg = tv_get_string_chk(&argvars[1]);
	if (arg == NULL)
	{
	    emsg(_(e_invarg));
	    return 0;
	}
    }
    if (job->jv_status == JOB_FAILED)
    {
	ch_log(job->jv_channel, "Job failed to start, job_stop() skipped");
	return 0;
    }
    if (job->jv_status == JOB_ENDED)
    {
	ch_log(job->jv_channel, "Job has already ended, job_stop() skipped");
	return 0;
    }
    ch_log(job->jv_channel, "Stopping job with '%s'", (char *)arg);
    if (mch_signal_job(job, arg) == FAIL)
	return 0;

    /* Assume that only "kill" will kill the job. */
    if (job->jv_channel != NULL && STRCMP(arg, "kill") == 0)
	job->jv_channel->ch_job_killed = TRUE;

    /* We don't try freeing the job, obviously the caller still has a
     * reference to it. */
    return 1;
}

    void
invoke_prompt_callback(void)
{
    typval_T	rettv;
    int		dummy;
    typval_T	argv[2];
    char_u	*text;
    char_u	*prompt;
    linenr_T	lnum = curbuf->b_ml.ml_line_count;

    // Add a new line for the prompt before invoking the callback, so that
    // text can always be inserted above the last line.
    ml_append(lnum, (char_u  *)"", 0, FALSE);
    curwin->w_cursor.lnum = lnum + 1;
    curwin->w_cursor.col = 0;

    if (curbuf->b_prompt_callback.cb_name == NULL
	    || *curbuf->b_prompt_callback.cb_name == NUL)
	return;
    text = ml_get(lnum);
    prompt = prompt_text();
    if (STRLEN(text) >= STRLEN(prompt))
	text += STRLEN(prompt);
    argv[0].v_type = VAR_STRING;
    argv[0].vval.v_string = vim_strsave(text);
    argv[1].v_type = VAR_UNKNOWN;

    call_callback(&curbuf->b_prompt_callback, -1,
	      &rettv, 1, argv, NULL, 0L, 0L, &dummy, TRUE, NULL);
    clear_tv(&argv[0]);
    clear_tv(&rettv);
}

/*
 * Return TRUE when the interrupt callback was invoked.
 */
    int
invoke_prompt_interrupt(void)
{
    typval_T	rettv;
    int		dummy;
    typval_T	argv[1];

    if (curbuf->b_prompt_interrupt.cb_name == NULL
	    || *curbuf->b_prompt_interrupt.cb_name == NUL)
	return FALSE;
    argv[0].v_type = VAR_UNKNOWN;

    got_int = FALSE; // don't skip executing commands
    call_callback(&curbuf->b_prompt_interrupt, -1,
	      &rettv, 0, argv, NULL, 0L, 0L, &dummy, TRUE, NULL);
    clear_tv(&rettv);
    return TRUE;
}

#endif /* FEAT_JOB_CHANNEL */
