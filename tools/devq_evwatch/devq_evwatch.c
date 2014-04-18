/*
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <libdevq.h>

int
main(int argc, char **argv)
{
	struct devq_evmon *e;
	struct devq_event *ev;
	bool verbose = false;

	if (argc == 2 && strcmp(argv[1], "-v") == 0)
		verbose = true;

	e = devq_event_monitor_init();

	while (devq_event_monitor_poll(e)) {
		ev = devq_event_monitor_read(e);
		if (ev == NULL)
			break;

		switch (devq_event_get_type(ev)) {
		case DEVQ_ATTACHED:
			printf("New device attached\n");
			break;
		case DEVQ_DETACHED:
			printf("A device has been detached\n");
			break;
		case DEVQ_NOTICE:
			printf("Notice received\n");
			break;
		case DEVQ_UNKNOWN:
			printf("Unknown event\n");
			break;
		}
		
		if (verbose)
			printf("%s", devq_event_dump(ev));
		devq_event_free(ev);
	}

	devq_event_monitor_fini(e);

	return (EXIT_SUCCESS);
}
