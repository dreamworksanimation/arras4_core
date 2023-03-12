arras4monitor queries the coordinator and the nodes for detailed information
about running sessions. Like “top” is refreshes periodically with selectable
fields (--format) and can be sorted by any field (--sort).

When the output is to a terminal If the columns don’t fit in the width of the
terminal fields will be clipped rather than wrapping and If all of the sessions
don’t fit on the height of the terminal the lower ones will be clipped rather
than scrolling.

When the output isn’t a terminal (i.e. when the output is directed to a file or
to a pipe) then all te columns and all of the sessions are written.

    --user
    -u
    --user=username
    -u username
        Specify a user whose sessions should be shown. If no username is
        provided then the current user will be user. Without this option all
        users will be shown.

    --session=sessionid[{,sessionid]
    -s sessionid[,sessionid]
        Specify a session or sessions to be displayed. The option allows a
        comma separated list and/or the option can be provided multiple times.
        Without this option all of the sessions are shown.

    --brief
    -b
        Only provide session summaries and no per computation detail

    --delay=seconds
    -d seconds
        How long  to wait, in seconds, between refreshes

    --n=iterations
    -n iterations
        The number of iterations to display. By default it iterates forever.
        This is mainly useful for just doing a single printout with -n 1

    --env=environment
    -e environment
    --local
    -l
        specify the environment to be displayed. the current choices are stb,
        stb2, uns, or prod. the default is stb. the special “local” option
        uses a hard coded port for the coordinator (localhost:8087) which
        would normally only be used by arras developers.

    --dc={datacenter}
        Specified the datacenter. The current current choices are las and gld.
        The default is gld.

    --logs=# of lines
        Show the last # lines of the session logs with each session.
        The default is 0.

    --wraplogs
    -w
        Wrap long log lines onto subsequent lines. By default long log lines
        are truncated to the terminal  width.

    --format=column[,column]
    --f column[,column]
        Specify the data to be displayed. The option allows a comma separated
        list and/or the option can be provided multiple times.

        Not providing this options is currently the equivalent of 
        --format=id,user,name,node,cpu,maxcpu,cputime,lastsent,lastrcvd,beat

    fid
    id
        The session id. “id” is an abbreviated UUID which should generally be
        unique enough while “fid” is the full id. Computation rows will be
        blank. The column is labeled “ID”.

        name
            The name of the computation. The session rows will be blank. The
            column is labeled “NAME”.

        user
            The user who created the session. Computation rows will be blank.
            The column is labeled “USER”.

        node
            The system the computation is running on. The session row shows
            the system that is the entry node for the session. The column is
            labeled “NODE”.

        compstat
            This is string which can be sent to anything by the computation to
            indicate what it is up to. This is not currently used by any
            computations.

        execstat
            The execution status of the computation. This will usually just
            be “running” but can be any of

            launching
                The computation process is being forked
            ready
	        The computation process has connected to the node
            running
	        The computation has been told to run normally
            stopping
                A stop request has been sent to the computation
            stopped
                The computation has stopped
            sent sigterm
                The computation didn’t exit when requested after 5 seconds so
                a SIGTERM was sent.
            sent sigkill
                The computation didn’t exit from a SIGTERM after 5 seconds so
                a SIGKILL was sent.
            uninterruptible
                All attempts to kill the computation process have failed. In
                modern Linux this should be extremely rare.

        reason
            The reason the reason the computation stopped. The session row
            will be blank. The column is labeled “STOP REASON”
        sig
            The signal (if any) which caused the computation to exit. The
            session row will be blank. The column is labeled “SIGNAL”.
        cpu
            The % of cpu usage in a recent 5 second interval. The session row
            shows a total for all of the computation in the session. The
            columns is labeled “%CPU”.
        maxcpu
            The maximum % of cpu in any of the 5 second intervals in the life
            of the computation. The session row shows a total for all
            computations in the session. The column is labeled “MAX%CPU”.
        cpu60
            The % of cpu usage in a recent 60 second interval. The session row
            shows total for all of the computation in the session. The column
            is labeled “%CPU60”.
        maxcpu60
T           he maximum % of cpu in any of the 60 second intervals in the life
            of the computation. The session row shows a total for all
            computations in the session. The column is labeled “MAX%CPU60”
        cputime
            The total cpu time used by the computation in its lifetime in
            hours:minutes:seconds. The session row shows a total for all
            computations in the session. The column is labeled “CPU_TIME”.
        sent5
            The number of messages the computation sent in a recent 5 second
            interval. The session row shows a total for all computations in
            the session. The column is labeled “SENT5”.
        send60
            The number of messages the computation sent in a recent 60 second
            interval. The session row shows a total for all computations in
            the session. The column is labeled “SENT60”.
        sent
            The total number of messages the computation has sent in its
            lifetime. The session row shows a total for all computations in
            the session. The column is labeled “SENT”.
        lastsent
            The time the most recent message from the computation was sent.
            For brevity the only time (not the date) shown if the message was
            sent today. If it was sent on a previous day only the date is
            shown. The session row shows the most recent send time of all of
            the computations in the session. The column is labeled “LAST_SENT”.
        rcvd5
            The number of messages the computation received in a recent 5
            second interval. The session row shows a total for all computations
            in the session. The column is labeled “RECEIVED5”.
        rcvd60
            The number of messages the computation received in a recent 60
            second interval. The session row shows a total for all computations
            in the session. The column is labeled “RECEIVED60”.
        rcvd
            The total number of messages the computation has received in its
            lifetime. The session row shows a total for all computations in
            the session. The column is labeled “RECEIVED”.
        lastrecvd
            The time the most recent message to the computation was received.
            For brevity the only time (not the date) is shown if the message
            was sent today. If it was sent on a previous day only the date is
            shown. The session row shows the most recent receive time of all
            the computations. The columns is labeled “LAST_RECEIVED”.

        beat

            The time of the most recent heartbeat message from the computation
            to node. These are supposed to be every 5 seconds. For brevity the
            only time (not the date) is shown if the heartbeat was today. If
            it is from a previous day only the date is shown. The session show
            shows the most recent heartbeat of all the computations in the
            session. The column is labeled “HEARTBEAT”

        mem
            The amount of memory the computation is currently using in
            megabytes. The session row shows the total for all of the
            computations in the session. The column is labeled “MEM_MB”.

            The measure is currently the amount of virtual memory the process
            is using rather than the amount of physical and swap memory.
            Virtual memory can be notoriously high due to allocated virtual
            memory which isn’t actually used. This measurement will hopefully
            be fixed soon.

        maxmem
            The maximum amount of the memory the computation has used in its
            lifetime in megabytes. The session row shows the total for all
            computations in the session. The column is labeled “MAX_MEM_MB”.

            This can be useful to see how the computation did relative to its
            allocation or for seeing how the current usage is relative to its
            maximum.

        rmem
            The amount of memory that was reserved for the computation in
            megabytes. The session row shows the total for all of the
            computations in the session. The column is labeled “RSV_MEMORY”.

            This can be useful to compare against the actual memory usage.

        cores
            The number of cores which is allocated to this computation. The
            session row shows the total for all of the computations in the
            session. The column is labeled “CORES”.

            This can be useful for comparing against the max and current cpu
            usage.

    --sort=field[,field]
        Specify the sorting of the sessions. The option allows a comma
        separated list and/or the option can be provided multiple times though
        many of the values will provide unique sorting on their own making
        additional items unimportant. The direction of the sort can be
        reversed using tilde (‘~’). (e.g. --sort=user,~cpu will sort by
        alphabetical and reverse sort cpu). See --format for a list of the
        fields. The field doesn’t need to be displayed to be used for sorting.

        Not providing this option is currently the equivalent of “--sort=~cpu”

        When the fields don’t all fit on the screen the extra fields will be
        clipped out and the right edge will have ‘>’ characters.

ID   	USER   	NAME         	   NODE   	  %CPU   CPU_TIME LAST_SENT  >
9debfba8 bepease                      ih0043       6.2   1:05:22  ???        >
                     mcrt             ih0043       6.2   1:05:22  ???        >
12d6f4f7 bmcconnell                   rubyharp     6.4  23:23:32  2018-12-21 >
                     mcrt             rubyharp     3.8  15:05:11  2018-12-21 >
                     rdla_conditioner rubyharp     2.6   8:18:21  2018-12-21 >
635201e2 cdugenio                     ih0040       6.2   0:00:14  ???        >
                     mcrt             ih0040       6.2   0:00:14  ???        >
33de110f gigi                         tealjudge    5.8  27:12:08  2018-12-12 >
                     render           tealjudge    5.8  27:12:08  2018-12-12 >
03f122c2 gigi                         tealjudge    6.0  27:13:53  2018-12-12 >
                     render           tealjudge    6.0  27:13:53  2018-12-12 >
21b2cb20 hnakamura                    rubyrain  3257.0   0:29:24  10:23:16   >
                     mcrt             rubyrain  3255.0   0:29:19  10:23:16   >
                     rdla_conditioner rubyrain     2.0	 0:00:05  10:21:00   >
c7bc1ede jguinea                      rubywarden   6.0   1:31:13  2019-01-03 >
                     mcrt             rubywarden   3.4   1:08:29  2019-01-03 >
                     rdla_conditioner rubywarden   2.6   0:22:44  2019-01-03 >
